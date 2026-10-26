#include "common/privacy.hpp"
#include "common/obfuscate.hpp"

#include <sys/prctl.h>
#include <cstring>
#include <algorithm>

namespace cs2esp {

bool set_process_name(const std::string& name) {
    if (name.empty()) return false;
    char comm[16]{};
    const std::size_t copy_len = std::min(name.size(), sizeof(comm) - 1);
    std::memcpy(comm, name.data(), copy_len);
    comm[copy_len] = '\0';
    return ::prctl(PR_SET_NAME, comm, 0, 0, 0) == 0;
}

void apply_process_privacy(int argc, char** argv, const std::string& spoof_name) {
    // Disable core dumps and ptrace attachment (Dumpable: 0 in /proc/<pid>/status)
    ::prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);

    const std::string name = spoof_name.empty() ? XOR_STR("kworker/u8:1") : spoof_name;
    set_process_name(name);

    if (argc > 0 && argv && argv[0]) {
        char* start = argv[0];
        char* end = argv[argc - 1] + std::strlen(argv[argc - 1]);
        const std::size_t total_len = static_cast<std::size_t>(end - start);
        std::memset(start, 0, total_len);
        const std::size_t copy_len = std::min(name.size(), total_len > 0 ? total_len - 1 : 0);
        std::memcpy(start, name.data(), copy_len);
    }
}

} // namespace cs2esp
