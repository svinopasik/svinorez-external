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

} // namespace

int ipc_server_start(const std::string& path, std::string& error) {
    // Занят ли файл сокета живым демоном?
    struct stat st{};
    if (::stat(path.c_str(), &st) == 0) {
        int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (probe != -1) {
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            if (path.size() < sizeof(addr.sun_path)) {
                std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
                if (::connect(probe, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
                    ::close(probe);
                    error = "socket " + path + " is busy: another cs2espd is listening there";
                    return -1;
                }
            }
            ::close(probe);
        }
        // Не отвечает → stale-файл от прошлого запуска, убираем.
        if (::unlink(path.c_str()) != 0) {
            error = "cannot unlink stale socket " + path + ": " + errno_text(errno);
            return -1;
        }
    } else if (errno != ENOENT) {
        error = "stat(" + path + ") failed: " + errno_text(errno);
        return -1;
    }

    if (path.size() >= sizeof(sockaddr_un{}.sun_path)) {
        error = "socket path too long: " + path;
        return -1;
    }

    const int srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv == -1) {
        error = "socket(AF_UNIX) failed: " + errno_text(errno);
        return -1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

    if (::bind(srv, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        error = "bind(" + path + ") failed: " + errno_text(errno);
        ::close(srv);
        return -1;
    }
    // espctl может быть кем угодно: делаем сокет доступным всем
    if (::chmod(path.c_str(), 0666) != 0) {
        error = "chmod(" + path + ") failed: " + errno_text(errno);
        ::close(srv);
        ::unlink(path.c_str());
        return -1;
    }
    if (::listen(srv, 16) == -1) {
        error = "listen() failed: " + errno_text(errno);
        ::close(srv);
        ::unlink(path.c_str());
        return -1;
    }
    error.clear();
    return srv;
}

int ipc_client_connect(const std::string& path, std::string& error) {
    if (path.size() >= sizeof(sockaddr_un{}.sun_path)) {
        error = "socket path too long: " + path;
        return -1;
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        error = "socket(AF_UNIX) failed: " + errno_text(errno);
        return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1) {
        const int e = errno;
        ::close(fd);
        if (e == ENOENT || e == ECONNREFUSED) {
            error = "cannot connect to " + path + ": cs2espd is not running";
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
            if (out.empty()) return true; // корректное закрытие
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
