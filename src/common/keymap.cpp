#include "common/keymap.hpp"

#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <cctype>
#include <cstdlib>

namespace cs2esp {

int resolve_hardware_keycode(Display* dpy, const std::string& name) {
    std::string s = name;
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (s == "alt" || s == "lalt" || s == "alt_l") return 64;
    if (s == "ralt" || s == "alt_r") return 108;
    if (s == "shift" || s == "lshift" || s == "shift_l") return 50;
    if (s == "rshift" || s == "shift_r") return 62;
    if (s == "ctrl" || s == "lctrl" || s == "control_l") return 37;
    if (s == "rctrl" || s == "control_r") return 105;
    if (s == "caps" || s == "capslock" || s == "caps_lock") return 66;
    if (s == "space") return 65;
    if (s == "tab") return 23;
    if (s == "insert" || s == "ins") return 118;
    if (s == "delete" || s == "del") return 119;
    if (s == "return" || s == "enter") return 36;
    if (s == "escape" || s == "esc") return 9;

    if (s == "a") return 38;
    if (s == "b") return 56;
    if (s == "c") return 54;
    if (s == "d") return 40;
    if (s == "e") return 26;
    if (s == "f") return 41;
    if (s == "g") return 42;
    if (s == "h") return 43;
    if (s == "i") return 31;
    if (s == "j") return 44;
    if (s == "k") return 45;
    if (s == "l") return 46;
    if (s == "m") return 58;
    if (s == "n") return 57;
    if (s == "o") return 32;
    if (s == "p") return 33;
    if (s == "q") return 24;
    if (s == "r") return 27;
    if (s == "s") return 39;
    if (s == "t") return 28;
    if (s == "u") return 30;
    if (s == "v") return 55;
    if (s == "w") return 25;
    if (s == "x") return 53;
    if (s == "y") return 29;
    if (s == "z") return 52;

    if (s == "1") return 10;
    if (s == "2") return 11;
    if (s == "3") return 12;
    if (s == "4") return 13;
    if (s == "5") return 14;
    if (s == "6") return 15;
    if (s == "7") return 16;
    if (s == "8") return 17;
    if (s == "9") return 18;
    if (s == "0") return 19;

    if (s == "f1") return 67;
    if (s == "f2") return 68;
    if (s == "f3") return 69;
    if (s == "f4") return 70;
    if (s == "f5") return 71;
    if (s == "f6") return 72;
    if (s == "f7") return 73;
    if (s == "f8") return 74;
    if (s == "f9") return 75;
    if (s == "f10") return 76;
    if (s == "f11") return 95;
    if (s == "f12") return 96;

    if (dpy) {
        KeySym sym = XStringToKeysym(name.c_str());
        if (sym != NoSymbol) {
            int kc = XKeysymToKeycode(dpy, sym);
            if (kc != 0) return kc;
        }
    }
    return 0;
}

std::string get_key_name(KeySym sym, unsigned int keycode) {
    if (sym == XK_Escape) return "ESC";
    if (sym == XK_Return || sym == XK_KP_Enter) return "ENTER";
    if (sym == XK_space) return "SPACE";
    if (sym == XK_Tab || sym == XK_ISO_Left_Tab) return "TAB";
    if (sym == XK_BackSpace) return "BACKSPACE";
    if (sym == XK_Insert || sym == XK_KP_Insert) return "INSERT";
    if (sym == XK_Delete || sym == XK_KP_Delete) return "DELETE";
    if (sym == XK_Home || sym == XK_KP_Home) return "HOME";
    if (sym == XK_End || sym == XK_KP_End) return "END";
    if (sym == XK_Page_Up || sym == XK_KP_Page_Up) return "PGUP";
    if (sym == XK_Page_Down || sym == XK_KP_Page_Down) return "PGDN";
    if (sym == XK_Caps_Lock) return "CAPS";
    if (sym == XK_Shift_L || sym == XK_Shift_R) return "SHIFT";
    if (sym == XK_Control_L || sym == XK_Control_R) return "CTRL";
    if (sym == XK_Alt_L || sym == XK_Alt_R) return "ALT";
    if (sym == XK_Super_L || sym == XK_Super_R) return "SUPER";

    if (sym >= XK_F1 && sym <= XK_F12) {
        return "F" + std::to_string(static_cast<int>(sym - XK_F1 + 1));
    }

    // Physical keycode lookup: guarantees correct Latin name even if active layout is Cyrillic
    switch (keycode) {
        case 38: return "A";
        case 56: return "B";
        case 54: return "C";
        case 40: return "D";
        case 26: return "E";
        case 41: return "F";
        case 42: return "G";
        case 43: return "H";
        case 31: return "I";
        case 44: return "J";
        case 45: return "K";
        case 46: return "L";
        case 58: return "M";
        case 57: return "N";
        case 32: return "O";
        case 33: return "P";
        case 24: return "Q";
        case 27: return "R";
        case 39: return "S";
        case 28: return "T";
        case 30: return "U";
        case 55: return "V";
        case 25: return "W";
        case 53: return "X";
        case 29: return "Y";
        case 52: return "Z";
        case 10: return "1";
        case 11: return "2";
        case 12: return "3";
        case 13: return "4";
        case 14: return "5";
        case 15: return "6";
        case 16: return "7";
        case 17: return "8";
        case 18: return "9";
        case 19: return "0";
        case 64: case 108: return "ALT";
        case 50: case 62: return "SHIFT";
        case 37: case 105: return "CTRL";
        case 65: return "SPACE";
        case 66: return "CAPS";
        case 23: return "TAB";
        case 118: return "INSERT";
        case 119: return "DELETE";
        default: break;
    }

    if (sym >= XK_a && sym <= XK_z) {
        return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(sym))));
    }
    if (sym >= XK_A && sym <= XK_Z) {
        return std::string(1, static_cast<char>(sym));
    }
    if (sym >= XK_0 && sym <= XK_9) {
        return std::string(1, static_cast<char>(sym));
    }

    const char* str = XKeysymToString(sym);
    if (str && *str) {
        std::string res(str);
        for (char& c : res) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return res;
    }

    if (keycode != 0) {
        return "KEY" + std::to_string(keycode);
    }
    return "NONE";
}

std::string get_mouse_button_name(unsigned int button) {
    switch (button) {
        case 1: return "MOUSE1";
        case 2: return "MOUSE3"; // Middle click in X11
        case 3: return "MOUSE2"; // Right click in X11
        case 4: return "MOUSE4";
        case 5: return "MOUSE5";
        case 8: return "MOUSE4"; // Side thumb back
        case 9: return "MOUSE5"; // Side thumb forward
        default: return "MOUSE" + std::to_string(button);
    }
}

bool is_mouse_button_pressed(Display* dpy, int button) {
    if (!dpy || button <= 0) return false;

    // Fast path: standard buttons 1..3 via core XQueryPointer
    if (button >= 1 && button <= 3) {
        Window root = DefaultRootWindow(dpy);
        Window root_ret = 0, child_ret = 0;
        int rx = 0, ry = 0, wx = 0, wy = 0;
        unsigned int mask = 0;
        if (XQueryPointer(dpy, root, &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask)) {
            if (button == 1) return (mask & Button1Mask) != 0;
            if (button == 2) return (mask & Button2Mask) != 0; // MMB
            if (button == 3) return (mask & Button3Mask) != 0; // RMB
        }
        return false;
    }

    // Extended buttons (e.g. Button 8 = MOUSE4, Button 9 = MOUSE5) via XInput2
    static int master_pointer_id = -1;
    if (master_pointer_id == -1) {
        int ndevices = 0;
        XIDeviceInfo* info = XIQueryDevice(dpy, XIAllMasterDevices, &ndevices);
        if (info) {
            for (int i = 0; i < ndevices; ++i) {
                if (info[i].use == XIMasterPointer) {
                    master_pointer_id = info[i].deviceid;
                    break;
                }
            }
            XIFreeDeviceInfo(info);
        }
        if (master_pointer_id == -1) master_pointer_id = 2; // Default Virtual core pointer
    }

    Window root = DefaultRootWindow(dpy);
    Window child = 0;
    double rx = 0, ry = 0, wx = 0, wy = 0;
    XIButtonState buttons{};
    XIModifierState mods{};
    XIGroupState group{};

    bool pressed = false;
    if (XIQueryPointer(dpy, master_pointer_id, root, &root, &child,
                       &rx, &ry, &wx, &wy, &buttons, &mods, &group)) {
        if (buttons.mask) {
            const int max_btn = buttons.mask_len * 8;
            if (button < max_btn && XIMaskIsSet(buttons.mask, button)) {
                pressed = true;
            } else if (button == 8 && 4 < max_btn && XIMaskIsSet(buttons.mask, 4)) {
                pressed = true; // Fallback for remaps
            } else if (button == 9 && 5 < max_btn && XIMaskIsSet(buttons.mask, 5)) {
                pressed = true; // Fallback for remaps
            }
            std::free(buttons.mask);
        }
    } else {
        master_pointer_id = -1; // Reset device cache on query failure
    }
    return pressed;
}

} // namespace cs2esp
