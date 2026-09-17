#include <cassert>
#include <filesystem>
#include <iostream>

#include "ac_re/config.hpp"
#include "ac_re/entity_source.hpp"
#include "ac_re/projection.hpp"

int main() {
    const ac_re::FixtureEntitySource source;
    const auto frame = source.read_frame();
    assert(frame.entities.size() == 3U);

    const auto rectangle = ac_re::project_entity(frame.entities.front(), frame.view_matrix, 1280, 720);
    assert(rectangle.has_value());
    assert(rectangle->bottom > rectangle->top);
    assert(!ac_re::project_entity(frame.entities.back(), frame.view_matrix, 1280, 720).has_value());

    const auto temp_config = std::filesystem::temp_directory_path() / "ac_re_default_config.toml";
    std::filesystem::remove(temp_config);
    const auto config = ac_re::load_config(temp_config);
    assert(std::filesystem::exists(temp_config));
    assert(config.window_width == 1280);
    std::filesystem::remove(temp_config);

    std::cout << "ac_re_tests: projection, fixture source, default config: PASS\n";
}
