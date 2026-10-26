#include "game.hpp"

#include "common/log.hpp"

#include <cmath>
#include <cstring>

namespace cs2esp {
namespace {

std::string hex_addr(std::uint64_t a) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(a));
    return std::string(buf);
}

std::uint64_t chunk_slot_of(std::uint64_t idx, int chunk_shift) {
    return idx >> chunk_shift;
}

std::uint64_t slot_in_chunk_of(std::uint64_t idx, int chunk_shift) {
    return idx & ((1ULL << chunk_shift) - 1);
}

// Itanium C++ ABI typeinfo::name() → mangled "<len><name>" (например "14C_CSPlayerPawn"
// для "C_CSPlayerPawn" длиной 14). Пропускаем цифровой префикс и сравниваем остаток.
bool class_name_equals(const char* raw, const char* expected) {
    while (*raw >= '0' && *raw <= '9') ++raw;
    return std::strcmp(raw, expected) == 0;
}

constexpr const char kPlayerPawnClass[] = "C_CSPlayerPawn";

} // namespace

Game::Game(const Config& cfg) : cfg_(cfg) {}

bool Game::read_pawn_meta(std::uint64_t pawn, int& health, int& team, bool& alive) const {
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_health == 0 || o.pawn_life_state == 0 || o.pawn_team_num == 0) {
        return false;
    }
    const std::uint32_t hp = mem_.read_or<std::uint32_t>(pawn + o.pawn_health, 0);
    const std::uint8_t  ls = mem_.read_or<std::uint8_t>(pawn + o.pawn_life_state, 1);
    const std::uint8_t  tn = mem_.read_or<std::uint8_t>(pawn + o.pawn_team_num, 0);
    health = static_cast<int>(hp);
    team   = static_cast<int>(tn);
    alive  = (ls == 0) && (hp > 0) && (hp < 100000);
    return true;
}

bool Game::read_pawn_origin(std::uint64_t pawn, Vec3& out) const {
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_p_game_scene_node == 0) return false;

    const std::uint64_t node = mem_.read_or<std::uint64_t>(pawn + o.pawn_p_game_scene_node, 0);
    if (node == 0) return false;

    float v[3];

    if (o.node_origin != 0 &&
        mem_.read_quiet(node + o.node_origin, v, sizeof(v))) {
        out.x = v[0]; out.y = v[1]; out.z = v[2];
        return true;
    }
    if (o.node_model_state != 0 && o.model_state_origin != 0) {
        const std::uint64_t ms = node + o.node_model_state + o.model_state_origin;
        if (mem_.read_quiet(ms, v, sizeof(v))) {
            out.x = v[0]; out.y = v[1]; out.z = v[2];
            return true;
        }
    }
    return false;
}

bool Game::read_pawn_bones(std::uint64_t pawn, std::vector<Vec3>& out) const {
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_p_game_scene_node == 0 || o.node_bone_array == 0) return false;

    const std::uint64_t node = mem_.read_or<std::uint64_t>(pawn + o.pawn_p_game_scene_node, 0);
    if (node == 0) return false;

    const std::uint64_t bone_ptr = mem_.read_or<std::uint64_t>(node + o.node_bone_array, 0);
    if (bone_ptr == 0) return false;

    constexpr size_t kNumBones = 24;
    float buf[kNumBones * 8]; // 24 * 32 bytes = 768 bytes
    if (!mem_.read_quiet(bone_ptr, buf, sizeof(buf))) return false;

    out.resize(kNumBones);
    for (size_t i = 0; i < kNumBones; ++i) {
        out[i].x = buf[i * 8 + 0];
        out[i].y = buf[i * 8 + 1];
        out[i].z = buf[i * 8 + 2];
    }
    return true;
}

bool Game::read_pawn_spotted(std::uint64_t pawn) const {
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_entity_spotted_state == 0) return false;

    const std::uint64_t state_addr = pawn + o.pawn_entity_spotted_state;
    std::uint8_t spotted = 0;
    std::uint32_t mask[2] = {0, 0};

    if (mem_.read_quiet(state_addr + 8, &spotted, 1) && spotted != 0) {
        return true;
    }
    if (mem_.read_quiet(state_addr + 12, mask, sizeof(mask))) {
        if (mask[0] != 0 || mask[1] != 0) {
            return true;
        }
    }
    return false;
}

bool Game::read_local_recoil(int& shots_fired, Vec2& punch) const {
    shots_fired = 0;
    punch = {0.0f, 0.0f};

    const std::uint64_t pawn = local_pawn_.load();
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_shots_fired == 0) return false;

    std::int32_t shots = 0;
    if (!mem_.read_quiet(pawn + o.pawn_shots_fired, &shots, sizeof(shots))) {
        return false;
    }
    shots_fired = shots;

    if (o.pawn_aim_punch_services == 0) return true;

    std::uint64_t punch_svc = 0;
    if (!mem_.read_quiet(pawn + o.pawn_aim_punch_services, &punch_svc, sizeof(punch_svc)) || punch_svc == 0) {
        return true;
    }

    // m_predictableBaseAngle (QAngle: pitch, yaw, roll)
    const std::uint64_t angle_off = (o.aim_punch_angle > 0) ? o.aim_punch_angle : 80;
    float angles[3] = {0.0f, 0.0f, 0.0f};
    if (mem_.read_quiet(punch_svc + angle_off, angles, sizeof(angles))) {
        punch.x = angles[0]; // pitch (negative when kicked up)
        punch.y = angles[1]; // yaw (horizontal displacement)
    }
    return true;
}

// m_iszPlayerName в CS2 — inline-строка внутри CBasePlayerController (не указатель).
// Читаем прямо из controller + offset и обрезаем по нулю.
std::string Game::read_pawn_name(std::uint64_t pawn) const {
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_controller_handle == 0 || o.controller_name == 0 ||
        entity_system_.load() == 0) {
        return {};
    }
    const std::uint32_t handle = mem_.read_or<std::uint32_t>(pawn + o.pawn_controller_handle, 0);
    if ((handle & 0x7FFFFFFF) == 0) return {};

    const std::uint64_t controller = resolve_handle(handle);
    if (controller == 0) return {};

    char buf[64] = {};
    if (!mem_.read_quiet(controller + o.controller_name, buf, sizeof(buf) - 1)) return {};
    buf[sizeof(buf) - 1] = '\0';
    // Отфильтруем явный мусор (не-ASCII в первом байте / пусто)
    if (static_cast<unsigned char>(buf[0]) < 0x20 ||
        static_cast<unsigned char>(buf[0]) > 0x7E) {
        return {};
    }
    return std::string(buf);
}

bool Game::is_player_pawn(std::uint64_t entity) {
    if (entity == 0) return false;
    const std::uint64_t vt = mem_.read_or<std::uint64_t>(entity, 0);
    if (vt < 0x1000) return false;

    auto it = vtable_cache_.find(vt);
    if (it != vtable_cache_.end()) return it->second;

    const std::uint64_t rtti = mem_.read_or<std::uint64_t>(vt - 8, 0);
    if (rtti < 0x1000) { vtable_cache_[vt] = false; return false; }
    const std::uint64_t name_ptr = mem_.read_or<std::uint64_t>(rtti + 8, 0);
    if (name_ptr < 0x1000) { vtable_cache_[vt] = false; return false; }

    char buf[48] = {};
    if (!mem_.read_quiet(name_ptr, buf, sizeof(buf) - 1)) {
        vtable_cache_[vt] = false;
        return false;
    }
    buf[sizeof(buf) - 1] = '\0';
    const bool is_pawn = class_name_equals(buf, kPlayerPawnClass);
    vtable_cache_[vt] = is_pawn;
    return is_pawn;
}

// Resolve CHandle → entity-pointer через bucket array CGameEntitySystem.
// entity_system_ уже закеширован в update() на кадре.
std::uint64_t Game::resolve_handle(std::uint32_t handle) const {
    const std::uint64_t es = entity_system_.load();
    if (es == 0) return 0;
    const Offsets& o = cfg_.offsets;
    const std::uint64_t idx = handle & o.el_index_mask;
    const std::uint64_t chunk = chunk_slot_of(idx, static_cast<int>(o.el_chunk_shift));
    const std::uint64_t slot  = slot_in_chunk_of(idx, static_cast<int>(o.el_chunk_shift));

    const std::uint64_t bucket = mem_.read_or<std::uint64_t>(
        es + o.el_chunk_stride + o.el_pointer_size * chunk, 0);
    if (bucket == 0) return 0;
    return mem_.read_or<std::uint64_t>(bucket + o.el_entity_stride * slot, 0);
}

void Game::walk_entity_list(Snapshot& out, std::uint64_t local_pawn) {
    const Offsets& o = cfg_.offsets;
    const int shift = static_cast<int>(o.el_chunk_shift);

    std::uint64_t cached_chunk_slot = 0xFFFFFFFFFFFFFFFFULL;
    std::uint64_t bucket_ptr = 0;

    constexpr std::uint64_t kMaxIndex = 16384;

    for (std::uint64_t idx = 1; idx <= o.el_index_mask && idx <= kMaxIndex; ++idx) {
        const std::uint64_t chunk_slot = chunk_slot_of(idx, shift);
        if (chunk_slot != cached_chunk_slot) {
            cached_chunk_slot = chunk_slot;
            bucket_ptr = mem_.read_or<std::uint64_t>(
                entity_system_.load() + o.el_chunk_stride + o.el_pointer_size * chunk_slot, 0);
        }
        if (bucket_ptr == 0) {
            const std::uint64_t next_chunk_start = (chunk_slot + 1) << shift;
            if (next_chunk_start > idx) {
                idx = next_chunk_start - 1;
            }
            continue;
        }

        const std::uint64_t entity = mem_.read_or<std::uint64_t>(
            bucket_ptr + o.el_entity_stride * slot_in_chunk_of(idx, shift), 0);
        if (entity == 0 || entity == local_pawn) continue;

        if (!is_player_pawn(entity)) continue;

        PlayerSnap p;
        if (!read_pawn_meta(entity, p.health, p.team, p.alive)) continue;
        if (!p.alive) continue;
        if (!read_pawn_origin(entity, p.origin)) continue;

        if (cfg_.team_check && out.have_local && p.team == out.local_team) continue;
        if (cfg_.max_distance > 0.f && out.have_local) {
            const float dx = out.local_origin.x - p.origin.x;
            const float dy = out.local_origin.y - p.origin.y;
            const float dz = out.local_origin.z - p.origin.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > cfg_.max_distance) continue;
        }
        p.has_bones = read_pawn_bones(entity, p.bones);
        p.is_visible = read_pawn_spotted(entity);
        p.name = read_pawn_name(entity);
        out.players.push_back(std::move(p));
    }
}

void Game::update(Snapshot& out) {
    out = Snapshot{};

    if (!mem_.attached()) {
        std::string err;
        if (mem_.open(cfg_.process_name, err)) {
            base_ = 0;
            log_info("attached to " + cfg_.process_name + " (pid " +
                     std::to_string(mem_.pid()) + ")");
        } else {
            log_throttled("game.not_found", 5, LogLevel::Info, "waiting for cs2: " + err);
            return;
        }
    }

    if (!mem_.process_alive()) {
        log_throttled("game.died", 5, LogLevel::Warn,
                      cfg_.process_name + " process is gone; waiting for restart");
        mem_.close();
        base_ = 0;
        entity_system_.store(0);
        local_pawn_.store(0);
        vtable_cache_.clear();
        return;
    }

    if (base_ == 0) {
        base_ = mem_.module_base(cfg_.game_module);
        if (base_ == 0) {
            log_throttled("game.module_base", 5, LogLevel::Info,
                          "module " + cfg_.game_module + " not mapped yet");
            out.attached = true;
            return;
        }
        log_info("module base: " + hex_addr(base_));
    }
    out.attached = true;

    const Offsets& o = cfg_.offsets;

    if (o.rva_entity_list == 0 || o.rva_view_matrix == 0) {
        if (!reported_missing_) {
            reported_missing_ = true;
            if (o.rva_entity_list == 0) {
                log_error("offsets.json: rvas.dwEntityList == 0 — entity list не читается; "
                          "заполни из свежего дампа (README: «Как обновлять offsets»)");
            }
            if (o.rva_view_matrix == 0) {
                log_error("offsets.json: rvas.dwViewMatrix == 0 — world-to-screen невозможен; "
                          "заполни из свежего дампа (README: «Как обновлять offsets»)");
            }
        }
        out.offsets_ready = false;
        return;
    }
    out.offsets_ready = true;

    // Entity system — хранится как указатель-глобал в rodata libclient.so.
    // Деред нужен, иначе будем ходить по байтам секции модуля.
    const std::uint64_t es = mem_.read_or<std::uint64_t>(base_ + o.rva_entity_list, 0);
    entity_system_.store(es);
    if (es == 0) {
        log_throttled("game.es_null", 5, LogLevel::Info,
                      "entity system pointer null (игра ещё не в карте)");
    }

    // view matrix
    float raw[16];
    if (mem_.read_quiet(base_ + o.rva_view_matrix, raw, sizeof(raw))) {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                out.matrix.m[r][c] = raw[r * 4 + c];
            }
        }
        out.matrix_ok = true;
    } else {
        log_throttled("game.matrix", 5, LogLevel::Warn, "view matrix read failed");
    }

    // локальный игрок: controller → m_hPawn → resolve → pawn
    std::uint64_t local_pawn = 0;
    if (o.rva_local_player_controller != 0 && o.controller_pawn_handle != 0 &&
        es != 0) {
        const std::uint64_t lc = mem_.read_or<std::uint64_t>(
            base_ + o.rva_local_player_controller, 0);
        if (lc != 0) {
            const std::uint32_t pawn_handle = mem_.read_or<std::uint32_t>(
                lc + o.controller_pawn_handle, 0);
            local_pawn = resolve_handle(pawn_handle);
            local_pawn_.store(local_pawn);
            if (local_pawn != 0) {
                int hp = 0, team = 0;
                bool alive = false;
                if (read_pawn_meta(local_pawn, hp, team, alive) && alive) {
                    Vec3 lo;
                    if (read_pawn_origin(local_pawn, lo)) {
                        out.have_local = true;
                        out.local_team = team;
                        out.local_origin = lo;
                        read_local_recoil(out.local_shots_fired, out.local_punch);
                    } else {
                        log_throttled("game.local_origin", 5, LogLevel::Warn,
                                      "cannot read local pawn origin");
                    }
                }
            } else {
                log_throttled("game.local_pawn_null", 5, LogLevel::Info,
                              "local pawn pointer is null (in menu / dead?)");
            }
        } else {
            log_throttled("game.lc_null", 5, LogLevel::Info,
                          "local controller pointer is null (в главном меню?)");
        }
    } else if (!reported_missing_) {
        log_warn("offsets.json: rvas.dwLocalPlayerController или controller.m_hPawn == 0 — "
                 "team check отключен");
    }

    walk_entity_list(out, local_pawn);
}

bool Game::get_crosshair_target(int& target_ent_idx, int& target_team, int& target_health) const {
    const std::uint64_t pawn = local_pawn_.load();
    const Offsets& o = cfg_.offsets;
    if (pawn == 0 || o.pawn_ident_index == 0 || !mem_.attached() || entity_system_.load() == 0) {
        return false;
    }

    std::int32_t ident = -1;
    if (!mem_.read_quiet(pawn + o.pawn_ident_index, &ident, sizeof(ident))) {
        return false;
    }

    if (ident <= 0) return false;

    const std::uint64_t target_pawn = resolve_handle(static_cast<std::uint32_t>(ident));
    if (target_pawn == 0) return false;

    int health = 0, team = 0;
    bool alive = false;
    if (!read_pawn_meta(target_pawn, health, team, alive) || !alive || health <= 0) {
        return false;
    }

    target_ent_idx = ident;
    target_team = team;
    target_health = health;
    return true;
}

} // namespace cs2esp
