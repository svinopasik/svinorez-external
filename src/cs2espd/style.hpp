#pragma once

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cs2esp {

// ImGui-compatible 2D vector for coordinates and dimensions
struct ImVec2 {
    float x = 0.0f;
    float y = 0.0f;
    ImVec2() = default;
    constexpr ImVec2(float _x, float _y) : x(_x), y(_y) {}
};

// ImGui-compatible 4D vector for colors and bounding rects
struct ImVec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    ImVec4() = default;
    constexpr ImVec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

// Pack 32-bit ARGB for X11 GC foreground
inline constexpr unsigned long argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<unsigned long>(a) << 24) |
           (static_cast<unsigned long>(r) << 16) |
           (static_cast<unsigned long>(g) << 8)  |
           static_cast<unsigned long>(b);
}

// Color indices for UI theme styling
enum UICol_ {
    UICol_WindowBg = 0,
    UICol_SidebarBg,
    UICol_SidebarActive,
    UICol_SidebarHover,
    UICol_SidebarIndicator,
    UICol_SubtabBg,
    UICol_SubtabActive,
    UICol_SubtabLine,
    UICol_CardBg,
    UICol_CardBorder,
    UICol_CardHeaderLine,
    UICol_CheckBg,
    UICol_CheckBorder,
    UICol_CheckActive,
    UICol_SliderTrack,
    UICol_SliderProgress,
    UICol_SliderGrab,
    UICol_ButtonBg,
    UICol_ButtonBorder,
    UICol_ButtonHover,
    UICol_ButtonActive,
    UICol_Border,
    UICol_COUNT
};

// ImGuiStyle configuration
struct ImGuiStyle {
    float WindowRounding = 4.0f;
    float FrameRounding  = 3.0f;
    float GrabRounding   = 2.0f;
    ImVec2 WindowPadding = ImVec2(12.0f, 12.0f);
    ImVec2 FramePadding  = ImVec2(6.0f, 4.0f);
    ImVec2 ItemSpacing   = ImVec2(8.0f, 6.0f);
    unsigned long Colors[UICol_COUNT]{};

    void init_theme();
};

// Low-level custom drawing list wrapper for X11 & Xft
class UIDrawList {
public:
    UIDrawList(Display* dpy, Window win, GC gc, XftDraw* xft, XftFont* font,
               Visual* visual, Colormap cmap);

    void add_rect_filled(int x, int y, int w, int h, unsigned long color);
    void add_rect(int x, int y, int w, int h, unsigned long color, int thickness = 1);
    void add_line(int x1, int y1, int x2, int y2, unsigned long color, int thickness = 1);

    void add_text(int x, int y, XftColor* color, const std::string& text);
    void add_text_right(int x, int y, int w, XftColor* color, const std::string& text);
    void add_text_centered(int x, int y, int w, int h, XftColor* color, const std::string& text);
    void get_text_size(const std::string& text, int& w, int& h);

    // Custom styled widgets
    void draw_sidebar_header(int x, int y, int w, int h,
                             const std::string& title, const std::string& subtitle);

    void draw_sidebar_item(int x, int y, int w, int h,
                           const std::string& icon, const std::string& label,
                           bool is_active, bool is_hovered);

    void draw_top_tab(int x, int y, int w, int h,
                      const std::string& label, bool is_active, bool is_hovered);

    void draw_group_box(int x, int y, int w, int h,
                        const std::string& title);

    void draw_custom_checkbox(int x, int y, int w, int h,
                              const std::string& label, bool checked,
                              bool is_hovered, bool is_selected);

    void draw_custom_slider(int x, int y, int w, int h,
                            const std::string& label, const std::string& val_str,
                            float progress_norm, bool is_hovered, bool is_selected);

    void draw_custom_selector(int x, int y, int w, int h,
                              const std::string& label, const std::string& val_str,
                              bool is_hovered, bool is_selected);

    void draw_custom_button(int x, int y, int w, int h,
                            const std::string& label, bool is_hovered, bool is_selected,
                            bool is_danger = false);

    void draw_custom_keybind(int x, int y, int w, int h,
                             const std::string& label, const std::string& key_str,
                             bool is_waiting_for_input,
                             bool is_hovered, bool is_selected);

    const ImGuiStyle& style() const { return style_; }

    // Xft colors passed in or cached
    XftColor* col_white = nullptr;
    XftColor* col_gray = nullptr;
    XftColor* col_dim = nullptr;
    XftColor* col_red = nullptr;
    XftColor* col_crimson = nullptr;
    XftColor* col_green = nullptr;
    XftColor* col_yellow = nullptr;

private:
    Display* dpy_ = nullptr;
    Window win_ = 0;
    GC gc_ = nullptr;
    XftDraw* xft_ = nullptr;
    XftFont* font_ = nullptr;
    Visual* visual_ = nullptr;
    Colormap cmap_ = 0;
    ImGuiStyle style_;
};

} // namespace cs2esp
