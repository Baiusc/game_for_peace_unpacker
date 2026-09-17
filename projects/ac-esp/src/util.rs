use crate::model;

/// 使用 4×4 视图矩阵将世界坐标转换为窗口像素坐标。
///
/// 当齐次坐标 `w` 过小或位于相机背面时，返回 `None`，调用方应跳过该点。
pub fn world_to_screen(
    world_position: model::Vec3,
    view_matrix: [f32; 16],
    window_width: i32,
    window_height: i32,
) -> Option<model::Vec2> {
    // 先计算齐次分量，避免投影除法在不可见点上产生无意义结果。
    let w = world_position.x * view_matrix[3]
        + world_position.y * view_matrix[7]
        + world_position.z * view_matrix[11]
        + view_matrix[15];

    if w < 0.001 {
        return None;
    }

    // 计算裁剪空间 x/y 分量，再除以 w 得到归一化设备坐标。
    let x = world_position.x * view_matrix[0]
        + world_position.y * view_matrix[4]
        + world_position.z * view_matrix[8]
        + view_matrix[12];
    let y = world_position.x * view_matrix[1]
        + world_position.y * view_matrix[5]
        + world_position.z * view_matrix[9]
        + view_matrix[13];

    let nx = x / w;
    let ny = y / w;

    // 将 [-1, 1] 范围映射到窗口像素；y 轴同时从向上改为向下。
    let window_center_x = (window_width / 2) as f32;
    let window_center_y = (window_height / 2) as f32;

    let screen_position = model::Vec2 {
        x: window_center_x + (window_center_x * nx),
        y: window_center_y - (window_center_y * ny),
    };

    Some(screen_position)
}

/// 以“模块/对象基址 + 相对偏移”的方式读取当前进程中的 Copy 类型数据。
///
/// 调用方必须确保地址、偏移和 `T` 的布局与目标进程版本相符。
pub fn read_memory<T>(base_addr: u32, offset: u32) -> T
where
    T: Copy,
{
    let data_ptr = (base_addr + offset) as *const T;
    unsafe { *data_ptr }
}
