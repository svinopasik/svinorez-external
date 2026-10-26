#pragma once

#include "common/config.hpp"
#include "style.hpp"

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace cs2esp {

enum class MenuCategory {
    Main = 0,
    Visuals = 1,
    Aimbot = 2,
    Triggerbot = 3,
    Settings = 4,
    Count = 5
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

    void set_category(int c) {
        if (c >= 0 && c < static_cast<int>(MenuCategory::Count)) {
            current_cat_ = static_cast<MenuCategory>(c);
            current_subtab_ = 0;
            selected_item_ = 0;
        }
    }

    bool handle_key(KeySym sym);
    bool handle_click(int button, int x, int y);
    void handle_mouse_move(int x, int y);
    void handle_mouse_up(int button, int x, int y);

    void draw(Display* dpy, Window win, GC gc, XftDraw* xft_draw, XftFont* font,
              Visual* visual, Colormap cmap, int screen_w, int screen_h);

    XRectangle rect(int screen_w, int screen_h) const;

    bool save(std::string& err);

private:
    Config& cfg_;
    std::string config_path_;
    std::atomic<bool>& esp_on_;
    std::atomic<bool>& aimbot_on_;
    std::atomic<bool>& trigger_on_;

    bool open_ = false;
    MenuCategory current_cat_ = MenuCategory::Main;
    int current_subtab_ = 0;
    int selected_item_ = 0;

    mutable int win_x_ = 100;
    mutable int win_y_ = 100;
    const int win_w_ = 720;
    const int win_h_ = 480;

    int mouse_x_ = -1;
    int mouse_y_ = -1;
    bool mouse_down_ = false;
    int dragging_slider_idx_ = -1;

    std::string status_msg_;
    std::chrono::steady_clock::time_point status_time_{};

    bool colors_init_ = false;
    XftColor col_white_{};
    XftColor col_gray_{};
    XftColor col_dim_{};
    XftColor col_red_{};
    XftColor col_crimson_{};
    XftColor col_green_{};
    XftColor col_yellow_{};
    void init_colors(Display* dpy, Visual* visual, Colormap cmap);

    enum class ItemType {
        Checkbox,
        SliderFloat,
        SliderInt,
        SelectorStr,
        SelectorInt,
        Button,
        Keybind
    };

    struct SubtabDef {
        std::string name;
    };

    struct CategoryDef {
        std::string icon;
        std::string name;
        std::vector<SubtabDef> subtabs;
    };

    struct RuntimeItem {
        int card_col = 0; // 0 = left card, 1 = right card
        ItemType type = ItemType::Checkbox;
        std::string label;
        std::string val_str;
        float progress = 0.0f;
        bool checked = false;
        bool is_danger = false;

        // Bounding box for mouse clicks/drags
        int bx = 0, by = 0, bw = 0, bh = 0;

        // Callback hooks for activation/cycles
        std::function<void()> on_toggle;
        std::function<void(float norm)> on_slide;
        std::function<void(int dir)> on_cycle;
        std::function<void()> on_click;
    };

    std::vector<CategoryDef> categories_;
    std::vector<RuntimeItem> active_items_;

    void setup_categories();
    void build_items_for_current_view();
    int current_item_count() const;
    void item_activate();
    void item_left();
    void item_right();
    void select_next_item();
    void select_prev_item();
};

} // namespace cs2esp
