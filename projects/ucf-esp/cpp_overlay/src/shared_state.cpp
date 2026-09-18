#include "shared_state.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ucf {

SharedTransport::SharedTransport(const char* name) {
    // 命名内存映射：Python 侧用 mmap(tagname=...) 写入，C++ 侧用同样名字打开。
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                  PAGE_READWRITE, 0,
                                  static_cast<DWORD>(sizeof(FrameSlots)),
                                  name);
    if (!h) return;
    map_ = h;
    void* view = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FrameSlots));
    if (!view) {
        CloseHandle(h);
        map_ = nullptr;
        return;
    }
    mapped_ = static_cast<FrameSlots*>(view);
    // 首次打开时把序号初始化为 0（CreateFileMapping 已是零填充，这里只是防御）。
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

SharedTransport::~SharedTransport() {
    if (mapped_) UnmapViewOfFile(mapped_);
    if (map_)    CloseHandle(static_cast<HANDLE>(map_));
}

void SharedTransport::write(const Frame& f) {
    if (!mapped_) return;
    int back = 1 - mapped_->cur.load(std::memory_order_relaxed);
    mapped_->slots[back] = f;
    mapped_->cur.store(back, std::memory_order_release);
}

void SharedTransport::read(Frame& out) const {
    if (!mapped_) return;
    out = mapped_->slots[mapped_->cur.load(std::memory_order_acquire)];
}

} // namespace ucf
#endif
