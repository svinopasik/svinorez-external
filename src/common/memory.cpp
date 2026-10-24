#include "common/memory.hpp"

#include "common/log.hpp"

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

namespace cs2esp {
namespace {

std::string errno_text(int e) {
    return std::string(::strerror(e)) + "(" + std::to_string(e) + ")";
}

std::string hex_addr(std::uint64_t a) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(a));
    return buf;
}

bool numeric_name(const char* s) {
    if (*s == '\0') return false;
    for (const char* c = s; *c; ++c) {
        if (*c < '0' || *c > '9') return false;
    }
    return true;
}

bool equals_ci(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string basename_of(const std::string& p) {
    const std::size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

std::string stem_of(const std::string& s) {
    const std::size_t dot = s.find('.');
    return dot == std::string::npos ? s : s.substr(0, dot);
}

std::string read_comm(int pid) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/comm");
    if (!f) return {};
    std::string line;
    std::getline(f, line);
    return trim(line);
}

std::string read_cmdline_name(int pid) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary);
    if (!f) return {};
    std::string tok;
    std::getline(f, tok, '\0');
    return basename_of(trim(tok));
}

// Ядро чтения: прямой системный вызов process_vm_readv (не оставляет открытых fd в /proc/self/fd);
// при необходимости — fallback на pread64 через открытый дескриптор.
bool read_full(int fd, int pid, std::uint64_t address, void* buffer, std::size_t size) {
    if (pid <= 0 || address == 0 || size == 0) return false;

    // 1) Прямой syscall process_vm_readv: быстрый и не оставляет открытых файлов /proc/<pid>/mem
    struct iovec local{ buffer, size };
    struct iovec remote{ reinterpret_cast<void*>(address), size };
    const ssize_t m = ::process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (m == static_cast<ssize_t>(size)) {
        return true;
    }

    // 2) Fallback: если открыт дескриптор /proc/<pid>/mem
    if (fd != -1) {
        std::size_t done = (m > 0) ? static_cast<std::size_t>(m) : 0;
        while (done < size) {
            const ssize_t n = ::pread64(fd,
                                        static_cast<char*>(buffer) + done,
                                        size - done,
                                        static_cast<off64_t>(address + done));
            if (n > 0) {
                done += static_cast<std::size_t>(n);
                continue;
            }
            return false;
        }
        return true;
    }
    return false;
}

} // namespace

ProcessMemory::~ProcessMemory() {
    close();
}

void ProcessMemory::close() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
    pid_ = 0;
}

bool ProcessMemory::open(const std::string& process_name, std::string& error) {
    close();

    const std::string wanted_full = trim(process_name);
    const std::string wanted_stem = stem_of(basename_of(wanted_full));

    // Два уровня матча: точное имя / stem (cs2.sh → cs2). Точный всегда приоритетнее:
    // у Steam есть bash-обёртка cs2.sh рядом с настоящим бинарником cs2.
    std::vector<int> exact_matches;
    std::vector<int> stem_matches;
    if (DIR* dir = ::opendir("/proc")) {
        while (const dirent* e = ::readdir(dir)) {
            if (!numeric_name(e->d_name)) continue;
            const int pid = std::atoi(e->d_name);
            if (pid <= 0) continue;

            const std::string comm = read_comm(pid);
            const std::string cmdline = read_cmdline_name(pid);
            if (comm.empty() && cmdline.empty()) continue;

            auto bucket = [&](const std::string& name) -> std::vector<int>* {
                if (name.empty()) return nullptr;
                if (equals_ci(name, wanted_full)) return &exact_matches;
                if (equals_ci(stem_of(name), wanted_stem)) return &stem_matches;
                return nullptr;
            };

            if (auto* b = bucket(comm))   b->push_back(pid);
            else if (auto* b = bucket(cmdline)) b->push_back(pid);
        }
        ::closedir(dir);
    }

    const std::vector<int>& matches =
        !exact_matches.empty() ? exact_matches : stem_matches;

    if (matches.empty()) {
        error = "process '" + process_name + "' not found in /proc";
        return false;
    }
    if (matches.size() > 1) {
        log_throttled("proc.match.multiple", 30, LogLevel::Warn,
                      "multiple processes match '" + process_name + "', using pid " +
                      std::to_string(matches.front()));
    }

    pid_ = matches.front();

    // Проверяем, доступен ли syscall process_vm_readv для этого PID.
    // Если доступен — не открываем /proc/<pid>/mem fd, чтобы дескриптор не светился в /proc/<self>/fd.
    char probe_buf[8];
    struct iovec local{ probe_buf, sizeof(probe_buf) };
    struct iovec remote{ reinterpret_cast<void*>(0x10000), sizeof(probe_buf) };
    errno = 0;
    ::process_vm_readv(pid_, &local, 1, &remote, 1, 0);
    if (errno != EPERM && errno != ESRCH) {
        fd_ = -1; // Скрытый режим: прямой syscall без постоянного fd
        error.clear();
        return true;
    }

    // Fallback: если ptrace_scope ограничил process_vm_readv, открываем /proc/<pid>/mem
    const std::string path = "/proc/" + std::to_string(pid_) + "/mem";
    fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd_ == -1) {
        error = "open(" + path + ", O_RDONLY) failed: " + errno_text(errno);
        pid_ = 0;
        return false;
    }
    error.clear();
    return true;
}

std::uint64_t ProcessMemory::module_base(const std::string& module_name) const {
    if (!attached()) return 0;

    std::ifstream maps("/proc/" + std::to_string(pid_) + "/maps");
    if (!maps) return 0;

    const std::string want = trim(basename_of(module_name));
    const std::string want_stem = stem_of(want);

    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream ls(line);
        std::string range, perms, offset, dev, inode;
        ls >> range >> perms >> offset >> dev >> inode;
        std::string path;
        std::getline(ls, path);
        path = trim(path);
        if (path.empty()) continue;
        if (path.ends_with("(deleted)")) continue;

        const std::string name = basename_of(path);
        if (!equals_ci(name, want) && !equals_ci(stem_of(name), want_stem)) continue;
        if (offset != "00000000") continue; // база файла — маппинг с offset 0

        const std::size_t dash = range.find('-');
        if (dash == std::string::npos) continue;
        const std::uint64_t base = static_cast<std::uint64_t>(
            std::strtoull(range.substr(0, dash).c_str(), nullptr, 16));
        if (base != 0) return base;
    }
    return 0;
}

bool ProcessMemory::read(std::uint64_t address, void* buffer, std::size_t size,
                         std::string& error) const {
    if (pid_ <= 0) {
        error = "memory: process not attached";
        return false;
    }
    if (!read_full(fd_, pid_, address, buffer, size)) {
        error = "read failed at " + hex_addr(address) + " size " + std::to_string(size) +
                " (pid=" + std::to_string(pid_) + "): " + errno_text(errno);
        return false;
    }
    return true;
}

bool ProcessMemory::read_quiet(std::uint64_t address, void* buffer, std::size_t size) const {
    if (pid_ <= 0 || address == 0 || size == 0) return false;
    return read_full(fd_, pid_, address, buffer, size);
}

bool ProcessMemory::process_alive() const {
    if (pid_ <= 0) return false;
    if (::kill(pid_, 0) == 0) {
        struct stat st{};
        if (::stat(("/proc/" + std::to_string(pid_) + "/maps").c_str(), &st) == 0) {
            return true;
        }
        return false; // zombie: maps пусты
    }
    if (errno == EPERM) return true; // процесс есть, просто не наш
    return false;
}

} // namespace cs2esp
