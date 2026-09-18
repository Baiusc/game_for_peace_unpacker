#include "shared_state.hpp"
#include "overlay_viz.hpp"
#include "smooth.hpp"
#include "config.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("  FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } \
} while (0)

static int approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

static void test_draw_list() {
    ucf::Viewport vp{0, 0, 1280, 720, 1, 1};   // 帧坐标 == 屏幕像素
    ucf::ScreenMark marks[2]{};
    marks[0] = {ucf::Kind::Enemy, 400, 300, 50, 100, 12.0f, true, false};
    marks[1] = {ucf::Kind::Teammate, 200, 150, 80, 100, 5.0f, false, false}; // 屏幕外
    ucf::DrawList dl{};
    ucf::build_draw_list(vp, marks, 2, dl);

    CHECK(dl.boxCount == 1);          // 屏幕外那个不画
    CHECK(dl.barCount == 1);
    CHECK(dl.labelCount == 1);
    // 框中心在 (400,300)，BOX 40x60 -> 左上角 (380,270)
    CHECK(approx(dl.boxes[0].x, 380.0f) && approx(dl.boxes[0].y, 270.0f));
    CHECK(approx(dl.boxes[0].w, 40.0f) && approx(dl.boxes[0].h, 60.0f));
    CHECK(std::strstr(dl.labels[0].text, "enemy") != nullptr);
    CHECK(std::strstr(dl.labels[0].text, "50/100") != nullptr);
    CHECK(std::strstr(dl.labels[0].text, "12m") != nullptr);
    // 血条比例 50/100 = 0.5
    CHECK(approx(dl.bars[0].ratio, 0.5f));

    ucf::DrawStyle style{};
    style.show_box = false;
    style.show_skeleton = true;
    style.show_health = false;
    style.show_distance = false;
    ucf::build_draw_list(vp, marks, 1, style, dl);
    CHECK(dl.boxCount == 0);
    CHECK(dl.barCount == 0);
    CHECK(dl.skeletonCount == 1);
}

static void test_smooth() {
    ucf::Angles cur{0, 0};
    ucf::Angles tgt{1.57f, -0.5f};
    for (int i = 0; i < 50; ++i) cur = ucf::smooth_angles(cur, tgt, 0.5f);
    CHECK(approx(cur.yaw, tgt.yaw, 1e-2f));
    CHECK(approx(cur.pitch, tgt.pitch, 1e-2f));
    // 系数 0 不改变
    ucf::Angles a{0, 0};
    ucf::Angles b = ucf::smooth_angles(a, tgt, 0.0f);
    CHECK(approx(b.yaw, 0.0f) && approx(b.pitch, 0.0f));
}

static void test_select_target() {
    ucf::Vec3 fwd{0, 0, 1};                  // 相机看向 +Z
    ucf::Candidate cs[3]{};
    cs[0] = {{0, 0, 5},   5.0f, 100};        // 正前方，最近
    cs[1] = {{1, 0, 0},   3.0f, 20};         // 正右方，与前方成 90°
    cs[2] = {{0, 0, 8},   8.0f, 50};         // 正前方，更远
    // FOV 90 -> 半锥 45°，cs[1] 角 90° 不在锥内
    int r = ucf::select_target(fwd, cs, 3, 90.0f, 0);
    CHECK(r == 0);                            // 最近且在锥内
    // 最低血量模式：cs[1] 被锥裁掉，剩下 0/2，cs[1]? hp 100 vs 50 -> 选 2
    r = ucf::select_target(fwd, cs, 3, 90.0f, 1);
    CHECK(r == 2);
    r = ucf::select_target(fwd, cs, 3, 90.0f, 2);
    CHECK(r == 0);
    const ucf::Angles a = ucf::angles_from_direction({1, 0, 1});
    CHECK(approx(a.yaw, 0.785398f, 1e-3f));
}

static void test_transport_roundtrip() {
    ucf::LocalTransport t;
    ucf::Frame f{};
    f.width = 1280; f.height = 720; f.inGame = true;
    f.playerCount = 3;
    f.players[0].pos[0] = 1.5f; f.players[0].hp = 73; f.players[0].team = 2;
    t.write(f);
    ucf::Frame out{};
    t.read(out);
    CHECK(out.playerCount == 3);
    CHECK(out.width == 1280);
    CHECK(approx(out.players[0].pos[0], 1.5f));
    CHECK(out.players[0].hp == 73);
}

static void test_config_roundtrip() {
    ucf::Settings s{};
    s.esp_enabled = false; s.show_enemy = false; s.fov_deg = 110.0f; s.target_mode = 1;
    s.color_enemy[0] = 0.1f;
    s.show_skeleton = true; s.aimbot_enabled = true; s.aim_max_distance = 42.0f;
    CHECK(ucf::save_settings(s, "settings_test.txt"));
    ucf::Settings r{};
    CHECK(ucf::load_settings(r, "settings_test.txt"));
    CHECK(r.esp_enabled == false);
    CHECK(r.show_enemy == false);
    CHECK(approx(r.fov_deg, 110.0f));
    CHECK(r.target_mode == 1);
    CHECK(approx(r.color_enemy[0], 0.1f));
    CHECK(r.show_skeleton && r.aimbot_enabled && approx(r.aim_max_distance, 42.0f));
    std::remove("settings_test.txt");
}

int main() {
    printf("[overlay_tests]\n");
    test_draw_list();
    test_smooth();
    test_select_target();
    test_transport_roundtrip();
    test_config_roundtrip();
    if (failures == 0) { printf("  all passed\n"); return 0; }
    printf("  %d check(s) failed\n", failures);
    return 1;
}
