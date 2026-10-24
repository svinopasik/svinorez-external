#include "common/ipc.hpp"
#include "common/json.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s <on|off|toggle|aim [on|off|toggle]|trigger [on|off|toggle]|rcs [on|off|toggle]|status|quit> [--config config/config.json]"
                 " [--socket /tmp/cs2esp.sock]\n"
                 "  on              enable ESP\n"
                 "  off             disable ESP\n"
                 "  toggle          switch ESP state\n"
                 "  aim [state]     control aimbot (on/off/toggle, default toggle)\n"
                 "  trigger [state] control triggerbot (on/off/toggle, default toggle)\n"
                 "  rcs [state]     control RCS recoil control (on/off/toggle, default toggle)\n"
                 "  status          current state\n"
                 "  quit            stop cs2espd\n",
                 argv0);
}

std::string trim_cr(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

// socket_path: явный --socket > socket_path из config.json > дефолт
std::string resolve_socket_path(const std::string& explicit_socket,
                                std::string config_path) {
    if (!explicit_socket.empty()) return explicit_socket;

    if (config_path.empty()) {
        std::FILE* probe = std::fopen("config/config.json", "rb");
        if (probe) {
            config_path = "config/config.json";
            std::fclose(probe);
        } else {
            probe = std::fopen("../config/config.json", "rb");
            if (probe) {
                config_path = "../config/config.json";
                std::fclose(probe);
            } else {
                return cs2esp::kDefaultSocketPath;
            }
        }
    }

    std::ifstream f(config_path, std::ios::binary);
    if (!f) return cs2esp::kDefaultSocketPath;
    std::ostringstream ss;
    ss << f.rdbuf();

    std::string perr;
    auto opt = json::parse(ss.str(), perr);
    if (opt) {
        const json::Value* sp = opt->find("socket_path");
        if (sp && sp->is_string()) return sp->string;
    }
    return cs2esp::kDefaultSocketPath;
}

} // namespace

int main(int argc, char** argv) {
    std::string cmd;
    std::string subcmd;
    std::string socket_path;
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--config" || a == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if ((a == "--socket" || a == "-s") && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (a == "--help" || a == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (cmd.empty() && !a.empty() && a[0] != '-') {
            cmd = a;
        } else if (!cmd.empty() && subcmd.empty() && !a.empty() && a[0] != '-') {
            subcmd = a;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (cmd == "aim" || cmd == "aimbot") {
        if (!subcmd.empty()) {
            if (subcmd != "on" && subcmd != "off" && subcmd != "toggle") {
                print_usage(argv[0]);
                return 2;
            }
            cmd = "aim " + subcmd;
        } else {
            cmd = "aim toggle";
        }
    } else if (cmd == "trigger" || cmd == "triggerbot") {
        if (!subcmd.empty()) {
            if (subcmd != "on" && subcmd != "off" && subcmd != "toggle") {
                print_usage(argv[0]);
                return 2;
            }
            cmd = "trigger " + subcmd;
        } else {
            cmd = "trigger toggle";
        }
    } else if (cmd == "rcs") {
        if (!subcmd.empty()) {
            if (subcmd != "on" && subcmd != "off" && subcmd != "toggle") {
                print_usage(argv[0]);
                return 2;
            }
            cmd = "rcs " + subcmd;
        } else {
            cmd = "rcs toggle";
        }
    } else if (!subcmd.empty()) {
        print_usage(argv[0]);
        return 2;
    }

    if (cmd.empty() ||
        (cmd != "on" && cmd != "off" && cmd != "toggle" &&
         cmd != "status" && cmd != "quit" &&
         cmd != "aim on" && cmd != "aim off" && cmd != "aim toggle" &&
         cmd != "trigger on" && cmd != "trigger off" && cmd != "trigger toggle" &&
         cmd != "rcs on" && cmd != "rcs off" && cmd != "rcs toggle")) {
        print_usage(argv[0]);
        return 2;
    }

    const std::string sock = resolve_socket_path(socket_path, config_path);

    std::string err;
    const int fd = cs2esp::ipc_client_connect(sock, err);
    if (fd == -1) {
        std::fprintf(stderr, "espctl: %s\n", err.c_str());
        return 1;
    }

    std::string send_err;
    if (!cs2esp::ipc_send_line(fd, cmd, send_err)) {
        std::fprintf(stderr, "espctl: send failed: %s\n", send_err.c_str());
        cs2esp::ipc_close(fd);
        return 1;
    }

    std::string reply, recv_err;
    if (!cs2esp::ipc_recv_line(fd, reply, recv_err)) {
        std::fprintf(stderr, "espctl: recv failed: %s\n", recv_err.c_str());
        cs2esp::ipc_close(fd);
        return 1;
    }
    cs2esp::ipc_close(fd);

    const std::string line = trim_cr(reply);
    std::printf("%s\n", line.c_str());

    return line.rfind("OK", 0) == 0 ? 0 : 1;
}
