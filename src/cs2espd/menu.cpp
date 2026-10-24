#include "menu.hpp"

#include "common/log.hpp"

#include <X11/keysym.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace cs2esp {
namespace {

inline unsigned long argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<unsigned long>(a) << 24) |
           (static_cast<unsigned long>(r) << 16) |
           (static_cast<unsigned long>(g) << 8)  |
           static_cast<unsigned long>(b);
}

const std::vector<std::string> kAimbotHotkeys = {
    "always", "alt", "shift", "ctrl", "caps", "c", "v", "x", "z", "mouse1", "mouse2"
};
const std::vector<std::string> kBones = {"head", "chest"};
const std::vector<float> kAimbotFovs = {40.0f, 60.0f, 80.0f, 120.0f, 160.0f, 200.0f, 250.0f, 300.0f};
const std::vector<float> kAimbotSmooths = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f, 10.0f, 15.0f};

const std::vector<std::string> kTriggerHotkeys = {
    "always", "alt", "shift", "ctrl", "caps", "x", "c", "v", "z", "mouse1", "mouse2"
};
const std::vector<int> kPreDelays = {0, 5, 10, 15, 20, 25, 30, 40, 50};
const std::vector<int> kPostDelays = {10, 15, 20, 25, 30, 40, 50};
const std::vector<int> kCooldowns = {60, 80, 100, 120, 150, 200, 250, 300};
const std::vector<float> kMaxDists = {0.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 8000.0f};
const std::vector<int> kFpsLimits = {60, 120, 144, 165, 240, 360};
const std::vector<std::string> kEspHotkeys = {"F8", "F7", "F9", "F10", "F11", "F12"};
const std::vector<float> kRcsScales = {0.0f, 0.25f, 0.5f, 0.75f, 0.85f, 1.0f, 1.15f, 1.25f, 1.5f};
const std::vector<float> kRcsSensitivities = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 3.0f, 3.5f, 4.0f};

template <typename T>
int find_idx(const std::vector<T>& list, const T& val) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] == val) return static_cast<int>(i);
    }
    return 0;
}

template <typename T>
void cycle_next(const std::vector<T>& list, T& val) {
    int idx = find_idx(list, val);
    idx = (idx + 1) % static_cast<int>(list.size());
    val = list[idx];
}

template <typename T>
void cycle_prev(const std::vector<T>& list, T& val) {
    int idx = find_idx(list, val);
    idx = (idx - 1 + static_cast<int>(list.size())) % static_cast<int>(list.size());
    val = list[idx];
}

} // namespace

Menu::Menu(Config& cfg, std::string config_path,
           std::atomic<bool>& esp_on, std::atomic<bool>& aimbot_on,
           std::atomic<bool>& trigger_on, std::atomic<bool>& rcs_on)
    : cfg_(cfg), config_path_(std::move(config_path)),
      esp_on_(esp_on), aimbot_on_(aimbot_on), trigger_on_(trigger_on), rcs_on_(rcs_on) {}

void Menu::toggle() {
    if (open_) close();
    else open();
}

void Menu::open() {
    open_ = true;
    selected_idx_ = 0;
}

void Menu::close() {
    open_ = false;
}

XRectangle Menu::rect(int screen_w, int screen_h) const {
    win_x_ = (screen_w - win_w_) / 2;
    win_y_ = (screen_h - win_h_) / 2;
    if (win_x_ < 20) win_x_ = 20;
    if (win_y_ < 20) win_y_ = 20;

    XRectangle r;
    r.x = static_cast<short>(win_x_);
    r.y = static_cast<short>(win_y_);
    r.width = static_cast<unsigned short>(win_w_);
    r.height = static_cast<unsigned short>(win_h_);
    return r;
}

int Menu::item_count() const {
    switch (current_tab_) {
        case MenuTab::Esp:        return 9;
        case MenuTab::Aimbot:     return 8;
        case MenuTab::Triggerbot: return 7;
        case MenuTab::Rcs:        return 5;
        case MenuTab::Binds:      return 7;
        default:                  return 0;
    }
}

void Menu::next_item() {
    const int count = item_count();
    if (count > 0) {
        selected_idx_ = (selected_idx_ + 1) % count;
    }
}

void Menu::prev_item() {
    const int count = item_count();
    if (count > 0) {
        selected_idx_ = (selected_idx_ - 1 + count) % count;
    }
}

bool Menu::handle_key(KeySym sym) {
    if (!open_) return false;

    if (sym == XK_Tab) {
        current_tab_ = static_cast<MenuTab>((static_cast<int>(current_tab_) + 1) % static_cast<int>(MenuTab::Count));
        selected_idx_ = 0;
        return true;
    }
    if (sym == XK_1 || sym == XK_KP_1) { current_tab_ = MenuTab::Esp; selected_idx_ = 0; return true; }
    if (sym == XK_2 || sym == XK_KP_2) { current_tab_ = MenuTab::Aimbot; selected_idx_ = 0; return true; }
    if (sym == XK_3 || sym == XK_KP_3) { current_tab_ = MenuTab::Triggerbot; selected_idx_ = 0; return true; }
    if (sym == XK_4 || sym == XK_KP_4) { current_tab_ = MenuTab::Rcs; selected_idx_ = 0; return true; }
    if (sym == XK_5 || sym == XK_KP_5) { current_tab_ = MenuTab::Binds; selected_idx_ = 0; return true; }

    if (sym == XK_Up)    { prev_item(); return true; }
    if (sym == XK_Down)  { next_item(); return true; }
    if (sym == XK_Left)  { item_left(); return true; }
    if (sym == XK_Right) { item_right(); return true; }
    if (sym == XK_Return || sym == XK_KP_Enter || sym == XK_space) {
        item_activate();
        return true;
    }
    return false;
}

void Menu::item_left() {
    if (selected_idx_ == 0) {
        current_tab_ = static_cast<MenuTab>((static_cast<int>(current_tab_) - 1 + static_cast<int>(MenuTab::Count)) % static_cast<int>(MenuTab::Count));
        return;
    }

    if (current_tab_ == MenuTab::Esp) {
        if (selected_idx_ == 1) esp_on_.store(!esp_on_.load());
        else if (selected_idx_ == 2) cfg_.esp.show_skeleton = !cfg_.esp.show_skeleton;
        else if (selected_idx_ == 3) cfg_.esp.box_height_units = (cfg_.esp.box_height_units > 0) ? 0.0f : 70.0f;
        else if (selected_idx_ == 4) cfg_.esp.show_names = !cfg_.esp.show_names;
        else if (selected_idx_ == 5) cfg_.esp.show_distance = !cfg_.esp.show_distance;
        else if (selected_idx_ == 6) cfg_.esp.health_bar_width = (cfg_.esp.health_bar_width > 0) ? 0.0f : 6.0f;
        else if (selected_idx_ == 7) cfg_.team_check = !cfg_.team_check;
        else if (selected_idx_ == 8) cycle_prev(kMaxDists, cfg_.max_distance);
    } else if (current_tab_ == MenuTab::Aimbot) {
        if (selected_idx_ == 1) aimbot_on_.store(!aimbot_on_.load());
        else if (selected_idx_ == 2) cycle_prev(kBones, cfg_.aimbot.bone);
        else if (selected_idx_ == 3) cycle_prev(kAimbotHotkeys, cfg_.aimbot.hotkey);
        else if (selected_idx_ == 4) cycle_prev(kAimbotFovs, cfg_.aimbot.fov);
        else if (selected_idx_ == 5) cycle_prev(kAimbotSmooths, cfg_.aimbot.smooth);
        else if (selected_idx_ == 6) cfg_.aimbot.visible_check = !cfg_.aimbot.visible_check;
        else if (selected_idx_ == 7) cfg_.aimbot.team_check = !cfg_.aimbot.team_check;
    } else if (current_tab_ == MenuTab::Triggerbot) {
        if (selected_idx_ == 1) trigger_on_.store(!trigger_on_.load());
        else if (selected_idx_ == 2) cfg_.team_check = !cfg_.team_check;
        else if (selected_idx_ == 3) cycle_prev(kTriggerHotkeys, cfg_.triggerbot.hotkey);
        else if (selected_idx_ == 4) cycle_prev(kPreDelays, cfg_.triggerbot.pre_delay_ms);
        else if (selected_idx_ == 5) cycle_prev(kPostDelays, cfg_.triggerbot.post_delay_ms);
        else if (selected_idx_ == 6) cycle_prev(kCooldowns, cfg_.triggerbot.cooldown_ms);
    } else if (current_tab_ == MenuTab::Rcs) {
        if (selected_idx_ == 1) rcs_on_.store(!rcs_on_.load());
        else if (selected_idx_ == 2) cycle_prev(kRcsScales, cfg_.rcs.scale_y);
        else if (selected_idx_ == 3) cycle_prev(kRcsScales, cfg_.rcs.scale_x);
        else if (selected_idx_ == 4) cycle_prev(kRcsSensitivities, cfg_.rcs.sensitivity);
    } else if (current_tab_ == MenuTab::Binds) {
        if (selected_idx_ == 2) cycle_prev(kEspHotkeys, cfg_.hotkey);
        else if (selected_idx_ == 3) cycle_prev(kAimbotHotkeys, cfg_.aimbot.hotkey);
        else if (selected_idx_ == 4) cycle_prev(kTriggerHotkeys, cfg_.triggerbot.hotkey);
        else if (selected_idx_ == 5) cycle_prev(kFpsLimits, cfg_.fps_limit);
        else if (selected_idx_ == 6) item_activate();
    }
}

void Menu::item_right() {
    if (selected_idx_ == 0) {
        current_tab_ = static_cast<MenuTab>((static_cast<int>(current_tab_) + 1) % static_cast<int>(MenuTab::Count));
        return;
    }

    if (current_tab_ == MenuTab::Esp) {
        if (selected_idx_ == 1) esp_on_.store(!esp_on_.load());
        else if (selected_idx_ == 2) cfg_.esp.show_skeleton = !cfg_.esp.show_skeleton;
        else if (selected_idx_ == 3) cfg_.esp.box_height_units = (cfg_.esp.box_height_units > 0) ? 0.0f : 70.0f;
        else if (selected_idx_ == 4) cfg_.esp.show_names = !cfg_.esp.show_names;
        else if (selected_idx_ == 5) cfg_.esp.show_distance = !cfg_.esp.show_distance;
        else if (selected_idx_ == 6) cfg_.esp.health_bar_width = (cfg_.esp.health_bar_width > 0) ? 0.0f : 6.0f;
        else if (selected_idx_ == 7) cfg_.team_check = !cfg_.team_check;
        else if (selected_idx_ == 8) cycle_next(kMaxDists, cfg_.max_distance);
    } else if (current_tab_ == MenuTab::Aimbot) {
        if (selected_idx_ == 1) aimbot_on_.store(!aimbot_on_.load());
        else if (selected_idx_ == 2) cycle_next(kBones, cfg_.aimbot.bone);
        else if (selected_idx_ == 3) cycle_next(kAimbotHotkeys, cfg_.aimbot.hotkey);
        else if (selected_idx_ == 4) cycle_next(kAimbotFovs, cfg_.aimbot.fov);
        else if (selected_idx_ == 5) cycle_next(kAimbotSmooths, cfg_.aimbot.smooth);
        else if (selected_idx_ == 6) cfg_.aimbot.visible_check = !cfg_.aimbot.visible_check;
        else if (selected_idx_ == 7) cfg_.aimbot.team_check = !cfg_.aimbot.team_check;
    } else if (current_tab_ == MenuTab::Triggerbot) {
        if (selected_idx_ == 1) trigger_on_.store(!trigger_on_.load());
        else if (selected_idx_ == 2) cfg_.team_check = !cfg_.team_check;
        else if (selected_idx_ == 3) cycle_next(kTriggerHotkeys, cfg_.triggerbot.hotkey);
        else if (selected_idx_ == 4) cycle_next(kPreDelays, cfg_.triggerbot.pre_delay_ms);
        else if (selected_idx_ == 5) cycle_next(kPostDelays, cfg_.triggerbot.post_delay_ms);
        else if (selected_idx_ == 6) cycle_next(kCooldowns, cfg_.triggerbot.cooldown_ms);
    } else if (current_tab_ == MenuTab::Rcs) {
        if (selected_idx_ == 1) rcs_on_.store(!rcs_on_.load());
        else if (selected_idx_ == 2) cycle_next(kRcsScales, cfg_.rcs.scale_y);
        else if (selected_idx_ == 3) cycle_next(kRcsScales, cfg_.rcs.scale_x);
        else if (selected_idx_ == 4) cycle_next(kRcsSensitivities, cfg_.rcs.sensitivity);
    } else if (current_tab_ == MenuTab::Binds) {
        if (selected_idx_ == 2) cycle_next(kEspHotkeys, cfg_.hotkey);
        else if (selected_idx_ == 3) cycle_next(kAimbotHotkeys, cfg_.aimbot.hotkey);
        else if (selected_idx_ == 4) cycle_next(kTriggerHotkeys, cfg_.triggerbot.hotkey);
        else if (selected_idx_ == 5) cycle_next(kFpsLimits, cfg_.fps_limit);
        else if (selected_idx_ == 6) item_activate();
    }
}

void Menu::item_activate() {
    if (selected_idx_ == 0) {
        current_tab_ = static_cast<MenuTab>((static_cast<int>(current_tab_) + 1) % static_cast<int>(MenuTab::Count));
        return;
    }

    if (current_tab_ == MenuTab::Esp) {
        if (selected_idx_ == 1) esp_on_.store(!esp_on_.load());
        else if (selected_idx_ == 2) cfg_.esp.show_skeleton = !cfg_.esp.show_skeleton;
        else if (selected_idx_ == 3) cfg_.esp.box_height_units = (cfg_.esp.box_height_units > 0) ? 0.0f : 70.0f;
        else if (selected_idx_ == 4) cfg_.esp.show_names = !cfg_.esp.show_names;
        else if (selected_idx_ == 5) cfg_.esp.show_distance = !cfg_.esp.show_distance;
        else if (selected_idx_ == 6) cfg_.esp.health_bar_width = (cfg_.esp.health_bar_width > 0) ? 0.0f : 6.0f;
        else if (selected_idx_ == 7) cfg_.team_check = !cfg_.team_check;
        else if (selected_idx_ == 8) cycle_next(kMaxDists, cfg_.max_distance);
    } else if (current_tab_ == MenuTab::Aimbot) {
        if (selected_idx_ == 1) aimbot_on_.store(!aimbot_on_.load());
        else if (selected_idx_ == 2) cycle_next(kBones, cfg_.aimbot.bone);
        else if (selected_idx_ == 3) cycle_next(kAimbotHotkeys, cfg_.aimbot.hotkey);
        else if (selected_idx_ == 4) cycle_next(kAimbotFovs, cfg_.aimbot.fov);
        else if (selected_idx_ == 5) cycle_next(kAimbotSmooths, cfg_.aimbot.smooth);
        else if (selected_idx_ == 6) cfg_.aimbot.visible_check = !cfg_.aimbot.visible_check;
        else if (selected_idx_ == 7) cfg_.aimbot.team_check = !cfg_.aimbot.team_check;
    } else if (current_tab_ == MenuTab::Triggerbot) {
        if (selected_idx_ == 1) trigger_on_.store(!trigger_on_.load());
        else if (selected_idx_ == 2) cfg_.team_check = !cfg_.team_check;
        else if (selected_idx_ == 3) cycle_next(kTriggerHotkeys, cfg_.triggerbot.hotkey);
        else if (selected_idx_ == 4) cycle_next(kPreDelays, cfg_.triggerbot.pre_delay_ms);
        else if (selected_idx_ == 5) cycle_next(kPostDelays, cfg_.triggerbot.post_delay_ms);
        else if (selected_idx_ == 6) cycle_next(kCooldowns, cfg_.triggerbot.cooldown_ms);
    } else if (current_tab_ == MenuTab::Rcs) {
        if (selected_idx_ == 1) rcs_on_.store(!rcs_on_.load());
        else if (selected_idx_ == 2) cycle_next(kRcsScales, cfg_.rcs.scale_y);
        else if (selected_idx_ == 3) cycle_next(kRcsScales, cfg_.rcs.scale_x);
        else if (selected_idx_ == 4) cycle_next(kRcsSensitivities, cfg_.rcs.sensitivity);
    } else if (current_tab_ == MenuTab::Binds) {
        if (selected_idx_ == 2) cycle_next(kEspHotkeys, cfg_.hotkey);
        else if (selected_idx_ == 3) cycle_next(kAimbotHotkeys, cfg_.aimbot.hotkey);
        else if (selected_idx_ == 4) cycle_next(kTriggerHotkeys, cfg_.triggerbot.hotkey);
        else if (selected_idx_ == 5) cycle_next(kFpsLimits, cfg_.fps_limit);
        else if (selected_idx_ == 6) {
            std::string err;
            if (save(err)) {
                status_msg_ = "Config saved to " + config_path_;
            } else {
                status_msg_ = "Save failed: " + err;
            }
            status_time_ = std::chrono::steady_clock::now();
        }
    }
}

bool Menu::save(std::string& err) {
    cfg_.esp.show_status_text = true;
    cfg_.aimbot.enabled = aimbot_on_.load();
    cfg_.triggerbot.enabled = trigger_on_.load();
    cfg_.rcs.enabled = rcs_on_.load();
    return save_config(config_path_, cfg_, err);
}

bool Menu::handle_click(int button, int x, int y) {
    if (!open_) return false;
    if (button != 1) return false; // Left click

    int rx = x - win_x_;
    int ry = y - win_y_;

    if (rx < 0 || rx > win_w_ || ry < 0 || ry > win_h_) {
        return false;
    }

    // Клик по 5 вкладкам (y: 35..65)
    if (ry >= 35 && ry <= 65) {
        int tab_w = win_w_ / 5;
        int clicked_tab = rx / tab_w;
        if (clicked_tab >= 0 && clicked_tab < 5) {
            current_tab_ = static_cast<MenuTab>(clicked_tab);
            selected_idx_ = 0;
            return true;
        }
    }

    // Клик по строкам элементов (y: 72 .. win_h_ - 35)
    int row_start_y = 72;
    int row_h = 28;
    if (ry >= row_start_y) {
        int clicked_row = (ry - row_start_y) / row_h;
        if (clicked_row >= 0 && clicked_row < item_count()) {
            selected_idx_ = clicked_row;
            if (rx > win_w_ / 2) {
                item_right();
            } else {
                item_activate();
            }
            return true;
        }
    }

    return true;
}

void Menu::init_colors(Display* dpy, Visual* visual, Colormap cmap) {
    if (colors_init_) return;

    auto alloc = [&](uint16_t r, uint16_t g, uint16_t b, XftColor& out) {
        XRenderColor rc;
        rc.red = r; rc.green = g; rc.blue = b; rc.alpha = 0xFFFF;
        XftColorAllocValue(dpy, visual, cmap, &rc, &out);
    };

    alloc(0xFFFF, 0xFFFF, 0xFFFF, col_white_);
    alloc(0x9494, 0xA3A3, 0xB8B8, col_gray_);
    alloc(0x6464, 0x7474, 0x8B8B, col_dim_);
    alloc(0x3B3B, 0x8282, 0xF6F6, col_blue_);
    alloc(0x3838, 0xBDBD, 0xF8F8, col_cyan_);
    alloc(0x2222, 0xC5C5, 0x5E5E, col_green_);
    alloc(0xEFEF, 0x4444, 0x4444, col_red_);
    alloc(0xFBFB, 0xB4B4, 0x1A1A, col_yellow_);

    colors_init_ = true;
}

void Menu::draw(Display* dpy, Window win, GC gc, XftDraw* xft_draw, XftFont* font,
                Visual* visual, Colormap cmap, int screen_w, int screen_h) {
    if (!open_ || !gc || !xft_draw || !font) return;

    init_colors(dpy, visual, cmap);

    rect(screen_w, screen_h);

    const int x = win_x_;
    const int y = win_y_;
    const int w = win_w_;
    const int h = win_h_;

    // 1) Тёмный фон окна (ARGB: alpha=0xF2, dark slate)
    XSetForeground(dpy, gc, argb(0xF2, 0x11, 0x14, 0x1C));
    XFillRectangle(dpy, win, gc, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));

    // 2) Внешняя рамка (акцентный синий #3B82F6)
    XSetForeground(dpy, gc, argb(0xFF, 0x3B, 0x82, 0xF6));
    XDrawRectangle(dpy, win, gc, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));
    XDrawRectangle(dpy, win, gc, x + 1, y + 1, static_cast<unsigned>(w - 2), static_cast<unsigned>(h - 2));

    // 3) Заголовок (header)
    XSetForeground(dpy, gc, argb(0xFF, 0x1A, 0x1F, 0x2C));
    XFillRectangle(dpy, win, gc, x + 2, y + 2, static_cast<unsigned>(w - 4), 30);

    const char* title = "CS2 LINUX EXTERNAL  |  [INSERT / ESC] Close";
    XftDrawStringUtf8(xft_draw, &col_cyan_, font, x + 14, y + 21,
                      reinterpret_cast<const FcChar8*>(title), static_cast<int>(std::strlen(title)));

    // 4) Вкладки (5 Tabs: ESP | AIMBOT | TRIGGERBOT | RCS | BINDS)
    const int tab_w = (w - 4) / 5;
    const char* tab_names[5] = {"[1] ESP", "[2] AIM", "[3] TRIGGER", "[4] RCS", "[5] BINDS"};

    for (int i = 0; i < 5; ++i) {
        int tx = x + 2 + i * tab_w;
        int ty = y + 33;
        bool active = (static_cast<int>(current_tab_) == i);

        XSetForeground(dpy, gc, active ? argb(0xFF, 0x25, 0x63, 0xEB) : argb(0xFF, 0x16, 0x1B, 0x26));
        XFillRectangle(dpy, win, gc, tx, ty, static_cast<unsigned>(tab_w), 26);

        XSetForeground(dpy, gc, argb(0xFF, 0x33, 0x41, 0x55));
        XDrawRectangle(dpy, win, gc, tx, ty, static_cast<unsigned>(tab_w), 26);

        XftColor& txt_col = active ? col_white_ : col_gray_;
        XftDrawStringUtf8(xft_draw, &txt_col, font, tx + 8, ty + 18,
                          reinterpret_cast<const FcChar8*>(tab_names[i]),
                          static_cast<int>(std::strlen(tab_names[i])));
    }

    // 5) Содержимое вкладки
    struct Row {
        std::string label;
        std::string value;
        bool is_on = false;
        bool is_toggle = false;
        bool is_action = false;
    };
    std::vector<Row> rows;

    if (current_tab_ == MenuTab::Esp) {
        rows.push_back({"Tab Category", "< [1] ESP (Left/Right to switch) >", false, false, false});
        rows.push_back({"ESP Master Switch", esp_on_.load() ? "ON" : "OFF", esp_on_.load(), true, false});
        rows.push_back({"Player Skeletons", cfg_.esp.show_skeleton ? "ON" : "OFF", cfg_.esp.show_skeleton, true, false});
        bool boxes_on = (cfg_.esp.box_height_units > 0);
        rows.push_back({"Bounding Boxes", boxes_on ? "ON" : "OFF", boxes_on, true, false});
        rows.push_back({"Player Names", cfg_.esp.show_names ? "ON" : "OFF", cfg_.esp.show_names, true, false});
        rows.push_back({"Distance Display", cfg_.esp.show_distance ? "ON" : "OFF", cfg_.esp.show_distance, true, false});
        bool hp_on = (cfg_.esp.health_bar_width > 0);
        rows.push_back({"Health Bars", hp_on ? "ON" : "OFF", hp_on, true, false});
        rows.push_back({"Team Check (Enemies Only)", cfg_.team_check ? "ON" : "OFF", cfg_.team_check, true, false});
        std::string dist_str = (cfg_.max_distance <= 0) ? "Unlimited" : (std::to_string(static_cast<int>(cfg_.max_distance)) + " units");
        rows.push_back({"Max Render Distance", "< " + dist_str + " >", false, false, false});
    } else if (current_tab_ == MenuTab::Aimbot) {
        rows.push_back({"Tab Category", "< [2] AIMBOT (Left/Right to switch) >", false, false, false});
        rows.push_back({"Aimbot Master Switch", aimbot_on_.load() ? "ON" : "OFF", aimbot_on_.load(), true, false});
        rows.push_back({"Target Bone", "< " + cfg_.aimbot.bone + " >", false, false, false});
        rows.push_back({"Activation Hotkey", "< " + cfg_.aimbot.hotkey + " >", false, false, false});
        rows.push_back({"FOV Radius", "< " + std::to_string(static_cast<int>(cfg_.aimbot.fov)) + " px >", false, false, false});
        rows.push_back({"Smooth Factor", "< " + std::to_string(static_cast<int>(cfg_.aimbot.smooth)) + " >", false, false, false});
        rows.push_back({"Visible Check (No Walls)", cfg_.aimbot.visible_check ? "ON" : "OFF", cfg_.aimbot.visible_check, true, false});
        rows.push_back({"Team Check (No Teammates)", cfg_.aimbot.team_check ? "ON" : "OFF", cfg_.aimbot.team_check, true, false});
    } else if (current_tab_ == MenuTab::Triggerbot) {
        rows.push_back({"Tab Category", "< [3] TRIGGERBOT (Left/Right to switch) >", false, false, false});
        rows.push_back({"Triggerbot Master", trigger_on_.load() ? "ON" : "OFF", trigger_on_.load(), true, false});
        rows.push_back({"Team Check (No Teammates)", cfg_.team_check ? "ON" : "OFF", cfg_.team_check, true, false});
        rows.push_back({"Activation Hotkey", "< " + cfg_.triggerbot.hotkey + " >", false, false, false});
        rows.push_back({"Reaction Pre-delay", "< " + std::to_string(cfg_.triggerbot.pre_delay_ms) + " ms >", false, false, false});
        rows.push_back({"Mouse Click Duration", "< " + std::to_string(cfg_.triggerbot.post_delay_ms) + " ms >", false, false, false});
        rows.push_back({"Cooldown Between Shots", "< " + std::to_string(cfg_.triggerbot.cooldown_ms) + " ms >", false, false, false});
    } else if (current_tab_ == MenuTab::Rcs) {
        rows.push_back({"Tab Category", "< [4] RECOIL CONTROL (RCS) (Left/Right to switch) >", false, false, false});
        rows.push_back({"RCS Master Switch", rcs_on_.load() ? "ON" : "OFF", rcs_on_.load(), true, false});
        char buf_y[32], buf_x[32], buf_s[32];
        std::snprintf(buf_y, sizeof(buf_y), "< %d%% >", static_cast<int>(cfg_.rcs.scale_y * 100.0f + 0.5f));
        std::snprintf(buf_x, sizeof(buf_x), "< %d%% >", static_cast<int>(cfg_.rcs.scale_x * 100.0f + 0.5f));
        std::snprintf(buf_s, sizeof(buf_s), "< %.2f >", cfg_.rcs.sensitivity);
        rows.push_back({"Pitch Control (Vertical Y)", buf_y, false, false, false});
        rows.push_back({"Yaw Control (Horizontal X)", buf_x, false, false, false});
        rows.push_back({"In-Game Sensitivity", buf_s, false, false, false});
    } else if (current_tab_ == MenuTab::Binds) {
        rows.push_back({"Tab Category", "< [5] BINDS & MISC (Left/Right to switch) >", false, false, false});
        rows.push_back({"Menu Toggle Key", "[ INSERT ] (Global)", false, false, false});
        rows.push_back({"ESP Toggle Hotkey", "< " + cfg_.hotkey + " >", false, false, false});
        rows.push_back({"Aimbot Hotkey", "< " + cfg_.aimbot.hotkey + " >", false, false, false});
        rows.push_back({"Triggerbot Hotkey", "< " + cfg_.triggerbot.hotkey + " >", false, false, false});
        rows.push_back({"Overlay Target FPS", "< " + std::to_string(cfg_.fps_limit) + " >", false, false, false});
        rows.push_back({"Save Settings To Disk", "[ PRESS ENTER TO SAVE ]", false, false, true});
    }

    const int row_y_start = y + 70;
    const int row_h = 27;

    for (size_t i = 0; i < rows.size(); ++i) {
        int ry = row_y_start + static_cast<int>(i) * row_h;
        bool selected = (static_cast<int>(i) == selected_idx_);

        if (selected) {
            XSetForeground(dpy, gc, argb(0xCC, 0x1E, 0x29, 0x3B));
            XFillRectangle(dpy, win, gc, x + 8, ry, static_cast<unsigned>(w - 16), static_cast<unsigned>(row_h - 2));

            XSetForeground(dpy, gc, argb(0xFF, 0x38, 0xBD, 0xF8));
            XFillRectangle(dpy, win, gc, x + 8, ry, 3, static_cast<unsigned>(row_h - 2));
        }

        XftColor& lbl_col = selected ? col_white_ : col_gray_;
        XftDrawStringUtf8(xft_draw, &lbl_col, font, x + 20, ry + 17,
                          reinterpret_cast<const FcChar8*>(rows[i].label.c_str()),
                          static_cast<int>(rows[i].label.size()));

        XftColor* val_col = &col_white_;
        if (rows[i].is_toggle) {
            val_col = rows[i].is_on ? &col_green_ : &col_dim_;
        } else if (rows[i].is_action) {
            val_col = &col_yellow_;
        } else {
            val_col = &col_cyan_;
        }

        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, font,
                           reinterpret_cast<const FcChar8*>(rows[i].value.c_str()),
                           static_cast<int>(rows[i].value.size()), &ext);
        int val_x = x + w - 20 - static_cast<int>(ext.width);
        XftDrawStringUtf8(xft_draw, val_col, font, val_x, ry + 17,
                          reinterpret_cast<const FcChar8*>(rows[i].value.c_str()),
                          static_cast<int>(rows[i].value.size()));
    }

    // 6) Нижний футер с подсказками / статусом
    XSetForeground(dpy, gc, argb(0xFF, 0x1A, 0x1F, 0x2C));
    XFillRectangle(dpy, win, gc, x + 2, y + h - 28, static_cast<unsigned>(w - 4), 26);

    const auto now = std::chrono::steady_clock::now();
    if (!status_msg_.empty() && std::chrono::duration_cast<std::chrono::seconds>(now - status_time_).count() < 4) {
        XftDrawStringUtf8(xft_draw, &col_green_, font, x + 16, y + h - 10,
                          reinterpret_cast<const FcChar8*>(status_msg_.c_str()),
                          static_cast<int>(status_msg_.size()));
    } else {
        const char* hint = "NAV: [Up/Down] Item  [Left/Right/Enter] Adjust  [INS/ESC] Close";
        XftDrawStringUtf8(xft_draw, &col_gray_, font, x + 16, y + h - 10,
                          reinterpret_cast<const FcChar8*>(hint), static_cast<int>(std::strlen(hint)));
    }
}

} // namespace cs2esp
