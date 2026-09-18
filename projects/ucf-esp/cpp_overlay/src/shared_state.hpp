#pragma once
#include <cstdint>
#include <atomic>
#include <cstddef>

// 与 frida_host.py 的 frame 契约对齐：
//   w2c / proj 都是列主序扁平 16（m[col*4 + row]），与 src/projection.cpp 一致；
//   players: pos[3], team, hp, maxHp, isDead, bones[19]
// 这是 C++ 宿主（D3D11 叠加层）与 Python 宿主（tkinter）共用的数据形状。
namespace ucf {

constexpr int MAX_PLAYERS = 64;
constexpr int MAX_BONES = 19;

// 固定 Humanoid 槽位：顺序与 frida_dump.js 的 BONE_IDS 一致。
// valid=false 表示该模型不是 Humanoid、该骨骼不存在或本帧读取失败。
struct BoneState {
    float pos[3] = {0, 0, 0};
    bool  valid = false;
    uint8_t pad[3] = {};
};
static_assert(sizeof(BoneState) == 16, "BoneState ABI must stay 16 bytes");

struct PlayerState {
    float pos[3] = {0, 0, 0};
    int   team   = 0;
    int   hp     = 100;
    int   maxHp  = 100;
    bool  isDead = false;
    char  name[32] = {};
    BoneState bones[MAX_BONES]{};
};
static_assert(sizeof(PlayerState) == 60 + MAX_BONES * sizeof(BoneState),
              "PlayerState ABI changed; update Python shared-memory packing");

struct Frame {
    float       w2c[16] = {};
    float       proj[16] = {};
    int         width  = 1280;
    int         height = 720;
    bool        inGame = false;
    PlayerState local{};
    PlayerState players[MAX_PLAYERS]{};
    int         playerCount = 0;
};

// 双缓冲 + 序号交换：写者写到后台槽，整帧拷贝完成后翻转 cur；读者读 cur 槽。
// 因为每次都是“整帧拷贝后再可见”，天然不会出现“读到半帧”。
// 单生产者（游戏/Python 宿主）/ 单消费者（C++ 叠加层）即可。
struct FrameSlots {
    std::atomic<int> cur{0};   // 当前可读槽（0 或 1）
    Frame            slots[2]{};
};

// 进程内双缓冲传输（测试、合成数据源用，不跨进程）。
class LocalTransport {
public:
    void write(const Frame& f) {
        int back = 1 - slots_.cur.load(std::memory_order_relaxed);
        slots_.slots[back] = f;                                   // 整帧拷贝
        slots_.cur.store(back, std::memory_order_release);        // 翻转：后台槽变可读
    }
    void read(Frame& out) const {
        out = slots_.slots[slots_.cur.load(std::memory_order_acquire)];
    }
private:
    FrameSlots slots_{};
};

// 跨进程共享内存传输（仅 Windows 编译；映射一块 sizeof(FrameSlots) 的内存，
// 内部结构就是上面的 FrameSlots，所以读写语义与 LocalTransport 完全一致）。
#ifdef _WIN32
class SharedTransport {
public:
    explicit SharedTransport(const char* name);
    ~SharedTransport();
    SharedTransport(const SharedTransport&) = delete;
    SharedTransport& operator=(const SharedTransport&) = delete;

    bool ok() const { return mapped_ != nullptr; }
    void write(const Frame& f);
    void read(Frame& out) const;
private:
    struct FrameSlots* mapped_ = nullptr;
    void*   map_ = nullptr;   // HANDLE
};
#endif

} // namespace ucf
