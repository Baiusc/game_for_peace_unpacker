// AssaultCube 1.3.0.2 的模块相对偏移量。
// 这些数值依赖特定版本；版本变化后应重新核对，而不是直接沿用。
pub const PLAYER_COUNT: u32 = 0x18AC0C;
// 视图矩阵用于把三维世界坐标投影到窗口坐标。
pub const VIEW_MATRIX: u32 = 0x17DFD0;
// 实体指针数组的起始地址。
pub const ENTITY_LIST: u32 = 0x18AC04;

// 以下字段相对于单个实体结构体的基址。
pub const ENTITY_HEAD_POSITION: u32 = 0x4;
pub const ENTITY_FEET_POSITION: u32 = 0x28;
pub const ENTITY_HEALTH: u32 = 0xEC;
