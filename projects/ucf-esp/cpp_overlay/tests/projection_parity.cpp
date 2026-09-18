#include "projection.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace {
bool close_enough(float a, float b) { return std::fabs(a - b) <= 1e-4f; }

int run_stream(std::istream& in) {
    int width = 0;
    int height = 0;
    int count = 0;
    std::array<float, 16> matrix{};
    if (!(in >> width >> height)) return 2;
    for (float& value : matrix) if (!(in >> value)) return 2;
    if (!(in >> count) || count < 0) return 2;
    for (int i = 0; i < count; ++i) {
        ucf::Vec3 point{};
        float expected_x = 0.0f;
        float expected_y = 0.0f;
        if (!(in >> point.x >> point.y >> point.z >> expected_x >> expected_y)) return 2;
        const auto actual = ucf::world_to_screen(point, matrix, width, height);
        if (!actual || !close_enough(actual->x, expected_x) ||
            !close_enough(actual->y, expected_y)) {
            std::fprintf(stderr, "parity mismatch at %d: actual=(%.9g,%.9g) expected=(%.9g,%.9g)\n",
                         i, actual ? actual->x : 0.0f, actual ? actual->y : 0.0f,
                         expected_x, expected_y);
            return 1;
        }
    }
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc == 2) {
        std::ifstream input(argv[1]);
        return input ? run_stream(input) : 2;
    }
    // ctest 无参数自检：identity VP 下验证屏幕中心与 y 翻转。
    std::array<float, 16> identity{};
    identity[0] = identity[5] = identity[10] = identity[15] = 1.0f;
    const auto center = ucf::world_to_screen({0.0f, 0.0f, 0.0f}, identity, 1280, 720);
    const auto top = ucf::world_to_screen({0.0f, 1.0f, 0.0f}, identity, 1280, 720);
    if (!center || !top || !close_enough(center->x, 640.0f) ||
        !close_enough(center->y, 360.0f) || !close_enough(top->y, 0.0f)) return 1;
    std::puts("projection parity self-test: PASS");
    return 0;
}
