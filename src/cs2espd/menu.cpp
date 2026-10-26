#include "menu.hpp"

#include "common/log.hpp"

#include <X11/keysym.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace cs2esp {
namespace {

const std::vector<std::string> kAimbotHotkeys = {
    "always", "alt", "shift", "ctrl", "caps", "c", "v", "x", "z", "mouse1", "mouse2"
};
const std::vector<std::string> kBones = {"head", "chest"};
const std::vector<std::string> kTriggerHotkeys = {
    "always", "alt", "shift", "ctrl", "caps", "x", "c", "v", "z", "mouse1", "mouse2"
};
const std::vector<std::string> kEspHotkeys = {"F8", "F7", "F9", "F10", "F11", "F12"};
const std::vector<int> kFpsLimits = {60, 120, 144, 165, 240, 360};

template <typename T>
int find_idx(const std::vector<T>& list, const T& val) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] == val) return static_cast<int>(i);
    }
    return 0;
}

template <typename T>
void cycle_next(const std::vector<T>& list, T& val) {
    int idx = find_idx(list, val);
    idx = (idx + 1) % static_cast<int>(list.size());
    val = list[idx];
}

template <typename T>
void cycle_prev(const std::vector<T>& list, T& val) {
    int idx = find_idx(list, val);
    idx = (idx - 1 + static_cast<int>(list.size())) % static_cast<int>(list.size());
    val = list[idx];
}

} // namespace

Menu::Menu(Config& cfg, std::string config_path,
           std::atomic<bool>& esp_on, std::atomic<bool>& aimbot_on,
           std::atomic<bool>& trigger_on)
    : cfg_(cfg), config_path_(std::move(config_path)),
      esp_on_(esp_on), aimbot_on_(aimbot_on), trigger_on_(trigger_on) {
    setup_categories();
}

void Menu::setup_categories() {
    categories_.clear();

    // 0: Main
    categories_.push_back({
        "[>]", "Main",
        { {"Overview"} }
    });

    // 1: Visuals
    categories_.push_back({
        "[#]", "Visuals",
        { {"General"}, {"Filters"} }
    });

    // 2: Aimbot
    categories_.push_back({
        "[*]", "Aimbot",
        { {"General"}, {"Tracking"} }
    });

    // 3: Triggerbot
    categories_.push_back({
        "[!]", "Trigger",
        { {"General"}, {"Delays"} }
    });

    // 4: Settings
    categories_.push_back({
        "[%]", "Settings",
        { {"Config & Binds"}, {"System"} }
    });
}

void Menu::toggle() {
    if (open_) close();
    else open();
}

void Menu::open() {
    open_ = true;
    selected_item_ = 0;
    dragging_slider_idx_ = -1;
}

void Menu::close() {
    open_ = false;
    dragging_slider_idx_ = -1;
}

XRectangle Menu::rect(int screen_w, int screen_h) const {
    win_x_ = (screen_w - win_w_) / 2;
    win_y_ = (screen_h - win_h_) / 2;
    if (win_x_ < 20) win_x_ = 20;
    if (win_y_ < 20) win_y_ = 20;

    XRectangle r;
    r.x = static_cast<short>(win_x_);
    r.y = static_cast<short>(win_y_);
    r.width = static_cast<unsigned short>(win_w_);
    r.height = static_cast<unsigned short>(win_h_);
    return r;
}

bool Menu::save(std::string& err) {
    cfg_.esp.show_status_text = true;
    cfg_.aimbot.enabled = aimbot_on_.load();
    cfg_.triggerbot.enabled = trigger_on_.load();
    return save_config(config_path_, cfg_, err);
}

void Menu::build_items_for_current_view() {
    active_items_.clear();

    if (current_cat_ == MenuCategory::Main) {
        // Col 0: Cheat Master
        {
            RuntimeItem it;
            it.card_col = 0;
            it.type = ItemType::Checkbox;
            it.label = "ESP Master";
            it.checked = esp_on_.load();
            it.on_toggle = [this]() { esp_on_.store(!esp_on_.load()); };
            active_items_.push_back(it);
        }
        {
            RuntimeItem it;
            it.card_col = 0;
            it.type = ItemType::Checkbox;
            it.label = "Aimbot Master";
            it.checked = aimbot_on_.load();
            it.on_toggle = [this]() { aimbot_on_.store(!aimbot_on_.load()); };
            active_items_.push_back(it);
        }
        {
            RuntimeItem it;
            it.card_col = 0;
            it.type = ItemType::Checkbox;
            it.label = "Triggerbot Master";
            it.checked = trigger_on_.load();
            it.on_toggle = [this]() { trigger_on_.store(!trigger_on_.load()); };
            active_items_.push_back(it);
        }
        {
            RuntimeItem it;
            it.card_col = 0;
            it.type = ItemType::Checkbox;
            it.label = "Enemies Only (Team Check)";
            it.checked = cfg_.team_check;
            it.on_toggle = [this]() { cfg_.team_check = !cfg_.team_check; };
            active_items_.push_back(it);
        }

        // Col 1: System & Actions
        {
            RuntimeItem it;
            it.card_col = 1;
            it.type = ItemType::Keybind;
            it.label = "Menu Toggle Key";
            it.val_str = "INSERT";
            active_items_.push_back(it);
        }
        {
            RuntimeItem it;
            it.card_col = 1;
            it.type = ItemType::Keybind;
            it.label = "Target Game Process";
            it.val_str = "cs2";
            active_items_.push_back(it);
        }
        {
            RuntimeItem it;
            it.card_col = 1;
            it.type = ItemType::Keybind;
            it.label = "Overlay Rendering";
            it.val_str = "Hyprland / X11";
            active_items_.push_back(it);
        }
        {
            RuntimeItem it;
            it.card_col = 1;
            it.type = ItemType::Button;
            it.label = "[ SAVE CONFIG TO DISK ]";
            it.on_click = [this]() {
                std::string err;
                if (save(err)) {
                    status_msg_ = "Config saved to " + config_path_;
                } else {
                    status_msg_ = "Save failed: " + err;
                }
                status_time_ = std::chrono::steady_clock::now();
            };
            active_items_.push_back(it);
        }
    } else if (current_cat_ == MenuCategory::Visuals) {
        if (current_subtab_ == 0) {
            // General Visuals
            // Col 0: Elements
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Enable ESP";
                it.checked = esp_on_.load();
                it.on_toggle = [this]() { esp_on_.store(!esp_on_.load()); };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Player Skeletons";
                it.checked = cfg_.esp.show_skeleton;
                it.on_toggle = [this]() { cfg_.esp.show_skeleton = !cfg_.esp.show_skeleton; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Bounding Boxes";
                it.checked = (cfg_.esp.box_height_units > 0);
                it.on_toggle = [this]() {
                    cfg_.esp.box_height_units = (cfg_.esp.box_height_units > 0) ? 0.0f : 70.0f;
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Player Names";
                it.checked = cfg_.esp.show_names;
                it.on_toggle = [this]() { cfg_.esp.show_names = !cfg_.esp.show_names; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Distance Display";
                it.checked = cfg_.esp.show_distance;
                it.on_toggle = [this]() { cfg_.esp.show_distance = !cfg_.esp.show_distance; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Health Bars";
                it.checked = (cfg_.esp.health_bar_width > 0);
                it.on_toggle = [this]() {
                    cfg_.esp.health_bar_width = (cfg_.esp.health_bar_width > 0) ? 0.0f : 6.0f;
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Overlay Status Watermark";
                it.checked = cfg_.esp.show_status_text;
                it.on_toggle = [this]() { cfg_.esp.show_status_text = !cfg_.esp.show_status_text; };
                active_items_.push_back(it);
            }

            // Col 1: Box & Bar Styles
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::SliderFloat;
                it.label = "Health Bar Width";
                it.val_str = std::to_string(static_cast<int>(cfg_.esp.health_bar_width)) + " px";
                it.progress = std::clamp(cfg_.esp.health_bar_width / 12.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.esp.health_bar_width = std::round(norm * 12.0f);
                };
                it.on_cycle = [this](int dir) {
                    cfg_.esp.health_bar_width = std::clamp(cfg_.esp.health_bar_width + dir * 1.0f, 0.0f, 12.0f);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::SliderFloat;
                it.label = "Box Width Ratio";
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.2f", cfg_.esp.box_width_ratio);
                it.val_str = buf;
                it.progress = std::clamp((cfg_.esp.box_width_ratio - 0.2f) / 0.6f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.esp.box_width_ratio = 0.2f + norm * 0.6f;
                };
                it.on_cycle = [this](int dir) {
                    cfg_.esp.box_width_ratio = std::clamp(cfg_.esp.box_width_ratio + dir * 0.05f, 0.2f, 0.8f);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::SliderFloat;
                it.label = "Box Height Units";
                it.val_str = std::to_string(static_cast<int>(cfg_.esp.box_height_units));
                it.progress = std::clamp((cfg_.esp.box_height_units - 30.0f) / 70.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.esp.box_height_units = std::round(30.0f + norm * 70.0f);
                };
                it.on_cycle = [this](int dir) {
                    cfg_.esp.box_height_units = std::clamp(cfg_.esp.box_height_units + dir * 5.0f, 30.0f, 100.0f);
                };
                active_items_.push_back(it);
            }
        } else {
            // Filters Subtab
            // Col 0: Targets & Range
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Team Check (Enemies Only)";
                it.checked = cfg_.team_check;
                it.on_toggle = [this]() { cfg_.team_check = !cfg_.team_check; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SliderFloat;
                it.label = "Max Render Distance";
                it.val_str = (cfg_.max_distance <= 0) ? "Unlimited" : (std::to_string(static_cast<int>(cfg_.max_distance)) + " u");
                it.progress = (cfg_.max_distance <= 0) ? 0.0f : std::clamp(cfg_.max_distance / 8000.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    if (norm < 0.05f) cfg_.max_distance = 0.0f;
                    else cfg_.max_distance = std::round(norm * 8000.0f);
                };
                it.on_cycle = [this](int dir) {
                    float d = cfg_.max_distance + dir * 500.0f;
                    if (d < 0.0f || d > 8000.0f) d = 0.0f;
                    cfg_.max_distance = d;
                };
                active_items_.push_back(it);
            }

            // Col 1: Presets
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ DISTANCE: UNLIMITED ]";
                it.on_click = [this]() { cfg_.max_distance = 0.0f; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ DISTANCE: 3000 UNITS ]";
                it.on_click = [this]() { cfg_.max_distance = 3000.0f; };
                active_items_.push_back(it);
            }
        }
    } else if (current_cat_ == MenuCategory::Aimbot) {
        if (current_subtab_ == 0) {
            // General Aimbot
            // Col 0: Toggles & Target
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Aimbot Enabled";
                it.checked = aimbot_on_.load();
                it.on_toggle = [this]() { aimbot_on_.store(!aimbot_on_.load()); };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SelectorStr;
                it.label = "Target Bone";
                it.val_str = cfg_.aimbot.bone;
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kBones, cfg_.aimbot.bone);
                    else cycle_prev(kBones, cfg_.aimbot.bone);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SelectorStr;
                it.label = "Activation Hotkey";
                it.val_str = cfg_.aimbot.hotkey;
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kAimbotHotkeys, cfg_.aimbot.hotkey);
                    else cycle_prev(kAimbotHotkeys, cfg_.aimbot.hotkey);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Visible Check (No Wallbang)";
                it.checked = cfg_.aimbot.visible_check;
                it.on_toggle = [this]() { cfg_.aimbot.visible_check = !cfg_.aimbot.visible_check; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Team Check (Enemies Only)";
                it.checked = cfg_.aimbot.team_check;
                it.on_toggle = [this]() { cfg_.aimbot.team_check = !cfg_.aimbot.team_check; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Humanize Aim Movement";
                it.checked = cfg_.aimbot.humanize;
                it.on_toggle = [this]() { cfg_.aimbot.humanize = !cfg_.aimbot.humanize; };
                active_items_.push_back(it);
            }

            // Col 1: Info & Mode
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Keybind;
                it.label = "Current Aim Key";
                it.val_str = cfg_.aimbot.hotkey;
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Keybind;
                it.label = "Target Priority";
                it.val_str = "Crosshair FOV";
                active_items_.push_back(it);
            }
        } else {
            // Tracking Subtab
            // Col 0: FOV & Smooth
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SliderFloat;
                it.label = "FOV Radius";
                it.val_str = std::to_string(static_cast<int>(cfg_.aimbot.fov)) + " px";
                it.progress = std::clamp((cfg_.aimbot.fov - 10.0f) / 240.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.aimbot.fov = std::round(10.0f + norm * 240.0f);
                };
                it.on_cycle = [this](int dir) {
                    cfg_.aimbot.fov = std::clamp(cfg_.aimbot.fov + dir * 5.0f, 10.0f, 250.0f);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SliderFloat;
                it.label = "Smooth Factor";
                it.val_str = std::to_string(static_cast<int>(cfg_.aimbot.smooth));
                it.progress = std::clamp((cfg_.aimbot.smooth - 2.0f) / 28.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.aimbot.smooth = std::round(2.0f + norm * 28.0f);
                };
                it.on_cycle = [this](int dir) {
                    cfg_.aimbot.smooth = std::clamp(cfg_.aimbot.smooth + dir * 1.0f, 2.0f, 30.0f);
                };
                active_items_.push_back(it);
            }

            // Col 1: Aim Presets
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ LEGIT PRESET (FOV 25, SM 22) ]";
                it.on_click = [this]() {
                    cfg_.aimbot.fov = 25.0f;
                    cfg_.aimbot.smooth = 22.0f;
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ RAGE PRESET (FOV 140, SM 4) ]";
                it.on_click = [this]() {
                    cfg_.aimbot.fov = 140.0f;
                    cfg_.aimbot.smooth = 4.0f;
                };
                active_items_.push_back(it);
            }
        }
    } else if (current_cat_ == MenuCategory::Triggerbot) {
        if (current_subtab_ == 0) {
            // General Trigger
            // Col 0: Master & Hotkey
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Triggerbot Master";
                it.checked = trigger_on_.load();
                it.on_toggle = [this]() { trigger_on_.store(!trigger_on_.load()); };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SelectorStr;
                it.label = "Activation Hotkey";
                it.val_str = cfg_.triggerbot.hotkey;
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kTriggerHotkeys, cfg_.triggerbot.hotkey);
                    else cycle_prev(kTriggerHotkeys, cfg_.triggerbot.hotkey);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Team Check (Enemies Only)";
                it.checked = cfg_.team_check;
                it.on_toggle = [this]() { cfg_.team_check = !cfg_.team_check; };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Checkbox;
                it.label = "Gaussian Humanized Delay";
                it.checked = cfg_.triggerbot.humanize;
                it.on_toggle = [this]() { cfg_.triggerbot.humanize = !cfg_.triggerbot.humanize; };
                active_items_.push_back(it);
            }

            // Col 1: Status
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Keybind;
                it.label = "Trigger Hotkey";
                it.val_str = cfg_.triggerbot.hotkey;
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Keybind;
                it.label = "Target Entity Check";
                it.val_str = "m_iIDEntIndex";
                active_items_.push_back(it);
            }
        } else {
            // Delays Subtab
            // Col 0: Timings
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SliderInt;
                it.label = "Pre-delay (Reaction)";
                it.val_str = std::to_string(cfg_.triggerbot.pre_delay_ms) + " ms";
                it.progress = std::clamp(static_cast<float>(cfg_.triggerbot.pre_delay_ms) / 300.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.triggerbot.pre_delay_ms = static_cast<int>(std::round(norm * 300.0f));
                };
                it.on_cycle = [this](int dir) {
                    cfg_.triggerbot.pre_delay_ms = std::clamp(cfg_.triggerbot.pre_delay_ms + dir * 10, 0, 300);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SliderInt;
                it.label = "Post-delay (Click Hold)";
                it.val_str = std::to_string(cfg_.triggerbot.post_delay_ms) + " ms";
                it.progress = std::clamp((static_cast<float>(cfg_.triggerbot.post_delay_ms) - 20.0f) / 60.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.triggerbot.post_delay_ms = static_cast<int>(std::round(20.0f + norm * 60.0f));
                };
                it.on_cycle = [this](int dir) {
                    cfg_.triggerbot.post_delay_ms = std::clamp(cfg_.triggerbot.post_delay_ms + dir * 5, 20, 80);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SliderInt;
                it.label = "Cooldown Between Shots";
                it.val_str = std::to_string(cfg_.triggerbot.cooldown_ms) + " ms";
                it.progress = std::clamp((static_cast<float>(cfg_.triggerbot.cooldown_ms) - 100.0f) / 350.0f, 0.0f, 1.0f);
                it.on_slide = [this](float norm) {
                    cfg_.triggerbot.cooldown_ms = static_cast<int>(std::round(100.0f + norm * 350.0f));
                };
                it.on_cycle = [this](int dir) {
                    cfg_.triggerbot.cooldown_ms = std::clamp(cfg_.triggerbot.cooldown_ms + dir * 25, 100, 450);
                };
                active_items_.push_back(it);
            }

            // Col 1: Delay Presets
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ LEGIT (190ms / 55ms / 350ms) ]";
                it.on_click = [this]() {
                    cfg_.triggerbot.pre_delay_ms = 190;
                    cfg_.triggerbot.post_delay_ms = 55;
                    cfg_.triggerbot.cooldown_ms = 350;
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ INSTANT TAP (0ms / 30ms / 150ms) ]";
                it.on_click = [this]() {
                    cfg_.triggerbot.pre_delay_ms = 0;
                    cfg_.triggerbot.post_delay_ms = 30;
                    cfg_.triggerbot.cooldown_ms = 150;
                };
                active_items_.push_back(it);
            }
        }
    } else if (current_cat_ == MenuCategory::Settings) {
        if (current_subtab_ == 0) {
            // Config & Binds
            // Col 0: Hotkeys
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Keybind;
                it.label = "Menu Toggle Key";
                it.val_str = "INSERT";
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SelectorStr;
                it.label = "ESP Quick Toggle";
                it.val_str = cfg_.hotkey;
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kEspHotkeys, cfg_.hotkey);
                    else cycle_prev(kEspHotkeys, cfg_.hotkey);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SelectorStr;
                it.label = "Aimbot Key";
                it.val_str = cfg_.aimbot.hotkey;
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kAimbotHotkeys, cfg_.aimbot.hotkey);
                    else cycle_prev(kAimbotHotkeys, cfg_.aimbot.hotkey);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::SelectorStr;
                it.label = "Triggerbot Key";
                it.val_str = cfg_.triggerbot.hotkey;
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kTriggerHotkeys, cfg_.triggerbot.hotkey);
                    else cycle_prev(kTriggerHotkeys, cfg_.triggerbot.hotkey);
                };
                active_items_.push_back(it);
            }

            // Col 1: Config & FPS
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::SelectorInt;
                it.label = "Overlay Target FPS";
                it.val_str = std::to_string(cfg_.fps_limit);
                it.on_cycle = [this](int dir) {
                    if (dir > 0) cycle_next(kFpsLimits, cfg_.fps_limit);
                    else cycle_prev(kFpsLimits, cfg_.fps_limit);
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ SAVE SETTINGS TO DISK ]";
                it.on_click = [this]() {
                    std::string err;
                    if (save(err)) {
                        status_msg_ = "Config saved to " + config_path_;
                    } else {
                        status_msg_ = "Save failed: " + err;
                    }
                    status_time_ = std::chrono::steady_clock::now();
                };
                active_items_.push_back(it);
            }
        } else {
            // System Subtab
            // Col 0: Paths
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Keybind;
                it.label = "Config File";
                it.val_str = "config/config.json";
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Keybind;
                it.label = "Offsets File";
                it.val_str = "config/offsets.json";
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 0;
                it.type = ItemType::Keybind;
                it.label = "IPC Abstract Socket";
                it.val_str = "@.cs_pipe_stream";
                active_items_.push_back(it);
            }

            // Col 1: Daemon Exit
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ RELOAD CONFIG ]";
                it.on_click = [this]() {
                    std::string err;
                    if (load_config(config_path_, "config/offsets.json", cfg_, err)) {
                        status_msg_ = "Config reloaded from disk";
                    } else {
                        status_msg_ = "Reload failed: " + err;
                    }
                    status_time_ = std::chrono::steady_clock::now();
                };
                active_items_.push_back(it);
            }
            {
                RuntimeItem it;
                it.card_col = 1;
                it.type = ItemType::Button;
                it.label = "[ QUIT DAEMON ]";
                it.is_danger = true;
                it.on_click = []() {
                    ::kill(::getpid(), SIGTERM);
                };
                active_items_.push_back(it);
            }
        }
    }
}

int Menu::current_item_count() const {
    return static_cast<int>(active_items_.size());
}

void Menu::select_next_item() {
    const int count = current_item_count();
    if (count > 0) {
        selected_item_ = (selected_item_ + 1) % count;
    }
}

void Menu::select_prev_item() {
    const int count = current_item_count();
    if (count > 0) {
        selected_item_ = (selected_item_ - 1 + count) % count;
    }
}

void Menu::item_activate() {
    build_items_for_current_view();
    if (selected_item_ < 0 || selected_item_ >= current_item_count()) return;

    auto& it = active_items_[selected_item_];
    if (it.on_toggle) it.on_toggle();
    else if (it.on_click) it.on_click();
    else if (it.on_cycle) it.on_cycle(1);
}

void Menu::item_left() {
    build_items_for_current_view();
    if (selected_item_ < 0 || selected_item_ >= current_item_count()) return;

    auto& it = active_items_[selected_item_];
    if (it.on_cycle) it.on_cycle(-1);
    else if (it.on_toggle) it.on_toggle();
}

void Menu::item_right() {
    build_items_for_current_view();
    if (selected_item_ < 0 || selected_item_ >= current_item_count()) return;

    auto& it = active_items_[selected_item_];
    if (it.on_cycle) it.on_cycle(1);
    else if (it.on_toggle) it.on_toggle();
}

bool Menu::handle_key(KeySym sym) {
    if (!open_) return false;

    if (sym == XK_Tab) {
        current_cat_ = static_cast<MenuCategory>((static_cast<int>(current_cat_) + 1) % static_cast<int>(MenuCategory::Count));
        current_subtab_ = 0;
        selected_item_ = 0;
        return true;
    }

    if (sym == XK_1 || sym == XK_KP_1) { current_cat_ = MenuCategory::Main; current_subtab_ = 0; selected_item_ = 0; return true; }
    if (sym == XK_2 || sym == XK_KP_2) { current_cat_ = MenuCategory::Visuals; current_subtab_ = 0; selected_item_ = 0; return true; }
    if (sym == XK_3 || sym == XK_KP_3) { current_cat_ = MenuCategory::Aimbot; current_subtab_ = 0; selected_item_ = 0; return true; }
    if (sym == XK_4 || sym == XK_KP_4) { current_cat_ = MenuCategory::Triggerbot; current_subtab_ = 0; selected_item_ = 0; return true; }
    if (sym == XK_5 || sym == XK_KP_5) { current_cat_ = MenuCategory::Settings; current_subtab_ = 0; selected_item_ = 0; return true; }

    if (sym == XK_Up)    { select_prev_item(); return true; }
    if (sym == XK_Down)  { select_next_item(); return true; }
    if (sym == XK_Left)  { item_left(); return true; }
    if (sym == XK_Right) { item_right(); return true; }
    if (sym == XK_Return || sym == XK_KP_Enter || sym == XK_space) {
        item_activate();
        return true;
    }

    return false;
}

void Menu::handle_mouse_move(int x, int y) {
    mouse_x_ = x;
    mouse_y_ = y;

    if (mouse_down_ && dragging_slider_idx_ >= 0 && dragging_slider_idx_ < static_cast<int>(active_items_.size())) {
        auto& it = active_items_[dragging_slider_idx_];
        if (it.on_slide && it.bw > 20) {
            float norm = std::clamp(static_cast<float>(x - (it.bx + 10)) / static_cast<float>(it.bw - 20), 0.0f, 1.0f);
            it.on_slide(norm);
        }
    }
}

void Menu::handle_mouse_up(int /*button*/, int /*x*/, int /*y*/) {
    mouse_down_ = false;
    dragging_slider_idx_ = -1;
}

bool Menu::handle_click(int button, int x, int y) {
    if (!open_) return false;
    if (button != 1) return false; // Left mouse button

    mouse_down_ = true;
    mouse_x_ = x;
    mouse_y_ = y;

    int rx = x - win_x_;
    int ry = y - win_y_;

    if (rx < 0 || rx > win_w_ || ry < 0 || ry > win_h_) {
        return false;
    }

    // 1) Click in left sidebar (x: 0..160)
    if (rx >= 0 && rx < 160) {
        const int item_h = 44;
        const int start_y = 65;
        if (ry >= start_y) {
            int clicked_idx = (ry - start_y) / item_h;
            if (clicked_idx >= 0 && clicked_idx < static_cast<int>(MenuCategory::Count)) {
                current_cat_ = static_cast<MenuCategory>(clicked_idx);
                current_subtab_ = 0;
                selected_item_ = 0;
                return true;
            }
        }
        return true;
    }

    // 2) Click in top subtabs area (x: 160..win_w_, y: 0..46)
    if (rx >= 160 && ry >= 0 && ry < 46) {
        int cat_idx = static_cast<int>(current_cat_);
        if (cat_idx >= 0 && cat_idx < static_cast<int>(categories_.size())) {
            const auto& cat = categories_[cat_idx];
            int subtab_w = 120;
            int clicked_sub = (rx - 170) / subtab_w;
            if (clicked_sub >= 0 && clicked_sub < static_cast<int>(cat.subtabs.size())) {
                current_subtab_ = clicked_sub;
                selected_item_ = 0;
                return true;
            }
        }
        return true;
    }

    // 3) Click on items in content cards
    for (size_t i = 0; i < active_items_.size(); ++i) {
        auto& it = active_items_[i];
        if (x >= it.bx && x < it.bx + it.bw && y >= it.by && y < it.by + it.bh) {
            selected_item_ = static_cast<int>(i);

            if (it.type == ItemType::Checkbox) {
                if (it.on_toggle) it.on_toggle();
            } else if (it.type == ItemType::SliderFloat || it.type == ItemType::SliderInt) {
                dragging_slider_idx_ = static_cast<int>(i);
                if (it.on_slide && it.bw > 20) {
                    float norm = std::clamp(static_cast<float>(x - (it.bx + 10)) / static_cast<float>(it.bw - 20), 0.0f, 1.0f);
                    it.on_slide(norm);
                }
            } else if (it.type == ItemType::SelectorStr || it.type == ItemType::SelectorInt) {
                int mid_x = it.bx + it.bw / 2;
                if (it.on_cycle) {
                    it.on_cycle((x > mid_x) ? 1 : -1);
                }
            } else if (it.type == ItemType::Button) {
                if (it.on_click) it.on_click();
            }
            return true;
        }
    }

    return true;
}

void Menu::init_colors(Display* dpy, Visual* visual, Colormap cmap) {
    if (colors_init_) return;

    auto alloc = [&](uint16_t r, uint16_t g, uint16_t b, XftColor& out) {
        XRenderColor rc;
        rc.red = r; rc.green = g; rc.blue = b; rc.alpha = 0xFFFF;
        XftColorAllocValue(dpy, visual, cmap, &rc, &out);
    };

    alloc(0xFFFF, 0xFFFF, 0xFFFF, col_white_);
    alloc(0x9E9E, 0x9E9E, 0xA8A8, col_gray_);
    alloc(0x5A5A, 0x5A5A, 0x6666, col_dim_);
    alloc(0xC4C4, 0x2626, 0x2626, col_red_);
    alloc(0xE5E5, 0x3939, 0x3535, col_crimson_);
    alloc(0x2222, 0xC5C5, 0x5E5E, col_green_);
    alloc(0xFBFB, 0xB4B4, 0x1A1A, col_yellow_);

    colors_init_ = true;
}

void Menu::draw(Display* dpy, Window win, GC gc, XftDraw* xft_draw, XftFont* font,
                Visual* visual, Colormap cmap, int screen_w, int screen_h) {
    if (!open_ || !gc || !xft_draw || !font) return;

    init_colors(dpy, visual, cmap);

    rect(screen_w, screen_h);

    UIDrawList draw(dpy, win, gc, xft_draw, font, visual, cmap);
    draw.col_white   = &col_white_;
    draw.col_gray    = &col_gray_;
    draw.col_dim     = &col_dim_;
    draw.col_red     = &col_red_;
    draw.col_crimson = &col_crimson_;
    draw.col_green   = &col_green_;
    draw.col_yellow  = &col_yellow_;

    const int x = win_x_;
    const int y = win_y_;
    const int w = win_w_;
    const int h = win_h_;

    // 1) Main Window Background (deep charcoal #141417)
    draw.add_rect_filled(x, y, w, h, draw.style().Colors[UICol_WindowBg]);
    // Outer border (subtle #2C2C38)
    draw.add_rect(x, y, w, h, draw.style().Colors[UICol_Border], 1);

    // 2) Left Sidebar (width: 160 px, full height)
    draw.add_rect_filled(x, y, 160, h, draw.style().Colors[UICol_SidebarBg]);
    // Sidebar right divider line
    draw.add_line(x + 159, y, x + 159, y + h, draw.style().Colors[UICol_Border], 1);

    // Sidebar header: Umbrella branding
    draw.draw_sidebar_header(x, y, 160, 56, "UMBRELLA", "CS2 EXTERNAL");

    // Sidebar Category Items
    const int cat_start_y = y + 65;
    const int cat_item_h = 44;
    for (size_t i = 0; i < categories_.size(); ++i) {
        int cy = cat_start_y + static_cast<int>(i) * cat_item_h;
        bool is_active = (static_cast<int>(current_cat_) == static_cast<int>(i));
        bool is_hover = (mouse_x_ >= x && mouse_x_ < x + 160 &&
                         mouse_y_ >= cy && mouse_y_ < cy + cat_item_h);

        draw.draw_sidebar_item(x, cy, 159, cat_item_h,
                               categories_[i].icon, categories_[i].name,
                               is_active, is_hover);
    }

    // Sidebar bottom version indicator
    draw.add_text(x + 18, y + h - 14, draw.col_dim, "v0.1.1  |  Linux Native");

    // 3) Top Subtabs Bar (inside selected category)
    const int content_x = x + 160;
    const int topbar_h = 44;
    draw.add_rect_filled(content_x, y, w - 160, topbar_h, draw.style().Colors[UICol_SubtabBg]);
    draw.add_line(content_x, y + topbar_h - 1, x + w, y + topbar_h - 1, draw.style().Colors[UICol_Border], 1);

    int cat_idx = static_cast<int>(current_cat_);
    if (cat_idx >= 0 && cat_idx < static_cast<int>(categories_.size())) {
        const auto& cat = categories_[cat_idx];
        const int tab_w = 120;
        for (size_t i = 0; i < cat.subtabs.size(); ++i) {
            int tx = content_x + 10 + static_cast<int>(i) * tab_w;
            bool is_active = (current_subtab_ == static_cast<int>(i));
            bool is_hover = (mouse_x_ >= tx && mouse_x_ < tx + tab_w &&
                             mouse_y_ >= y && mouse_y_ < y + topbar_h);

            draw.draw_top_tab(tx, y, tab_w, topbar_h, cat.subtabs[i].name, is_active, is_hover);
        }
    }

    // 4) Cards / GroupBoxes Layout
    const int card_top = y + topbar_h + 10;
    const int card_h = h - topbar_h - 44; // leave 34px for bottom footer
    const int card_w = 260;
    const int card0_x = content_x + 12;
    const int card1_x = content_x + 12 + card_w + 12;

    std::string card0_title = "CONFIGURATION";
    std::string card1_title = "OPTIONS";

    if (current_cat_ == MenuCategory::Main) {
        card0_title = "CHEAT MASTER SWITCHES";
        card1_title = "SYSTEM & ACTIONS";
    } else if (current_cat_ == MenuCategory::Visuals) {
        if (current_subtab_ == 0) {
            card0_title = "PLAYER VISUALS";
            card1_title = "BOX & BAR STYLES";
        } else {
            card0_title = "TARGET FILTERS";
            card1_title = "DISTANCE PRESETS";
        }
    } else if (current_cat_ == MenuCategory::Aimbot) {
        if (current_subtab_ == 0) {
            card0_title = "AIMBOT ACTIVATION";
            card1_title = "TARGETING MODE";
        } else {
            card0_title = "ANGLES & SMOOTHING";
            card1_title = "AIMBOT PRESETS";
        }
    } else if (current_cat_ == MenuCategory::Triggerbot) {
        if (current_subtab_ == 0) {
            card0_title = "TRIGGERBOT MASTER";
            card1_title = "STATUS & TRIGGER";
        } else {
            card0_title = "TIMINGS & DELAYS (MS)";
            card1_title = "DELAY PRESETS";
        }
    } else if (current_cat_ == MenuCategory::Settings) {
        if (current_subtab_ == 0) {
            card0_title = "ACTIVE HOTKEYS";
            card1_title = "STORAGE & FPS";
        } else {
            card0_title = "FILE PATHS & IPC";
            card1_title = "DAEMON MANAGEMENT";
        }
    }

    draw.draw_group_box(card0_x, card_top, card_w, card_h, card0_title);
    draw.draw_group_box(card1_x, card_top, card_w, card_h, card1_title);

    // 5) Build and Render Items inside Cards
    build_items_for_current_view();

    int next_y[2] = {card_top + 38, card_top + 38};

    for (size_t i = 0; i < active_items_.size(); ++i) {
        auto& it = active_items_[i];
        int col = it.card_col;
        int cx = (col == 0) ? card0_x : card1_x;
        int cw = card_w;

        int row_h = 28;
        if (it.type == ItemType::SliderFloat || it.type == ItemType::SliderInt) {
            row_h = 36;
        }

        it.bx = cx + 6;
        it.by = next_y[col];
        it.bw = cw - 12;
        it.bh = row_h;

        bool is_sel = (static_cast<int>(i) == selected_item_);
        bool is_hov = (mouse_x_ >= it.bx && mouse_x_ < it.bx + it.bw &&
                       mouse_y_ >= it.by && mouse_y_ < it.by + it.bh);

        if (it.type == ItemType::Checkbox) {
            draw.draw_custom_checkbox(it.bx, it.by, it.bw, it.bh, it.label, it.checked, is_hov, is_sel);
        } else if (it.type == ItemType::SliderFloat || it.type == ItemType::SliderInt) {
            draw.draw_custom_slider(it.bx, it.by, it.bw, it.bh, it.label, it.val_str, it.progress, is_hov, is_sel);
        } else if (it.type == ItemType::SelectorStr || it.type == ItemType::SelectorInt) {
            draw.draw_custom_selector(it.bx, it.by, it.bw, it.bh, it.label, it.val_str, is_hov, is_sel);
        } else if (it.type == ItemType::Button) {
            draw.draw_custom_button(it.bx, it.by, it.bw, it.bh, it.label, is_hov, is_sel, it.is_danger);
        } else if (it.type == ItemType::Keybind) {
            draw.draw_custom_keybind(it.bx, it.by, it.bw, it.bh, it.label, it.val_str, is_hov, is_sel);
        }

        next_y[col] += row_h + 4;
    }

    // 6) Bottom Footer Bar
    const int footer_y = y + h - 30;
    draw.add_rect_filled(content_x, footer_y, w - 160, 30, argb(0xFF, 0x12, 0x12, 0x15));
    draw.add_line(content_x, footer_y, x + w, footer_y, draw.style().Colors[UICol_Border], 1);

    const auto now = std::chrono::steady_clock::now();
    if (!status_msg_.empty() && std::chrono::duration_cast<std::chrono::seconds>(now - status_time_).count() < 4) {
        draw.add_text(content_x + 14, footer_y + 19, draw.col_green, "[✓] " + status_msg_);
    } else {
        const char* hint = "NAV: [Arrows] Navigate / Adjust   [Enter/Click] Select   [INS/ESC] Close";
        draw.add_text(content_x + 14, footer_y + 19, draw.col_dim, hint);
    }
}

} // namespace cs2esp
