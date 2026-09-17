#include <filesystem>

#include "ac_re/app.hpp"

int main() {
    const auto config = ac_re::load_config("config.toml");
    const ac_re::FixtureEntitySource entity_source;
    return ac_re::Application(config, entity_source).run_once();
}
