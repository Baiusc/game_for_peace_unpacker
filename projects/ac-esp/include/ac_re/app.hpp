#pragma once

#include "ac_re/config.hpp"
#include "ac_re/entity_source.hpp"

namespace ac_re {

class Application {
public:
    Application(AppConfig config, const EntitySource& entity_source);
    int run_once() const;

private:
    AppConfig config_;
    const EntitySource& entity_source_;
};

}  // namespace ac_re
