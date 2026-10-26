#pragma once

#include "common/config.hpp"

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace cs2esp {

enum class MenuTab {
    Esp = 0,
    Aimbot = 1,
    Triggerbot = 2,
    Binds = 3,
    Count = 4
};

class Menu {
public:
    Menu(Config& cfg, std::string config_path,
         std::atomic<bool>& esp_on, std::atomic<bool>& aimbot_on,
         std::atomic<bool>& trigger_on);
    ~Menu() = default;

    bool is_open() const { return open_; }
    void toggle();
    void open();
    void close();

    // Обработка клавиш навигации (Up/Down/Left/Right/Enter/Space/Tab/1..3)
    bool handle_key(KeySym sym);

    // Обработка клика мыши внутри окна меню
    bool handle_click(int button, int x, int y);

    // Отрисовка меню через Xlib (прямоугольники) и Xft (шрифт)
    void draw(Display* dpy, Window win, GC gc, XftDraw* xft_draw, XftFont* font,
              Visual* visual, Colormap cmap, int screen_w, int screen_h);

    // Прямоугольник окна меню (для XShape input clip)
    XRectangle rect(int screen_w, int screen_h) const;

    // Сохранить текущие параметры в config.json
    bool save(std::string& err);

private:
    void next_item();
    void prev_item();
    void item_left();
    void item_right();
    void item_activate();
    int  item_count() const;

    Config& cfg_;
    std::string config_path_;
    std::atomic<bool>& esp_on_;
    std::atomic<bool>& aimbot_on_;
    std::atomic<bool>& trigger_on_;

    bool open_ = false;
    MenuTab current_tab_ = MenuTab::Esp;
    int selected_idx_ = 0;

    mutable int win_x_ = 100;
    mutable int win_y_ = 100;
    const int win_w_ = 540;
    const int win_h_ = 380;

    std::string status_msg_;
    std::chrono::steady_clock::time_point status_time_{};

    // Цвета Xft (кешируются)
    bool colors_init_ = false;
    XftColor col_white_{};
    XftColor col_gray_{};
    XftColor col_dim_{};
    XftColor col_blue_{};
    XftColor col_cyan_{};
    XftColor col_green_{};
    XftColor col_red_{};
    XftColor col_yellow_{};

    void init_colors(Display* dpy, Visual* visual, Colormap cmap);
};

} // namespace cs2esp
