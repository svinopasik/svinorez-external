#include "style.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cs2esp {

void ImGuiStyle::init_theme() {
    WindowRounding = 4.0f;
    FrameRounding  = 3.0f;
    GrabRounding   = 2.0f;

    Colors[UICol_WindowBg]         = argb(0xFF, 0x14, 0x14, 0x17); // #141417 (solid deep charcoal)
    Colors[UICol_SidebarBg]        = argb(0xFF, 0x0E, 0x0E, 0x11); // #0e0e11 (near black)
    Colors[UICol_SidebarActive]    = argb(0xFF, 0x24, 0x12, 0x15); // #241215 (dark crimson tint)
    Colors[UICol_SidebarHover]     = argb(0xFF, 0x18, 0x18, 0x1F); // #18181f
    Colors[UICol_SidebarIndicator] = argb(0xFF, 0xC4, 0x26, 0x26); // #c42626 (Crimson accent)

    Colors[UICol_SubtabBg]         = argb(0xFF, 0x14, 0x14, 0x17);
    Colors[UICol_SubtabActive]     = argb(0xFF, 0x1A, 0x1A, 0x22);
    Colors[UICol_SubtabLine]       = argb(0xFF, 0xC4, 0x26, 0x26); // Crimson underline

    Colors[UICol_CardBg]           = argb(0xFF, 0x1A, 0x1A, 0x20); // #1a1a20 (groupbox card)
    Colors[UICol_CardBorder]       = argb(0xFF, 0x2A, 0x2A, 0x36); // #2a2a36 (subtle border)
    Colors[UICol_CardHeaderLine]   = argb(0xFF, 0x24, 0x24, 0x2E);

    Colors[UICol_CheckBg]          = argb(0xFF, 0x12, 0x12, 0x16);
    Colors[UICol_CheckBorder]      = argb(0xFF, 0x36, 0x36, 0x46);
    Colors[UICol_CheckActive]      = argb(0xFF, 0xC4, 0x26, 0x26); // Crimson checked

    Colors[UICol_SliderTrack]      = argb(0xFF, 0x25, 0x25, 0x32);
    Colors[UICol_SliderProgress]   = argb(0xFF, 0xC4, 0x26, 0x26);
    Colors[UICol_SliderGrab]       = argb(0xFF, 0xFF, 0xFF, 0xFF);

    Colors[UICol_ButtonBg]         = argb(0xFF, 0x20, 0x20, 0x28);
    Colors[UICol_ButtonBorder]     = argb(0xFF, 0x32, 0x32, 0x42);
    Colors[UICol_ButtonHover]      = argb(0xFF, 0x2D, 0x2D, 0x3A);
    Colors[UICol_ButtonActive]     = argb(0xFF, 0xC4, 0x26, 0x26);

    Colors[UICol_Border]           = argb(0xFF, 0x2C, 0x2C, 0x38);
}

UIDrawList::UIDrawList(Display* dpy, Window win, GC gc, XftDraw* xft, XftFont* font,
                       Visual* visual, Colormap cmap)
    : dpy_(dpy), win_(win), gc_(gc), xft_(xft), font_(font),
      visual_(visual), cmap_(cmap) {
    style_.init_theme();
}

void UIDrawList::add_rect_filled(int x, int y, int w, int h, unsigned long color) {
    if (w <= 0 || h <= 0) return;
    XSetForeground(dpy_, gc_, color);
    XFillRectangle(dpy_, win_, gc_, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));
}

void UIDrawList::add_rect(int x, int y, int w, int h, unsigned long color, int thickness) {
    if (w <= 0 || h <= 0) return;
    XSetForeground(dpy_, gc_, color);
    for (int i = 0; i < thickness; ++i) {
        XDrawRectangle(dpy_, win_, gc_, x + i, y + i,
                       static_cast<unsigned>(std::max(0, w - 1 - 2 * i)),
                       static_cast<unsigned>(std::max(0, h - 1 - 2 * i)));
    }
}

void UIDrawList::add_line(int x1, int y1, int x2, int y2, unsigned long color, int thickness) {
    XSetForeground(dpy_, gc_, color);
    if (thickness <= 1) {
        XDrawLine(dpy_, win_, gc_, x1, y1, x2, y2);
    } else {
        for (int i = 0; i < thickness; ++i) {
            XDrawLine(dpy_, win_, gc_, x1, y1 + i, x2, y2 + i);
        }
    }
}

void UIDrawList::add_text(int x, int y, XftColor* color, const std::string& text) {
    if (text.empty() || !xft_ || !font_) return;
    XftDrawStringUtf8(xft_, color, font_, x, y,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));
}

void UIDrawList::add_text_right(int x, int y, int w, XftColor* color, const std::string& text) {
    if (text.empty() || !xft_ || !font_) return;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy_, font_,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &ext);
    int tx = x + w - static_cast<int>(ext.width);
    XftDrawStringUtf8(xft_, color, font_, tx, y,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));
}

void UIDrawList::add_text_centered(int x, int y, int w, int h, XftColor* color, const std::string& text) {
    if (text.empty() || !xft_ || !font_) return;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy_, font_,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &ext);
    int tx = x + (w - static_cast<int>(ext.width)) / 2;
    int ty = y + (h + font_->ascent - font_->descent) / 2;
    XftDrawStringUtf8(xft_, color, font_, tx, ty,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));
}

void UIDrawList::get_text_size(const std::string& text, int& w, int& h) {
    if (text.empty() || !font_) {
        w = 0;
        h = font_ ? (font_->ascent + font_->descent) : 14;
        return;
    }
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy_, font_,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &ext);
    w = static_cast<int>(ext.width);
    h = font_->ascent + font_->descent;
}

void UIDrawList::draw_sidebar_header(int x, int y, int w, int h,
                                     const std::string& title, const std::string& subtitle) {
    // Fill sidebar header
    add_rect_filled(x, y, w, h, style_.Colors[UICol_SidebarBg]);

    // Red accent block / icon
    add_rect_filled(x + 14, y + 16, 18, 18, style_.Colors[UICol_SidebarIndicator]);
    add_text_centered(x + 14, y + 16, 18, 18, col_white, "S");

    // Title & subtitle
    add_text(x + 38, y + 25, col_white, title);
    add_text(x + 38, y + 39, col_dim, subtitle);

    // Separator line
    add_line(x + 10, y + h - 1, x + w - 10, y + h - 1, style_.Colors[UICol_CardBorder]);
}

void UIDrawList::draw_sidebar_item(int x, int y, int w, int h,
                                   const std::string& icon, const std::string& label,
                                   bool is_active, bool is_hovered) {
    if (is_active) {
        add_rect_filled(x, y, w, h, style_.Colors[UICol_SidebarActive]);
        // Crimson indicator bar on left edge (3px)
        add_rect_filled(x, y, 3, h, style_.Colors[UICol_SidebarIndicator]);
    } else if (is_hovered) {
        add_rect_filled(x, y, w, h, style_.Colors[UICol_SidebarHover]);
    }

    XftColor* icon_col = is_active ? col_crimson : (is_hovered ? col_white : col_dim);
    XftColor* txt_col  = is_active ? col_white   : (is_hovered ? col_white : col_gray);

    add_text(x + 16, y + (h + font_->ascent - font_->descent) / 2, icon_col, icon);
    add_text(x + 42, y + (h + font_->ascent - font_->descent) / 2, txt_col, label);
}

void UIDrawList::draw_top_tab(int x, int y, int w, int h,
                              const std::string& label, bool is_active, bool is_hovered) {
    if (is_active) {
        add_rect_filled(x, y, w, h, style_.Colors[UICol_SubtabActive]);
        // Crimson 2px underline at the bottom of the active tab
        add_rect_filled(x + 8, y + h - 2, w - 16, 2, style_.Colors[UICol_SubtabLine]);
    } else if (is_hovered) {
        add_rect_filled(x, y, w, h, style_.Colors[UICol_SidebarHover]);
    }

    XftColor* txt_col = is_active ? col_white : (is_hovered ? col_white : col_gray);
    add_text_centered(x, y, w, h - 2, txt_col, label);
}

void UIDrawList::draw_group_box(int x, int y, int w, int h,
                                const std::string& title) {
    // Card background
    add_rect_filled(x, y, w, h, style_.Colors[UICol_CardBg]);
    // Card border
    add_rect(x, y, w, h, style_.Colors[UICol_CardBorder], 1);

    // Title indicator pip (Crimson marker)
    add_rect_filled(x + 12, y + 11, 3, 12, style_.Colors[UICol_SidebarIndicator]);

    // Title text
    add_text(x + 20, y + 21, col_white, title);

    // Horizontal divider under card header
    add_line(x + 8, y + 30, x + w - 8, y + 30, style_.Colors[UICol_CardHeaderLine]);
}

void UIDrawList::draw_custom_checkbox(int x, int y, int w, int h,
                                      const std::string& label, bool checked,
                                      bool is_hovered, bool is_selected) {
    if (is_selected || is_hovered) {
        add_rect_filled(x, y, w, h, argb(0x50, 0x2A, 0x2A, 0x38));
    }

    const int box_size = 14;
    const int bx = x + 10;
    const int by = y + (h - box_size) / 2;

    if (checked) {
        // Active: filled with Crimson
        add_rect_filled(bx, by, box_size, box_size, style_.Colors[UICol_CheckActive]);
        add_rect(bx, by, box_size, box_size, argb(0xFF, 0xE5, 0x39, 0x35), 1);
        // Inner white pip
        add_rect_filled(bx + 4, by + 4, box_size - 8, box_size - 8, argb(0xFF, 0xFF, 0xFF, 0xFF));
    } else {
        // Inactive: dark hollow square with border
        add_rect_filled(bx, by, box_size, box_size, style_.Colors[UICol_CheckBg]);
        add_rect(bx, by, box_size, box_size, style_.Colors[UICol_CheckBorder], 1);
    }

    XftColor* txt_col = checked ? col_white : (is_selected ? col_white : col_gray);
    add_text(bx + box_size + 10, y + (h + font_->ascent - font_->descent) / 2, txt_col, label);
}

void UIDrawList::draw_custom_slider(int x, int y, int w, int h,
                                    const std::string& label, const std::string& val_str,
                                    float progress_norm, bool is_hovered, bool is_selected) {
    if (is_selected || is_hovered) {
        add_rect_filled(x, y, w, h, argb(0x50, 0x2A, 0x2A, 0x38));
    }

    // Label on left, value on right
    XftColor* lbl_col = (is_selected || is_hovered) ? col_white : col_gray;
    add_text(x + 10, y + 14, lbl_col, label);
    add_text_right(x + 10, y + 14, w - 20, col_crimson, val_str);

    // Track
    const int track_x = x + 10;
    const int track_y = y + h - 10;
    const int track_w = w - 20;
    const int track_h = 5;

    add_rect_filled(track_x, track_y, track_w, track_h, style_.Colors[UICol_SliderTrack]);

    float norm = std::clamp(progress_norm, 0.0f, 1.0f);
    int fill_w = static_cast<int>(std::lround(track_w * norm));
    if (fill_w > 0) {
        add_rect_filled(track_x, track_y, fill_w, track_h, style_.Colors[UICol_SliderProgress]);
    }

    // Slider Knob
    int grab_x = track_x + fill_w - 4;
    int grab_y = track_y - 3;
    add_rect_filled(grab_x, grab_y, 8, 11, style_.Colors[UICol_SliderGrab]);
    add_rect(grab_x, grab_y, 8, 11, argb(0xFF, 0x1A, 0x1A, 0x20), 1);
}

void UIDrawList::draw_custom_selector(int x, int y, int w, int h,
                                      const std::string& label, const std::string& val_str,
                                      bool is_hovered, bool is_selected) {
    if (is_selected || is_hovered) {
        add_rect_filled(x, y, w, h, argb(0x50, 0x2A, 0x2A, 0x38));
    }

    XftColor* lbl_col = (is_selected || is_hovered) ? col_white : col_gray;
    add_text(x + 10, y + (h + font_->ascent - font_->descent) / 2, lbl_col, label);

    // Badge on right with < value >
    std::string badge = "< " + val_str + " >";
    int bw = 0, bh = 0;
    get_text_size(badge, bw, bh);

    int badge_x = x + w - bw - 18;
    int badge_y = y + (h - 20) / 2;
    add_rect_filled(badge_x, badge_y, bw + 10, 20, argb(0xFF, 0x22, 0x22, 0x2C));
    add_rect(badge_x, badge_y, bw + 10, 20, style_.Colors[UICol_CardBorder], 1);

    add_text_centered(badge_x, badge_y, bw + 10, 20, col_crimson, badge);
}

void UIDrawList::draw_custom_button(int x, int y, int w, int h,
                                    const std::string& label, bool is_hovered, bool is_selected,
                                    bool is_danger) {
    unsigned long bg_col = is_danger ? argb(0xFF, 0x38, 0x14, 0x18) : style_.Colors[UICol_ButtonBg];
    unsigned long border_col = (is_hovered || is_selected) ? style_.Colors[UICol_ButtonActive]
                                                           : style_.Colors[UICol_ButtonBorder];

    if (is_hovered || is_selected) {
        bg_col = is_danger ? argb(0xFF, 0x55, 0x18, 0x20) : style_.Colors[UICol_ButtonHover];
    }

    add_rect_filled(x, y, w, h, bg_col);
    add_rect(x, y, w, h, border_col, 1);

    XftColor* txt_col = is_danger ? col_red : ((is_hovered || is_selected) ? col_white : col_gray);
    add_text_centered(x, y, w, h, txt_col, label);
}

void UIDrawList::draw_custom_keybind(int x, int y, int w, int h,
                                     const std::string& label, const std::string& key_str,
                                     bool is_waiting_for_input,
                                     bool is_hovered, bool is_selected) {
    if (is_waiting_for_input) {
        add_rect_filled(x, y, w, h, argb(0x50, 0x4A, 0x14, 0x1C));
    } else if (is_selected || is_hovered) {
        add_rect_filled(x, y, w, h, argb(0x50, 0x2A, 0x2A, 0x38));
    }

    XftColor* lbl_col = (is_waiting_for_input || is_selected || is_hovered) ? col_white : col_gray;
    add_text(x + 10, y + (h + font_->ascent - font_->descent) / 2, lbl_col, label);

    std::string badge;
    if (is_waiting_for_input) {
        badge = "[ PRESS KEY... ]";
    } else {
        badge = "[ " + key_str + " ]";
    }

    int bw = 0, bh = 0;
    get_text_size(badge, bw, bh);

    int badge_x = x + w - bw - 18;
    int badge_y = y + (h - 20) / 2;

    if (is_waiting_for_input) {
        // Bright active red background for badge with white text
        add_rect_filled(badge_x, badge_y, bw + 10, 20, style_.Colors[UICol_SidebarIndicator]);
        add_rect(badge_x, badge_y, bw + 10, 20, argb(0xFF, 0xFF, 0x55, 0x55), 1);
        add_text_centered(badge_x, badge_y, bw + 10, 20, col_white, badge);
    } else {
        add_rect_filled(badge_x, badge_y, bw + 10, 20, argb(0xFF, 0x25, 0x12, 0x16));
        unsigned long border_col = (is_hovered || is_selected) ? argb(0xFF, 0xE5, 0x39, 0x35)
                                                               : style_.Colors[UICol_SidebarIndicator];
        add_rect(badge_x, badge_y, bw + 10, 20, border_col, 1);
        add_text_centered(badge_x, badge_y, bw + 10, 20, col_crimson, badge);
    }
}

} // namespace cs2esp
