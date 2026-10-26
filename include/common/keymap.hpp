#pragma once

#include <X11/Xlib.h>
#include <string>

namespace cs2esp {

// Maps a key name to an X11 keycode. Fixed PC105 scancodes are checked first
// so bindings remain layout-independent (e.g. Cyrillic vs Latin), with fallback
// to XStringToKeysym when a hardware match is not present.
int resolve_hardware_keycode(Display* dpy, const std::string& name);

// Returns a clean, human-readable name for a given KeySym / hardware keycode
// (e.g. "V", "SPACE", "F8", "ENTER", "ESC", "SHIFT", "ALT").
std::string get_key_name(KeySym sym, unsigned int keycode = 0);

// Returns a clean name for a mouse button (e.g. "MOUSE1", "MOUSE2", "MOUSE3", "MOUSE4", "MOUSE5").
std::string get_mouse_button_name(unsigned int button);

// Returns true if the specified mouse button is currently held down.
// Supports button 1 (LMB), 2 (MMB), 3 (RMB), 8 (MOUSE4 / side back), 9 (MOUSE5 / side forward),
// and any other physical buttons reported via XInput2.
bool is_mouse_button_pressed(Display* dpy, int button);

} // namespace cs2esp
