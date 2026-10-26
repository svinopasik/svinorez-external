#pragma once

#include <X11/Xlib.h>
#include <string>

namespace cs2esp {

// Maps a key name to an X11 keycode. Fixed PC105 scancodes are checked first
// so bindings remain layout-independent (e.g. Cyrillic vs Latin), with fallback
// to XStringToKeysym when a hardware match is not present.
int resolve_hardware_keycode(Display* dpy, const std::string& name);

} // namespace cs2esp
