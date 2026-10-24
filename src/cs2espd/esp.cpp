#include "esp.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace cs2esp {
namespace {

// "#RRGGBBAA" → 0xAARRGGBB (для XSetForeground); при кривом формате — fallback 0xFFFFFFFF
unsigned parse_argb(const std::string& s) {
    if (s.size() != 9 || s[0] != '#') return 0xFFFFFFFFu;
    auto hex4 = [](const char* p) -> unsigned {
        char buf[5] = {p[0], p[1], p[2], p[3], '\0'};
        return static_cast<unsigned>(std::strtoul(buf, nullptr, 16));
    };
    const unsigned rgb = hex4(s.c_str() + 1);
    const unsigned a = hex4(s.c_str() + 7);
    return (a << 24) | rgb;
}

// Мировая точка → экран. CS2 хранит VP row-major: clip = M · [pos, 1].
// clip.x = row[0] · [v, 1] и т.д.
bool world_to_screen(const Vec3& v, const Matrix4& m, float w, float h, Vec2& out) {
    const float cx = v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2] + m.m[0][3];
    const float cy = v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2] + m.m[1][3];
    const float cw = v.x * m.m[3][0] + v.y * m.m[3][1] + v.z * m.m[3][2] + m.m[3][3];
    if (cw <= 0.001f) return false; // за камерой
    out.x = (cx / cw * 0.5f + 0.5f) * w;
    out.y = (0.5f - cy / cw * 0.5f) * h; // инверсия Y: экранная ось вниз
    return true;
}

} // namespace

Esp::Esp(const Config& cfg)
    : cfg_(cfg),
      color_enemy_(parse_argb(cfg.color_enemy)),
      color_teammate_(parse_argb(cfg.color_teammate)) {}

void Esp::to_boxes(const Snapshot& snap, int screen_w, int screen_h,
                   std::vector<ScreenBox>& out) {
    out.clear();
    if (!snap.matrix_ok || screen_w <= 0 || screen_h <= 0) return;

    for (const PlayerSnap& p : snap.players) {
        Vec2 feet, head;
        const Vec3 head_world = {p.origin.x, p.origin.y, p.origin.z + cfg_.esp.box_height_units};
        if (!world_to_screen(p.origin, snap.matrix, static_cast<float>(screen_w),
                             static_cast<float>(screen_h), feet)) {
            continue;
        }
        if (!world_to_screen(head_world, snap.matrix, static_cast<float>(screen_w),
                             static_cast<float>(screen_h), head)) {
            continue;
        }

        const float h_px = std::fabs(feet.y - head.y);
        if (h_px < 4.f) continue; // слишком далеко — бесполезный точечный маркер

        const float w_px = h_px * cfg_.esp.box_width_ratio;
        ScreenBox b;
        b.x = std::min(feet.x, head.x) - w_px * 0.5f;
        b.y = std::min(feet.y, head.y);
        b.w = w_px;
        b.h = h_px;

        b.health = p.health;
        if (!snap.have_local || p.team != snap.local_team) {
            b.color = color_enemy_;
        } else {
            // team_check уже отфильтровал своих, сюда попадаем только при team_check=false
            b.color = color_teammate_;
        }

        if (cfg_.esp.show_distance && snap.have_local) {
            const float dx = snap.local_origin.x - p.origin.x;
            const float dy = snap.local_origin.y - p.origin.y;
            const float dz = snap.local_origin.z - p.origin.z;
            b.dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        if (cfg_.esp.show_names) {
            b.name = p.name;
        }

        if (cfg_.esp.show_skeleton && p.has_bones && p.bones.size() >= 23) {
            static const std::pair<int, int> kBonePairs[] = {
                // Spine & Head
                {6, 7},   // Neck -> Head
                {5, 6},   // Chest -> Neck
                {4, 5},   // Spine -> Chest
                {2, 4},   // Pelvis -> Spine
                // Left Arm
                {5, 8},   // Chest -> L_Shoulder
                {8, 9},   // L_Shoulder -> L_Arm
                {9, 10},  // L_Arm -> L_Elbow
                {10, 11}, // L_Elbow -> L_Hand
                // Right Arm
                {5, 12},  // Chest -> R_Shoulder
                {12, 13}, // R_Shoulder -> R_Arm
                {13, 14}, // R_Arm -> R_Elbow
                {14, 15}, // R_Elbow -> R_Hand
                // Left Leg
                {2, 17},  // Pelvis -> L_Thigh
                {17, 18}, // L_Thigh -> L_Knee
                {18, 19}, // L_Knee -> L_Foot
                // Right Leg
                {2, 20},  // Pelvis -> R_Thigh
                {20, 21}, // R_Thigh -> R_Knee
                {21, 22}  // R_Knee -> R_Foot
            };

            for (const auto& pair : kBonePairs) {
                Vec2 s1, s2;
                if (world_to_screen(p.bones[pair.first], snap.matrix,
                                    static_cast<float>(screen_w), static_cast<float>(screen_h), s1) &&
                    world_to_screen(p.bones[pair.second], snap.matrix,
                                    static_cast<float>(screen_w), static_cast<float>(screen_h), s2)) {
                    b.skeleton_lines.push_back({s1.x, s1.y, s2.x, s2.y});
                }
            }
        }

        out.push_back(std::move(b));
    }
}

} // namespace cs2esp
