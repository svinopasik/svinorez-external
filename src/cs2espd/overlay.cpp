#include "overlay.hpp"
#include "esp.hpp"
#include "common/keymap.hpp"
#include "common/log.hpp"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unistd.h>

namespace cs2esp {
namespace {

//Глобальный маркер BadAccess — lambda-обработчик без захватов → чистый указатель на функцию
bool g_grab_failed = false;

int grab_error_handler(Display*, XErrorEvent* e) {
    if (e->error_code == BadAccess) {
        g_grab_failed = true;
    }
    return 0;
}

// Глобальный X-хендлер ошибок: по умолчанию Xlib на любую X-ошибку делает exit().
// Нам это не подходит — BadWindow/BadDrawable на гонках (окно умерло между
// запросами) — штатные ситуации: логируем и продолжаем.
int x_error_handler(Display*, XErrorEvent* e) {
    log_throttled("x.error", 5, LogLevel::Warn,
                  std::string("X error: request ") + std::to_string(e->request_code) +
                  ", code " + std::to_string(e->error_code) +
                  " (serial " + std::to_string(e->serial) + ")");
    return 0;
}

int x_io_error_handler(Display*) {
    // libX11 fatal IO error handler (server shutdown or connection closed).
    std::_Exit(1);
}

} // namespace

Overlay::Overlay(Config& cfg, std::string config_path,
                 std::atomic<bool>& esp_on, std::atomic<bool>& aimbot_on,
                 std::atomic<bool>& trigger_on, std::atomic<bool>& rcs_on)
    : cfg_(cfg), menu_(cfg, std::move(config_path), esp_on, aimbot_on, trigger_on, rcs_on) {}

Overlay::~Overlay() {
    if (dpy_) {
        ungrab_menu_navigation();
        if (hotkey_insert_keycode_ != 0) {
            XUngrabKey(dpy_, hotkey_insert_keycode_, AnyModifier, root_);
        }
        if (hotkey_grabbed_ && hotkey_keycode_ != 0) {
            XUngrabKey(dpy_, hotkey_keycode_, AnyModifier, root_);
        }
        destroy_window();
        if (font_) {
            XftFontClose(dpy_, font_);
            font_ = nullptr;
        }
        if (font_color_ok_) {
            XftColorFree(dpy_, visual_, cmap_, &font_color_);
            font_color_ok_ = false;
        }
        if (cmap_) {
            XFreeColormap(dpy_, cmap_);
            cmap_ = 0;
        }
        XCloseDisplay(dpy_);
        dpy_ = nullptr;
    }
}

bool Overlay::init(std::string& error) {
    XSetErrorHandler(x_error_handler);
    XSetIOErrorHandler(x_io_error_handler);

    if (window_class_.empty()) {
        if (cfg_.stealth.randomize_window_class) {
            char buf[32];
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            std::snprintf(buf, sizeof(buf), "wl_srv_%04x", static_cast<unsigned>(now & 0xFFFF));
            window_class_ = buf;
        } else {
            window_class_ = "cs2esp";
        }
    }

    // Автоматическое отключение блюра в Hyprland под класс окна оверлея
    if (std::getenv("HYPRLAND_INSTANCE_SIGNATURE") != nullptr) {
        // 1. Для современного Hyprland с Lua API (hyprland.lua)
        std::string lua_cmd = "hyprctl eval 'hl.window_rule({ match = { class = \"" + window_class_ + "\" }, no_blur = true, no_focus = true, no_initial_focus = true })' >/dev/null 2>&1";
        [[maybe_unused]] int rc1 = ::system(lua_cmd.c_str());
        (void)rc1;

        if (window_class_ != "cs2esp") {
            [[maybe_unused]] int rc3 = ::system("hyprctl eval 'hl.window_rule({ match = { class = \"cs2esp\" }, no_blur = true, no_focus = true, no_initial_focus = true })' >/dev/null 2>&1");
            (void)rc3;
        }

        // 2. Для легаси парсера hyprland.conf (keyword)
        std::string conf_cmd = "hyprctl keyword windowrulev2 'noblur,class:^(" + window_class_ + ")$' >/dev/null 2>&1; "
                               "hyprctl keyword windowrulev2 'nofocus,class:^(" + window_class_ + ")$' >/dev/null 2>&1; "
                               "hyprctl keyword windowrulev2 'noinitialfocus,class:^(" + window_class_ + ")$' >/dev/null 2>&1";
        [[maybe_unused]] int rc2 = ::system(conf_cmd.c_str());
        (void)rc2;
    }

    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        error = "X11: cannot open display — нужен рабочий X-сервер или XWayland "
                "(проверь переменную DISPLAY)";
        return false;
    }
    screen_ = DefaultScreen(dpy_);
    root_ = DefaultRootWindow(dpy_);

    int xrr_event = 0, xrr_error = 0;
    xrandr_ok_ = XRRQueryExtension(dpy_, &xrr_event, &xrr_error) != 0;
    if (!xrandr_ok_) {
        log_warn("XRandR недоступен — оверлей накроет весь root вместо одного монитора");
    }

    XVisualInfo vinfo{};
    if (XMatchVisualInfo(dpy_, screen_, 32, TrueColor, &vinfo) == 0) {
        error = "X11: 32-битный ARGB TrueColor visual не найден — оверлею нужен "
                "compositor (XWayland всегда композитит; bare X11 без picom не подойдёт)";
        return false;
    }
    visual_ = vinfo.visual;
    depth_ = vinfo.depth;

    cmap_ = XCreateColormap(dpy_, root_, visual_, AllocNone);

    // шрифт: сначала из конфига; если fontconfig подставил свой — тоже ок
    font_ = XftFontOpenName(dpy_, screen_, cfg_.esp.font.c_str());
    if (!font_) {
        font_ = XftFontOpenName(dpy_, screen_, "DejaVu Sans:size=10");
        if (!font_) {
            error = "X11/Xft: не удалось открыть ни '" + cfg_.esp.font +
                    "', ни fallback-шрифт DejaVu Sans:size=10";
            return false;
        }
        log_warn("font '" + cfg_.esp.font + "' не открылся — рисуем fallback'DejaVu Sans:size=10'");
    }

    XRenderColor rc;
    rc.red = rc.green = rc.blue = 0xFFFF;
    rc.alpha = 0xFFFF;
    if (XftColorAllocValue(dpy_, visual_, cmap_, &rc, &font_color_)) {
        font_color_ok_ = true;
    }

    select_geometry();
    setup_hotkey();

    log_info("overlay ready: " + std::to_string(head_w_) + "x" + std::to_string(head_h_) +
             " at (" + std::to_string(head_x_) + "," + std::to_string(head_y_) + ")" +
             ", hotkey " + cfg_.hotkey +
             (hotkey_grabbed_ ? " (grabbed)" : " (grab failed — только espctl)"));
    error.clear();
    return true;
}

bool Overlay::select_geometry() {
    XRRMonitorInfo* mons = nullptr;
    int n = 0;
    if (xrandr_ok_) {
        mons = XRRGetMonitors(dpy_, root_, True, &n);
    }

    int want_x = 0, want_y = 0, want_w = 0, want_h = 0;

    int chosen = -1;
    if (n > 0 && mons != nullptr) {
        if (cfg_.esp.monitor >= 0) {
            chosen = std::min(cfg_.esp.monitor, n - 1);
            if (cfg_.esp.monitor >= n) {
                log_throttled("overlay.monitor_idx", 30, LogLevel::Warn,
                              "esp.monitor=" + std::to_string(cfg_.esp.monitor) +
                              " вне диапазона (" + std::to_string(n) + " мониторов) — беру 0");
                chosen = 0;
            }
        } else {
            chosen = pick_auto_monitor(mons, n);
        }
        if (chosen >= 0) {
            const XRRMonitorInfo& m = mons[chosen];
            want_x = m.x;
            want_y = m.y;
            want_w = m.width;
            want_h = m.height;
        }
        XRRFreeMonitors(mons);
    }

    if (chosen < 0) {
        // XRandR нет: весь root как единственный head
        Window rret = 0;
        int rx = 0, ry = 0;
        unsigned rw = 0, rh = 0, bw = 0, dep = 0;
        if (XGetGeometry(dpy_, root_, &rret, &rx, &ry, &rw, &rh, &bw, &dep) == 0) {
            rw = 1920;
            rh = 1080;
        }
        want_x = rx;
        want_y = ry;
        want_w = static_cast<int>(rw);
        want_h = static_cast<int>(rh);
    }

    if (head_x_ != want_x || head_y_ != want_y ||
        head_w_ != want_w || head_h_ != want_h) {
        // тот же кандидат, что в прошлый раз? копим голоса за смену
        if (pend_x_ == want_x && pend_y_ == want_y &&
            pend_w_ == want_w && pend_h_ == want_h) {
            ++pend_hits_;
        } else {
            pend_x_ = want_x;
            pend_y_ = want_y;
            pend_w_ = want_w;
            pend_h_ = want_h;
            pend_hits_ = 1;
        }
        if (pend_hits_ >= 3 || cur_w_ < 0 || win_ == 0) {
            // устоявшийся кандидат — или самый первый выбор при init
            head_x_ = want_x;
            head_y_ = want_y;
            head_w_ = want_w;
            head_h_ = want_h;
            pend_hits_ = 0;
            log_info("overlay target monitor: " + std::to_string(head_w_) + "x" +
                     std::to_string(head_h_) + " at (" + std::to_string(head_x_) +
                     "," + std::to_string(head_y_) + ")");
            create_window();
        }
    } else {
        pend_hits_ = 0;
    }
    return true;
}

// Автовыбор монитора: окно игры по pid → active window → курсор → 0.
int Overlay::pick_auto_monitor(const XRRMonitorInfo* mons, int n) const {
    long cx = -1, cy = -1;

    auto center_of = [&](Window w) -> bool {
        if (w == 0) return false;
        Window rret = 0;
        int x = 0, y = 0;
        unsigned ww = 0, wh = 0, bw = 0, dep = 0;
        if (XGetGeometry(dpy_, w, &rret, &x, &y, &ww, &wh, &bw, &dep) != 0) {
            int rx = 0, ry = 0;
            Window child = 0;
            if (XTranslateCoordinates(dpy_, w, root_, 0, 0, &rx, &ry, &child)) {
                x = rx;
                y = ry;
            }
            if (ww < 100 || wh < 100) return false;
            cx = x + static_cast<long>(ww) / 2;
            cy = y + static_cast<long>(wh) / 2;
            return true;
        }
        return false;
    };

    // 1) окно игры по _NET_WM_PID
    int want_pid = game_pid_.load();
    if (want_pid > 0) {
        Window found = find_window_by_pid(root_, want_pid, 0);
        if (found != 0 && center_of(found)) {
            return monitor_at(mons, n, cx, cy);
        }
    }

    // 2) active window (может быть None=0 — пропускаем)
    Atom prop = XInternAtom(dpy_, "_NET_ACTIVE_WINDOW", False);
    Atom type = 0;
    int fmt = 0;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy_, root_, prop, 0, 1, False, XA_WINDOW, &type, &fmt,
                           &nitems, &bytes_after, &data) == Success &&
        type == XA_WINDOW && nitems == 1 && data != nullptr) {
        const Window aw = *reinterpret_cast<const Window*>(data);
        if (aw != 0 && aw != root_) {
            center_of(aw);
        }
        XFree(data);
    }
    if (cx >= 0) {
        return monitor_at(mons, n, cx, cy);
    }

    // 3) курсор
    Window rret = 0, child = 0;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned mask = 0;
    if (XQueryPointer(dpy_, root_, &rret, &child, &rx, &ry, &wx, &wy, &mask)) {
        return monitor_at(mons, n, rx, ry);
    }
    return 0;
}

int Overlay::monitor_at(const XRRMonitorInfo* mons, int n, long x, long y) const {
    for (int i = 0; i < n; ++i) {
        if (x >= mons[i].x && x < mons[i].x + mons[i].width &&
            y >= mons[i].y && y < mons[i].y + mons[i].height) {
            return i;
        }
    }
    return 0;
}

Window Overlay::find_window_by_pid(Window top, int pid, int depth) const {
    if (depth > 4) return 0;
    Window rret = 0, parent = 0;
    Window* child_array = nullptr;
    unsigned int nchildren = 0;
    if (XQueryTree(dpy_, top, &rret, &parent, &child_array, &nchildren) == 0) return 0;

    Window result = 0;
    if (child_array != nullptr) {
        for (unsigned int i = 0; i < nchildren && result == 0; ++i) {
            const Window w = child_array[i];

            Atom prop = XInternAtom(dpy_, "_NET_WM_PID", False);
            Atom type = 0;
            int fmt = 0;
            unsigned long nitems = 0, bytes_after = 0;
            unsigned char* data = nullptr;
            int match = 0;
            if (XGetWindowProperty(dpy_, w, prop, 0, 1, False, XA_CARDINAL, &type, &fmt,
                                   &nitems, &bytes_after, &data) == Success &&
                type == XA_CARDINAL && nitems == 1 && data != nullptr) {
                const unsigned long prop_pid = *reinterpret_cast<const unsigned long*>(data);
                match = (prop_pid == static_cast<unsigned long>(pid));
                XFree(data);
            }
            if (match) {
                result = w;
            } else {
                result = find_window_by_pid(w, pid, depth + 1);
            }
        }
        XFree((void*)child_array);
    }
    return result;
}

void Overlay::create_window() {
    if (win_ != 0) destroy_window();

    width_.store(head_w_);
    height_.store(head_h_);
    cur_x_ = head_x_;
    cur_y_ = head_y_;
    cur_w_ = head_w_;
    cur_h_ = head_h_;

    const Window parent = root_;

    XSetWindowAttributes attr{};
    attr.colormap = cmap_;
    attr.override_redirect = True;   // нет декора и WM-буферизации
    attr.background_pixel = 0;       // полностью прозрачный ARGB
    attr.border_pixel = 0;
    attr.event_mask = ExposureMask | StructureNotifyMask;

    win_ = XCreateWindow(dpy_, parent, head_x_, head_y_,
                         static_cast<unsigned>(head_w_), static_cast<unsigned>(head_h_),
                         0, depth_, InputOutput, visual_,
                         CWColormap | CWOverrideRedirect | CWBackPixel |
                             CWBorderPixel | CWEventMask,
                         &attr);

    // WM_CLASS: динамический или маскировочный класс окна
    {
        XClassHint ch{};
        ch.res_name  = const_cast<char*>(window_class_.c_str());
        ch.res_class = const_cast<char*>(window_class_.c_str());
        XSetClassHint(dpy_, win_, &ch);
    }

    // Сообщаем XWayland/WM, что оверлей никогда не принимает фокус ввода (InputHint = False),
    // чтобы игра (CS2) никогда не теряла фокус и эксклюзивный захват курсора.
    {
        XWMHints hints{};
        hints.flags = InputHint;
        hints.input = False;
        XSetWMHints(dpy_, win_, &hints);
    }

    // click-through / menu input region
    update_input_shape();

    XMapWindow(dpy_, win_);
    XRaiseWindow(dpy_, win_);

    if (gc_) XFreeGC(dpy_, gc_);
    gc_ = XCreateGC(dpy_, win_, 0, nullptr);

    if (xft_draw_) XftDrawDestroy(xft_draw_);
    xft_draw_ = XftDrawCreate(dpy_, win_, visual_, cmap_);

    last_drawn_.clear();
}

void Overlay::destroy_window() {
    if (xft_draw_) {
        XftDrawDestroy(xft_draw_);
        xft_draw_ = nullptr;
    }
    if (gc_) {
        XFreeGC(dpy_, gc_);
        gc_ = nullptr;
    }
    if (win_ != 0) {
        XDestroyWindow(dpy_, win_);
        win_ = 0;
    }
}

void Overlay::setup_hotkey() {
    if (hotkey_grabbed_ && hotkey_keycode_ != 0) {
        XUngrabKey(dpy_, hotkey_keycode_, AnyModifier, root_);
        hotkey_grabbed_ = false;
        hotkey_keycode_ = 0;
    }

    cached_hotkey_ = cfg_.hotkey;

    hotkey_keycode_ = resolve_hardware_keycode(dpy_, cfg_.hotkey);
    if (hotkey_keycode_ == 0) {
        log_warn("hotkey '" + cfg_.hotkey + "' не смапился в KeyCode — только espctl");
        return;
    }

    g_grab_failed = false;
    XErrorHandler old = XSetErrorHandler(grab_error_handler);
    XGrabKey(dpy_, hotkey_keycode_, AnyModifier, root_, True,
             GrabModeAsync, GrabModeAsync);
    XSync(dpy_, False);
    XSetErrorHandler(old);

    if (g_grab_failed) {
        hotkey_grabbed_ = false;
        log_warn("XGrabKey(" + cfg_.hotkey + ") failed: клавиша занята другим клиентом — "
                 "управление через espctl продолжает работать");
        hotkey_keycode_ = 0;
    } else {
        hotkey_grabbed_ = true;
    }

    // Глобальный перехват клавиши Insert для открытия меню
    if (hotkey_insert_keycode_ == 0) {
        hotkey_insert_keycode_ = resolve_hardware_keycode(dpy_, "insert");
        if (hotkey_insert_keycode_ == 0) {
            hotkey_insert_keycode_ = XKeysymToKeycode(dpy_, XK_Insert);
        }
        if (hotkey_insert_keycode_ == 0) {
            hotkey_insert_keycode_ = 118; // fallback: PC Insert
        }
        const unsigned int mod_masks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask, AnyModifier};
        for (unsigned int m : mod_masks) {
            XGrabKey(dpy_, hotkey_insert_keycode_, m, root_, True,
                     GrabModeAsync, GrabModeAsync);
        }
    }
}

void Overlay::grab_menu_navigation() {
    if (menu_nav_grabbed_) return;
    const KeySym nav_syms[] = {
        XK_Up, XK_Down, XK_Left, XK_Right,
        XK_Return, XK_KP_Enter, XK_Escape
    };
    const unsigned int mod_masks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

    XErrorHandler old = XSetErrorHandler(grab_error_handler);
    for (KeySym s : nav_syms) {
        KeyCode kc = XKeysymToKeycode(dpy_, s);
        if (kc != 0) {
            for (unsigned int m : mod_masks) {
                XGrabKey(dpy_, kc, m, root_, True, GrabModeAsync, GrabModeAsync);
            }
        }
    }
    XSync(dpy_, False);
    XSetErrorHandler(old);
    menu_nav_grabbed_ = true;
}

void Overlay::ungrab_menu_navigation() {
    if (!menu_nav_grabbed_) return;
    const KeySym nav_syms[] = {
        XK_Up, XK_Down, XK_Left, XK_Right,
        XK_Return, XK_KP_Enter, XK_Escape
    };
    const unsigned int mod_masks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

    for (KeySym s : nav_syms) {
        KeyCode kc = XKeysymToKeycode(dpy_, s);
        if (kc != 0) {
            for (unsigned int m : mod_masks) {
                XUngrabKey(dpy_, kc, m, root_);
            }
        }
    }
    menu_nav_grabbed_ = false;
}

void Overlay::update_input_shape() {
    if (win_ == 0) return;

    int shape_event_base = 0, shape_error_base = 0;
    const bool have_shape = XShapeQueryExtension(dpy_, &shape_event_base, &shape_error_base) != 0;

    if (menu_.is_open()) {
        if (have_shape) {
            XRectangle r = menu_.rect(head_w_, head_h_);
            XShapeCombineRectangles(dpy_, win_, ShapeInput, 0, 0, &r, 1, ShapeSet, YXBanded);
        }
        XSelectInput(dpy_, win_, ExposureMask | StructureNotifyMask | KeyPressMask |
                                     ButtonPressMask | PointerMotionMask);
        grab_menu_navigation();
        XRaiseWindow(dpy_, win_);
    } else {
        if (have_shape) {
            XShapeCombineRectangles(dpy_, win_, ShapeInput, 0, 0, nullptr, 0, ShapeSet, YXBanded);
        }
        XSelectInput(dpy_, win_, ExposureMask | StructureNotifyMask);
        ungrab_menu_navigation();
    }
}

void Overlay::clamp_to(int& x, int& y, int& w, int& h) const {
    const int W = width_.load();
    const int H = height_.load();
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w > W - x) w = W - x;
    if (h > H - y) h = H - y;
    if (w <= 0 || h <= 0) { w = 0; h = 0; }
}

void Overlay::track_last_rect(int x0, int y0, int x1, int y1) {
    IRect r;
    r.x = x0;
    r.y = y0;
    r.w = x1 - x0;
    r.h = y1 - y0;
    clamp_to(r.x, r.y, r.w, r.h);
    if (r.w > 0 && r.h > 0) last_drawn_.push_back(r);
}

void Overlay::draw_box(const ScreenBox& b) {
    if (!gc_ || !font_) return;

    const int W = width_.load();
    const int H = height_.load();

    int bx = static_cast<int>(std::lround(b.x));
    int by = static_cast<int>(std::lround(b.y));
    int bw = static_cast<int>(std::lround(b.w));
    int bh = static_cast<int>(std::lround(b.h));
    if (bw <= 0 || bh <= 0) return;

    int bx1 = bx + bw;
    int by1 = by + bh;

    // зона стирания на следующем кадре: рамка + hp-бар + текст над/под боксом
    const int text_h = font_->ascent + font_->descent + 4;
    track_last_rect(bx - static_cast<int>(cfg_.esp.health_bar_width) - 4,
                    by - (b.name.empty() ? 0 : text_h),
                    bx1 + 4,
                    by1 + (cfg_.esp.show_distance ? text_h : 0));

    // тёмный helper: рисование прямоугольников и учёт зон под стирание.
    // Рамка бокса = 4 тонких прямоугольника: без XClearArea-интерьера, который
    // на XWayland+cовместителем периодически не сбрасывает пиксели (мусорные
    // "кляксы" от прошлых кадров).
    auto fill = [&](int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > W) w = W - x;
        if (y + h > H) h = H - y;
        if (w <= 0 || h <= 0) return;
        XFillRectangle(dpy_, win_, gc_, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));
    };
    auto track_text = [&](const std::string& s, int tx, int baseline_y) {
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy_, font_,
                           reinterpret_cast<const FcChar8*>(s.c_str()),
                           static_cast<int>(s.size()), &ext);
        track_last_rect(tx - 2, baseline_y - font_->ascent - 2,
                        tx + static_cast<int>(ext.width) + 2,
                        baseline_y + font_->descent + 2);
    };

    // рамка
    if (cfg_.esp.box_height_units > 0) {
        constexpr int kBorder = 2;
        XSetForeground(dpy_, gc_, b.color);
        fill(bx, by, bw, kBorder);              // top
        fill(bx, by1 - kBorder, bw, kBorder);   // bottom
        fill(bx, by, kBorder, bh);              // left
        fill(bx1 - kBorder, by, kBorder, bh);   // right
    }

    // скелет
    if (!b.skeleton_lines.empty()) {
        XSetForeground(dpy_, gc_, b.color);
        for (const ScreenLine& l : b.skeleton_lines) {
            int x1 = static_cast<int>(std::lround(l.x1));
            int y1 = static_cast<int>(std::lround(l.y1));
            int x2 = static_cast<int>(std::lround(l.x2));
            int y2 = static_cast<int>(std::lround(l.y2));
            XDrawLine(dpy_, win_, gc_, x1, y1, x2, y2);
            track_last_rect(std::min(x1, x2) - 2, std::min(y1, y2) - 2,
                            std::max(x1, x2) + 2, std::max(y1, y2) + 2);
        }
    }

    // hp-полоска слева
    const int bar_w = static_cast<int>(std::lround(cfg_.esp.health_bar_width));
    const float hp_frac = std::clamp(b.health / 100.0f, 0.0f, 1.0f);
    const int bar_h = static_cast<int>(std::lround((bh - 2) * hp_frac));
    if (bar_w > 0 && bar_h > 0) {
        const unsigned r = static_cast<unsigned>(255 * (1.0f - hp_frac));
        const unsigned g = static_cast<unsigned>(255 * hp_frac);
        const unsigned argb = 0xFF000000u | (r << 16) | (g << 8);
        XSetForeground(dpy_, gc_, argb);
        fill(bx - bar_w - 2, by + bh - bar_h, bar_w, bar_h);
    }

    // текст: ник над боксом, дистанция под ним
    if (xft_draw_ && font_color_ok_) {
        char num[32];
        if (!b.name.empty()) {
            const int ty = by - font_->descent;
            if (ty - font_->ascent > 0) {
                track_text(b.name, bx, ty);
                XftDrawStringUtf8(xft_draw_, &font_color_, font_, bx,
                                  ty, reinterpret_cast<const FcChar8*>(b.name.c_str()),
                                  static_cast<int>(b.name.size()));
            }
        }
        if (cfg_.esp.show_distance && b.dist_units > 0.f) {
            static constexpr float kUnitsPerMeter = 1.0f / 0.0254f;
            std::snprintf(num, sizeof(num), "%.1fm", b.dist_units / kUnitsPerMeter);
            const std::string dist_txt(num);
            const int ty = std::min(by1 + font_->ascent, H - 1);
            if (ty - font_->ascent > 0) {
                track_text(dist_txt, bx, ty);
                XftDrawStringUtf8(xft_draw_, &font_color_, font_, bx,
                                  ty, reinterpret_cast<const FcChar8*>(dist_txt.c_str()),
                                  static_cast<int>(dist_txt.size()));
            }
        }
    }
}

void Overlay::draw_status_text(bool esp_on, bool aimbot_on, bool trigger_on, bool rcs_on, bool attached) {
    if (!xft_draw_ || !font_ || !font_color_ok_) return;
    std::string txt = "[LIVE] ";
    txt += esp_on ? "ESP:ON" : "ESP:OFF";
    txt += aimbot_on ? "  AIM:ON" : "  AIM:OFF";
    txt += trigger_on ? "  TRIGGER:ON" : "  TRIGGER:OFF";
    txt += rcs_on ? "  RCS:ON" : "  RCS:OFF";
    txt += "  |  [INS] Menu";
    if (!attached) txt += " (no cs2)";

    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy_, font_, reinterpret_cast<const FcChar8*>(txt.c_str()),
                       static_cast<int>(txt.size()), &ext);
    track_last_rect(6, 6, 8 + static_cast<int>(ext.width) + 2,
                    8 + font_->ascent + font_->descent + 2);
    XftDrawStringUtf8(xft_draw_, &font_color_, font_, 8, 8 + font_->ascent,
                      reinterpret_cast<const FcChar8*>(txt.c_str()),
                      static_cast<int>(txt.size()));
}

bool Overlay::frame(const std::vector<ScreenBox>& boxes, bool esp_on, bool aimbot_on, bool trigger_on, bool rcs_on, bool attached) {
    if (cfg_.hotkey != cached_hotkey_) {
        setup_hotkey();
    }

    if (menu_toggle_req_.exchange(false)) {
        menu_.toggle();
        update_input_shape();
    }
    if (menu_open_req_.exchange(false)) {
        if (!menu_.is_open()) {
            menu_.open();
            update_input_shape();
        }
    }
    if (menu_close_req_.exchange(false)) {
        if (menu_.is_open()) {
            menu_.close();
            update_input_shape();
        }
    }

    // события X: hotkey, expose, menu input, root resize
    while (XPending(dpy_) > 0) {
        XEvent ev;
        XNextEvent(dpy_, &ev);
        if (ev.type == KeyPress) {
            KeySym sym = XLookupKeysym(&ev.xkey, 0);
            if ((hotkey_insert_keycode_ != 0 && ev.xkey.keycode == hotkey_insert_keycode_) ||
                sym == XK_Insert) {
                menu_.toggle();
                update_input_shape();
            } else if (menu_.is_open() && sym == XK_Escape) {
                menu_.close();
                update_input_shape();
            } else if ((hotkey_keycode_ != 0 && ev.xkey.keycode == hotkey_keycode_) ||
                       (hotkey_keysym_ != 0 && sym == hotkey_keysym_)) {
                toggle_flag_.store(true);
            } else if (menu_.is_open()) {
                menu_.handle_key(sym);
            }
        } else if (ev.type == ButtonPress && menu_.is_open()) {
            menu_.handle_click(ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
        }
    }

    // пересчёт геометрии: окно игры переехало на другой монитор / изменилось разрешение
    if ((++frame_counter_ % 300) == 0) {
        select_geometry();
    }

    // гарантированная полная очистка раз в ~90 кадров: убивает любой
    // застрявший мусор, который не попал в last_drawn_ (гонки resize'а и т.п.)
    if ((frame_counter_ % 90) == 0) {
        XClearArea(dpy_, win_, 0, 0, width_.load(), height_.load(), False);
        last_drawn_.clear();
    }

    // стирание прошлого кадра
    for (const IRect& r : last_drawn_) {
        XClearArea(dpy_, win_, r.x, r.y, r.w, r.h, False);
    }
    last_drawn_.clear();

    if (cfg_.esp.show_status_text) {
        draw_status_text(esp_on, aimbot_on, trigger_on, rcs_on, attached);
    }

    if (esp_on) {
        for (const ScreenBox& b : boxes) {
            draw_box(b);
        }
    }

    if (menu_.is_open()) {
        menu_.draw(dpy_, win_, gc_, xft_draw_, font_, visual_, cmap_, head_w_, head_h_);
        XRectangle mr = menu_.rect(head_w_, head_h_);
        track_last_rect(mr.x - 4, mr.y - 4, mr.x + mr.width + 4, mr.y + mr.height + 4);
    }

    XFlush(dpy_);
    return true;
}

bool Overlay::consume_toggle() {
    return toggle_flag_.exchange(false, std::memory_order_relaxed);
}

std::pair<int, int> Overlay::screen_size() const {
    return {width_.load(), height_.load()};
}

} // namespace cs2esp
