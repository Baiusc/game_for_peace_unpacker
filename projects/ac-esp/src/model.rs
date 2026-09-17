use crate::{offset, util};

/// 投影到窗口平面后的二维坐标。
pub struct Vec2 {
    pub x: f32,
    pub y: f32,
}

/// 游戏世界中的三维坐标。
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// 实体结构在当前进程地址空间中的基址。
pub struct Entity {
    pub base_addr: u32,
}

impl Entity {
    /// 读取实体生命值；非正值实体不会参与后续投影。
    pub fn health(&self) -> i32 {
        util::read_memory::<i32>(self.base_addr, offset::ENTITY_HEALTH)
    }

    /// 从实体结构读取头部三维坐标。
    pub fn head_position(&self) -> Vec3 {
        let xyz = util::read_memory::<[f32; 3]>(self.base_addr, offset::ENTITY_HEAD_POSITION);
        Vec3 {
            x: xyz[0],
            y: xyz[1],
            z: xyz[2],
        }
    }

    /// 从实体结构读取脚部三维坐标。
    pub fn feet_position(&self) -> Vec3 {
        let xyz = util::read_memory::<[f32; 3]>(self.base_addr, offset::ENTITY_FEET_POSITION);
        Vec3 {
            x: xyz[0],
            y: xyz[1],
            z: xyz[2],
        }
    }
}
