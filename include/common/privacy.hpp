#pragma once

#include <string>

namespace cs2esp {

// Changes process comm name via prctl(PR_SET_NAME), rewrites argv cmdline,
// and disables core dumps / ptrace attach via prctl(PR_SET_DUMPABLE, 0).
void apply_process_privacy(int argc = 0, char** argv = nullptr,
                           const std::string& spoof_name = "");

// Direct wrapper for prctl(PR_SET_NAME)
bool set_process_name(const std::string& name);

} // namespace cs2esp
