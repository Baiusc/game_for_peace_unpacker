mod model;
mod offset;
mod util;

use anyhow::Result;
use std::{
    sync::{Arc, RwLock},
    thread,
    time::{Duration, Instant},
};
use windows::{
    Win32::{
        Foundation::*,
        System::{LibraryLoader::*, SystemServices::*},
        UI::WindowsAndMessaging::*,
    },
    core::*,
};

/// 覆盖层与数据采集循环使用的目标刷新率。
const FRAME_RATE: u64 = 60;
const TICK_RATE: Duration = Duration::from_millis(1000 / FRAME_RATE);

/// 初始化窗口信息、覆盖层线程和实体读取循环。
fn run() -> Result<()> {
    // 覆盖层线程与读取线程共享的矩形列表；读写锁确保每帧替换时不会撕裂数据。
    let draw_rect_list = Arc::new(RwLock::new(Vec::<RECT>::with_capacity(32)));

    // 通过窗口标题取得目标窗口句柄，并读取其客户区范围。
    let game_window = unsafe { FindWindowA(None, s!("AssaultCube")) }?;

    let mut window_info = WINDOWINFO::default();
    unsafe { GetWindowInfo(game_window, &mut window_info) }?;

    // 覆盖层在独立线程运行，持续消费最新的屏幕矩形列表。
    let draw_rect_list_clone = Arc::clone(&draw_rect_list);
    thread::spawn(move || {
        let mut overlay = windows_ez_overlay::Overlay::new(
            window_info.rcClient.left,
            window_info.rcClient.top,
            window_info.rcClient.right,
            window_info.rcClient.bottom,
            draw_rect_list_clone,
            true,
        )
        .unwrap();
        let _ = overlay.run();
    });

    let window_width = window_info.rcClient.right - window_info.rcClient.left;
    let window_height = window_info.rcClient.bottom - window_info.rcClient.top;

    // 当前实现面向 32 位目标进程，因此模块基址以 u32 保存。
    let module_base_addr = unsafe { GetModuleHandleA(s!("ac_client.exe")).map(|h| h.0 as u32) }?;

    // 实体列表本身存放的是指针，其内容需要先从模块相对位置读取一次。
    let entity_list_base_addr = util::read_memory::<u32>(module_base_addr, offset::ENTITY_LIST);

    read_game_data_loop(
        module_base_addr,
        entity_list_base_addr,
        window_width,
        window_height,
        draw_rect_list,
    );

    Ok(())
}

/// 按固定节奏读取可见实体，投影其头脚位置，并发布对应的屏幕矩形。
fn read_game_data_loop(
    module_base_addr: u32,
    entity_list_base_addr: u32,
    window_width: i32,
    window_height: i32,
    draw_rect_list: Arc<RwLock<Vec<RECT>>>,
) {
    let mut last_tick = Instant::now();
    loop {
        // 每帧读取数量与视图矩阵；二者均相对于目标模块基址。
        let player_count = util::read_memory::<u32>(module_base_addr, offset::PLAYER_COUNT);
        let view_matrix = util::read_memory::<[f32; 16]>(module_base_addr, offset::VIEW_MATRIX);

        let new_draw_rect_list = (1..player_count)
            .filter_map(|i| {
                // 实体数组按 4 字节指针步长排列。
                let entity = model::Entity {
                    base_addr: util::read_memory::<u32>(entity_list_base_addr, i * 0x4),
                };

                // 跳过失效实体以及不能投影到当前窗口的实体。
                if entity.health() <= 0 {
                    return None;
                }

                let head_screen_position = util::world_to_screen(
                    entity.head_position(),
                    view_matrix,
                    window_width,
                    window_height,
                )?;

                let feet_screen_position = util::world_to_screen(
                    entity.feet_position(),
                    view_matrix,
                    window_width,
                    window_height,
                )?;

                // 用头脚投影的高度估算矩形，并以 1:2 的宽高比绘制。
                let height = (feet_screen_position.y - head_screen_position.y) as i32;
                let width = height / 2;
                let left = head_screen_position.x as i32 - width / 2;
                let top = head_screen_position.y as i32;

                Some(RECT {
                    left,
                    right: left + width,
                    top,
                    bottom: top + height,
                })
            })
            .collect::<Vec<RECT>>();

        // 一次性替换共享列表，使覆盖层只看到完整帧的结果。
        if let Ok(mut draw_rect_list) = draw_rect_list.write() {
            draw_rect_list.clear();
            draw_rect_list.extend(new_draw_rect_list);
        }

        // 若本帧耗时超过目标周期，saturating_sub 会立即进入下一帧。
        let timeout = TICK_RATE.saturating_sub(last_tick.elapsed());
        thread::sleep(timeout);
        last_tick = Instant::now();
    }
}

/// DLL 装载入口：仅在进程附加时启动工作线程，避免阻塞加载器回调。
#[unsafe(no_mangle)]
extern "system" fn DllMain(_dll_module: HINSTANCE, call_reason: u32, _reserved: *mut ()) -> bool {
    if call_reason == DLL_PROCESS_ATTACH {
        thread::spawn(move || {
            let _ = run();
        });
    }
    true
}
