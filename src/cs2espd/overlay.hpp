#pragma once

#include "common/config.hpp"
#include "menu.hpp"

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>

#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace cs2esp {

struct ScreenBox;

class Overlay {
public:
    Overlay(Config& cfg, std::string config_path,
            std::atomic<bool>& esp_on, std::atomic<bool>& aimbot_on,
            std::atomic<bool>& trigger_on, std::atomic<bool>& rcs_on);
    ~Overlay();

    bool init(std::string& error);

    // Один кадр: события X (в т.ч. hotkey, меню), стирание прошлого, отрисовка нового.
    // false — X-соединение потеряно, поток рисования завершается.
    bool frame(const std::vector<ScreenBox>& boxes, bool esp_on, bool aimbot_on, bool trigger_on, bool rcs_on, bool attached);

    // true, если F8 нажата с прошлого вызова
    bool consume_toggle();

    // pid процесса игры: для авто-определения того монитора, где игровое окно
    void set_game_pid(int pid) { game_pid_.store(pid); }

    // геометрия головы (атомики), на которую проецируем
    std::pair<int, int> screen_size() const;

    bool is_menu_open() const { return menu_.is_open(); }
    void toggle_menu() { menu_toggle_req_.store(true); }
    void open_menu()   { menu_open_req_.store(true); }
    void close_menu()  { menu_close_req_.store(true); }

private:
    void create_window();
    void destroy_window();
    void setup_hotkey();
    void update_input_shape();
    void grab_menu_navigation();
    void ungrab_menu_navigation();

    // выбрать монитор (config.esp.monitor или авто по окну игры/курсору);
    // при изменении — пересоздать окно на новом head. false — X-коннект умер.
    bool select_geometry();
    int  pick_auto_monitor(const XRRMonitorInfo* mons, int n) const;
    int  monitor_at(const XRRMonitorInfo* mons, int n, long x, long y) const;
    Window find_window_by_pid(Window top, int pid, int depth) const;
    void clamp_to(int& x, int& y, int& w, int& h) const;

    void draw_box(const ScreenBox& b);
    void draw_status_text(bool esp_on, bool aimbot_on, bool trigger_on, bool rcs_on, bool attached);
    void track_last_rect(int x0, int y0, int x1, int y1);

    struct IRect {
        int x = 0, y = 0, w = 0, h = 0;
    };

    Config& cfg_;
    Menu menu_;

    Display* dpy_ = nullptr;
    int screen_ = 0;
    Window root_ = 0;
    Window win_ = 0;
    Visual* visual_ = nullptr;
    int depth_ = 24;
    Colormap cmap_ = 0;
    GC gc_ = nullptr;
    XftDraw* xft_draw_ = nullptr;
    XftFont* font_ = nullptr;
    XftColor font_color_{};
    bool font_color_ok_ = false;

    KeySym hotkey_keysym_ = 0;
    KeyCode hotkey_keycode_ = 0;
    bool hotkey_grabbed_ = false;
    std::string cached_hotkey_;

    KeyCode hotkey_insert_keycode_ = 0;
    bool menu_nav_grabbed_ = false;

    std::atomic<bool> toggle_flag_{false};
    std::atomic<bool> menu_toggle_req_{false};
    std::atomic<bool> menu_open_req_{false};
    std::atomic<bool> menu_close_req_{false};

    bool xrandr_ok_ = false;
    // геометрия выбранного head'а (где окно игры / курсор / конфиг-индекс)
    int head_x_ = 0, head_y_ = 0, head_w_ = 0, head_h_ = 0;
    // что реально занимал оверлей в последний create_window()
    int cur_x_ = -1, cur_y_ = -1, cur_w_ = -1, cur_h_ = -1;
    std::atomic<int> game_pid_{0};

    int pend_x_ = 0, pend_y_ = 0, pend_w_ = 0, pend_h_ = 0, pend_hits_ = 0;

    std::atomic<int> width_{0}, height_{0};
    std::vector<IRect> last_drawn_;
    int frame_counter_ = 0;
};

} // namespace cs2esp
