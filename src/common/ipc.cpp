#include "common/ipc.hpp"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace cs2esp {
namespace {

std::string errno_text(int e) {
    return std::string(::strerror(e)) + "(" + std::to_string(e) + ")";
}

bool fill_sockaddr(const std::string& path, sockaddr_un& addr, socklen_t& len, std::string& error) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (path.empty()) {
        error = "empty socket path";
        return false;
    }

    if (path[0] == '@') {
        // Abstract Unix Domain Socket (Linux-specific, no filesystem inode)
        const std::string name = path.substr(1);
        if (name.size() >= sizeof(addr.sun_path)) {
            error = "abstract socket name too long: " + path;
            return false;
        }
        addr.sun_path[0] = '\0';
        std::memcpy(&addr.sun_path[1], name.data(), name.size());
        len = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + 1 + name.size());
        return true;
    }

    // Standard filesystem Unix socket
    if (path.size() >= sizeof(addr.sun_path)) {
        error = "socket path too long: " + path;
        return false;
    }
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    len = sizeof(addr);
    return true;
}

} // namespace

int ipc_server_start(const std::string& path, std::string& error) {
    sockaddr_un addr{};
    socklen_t addr_len = 0;
    if (!fill_sockaddr(path, addr, addr_len, error)) {
        return -1;
    }

    const bool is_abstract = (path[0] == '@');

    if (!is_abstract) {
        struct stat st{};
        if (::stat(path.c_str(), &st) == 0) {
            int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (probe != -1) {
                if (::connect(probe, reinterpret_cast<const sockaddr*>(&addr), addr_len) == 0) {
                    ::close(probe);
                    error = "socket " + path + " is busy: another instance is listening there";
                    return -1;
                }
                ::close(probe);
            }
            if (::unlink(path.c_str()) != 0) {
                error = "cannot unlink stale socket " + path + ": " + errno_text(errno);
                return -1;
            }
        }
    }

    const int srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv == -1) {
        error = "socket(AF_UNIX) failed: " + errno_text(errno);
        return -1;
    }

    if (::bind(srv, reinterpret_cast<const sockaddr*>(&addr), addr_len) == -1) {
        const int e = errno;
        ::close(srv);
        if (is_abstract && e == EADDRINUSE) {
            error = "abstract socket " + path + " is busy: another instance is running";
        } else {
            error = "bind(" + path + ") failed: " + errno_text(e);
        }
        return -1;
    }

    if (!is_abstract) {
        ::chmod(path.c_str(), 0666);
    }

    if (::listen(srv, 16) == -1) {
        error = "listen() failed: " + errno_text(errno);
        ::close(srv);
        if (!is_abstract) ::unlink(path.c_str());
        return -1;
    }

    error.clear();
    return srv;
}

int ipc_client_connect(const std::string& path, std::string& error) {
    sockaddr_un addr{};
    socklen_t addr_len = 0;
    if (!fill_sockaddr(path, addr, addr_len, error)) {
        return -1;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        error = "socket(AF_UNIX) failed: " + errno_text(errno);
        return -1;
    }

    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), addr_len) == -1) {
        const int e = errno;
        ::close(fd);
        if (e == ENOENT || e == ECONNREFUSED) {
            error = "cannot connect to " + path + ": daemon is not running";
        } else {
            error = "connect(" + path + ") failed: " + errno_text(e);
        }
        return -1;
    }

    error.clear();
    return fd;
}

bool ipc_send_line(int fd, const std::string& line, std::string& error) {
    std::string buf = line;
    buf.push_back('\n');
    std::size_t done = 0;
    while (done < buf.size()) {
        const ssize_t n = ::send(fd, buf.data() + done, buf.size() - done, MSG_NOSIGNAL);
        if (n > 0) {
            done += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        error = "send() failed: " + errno_text(errno);
        return false;
    }
    return true;
}

bool ipc_recv_line(int fd, std::string& out, std::string& error) {
    out.clear();
    char c;
    while (out.size() < 512) {
        const ssize_t n = ::recv(fd, &c, 1, 0);
        if (n > 0) {
            if (c == '\n') return true;
            out.push_back(c);
            continue;
        }
        if (n == 0) {
            if (out.empty()) return false; // EOF: закрытие соединения
            return true;
        }
        if (errno == EINTR) continue;
        error = "recv() failed: " + errno_text(errno);
        return false;
    }
    error = "recv: line too long (>512 bytes)";
    return false;
}

void ipc_close(int fd) {
    if (fd != -1) {
        ::close(fd);
    }
}

} // namespace cs2esp
