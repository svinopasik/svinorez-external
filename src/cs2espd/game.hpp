#pragma once

#include "common/config.hpp"
#include "common/memory.hpp"

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace cs2esp {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Vec2 {
    float x = 0, y = 0;
};

// Row-major (m[row][col]); не хранит имя расширенного типа — расчёт в esp.cpp
struct Matrix4 {
    float m[4][4];
};

// Снимок одного пешки
struct PlayerSnap {
    Vec3 origin;              // ноги (origin из scene node)
    int  health = 0;
    int  team = 0;
    bool alive = false;
    bool is_visible = false;  // виден локальному игроку / спотнут
    std::string name;         // пусто, если offsets ника не заполнены
    std::vector<Vec3> bones;  // кости скелета (0..23)
    bool has_bones = false;
};

// Есть всё, что нужно Esp'у за один кадр
struct Snapshot {
    bool attached = false;      // cs2.exe найден, fd открыт
    bool offsets_ready = false; // критичные offsets ≠ 0
    bool have_local = false;    // локальный пешка прочитан
    int  local_team = -1;
    Vec3 local_origin;
    int  local_shots_fired = 0; // выстрелы текущего спрея
    Vec2 local_punch{0.f, 0.f}; // x = pitch, y = yaw (отдача)
    bool matrix_ok = false;
    Matrix4 matrix{};
    std::vector<PlayerSnap> players;
};

class Game {
public:
    explicit Game(const Config& cfg);

    int pid() const { return mem_.pid(); }
    std::uint64_t local_pawn() const { return local_pawn_.load(); }

    // Проверка цели под прицелом (m_iIDEntIndex) для triggerbot
    bool get_crosshair_target(int& target_ent_idx, int& target_team, int& target_health) const;

    // Один тик обновления: attach / чтение памяти / снапшот.
    // Ошибки гасятся в лог через throttling; функции не бросают.
    void update(Snapshot& out);

private:
    bool read_pawn_meta(std::uint64_t pawn, int& health, int& team, bool& alive) const;
    bool read_pawn_origin(std::uint64_t pawn, Vec3& out) const;
    bool read_pawn_bones(std::uint64_t pawn, std::vector<Vec3>& out) const;
    bool read_pawn_spotted(std::uint64_t pawn) const;
    bool read_local_recoil(int& shots_fired, Vec2& punch) const;
    std::string read_pawn_name(std::uint64_t pawn) const;
    bool is_player_pawn(std::uint64_t entity);
    std::uint64_t resolve_handle(std::uint32_t handle) const;
    void walk_entity_list(Snapshot& out, std::uint64_t local_pawn);

    const Config& cfg_;
    ProcessMemory mem_;
    std::uint64_t base_ = 0;
    std::uint64_t entity_system_ = 0;   // кеш CGameEntitySystem* на кадр
    mutable std::atomic<std::uint64_t> local_pawn_{0};
    bool reported_missing_ = false;
    std::unordered_map<std::uint64_t, bool> vtable_cache_;
};

} // namespace cs2esp
