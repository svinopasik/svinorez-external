#include "common/config.hpp"
#include "common/ipc.hpp"
#include "common/log.hpp"
#include "aimbot.hpp"
#include "triggerbot.hpp"
#include "rcs.hpp"
#include "esp.hpp"
#include "game.hpp"
#include "overlay.hpp"
#include "build_salt.hpp"

#include <X11/Xlib.h>
#include <sys/socket.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <sys/prctl.h>

using namespace std::chrono_literals;

namespace {

[[maybe_unused]] volatile const char* g_salt_anchor = cs2esp::kBuildSalt;

std::string resolve_socket_path(const std::string& path, const std::string& cfg_path) {
    if (path.empty() || path[0] == '/' || path[0] == '@') return path;
    try {
        std::filesystem::path cp(cfg_path);
        return (cp.parent_path() / path).lexically_normal().string();
    } catch (...) {
        return path;
    }
}

void apply_stealth_hardening(int argc, char** argv, const std::string& spoof_name) {
    // Disable ptrace attach and core dumps (Dumpable: 0 in /proc/<pid>/status).
    ::prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);

    const std::string name = spoof_name.empty() ? "pipewire-sys" : spoof_name;
    ::prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);

    if (argc > 0 && argv && argv[0]) {
        char* start = argv[0];
        char* end = argv[argc - 1] + std::strlen(argv[argc - 1]);
        const std::size_t total_len = static_cast<std::size_t>(end - start);
        std::memset(start, 0, total_len);
        std::strncpy(start, name.c_str(), total_len > 0 ? total_len - 1 : 0);
    }
}

// --- сигналы: минимальные обработчики, только флаги ---
volatile sig_atomic_t g_usr1_seen = 0;
volatile sig_atomic_t g_stop_seen = 0;

void on_usr1(int) {
    g_usr1_seen = 1;
}

void on_stop(int) {
    g_stop_seen = 1;
}

std::atomic<bool> g_running{true};
std::atomic<bool> g_esp_on{true};
std::atomic<bool> g_aimbot_on{false};
std::atomic<bool> g_trigger_on{false};
std::atomic<bool> g_rcs_on{true};
std::atomic<bool> g_quit_from_ipc{false};

// --- разделяемое между игровым и рисующим потоками ---
struct SharedState {
    std::mutex mutex;
    std::vector<cs2esp::ScreenBox> boxes;
    cs2esp::Snapshot snap;
    std::atomic<bool> attached{false};
    std::atomic<int>  game_pid{0};
    std::atomic<int>  local_team{-1};
} g_shared;

struct Args {
    std::string config_path  = "config/config.json";
    std::string offsets_path = "config/offsets.json";
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s [--config config/config.json] [--offsets config/offsets.json]\n",
                 argv0);
}

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            a.config_path = argv[++i];
        } else if ((arg == "--offsets" || arg == "-o") && i + 1 < argc) {
            a.offsets_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            print_usage(argv[0]);
            std::exit(2);
        }
    }
    // Если запущено из build/ и путь по умолчанию отсутствует, пробуем ../config/
    if (a.config_path == "config/config.json" && !std::filesystem::exists(a.config_path)) {
        if (std::filesystem::exists("../config/config.json")) {
            a.config_path = "../config/config.json";
        }
    }
    if (a.offsets_path == "config/offsets.json" && !std::filesystem::exists(a.offsets_path)) {
        if (std::filesystem::exists("../config/offsets.json")) {
            a.offsets_path = "../config/offsets.json";
        }
    }
    return a;
}

void install_signals() {
    struct sigaction sa{};
    sa.sa_handler = on_usr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);

    struct sigaction sa_term{};
    sa_term.sa_handler = on_stop;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0; // без SA_RESTART — чтобы прервать блокирующие вызовы (sleep)
    sigaction(SIGINT,  &sa_term, nullptr);
    sigaction(SIGTERM, &sa_term, nullptr);
}

void log_toggle_state() {
    cs2esp::log_info(std::string("ESP state -> ") + (g_esp_on.load() ? "ON" : "OFF"));
}

void log_aimbot_state() {
    cs2esp::log_info(std::string("AIMBOT state -> ") +
                     (g_aimbot_on.load() ? "ON" : "OFF"));
}

void log_trigger_state() {
    cs2esp::log_info(std::string("TRIGGER state -> ") +
                     (g_trigger_on.load() ? "ON" : "OFF"));
}

void log_rcs_state() {
    cs2esp::log_info(std::string("RCS state -> ") +
                     (g_rcs_on.load() ? "ON" : "OFF"));
}

void game_loop(cs2esp::Game& game, cs2esp::Esp& esp, cs2esp::Overlay& overlay,
               const cs2esp::Config& cfg) {
    const auto frame_time = std::chrono::milliseconds(1000 / cfg.fps_limit);

    while (g_running.load()) {
        const auto next = std::chrono::steady_clock::now() + frame_time;

        cs2esp::Snapshot snap;
        game.update(snap);
        g_shared.attached.store(snap.attached);
        g_shared.game_pid.store(game.pid());
        g_shared.local_team.store(snap.local_team);

        std::vector<cs2esp::ScreenBox> boxes;
        if (g_esp_on.load() && snap.offsets_ready) {
            const auto [sw, sh] = overlay.screen_size();
            esp.to_boxes(snap, sw, sh, boxes);
        }
        {
            std::lock_guard<std::mutex> lock(g_shared.mutex);
            g_shared.snap = std::move(snap);
            g_shared.boxes = std::move(boxes);
        }

        std::this_thread::sleep_until(next);
    }
}

void render_loop(cs2esp::Overlay& overlay, const cs2esp::Config& cfg) {
    const auto frame_time = std::chrono::milliseconds(1000 / cfg.fps_limit);

    while (g_running.load()) {
        const auto next = std::chrono::steady_clock::now() + frame_time;

        std::vector<cs2esp::ScreenBox> boxes;
        {
            std::lock_guard<std::mutex> lock(g_shared.mutex);
            boxes = g_shared.boxes;
        }

        if (!overlay.frame(boxes, g_esp_on.load(), g_aimbot_on.load(), g_trigger_on.load(), g_rcs_on.load(), g_shared.attached.load())) {
            cs2esp::log_error("X connection lost — демона останавливаю");
            g_running.store(false);
            break;
        }

        overlay.set_game_pid(g_shared.game_pid.load());

        // F8
        if (overlay.consume_toggle()) {
            const bool now_on = !g_esp_on.load();
            g_esp_on.store(now_on);
            log_toggle_state();
        }

        std::this_thread::sleep_until(next);
    }
}

void aimbot_loop(cs2esp::Aimbot& aimbot, cs2esp::Overlay& overlay) {
    while (g_running.load()) {
        if (g_aimbot_on.load()) {
            cs2esp::Snapshot snap_copy;
            {
                std::lock_guard<std::mutex> lock(g_shared.mutex);
                snap_copy = g_shared.snap;
            }
            const auto [sw, sh] = overlay.screen_size();
            aimbot.tick(snap_copy, sw, sh);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void trigger_loop(cs2esp::Triggerbot& trigger) {
    while (g_running.load()) {
        if (g_trigger_on.load()) {
            const int local_team = g_shared.local_team.load();
            trigger.tick(local_team);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void rcs_loop(cs2esp::Rcs& rcs) {
    while (g_running.load()) {
        if (g_rcs_on.load()) {
            int shots = 0;
            cs2esp::Vec2 punch{0.0f, 0.0f};
            {
                std::lock_guard<std::mutex> lock(g_shared.mutex);
                shots = g_shared.snap.local_shots_fired;
                punch = g_shared.snap.local_punch;
            }
            rcs.tick(shots, punch);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

void ipc_loop(int srv_fd, cs2esp::Overlay& overlay) {
    while (g_running.load()) {
        int fd = ::accept(srv_fd, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EINTR) continue;
            cs2esp::log_error("accept() failed: " + std::string(std::strerror(errno)));
            break;
        }

        std::string line, err;
        if (cs2esp::ipc_recv_line(fd, line, err)) {
            std::string reply;
            if (line == "on") {
                g_esp_on.store(true);
                reply = "OK ON";
            } else if (line == "off") {
                g_esp_on.store(false);
                reply = "OK OFF";
            } else if (line == "toggle") {
                const bool now_on = !g_esp_on.load();
                g_esp_on.store(now_on);
                reply = now_on ? "OK ON" : "OK OFF";
            } else if (line == "aim on" || line == "aimbot on") {
                g_aimbot_on.store(true);
                reply = "OK AIM ON";
            } else if (line == "aim off" || line == "aimbot off") {
                g_aimbot_on.store(false);
                reply = "OK AIM OFF";
            } else if (line == "aim toggle" || line == "aim" ||
                       line == "aimbot toggle" || line == "aimbot") {
                const bool now = !g_aimbot_on.load();
                g_aimbot_on.store(now);
                reply = now ? "OK AIM ON" : "OK AIM OFF";
            } else if (line == "trigger on" || line == "triggerbot on") {
                g_trigger_on.store(true);
                reply = "OK TRIGGER ON";
            } else if (line == "trigger off" || line == "triggerbot off") {
                g_trigger_on.store(false);
                reply = "OK TRIGGER OFF";
            } else if (line == "trigger toggle" || line == "trigger" ||
                       line == "triggerbot toggle" || line == "triggerbot") {
                const bool now = !g_trigger_on.load();
                g_trigger_on.store(now);
                reply = now ? "OK TRIGGER ON" : "OK TRIGGER OFF";
            } else if (line == "rcs on") {
                g_rcs_on.store(true);
                reply = "OK RCS ON";
            } else if (line == "rcs off") {
                g_rcs_on.store(false);
                reply = "OK RCS OFF";
            } else if (line == "rcs toggle" || line == "rcs") {
                const bool now = !g_rcs_on.load();
                g_rcs_on.store(now);
                reply = now ? "OK RCS ON" : "OK RCS OFF";
            } else if (line == "menu" || line == "menu toggle") {
                overlay.toggle_menu();
                reply = "OK MENU TOGGLED";
            } else if (line == "menu open") {
                overlay.open_menu();
                reply = "OK MENU OPEN";
            } else if (line == "menu close") {
                overlay.close_menu();
                reply = "OK MENU CLOSED";
            } else if (line == "status") {
                reply = std::string("OK ESP:") + (g_esp_on.load() ? "ON" : "OFF") +
                        " AIM:" + (g_aimbot_on.load() ? "ON" : "OFF") +
                        " TRIGGER:" + (g_trigger_on.load() ? "ON" : "OFF") +
                        " RCS:" + (g_rcs_on.load() ? "ON" : "OFF") +
                        " MENU:" + (overlay.is_menu_open() ? "OPEN" : "CLOSED");
            } else if (line == "quit") {
                reply = "OK";
            } else {
                reply = "ERR unknown command '" + line + "'";
            }

            std::string send_err;
            if (!cs2esp::ipc_send_line(fd, reply, send_err)) {
                // EPIPE/ECONNRESET — клиент ушёл до ответа, это штатно (timeout espctl)
                if (send_err.find("Broken pipe") == std::string::npos &&
                    send_err.find("Connection reset") == std::string::npos) {
                    cs2esp::log_warn("ipc reply failed: " + send_err);
                }
            }

            if (line == "toggle" || line == "on" || line == "off") {
                log_toggle_state();
            }
            if (line.rfind("aim", 0) == 0) {
                log_aimbot_state();
            }
            if (line.rfind("trigger", 0) == 0) {
                log_trigger_state();
            }
            if (line.rfind("rcs", 0) == 0) {
                log_rcs_state();
            }
            if (line == "quit") {
                cs2esp::log_info("quit via espctl");
                g_running.store(false);
                g_quit_from_ipc.store(true);
            }
        }
        cs2esp::ipc_close(fd);
    }
}

// Разбудит блокированный accept(), чтобы поток ipc завершился.
void unblock_accept(const std::string& socket_path) {
    std::string err;
    int cfd = cs2esp::ipc_client_connect(socket_path, err);
    if (cfd >= 0) {
        cs2esp::ipc_send_line(cfd, "quit", err);
        cs2esp::ipc_close(cfd);
    }
}

} // namespace

int main(int argc, char** argv) {
    // Многопоточная инициализация Xlib — обязательна для потоков overlay и triggerbot
    XInitThreads();

    const Args args = parse_args(argc, argv);

    cs2esp::Config config;
    std::string error;
    if (!cs2esp::load_config(args.config_path, args.offsets_path, config, error)) {
        cs2esp::log_error(error);
        return 1;
    }
    cs2esp::log_info("config loaded: " + args.config_path + " + " + args.offsets_path);

    // Применяем маскировку процесса сразу после парсинга аргументов командной строки
    apply_stealth_hardening(argc, argv, config.stealth.spoof_process_name);

    config.socket_path = resolve_socket_path(config.socket_path, args.config_path);

    int srv_fd = cs2esp::ipc_server_start(config.socket_path, error);
    if (srv_fd < 0) {
        cs2esp::log_error("ipc listen failed: " + error);
        return 1;
    }
    cs2esp::log_info("ipc server: " + config.socket_path);

    install_signals();

    cs2esp::Game game(config);
    cs2esp::Overlay overlay(config, args.config_path, g_esp_on, g_aimbot_on, g_trigger_on, g_rcs_on);
    if (game.pid() > 0) {
        overlay.set_game_pid(game.pid());
    }
    if (!overlay.init(error)) {
        cs2esp::log_error("overlay: " + error);
        cs2esp::ipc_close(srv_fd);
        if (!config.socket_path.empty() && config.socket_path[0] != '@') {
            ::unlink(config.socket_path.c_str());
        }
        return 2;
    }

    cs2esp::Esp esp(config);
    cs2esp::Aimbot aimbot(config, game);
    std::string aerr;
    if (!aimbot.init(aerr)) {
        cs2esp::log_warn("aimbot init failed: " + aerr);
    }
    g_aimbot_on.store(config.aimbot.enabled);

    cs2esp::Triggerbot trigger(config, game);
    std::string terr;
    if (!trigger.init(terr)) {
        cs2esp::log_warn("triggerbot init failed: " + terr);
    }
    g_trigger_on.store(config.triggerbot.enabled);

    cs2esp::Rcs rcs(config);
    std::string rerr;
    if (!rcs.init(rerr)) {
        cs2esp::log_warn("rcs init failed: " + rerr);
    }
    g_rcs_on.store(config.rcs.enabled);

    std::thread t_game([&] { game_loop(game, esp, overlay, config); });
    std::thread t_render([&] { render_loop(overlay, config); });
    std::thread t_aimbot([&] { aimbot_loop(aimbot, overlay); });
    std::thread t_trigger([&] { trigger_loop(trigger); });
    std::thread t_rcs([&] { rcs_loop(rcs); });
    std::thread t_ipc([&] { ipc_loop(srv_fd, overlay); });

    // Головной поток: сигналы + graceful shutdown
    while (g_running.load() && g_stop_seen == 0) {
        if (g_usr1_seen) {
            g_usr1_seen = 0;
            const bool now_on = !g_esp_on.load();
            g_esp_on.store(now_on);
            log_toggle_state();
        }
        std::this_thread::sleep_for(100ms);
    }
    g_running.store(false);

    // Разбудить поток IPC
    unblock_accept(config.socket_path);

    t_game.join();
    t_render.join();
    t_aimbot.join();
    t_trigger.join();
    t_rcs.join();
    t_ipc.join();

    cs2esp::ipc_close(srv_fd);
    if (!config.socket_path.empty() && config.socket_path[0] != '@') {
        ::unlink(config.socket_path.c_str());
    }
    cs2esp::log_info("daemon stopped");
    return 0;
}
