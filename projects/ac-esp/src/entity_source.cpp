#include "ac_re/entity_source.hpp"

namespace ac_re {

FrameSnapshot FixtureEntitySource::read_frame() const {
    // 单位矩阵用于构造可预测的投影测试场景。
    FrameSnapshot snapshot;
    snapshot.view_matrix = {
        1.F, 0.F, 0.F, 0.F,
        0.F, 1.F, 0.F, 0.F,
        0.F, 0.F, 1.F, 0.F,
        0.F, 0.F, 0.F, 1.F,
    };
    snapshot.entities = {
        {1, "fixture_alpha", 100, {-0.35F, 0.55F, 0.F}, {-0.35F, -0.35F, 0.F}},
        {2, "fixture_bravo", 75, {0.40F, 0.40F, 0.F}, {0.40F, -0.45F, 0.F}},
        {3, "fixture_inactive", 0, {0.F, 0.F, 0.F}, {0.F, -0.5F, 0.F}},
    };
    return snapshot;
}

}  // namespace ac_re
