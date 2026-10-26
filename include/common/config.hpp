#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cs2esp {

// offsets.json — RVA в клиентском модуле игры и смещения полей структур.
// Значение 0 = «не заполнено»: демоны напишут в лог и не упадут.
struct Offsets {
    // RVA в клиентский модуль (libclient.so)
    std::uint64_t rva_local_player_controller = 0;  // dwLocalPlayerController (pointer)
    std::uint64_t rva_entity_list = 0;         // dwEntityList (указатель на CGameEntitySystem)
    std::uint64_t rva_view_matrix = 0;         // dwViewMatrix

    // Структура entity list (константы схемы, не смещения)
    std::uint64_t el_pointer_size    = 8;    // sizeof записи указателя
    std::uint64_t el_chunk_stride    = 0;    // смещение к bucket array внутри entity_system (0 если dwEntityList уже → bucket array)
    std::uint64_t el_entity_stride   = 112;  // sizeof(CEntityIdentity)
    std::uint64_t el_chunk_shift     = 9;    // index >> 9 → номер чанка
    std::uint64_t el_index_mask      = 0x7FFF;

    // Поля C_BasePlayerPawn
    std::uint64_t pawn_p_game_scene_node = 0;   // m_pGameSceneNode
    std::uint64_t pawn_health            = 0;   // m_iHealth (uint32)
    std::uint64_t pawn_life_state        = 0;   // m_lifeState (uint8)
    std::uint64_t pawn_team_num          = 0;   // m_iTeamNum
    std::uint64_t pawn_controller_handle = 0;   // m_hController (CHandle)
    std::uint64_t pawn_ident_index       = 17796; // m_iIDEntIndex (int32, crosshair entity ID)
    std::uint64_t pawn_entity_spotted_state = 11544; // m_entitySpottedState (0x2D18)

    // Поля C_GameSceneNode
    std::uint64_t node_origin        = 0;   // m_vecAbsOrigin (Vector3, абсолютные мировые координаты)
    std::uint64_t node_model_state   = 0;   // m_modelState (альтернативный путь до origin)
    std::uint64_t model_state_origin = 0;   // смещение внутри CModelState
    std::uint64_t node_bone_array    = 448; // m_pBoneArray внутри C_GameSceneNode (0x1C0)

    // Никнейм (опционально): CBasePlayerController::m_iszPlayerName (inline-строка)
    std::uint64_t controller_name = 0;
    // CBasePlayerController::m_hPawn (resolve local pawn через CHandle)
    std::uint64_t controller_pawn_handle = 0;
};

struct Config {
    std::string process_name = "cs2";           // имя процесса игры
    std::string game_module  = "libclient.so";  // клиентский модуль
    std::string socket_path  = "@.cs_pipe_stream"; // абстрактный сокет (без файлов в /tmp)
    std::string hotkey       = "F8";
    int  fps_limit           = 144;
    bool team_check          = true;            // не рисовать своих
    float max_distance       = 0.0f;            // в игровых юнитах; 0 = без ограничения

    struct Stealth {
        std::string spoof_process_name = "pipewire-sys"; // маскировка в /proc/<pid>/comm и cmdline
        bool randomize_window_class    = true;           // случайный WM_CLASS для оверлея
    } stealth;

    // Цвета в формате #RRGGBBAA
    std::string color_local    = "#00FF00FF";
    std::string color_enemy    = "#FF4040FF";
    std::string color_teammate = "#40A0FFFF";

    struct Esp {
        float box_height_units   = 70.0f;  // высота боксируемых в игровых юнитах
        float box_width_ratio    = 0.45f;  // ширина = высота_в_пикселях * ratio
        float health_bar_width   = 6.0f;   // px
        bool  show_names         = true;   // если offsets заполнены
        bool  show_distance      = true;
        bool  show_status_text   = true;
        bool  show_skeleton      = true;   // Рисовать скелеты игроков (кости 0..23)
        std::string font         = "DejaVu Sans:size=10";
        int   monitor            = -1;     // -1 = авто (следим за окном игры); иначе индекс XRR-монитора
    } esp;

    struct Aimbot {
        bool enabled             = false;
        std::string hotkey       = "c";      // "always", "alt", "shift", "ctrl", "caps", "c", "v", "x"
        std::string bone         = "chest";  // "head" (кость 7) или "chest" (кость 5)
        float fov                = 35.0f;    // радиус захвата FOV в пикселях
        float smooth             = 20.0f;    // фактор сглаживания доводки мыши
        bool team_check          = true;     // целиться только во врагов
        bool visible_check       = true;     // не целиться через стены (проверка видимости)
        bool humanize            = true;     // нелинейная доводка и микро-джиттер
    } aimbot;

    struct Triggerbot {
        bool enabled             = false;
        std::string hotkey       = "alt";    // "always", "ALT", "SHIFT", "X", etc.
        int pre_delay_ms         = 190;      // задержка до выстрела (мс, центр гауссова распределения)
        int post_delay_ms        = 55;       // длительность удержания клика (мс)
        int cooldown_ms          = 350;      // кулдаун между выстрелами (мс)
        bool humanize            = true;     // нормальное гауссово распределение времени реакции
    } triggerbot;

    Offsets offsets;
};

// Загрузка config.json + offsets.json; при ошибке пишет в error
// строку с именем файла и проблемным полем.
bool load_config(const std::string& config_file,
                 const std::string& offsets_file,
                 Config& out,
                 std::string& error);

// Сохранение config.json на диск
bool save_config(const std::string& config_file,
                 const Config& cfg,
                 std::string& error);

// Незаполненные (0 или null) поля offsets в виде человекочитаемых путей JSON.
std::vector<std::string> missing_offsets(const Offsets& o);

} // namespace cs2esp
