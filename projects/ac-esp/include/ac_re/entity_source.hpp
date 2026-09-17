#pragma once

#include "ac_re/model.hpp"

namespace ac_re {

class EntitySource {
public:
    virtual ~EntitySource() = default;
    virtual FrameSnapshot read_frame() const = 0;
};

// 默认数据源只提供内置合成帧，不读取进程、驱动或网络数据。
class FixtureEntitySource final : public EntitySource {
public:
    FrameSnapshot read_frame() const override;
};

}  // namespace ac_re
