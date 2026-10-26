#include "common/config.hpp"

#include "common/json.hpp"
#include "common/log.hpp"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

namespace cs2esp {
namespace {

std::string read_file(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = path + ": cannot open file (" + std::strerror(errno) + ")";
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// --- helpers for typed access with clear error messages ---

bool get_string(const json::Value& obj, const std::string& key,
                std::string& out, const std::string& file, std::string& error) {
    const json::Value* v = obj.find(key);
    if (!v || v->is_null()) return true; // not present — we keep the default
    if (!v->is_string()) {
        error = file + ": field '" + key + "' must be a string";
        return false;
    }
    out = v->string;
    return true;
}

bool get_int(const json::Value& obj, const std::string& key,
             int& out, const std::string& file, std::string& error) {
    const json::Value* v = obj.find(key);
    if (!v || v->is_null()) return true;
    if (!v->is_number()) {
        error = file + ": field '" + key + "' must be a number";
        return false;
    }
    out = static_cast<int>(v->number);
    return true;
}

bool get_float(const json::Value& obj, const std::string& key,
               float& out, const std::string& file, std::string& error) {
    const json::Value* v = obj.find(key);
    if (!v || v->is_null()) return true;
    if (!v->is_number()) {
        error = file + ": field '" + key + "' must be a number";
        return false;
    }
    out = static_cast<float>(v->number);
    return true;
}

bool get_bool(const json::Value& obj, const std::string& key,
              bool& out, const std::string& file, std::string& error) {
    const json::Value* v = obj.find(key);
    if (!v || v->is_null()) return true;
    if (!v->is_bool()) {
        error = file + ": field '" + key + "' must be a boolean";
        return false;
    }
    out = v->boolean;
    return true;
}

// "#RRGGBBAA" → stored as-is until the overlay (validated format)
bool get_color(const json::Value& obj, const std::string& key,
               std::string& out, const std::string& file, std::string& error) {
    if (!get_string(obj, key, out, file, error)) return false;
    if (out.size() != 9 || out[0] != '#') {
        error = file + ": '" + key + "' must look like #RRGGBBAA (9 chars), got '" + out + "'";
        return false;
    }
    for (std::size_t i = 1; i < out.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(out[i]))) {
            error = file + ": '" + key + "' has non-hex digit at position " + std::to_string(i);
            return false;
        }
    }
    return true;
}

// uint64 offsets: number (may be integer) or null, null = 0
bool get_u64(const json::Value& obj, const std::string& key,
             std::uint64_t& out, const std::string& file, std::string& error) {
    const json::Value* v = obj.find(key);
    if (!v) return true; // field absent → stays 0 / default
    if (v->is_null()) { out = 0; return true; }
    if (!v->is_number() || v->number < 0.0) {
        error = file + ": field '" + key + "' must be a non-negative number or null";
        return false;
    }
    out = static_cast<std::uint64_t>(v->number);
    return true;
}

const json::Value* optional_section(const json::Value& root, const std::string& name,
                                    const std::string& file, std::string& error) {
    const json::Value* v = root.find(name);
    if (!v || v->is_null()) return nullptr;
    if (!v->is_object()) {
        error = file + ": section '" + name + "' must be an object or null";
        return nullptr;
    }
    return v;
}

} // namespace

bool load_config(const std::string& config_file,
                 const std::string& offsets_file,
                 Config& out,
                 std::string& error) {
    // --- config.json ---
    {
        const std::string text = read_file(config_file, error);
        if (text.empty()) return false;

        json::Value root;
        if (auto parsed = json::parse(text, error)) {
            root = std::move(*parsed);
        } else {
            error = config_file + ": JSON parse error: " + error;
            return false;
        }
        if (!root.is_object()) {
            error = config_file + ": top-level must be an object";
            return false;
        }

        if (!get_string(root, "process_name", out.process_name, config_file, error)) return false;
        if (!get_string(root, "game_module",  out.game_module,  config_file, error)) return false;
        if (!get_string(root, "socket_path",  out.socket_path,  config_file, error)) return false;
        if (!get_string(root, "hotkey",       out.hotkey,       config_file, error)) return false;
        if (!get_int(root,   "fps_limit",     out.fps_limit,    config_file, error)) return false;
        if (!get_bool(root,  "team_check",    out.team_check,   config_file, error)) return false;
        if (!get_float(root, "max_distance",  out.max_distance, config_file, error)) return false;

        if (out.fps_limit <= 0 || out.fps_limit > 1000) {
            error = config_file + ": 'fps_limit' must be in 1..1000, got " + std::to_string(out.fps_limit);
            return false;
        }

        if (const json::Value* st = optional_section(root, "stealth", config_file, error)) {
            get_string(*st, "spoof_process_name",    out.stealth.spoof_process_name,    config_file, error);
            get_bool(*st,   "randomize_window_class", out.stealth.randomize_window_class, config_file, error);
        }

        if (const json::Value* colors = optional_section(root, "colors", config_file, error)) {
            if (!get_color(*colors, "local",    out.color_local,    config_file, error)) return false;
            if (!get_color(*colors, "enemy",    out.color_enemy,    config_file, error)) return false;
            if (!get_color(*colors, "teammate", out.color_teammate, config_file, error)) return false;
        }

        if (const json::Value* esp = optional_section(root, "esp", config_file, error)) {
            if (!get_float(*esp, "box_height_units", out.esp.box_height_units, config_file, error)) return false;
            if (!get_float(*esp, "box_width_ratio",  out.esp.box_width_ratio,  config_file, error)) return false;
            if (!get_float(*esp, "health_bar_width", out.esp.health_bar_width, config_file, error)) return false;
            if (!get_bool(*esp,  "show_names",       out.esp.show_names,       config_file, error)) return false;
            if (!get_bool(*esp,  "show_distance",    out.esp.show_distance,    config_file, error)) return false;
            if (!get_bool(*esp,  "show_status_text", out.esp.show_status_text, config_file, error)) return false;
            if (!get_bool(*esp,  "show_skeleton",    out.esp.show_skeleton,    config_file, error)) return false;
            if (!get_string(*esp, "font",            out.esp.font,             config_file, error)) return false;
            if (!get_int(*esp,   "monitor",          out.esp.monitor,          config_file, error)) return false;
        }

        // Параметры aimbot
        if (const json::Value* ab = optional_section(root, "aimbot", config_file, error)) {
            get_bool(*ab,   "enabled",    out.aimbot.enabled,    config_file, error);
            get_string(*ab, "hotkey",     out.aimbot.hotkey,     config_file, error);
            get_string(*ab, "bone",       out.aimbot.bone,       config_file, error);
            get_float(*ab,  "fov",        out.aimbot.fov,        config_file, error);
            get_float(*ab,  "smooth",        out.aimbot.smooth,        config_file, error);
            get_bool(*ab,   "team_check",    out.aimbot.team_check,    config_file, error);
            get_bool(*ab,   "visible_check", out.aimbot.visible_check, config_file, error);
            get_bool(*ab,   "humanize",      out.aimbot.humanize,      config_file, error);
        }
        if (out.aimbot.fov < 10.0f) out.aimbot.fov = 10.0f;
        if (out.aimbot.smooth < 1.0f) out.aimbot.smooth = 1.0f;

        // Параметры triggerbot (как на верхнем уровне, так и в блоке "triggerbot")
        get_bool(root, "triggerbot_enabled", out.triggerbot.enabled, config_file, error);
        get_string(root, "triggerbot_hotkey", out.triggerbot.hotkey, config_file, error);
        get_int(root, "triggerbot_pre_delay_ms", out.triggerbot.pre_delay_ms, config_file, error);
        get_int(root, "triggerbot_post_delay_ms", out.triggerbot.post_delay_ms, config_file, error);
        get_int(root, "triggerbot_cooldown_ms", out.triggerbot.cooldown_ms, config_file, error);

        if (const json::Value* tb = optional_section(root, "triggerbot", config_file, error)) {
            get_bool(*tb,   "enabled",          out.triggerbot.enabled,         config_file, error);
            get_string(*tb, "hotkey",           out.triggerbot.hotkey,          config_file, error);
            get_int(*tb,    "pre_delay_ms",     out.triggerbot.pre_delay_ms,    config_file, error);
            get_int(*tb,    "post_delay_ms",    out.triggerbot.post_delay_ms,   config_file, error);
            get_int(*tb,    "cooldown_ms",      out.triggerbot.cooldown_ms,     config_file, error);
            get_bool(*tb,   "humanize",         out.triggerbot.humanize,        config_file, error);
        }
        if (out.triggerbot.pre_delay_ms < 0) out.triggerbot.pre_delay_ms = 0;
        if (out.triggerbot.post_delay_ms < 1) out.triggerbot.post_delay_ms = 20;
        if (out.triggerbot.cooldown_ms < 1) out.triggerbot.cooldown_ms = 100;
    }

    // --- offsets.json ---
    {
        const std::string text = read_file(offsets_file, error);
        if (text.empty()) return false;

        json::Value root;
        if (auto parsed = json::parse(text, error)) {
            root = std::move(*parsed);
        } else {
            error = offsets_file + ": JSON parse error: " + error;
            return false;
        }
        if (!root.is_object()) {
            error = offsets_file + ": top-level must be an object";
            return false;
        }
        Offsets& o = out.offsets;

        if (const json::Value* rvas = optional_section(root, "rvas", offsets_file, error)) {
            if (!get_u64(*rvas, "dwLocalPlayerController", o.rva_local_player_controller, offsets_file, error)) return false;
            if (!get_u64(*rvas, "dwEntityList",      o.rva_entity_list,      offsets_file, error)) return false;
            if (!get_u64(*rvas, "dwViewMatrix",      o.rva_view_matrix,      offsets_file, error)) return false;
        }

        if (const json::Value* el = optional_section(root, "entity_list", offsets_file, error)) {
            if (!get_u64(*el, "pointer_size", out.offsets.el_pointer_size, offsets_file, error)) return false;
            if (!get_u64(*el, "chunk_stride", out.offsets.el_chunk_stride,  offsets_file, error)) return false;
            if (!get_u64(*el, "entity_stride", out.offsets.el_entity_stride, offsets_file, error)) return false;
            if (!get_u64(*el, "chunk_shift",  out.offsets.el_chunk_shift,   offsets_file, error)) return false;
            if (!get_u64(*el, "index_mask",   out.offsets.el_index_mask,   offsets_file, error)) return false;

            if (o.el_chunk_shift > 24 ||
                o.el_entity_stride == 0 || o.el_pointer_size == 0) {
                error = offsets_file + ": entity_list constants look wrong "
                        "(chunk_shift <= 24, pointer_size/entity_stride > 0 required)";
                return false;
            }
        }

        if (const json::Value* pawn = optional_section(root, "pawn", offsets_file, error)) {
            if (!get_u64(*pawn, "m_pGameSceneNode",   o.pawn_p_game_scene_node,   offsets_file, error)) return false;
            if (!get_u64(*pawn, "m_iHealth",          o.pawn_health,              offsets_file, error)) return false;
            if (!get_u64(*pawn, "m_lifeState",        o.pawn_life_state,          offsets_file, error)) return false;
            if (!get_u64(*pawn, "m_iTeamNum",         o.pawn_team_num,            offsets_file, error)) return false;
            if (!get_u64(*pawn, "m_hController",      o.pawn_controller_handle,   offsets_file, error)) return false;
            get_u64(*pawn, "m_iIDEntIndex", o.pawn_ident_index, offsets_file, error);
            get_u64(*pawn, "m_entitySpottedState", o.pawn_entity_spotted_state, offsets_file, error);
        }

        if (const json::Value* node = optional_section(root, "game_scene_node", offsets_file, error)) {
            if (!get_u64(*node, "m_vecAbsOrigin",   o.node_origin,         offsets_file, error)) return false;
            if (!get_u64(*node, "m_modelState",     o.node_model_state,    offsets_file, error)) return false;
            if (!get_u64(*node, "modelState_origin", o.model_state_origin, offsets_file, error)) return false;
            get_u64(*node, "m_pBoneArray", o.node_bone_array, offsets_file, error);
        }

        if (const json::Value* ctl = optional_section(root, "controller", offsets_file, error)) {
            if (!get_u64(*ctl, "m_iszPlayerName", o.controller_name,         offsets_file, error)) return false;
            if (!get_u64(*ctl, "m_hPawn",         o.controller_pawn_handle,  offsets_file, error)) return false;
        }
    }

    error.clear();
    return true;
}

bool save_config(const std::string& config_file,
                 const Config& cfg,
                 std::string& error) {
    std::ofstream f(config_file, std::ios::trunc);
    if (!f.is_open()) {
        error = "cannot open file for writing: " + config_file;
        return false;
    }

    f << "{\n"
      << "  \"process_name\": \"" << cfg.process_name << "\",\n"
      << "  \"game_module\": \"" << cfg.game_module << "\",\n"
      << "  \"socket_path\": \"" << cfg.socket_path << "\",\n"
      << "  \"hotkey\": \"" << cfg.hotkey << "\",\n"
      << "  \"fps_limit\": " << cfg.fps_limit << ",\n"
      << "  \"team_check\": " << (cfg.team_check ? "true" : "false") << ",\n"
      << "  \"max_distance\": " << cfg.max_distance << ",\n\n"
      << "  \"stealth\": {\n"
      << "    \"spoof_process_name\": \"" << cfg.stealth.spoof_process_name << "\",\n"
      << "    \"randomize_window_class\": " << (cfg.stealth.randomize_window_class ? "true" : "false") << "\n"
      << "  },\n\n"
      << "  \"colors\": {\n"
      << "    \"local\": \"" << cfg.color_local << "\",\n"
      << "    \"enemy\": \"" << cfg.color_enemy << "\",\n"
      << "    \"teammate\": \"" << cfg.color_teammate << "\"\n"
      << "  },\n\n"
      << "  \"esp\": {\n"
      << "    \"box_height_units\": " << cfg.esp.box_height_units << ",\n"
      << "    \"box_width_ratio\": " << cfg.esp.box_width_ratio << ",\n"
      << "    \"health_bar_width\": " << cfg.esp.health_bar_width << ",\n"
      << "    \"show_names\": " << (cfg.esp.show_names ? "true" : "false") << ",\n"
      << "    \"show_distance\": " << (cfg.esp.show_distance ? "true" : "false") << ",\n"
      << "    \"show_status_text\": " << (cfg.esp.show_status_text ? "true" : "false") << ",\n"
      << "    \"show_skeleton\": " << (cfg.esp.show_skeleton ? "true" : "false") << ",\n"
      << "    \"font\": \"" << cfg.esp.font << "\",\n"
      << "    \"monitor\": " << cfg.esp.monitor << "\n"
      << "  },\n\n"
      << "  \"aimbot\": {\n"
      << "    \"enabled\": " << (cfg.aimbot.enabled ? "true" : "false") << ",\n"
      << "    \"hotkey\": \"" << cfg.aimbot.hotkey << "\",\n"
      << "    \"bone\": \"" << cfg.aimbot.bone << "\",\n"
      << "    \"fov\": " << cfg.aimbot.fov << ",\n"
      << "    \"smooth\": " << cfg.aimbot.smooth << ",\n"
      << "    \"team_check\": " << (cfg.aimbot.team_check ? "true" : "false") << ",\n"
      << "    \"visible_check\": " << (cfg.aimbot.visible_check ? "true" : "false") << ",\n"
      << "    \"humanize\": " << (cfg.aimbot.humanize ? "true" : "false") << "\n"
      << "  },\n\n"
      << "  \"triggerbot\": {\n"
      << "    \"enabled\": " << (cfg.triggerbot.enabled ? "true" : "false") << ",\n"
      << "    \"hotkey\": \"" << cfg.triggerbot.hotkey << "\",\n"
      << "    \"pre_delay_ms\": " << cfg.triggerbot.pre_delay_ms << ",\n"
      << "    \"post_delay_ms\": " << cfg.triggerbot.post_delay_ms << ",\n"
      << "    \"cooldown_ms\": " << cfg.triggerbot.cooldown_ms << ",\n"
      << "    \"humanize\": " << (cfg.triggerbot.humanize ? "true" : "false") << "\n"
      << "  }\n"
      << "}\n";

    if (!f.good()) {
        error = "failed writing to file: " + config_file;
        return false;
    }
    error.clear();
    return true;
}

std::vector<std::string> missing_offsets(const Offsets& o) {
    std::vector<std::string> out;

    auto add = [&out](bool missing, const std::string& path) {
        if (missing) out.push_back(path);
    };

    add(o.rva_entity_list == 0,   "rvas.dwEntityList");
    add(o.rva_view_matrix == 0,   "rvas.dwViewMatrix");
    add(o.rva_local_player_controller == 0, "rvas.dwLocalPlayerController");
    add(o.controller_pawn_handle == 0, "controller.m_hPawn");
    add(o.pawn_p_game_scene_node == 0, "pawn.m_pGameSceneNode");
    add(o.pawn_health == 0,       "pawn.m_iHealth");
    add(o.pawn_life_state == 0,   "pawn.m_lifeState");
    add(o.pawn_team_num == 0,     "pawn.m_iTeamNum");
    // origin: берётся прямо из node или через model_state — достаточно одного из путей
    add(o.node_origin == 0 && (o.node_model_state == 0 || o.model_state_origin == 0),
        "game_scene_node.m_vecAbsOrigin (или пара m_modelState + modelState_origin)");

    return out;
}

} // namespace cs2esp
