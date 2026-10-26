#include "common/keymap.hpp"

#include <X11/keysym.h>
#include <cctype>

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

    if (s == "c") return 54;
    if (s == "v") return 55;
    if (s == "x") return 53;
    if (s == "z") return 52;
    if (s == "e") return 26;
    if (s == "q") return 24;
    if (s == "r") return 27;
    if (s == "f") return 41;

    if (s == "1") return 10;
    if (s == "2") return 11;
    if (s == "3") return 12;
    if (s == "4") return 13;
    if (s == "5") return 14;

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

} // namespace cs2esp
