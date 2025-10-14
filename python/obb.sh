#!/data/data/com.termux/files/usr/bin/bash

# ===============================
# Termux UEXP 三合一工具（最终修复版）
# 修复了所有路径和 command not found 问题
# ===============================

# --- 颜色定义 ---
NOCOLOR='\033[0m'
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
LIGHTPURPLE='\033[1;35m'

# --- 路径定义 ---
BASE_DIR="$HOME/64K解包打包系统"
PAK_DIR="$BASE_DIR/PAK"
UNPACK_DIR="$BASE_DIR/解包"
PACK_DIR="$BASE_DIR/打包"

# --- 共享存储目录 ---
SHARED_PAK_DIR="/storage/emulated/0/Download/UEXP三合一/PAK"
SHARED_UNPACK_DIR="/storage/emulated/0/Download/UEXP三合一/UEXP解包"
SHARED_PACK_DIR="/storage/emulated/0/Download/UEXP三合一/UEXP打包"

# --- 临时目录定义 ---
TEMP_DIR="$HOME/temp_data" 

# --- 初始化目录 ---
echo -e "${CYAN}正在初始化目录...${NOCOLOR}"
mkdir -p "$PAK_DIR" "$UNPACK_DIR" "$PACK_DIR"
mkdir -p "$SHARED_PAK_DIR" "$SHARED_UNPACK_DIR" "$SHARED_PACK_DIR"

# 关键：确保临时目录先创建，才能进行清理和写入操作
mkdir -p "$TEMP_DIR"

# --- 清理临时目录中的数据 ---
echo -e "${CYAN}正在清理临时数据...${NOCOLOR}"
# 使用 find 命令查找并删除临时目录下的所有文件
find "$TEMP_DIR" -type f -delete 2>/dev/null 
echo -e "${GREEN}所有核心目录已创建完成！${NOCOLOR}"
sleep 1

# --- 自动设置工具执行权限 (新增/优化) ---
echo -e "${CYAN}正在设置工具执行权限...${NOCOLOR}"

# 假设这两个 .bms 文件位于 $HOME 目录下（即脚本执行的当前目录）

# 1. 设置 uexp解包.bms 的权限
if [ -f "$HOME/uexp解包.bms" ]; then
    chmod +x "$HOME/uexp解包.bms"
    # 打印单个文件设置成功的消息
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  > uexp解包.bms 权限设置完成。${NOCOLOR}"
    else
        echo -e "${RED}  > 错误：无法设置 uexp解包.bms 权限。${NOCOLOR}"
    fi
else
    echo -e "${YELLOW}  > 警告：uexp解包.bms 文件未找到。${NOCOLOR}"
fi

# 2. 设置 uexp打包.bms 的权限
if [ -f "$HOME/uexp打包.bms" ]; then
    chmod +x "$HOME/uexp打包.bms"
    # 打印单个文件设置成功的消息
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  > uexp打包.bms 权限设置完成。${NOCOLOR}"
    else
        echo -e "${RED}  > 错误：无法设置 uexp打包.bms 权限。${NOCOLOR}"
    fi
else
    echo -e "${YELLOW}  > 警告：uexp打包.bms 文件未找到。${NOCOLOR}"
fi

# 移除最后的总确认信息，保留 clear
sleep 1
clear

# --- 自动导入文件 ---
echo -e "${CYAN}>>> 正在从共享目录自动导入文件到 Termux 内部...${NOCOLOR}"

if [ "$(ls -A "$SHARED_PAK_DIR" 2>/dev/null)" ]; then
    echo -e "${YELLOW}  > 导入 .pak 文件...${NOCOLOR}"
    mv "$SHARED_PAK_DIR"/* "$PAK_DIR"/ 2>/dev/null
fi

if [ "$(ls -A "$SHARED_PACK_DIR" 2>/dev/null)" ]; then
    echo -e "${YELLOW}  > 导入待打包文件...${NOCOLOR}"
    mv "$SHARED_PACK_DIR"/* "$PACK_DIR"/ 2>/dev/null
fi

echo -e "${GREEN}文件导入完成。${NOCOLOR}"
sleep 1
clear


# ================================================================
# 选择 PAK 文件（修复写入路径）
# ================================================================
function select_pak_file {
    local files
    mapfile -t files < <(find "$PAK_DIR" -maxdepth 1 -type f -name "*.pak")

    if [ ${#files[@]} -eq 0 ]; then
        echo -e "${RED}错误：在 $PAK_DIR 中没有找到 .pak 文件。${NOCOLOR}" >&2
        return 1
    fi

    # 优化：清屏，解决输出混乱
    clear
    echo -e "${YELLOW}找到以下 .pak 文件：${NOCOLOR}"
    for i in "${!files[@]}"; do
        local filename
        filename=$(basename "${files[i]}")
        echo "$((i+1)). $filename"
    done
    echo ""

    local choice
    while true; do
        read -rp "请选择要操作的 .pak 文件（输入数字）: " choice
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le ${#files[@]} ]; then
            # *** 修复写入路径：使用 $TEMP_DIR ***
            echo "${files[$((choice - 1))]}" > "$TEMP_DIR/selected_pak_path.txt"
            echo "$(basename "${files[$((choice - 1))]}")" > "$TEMP_DIR/selected_pak_name.txt"
            return 0
        else
            echo -e "${RED}无效输入，请重新输入。${NOCOLOR}"
        fi
    done
}

# ================================================================
# 解包函数
# ================================================================
function jb_simplified {
    local TOOL_CMD="./uexp解包.bms" 

    select_pak_file || return 1

    local pak_full_path
    local pak_file
    
    # 临时路径使用 $TEMP_DIR 
    pak_full_path=$(cat "$TEMP_DIR/selected_pak_path.txt")
    pak_file=$(cat "$TEMP_DIR/selected_pak_name.txt")

    echo -e "${YELLOW}当前PAK文件路径：$pak_full_path${NOCOLOR}"
    echo -e "${LIGHTPURPLE}开始解包文件：$pak_file${NOCOLOR}"

    if ! "$TOOL_CMD" -a "$pak_full_path" "$UNPACK_DIR"; then
        echo -e "${RED}解包失败，请检查 $TOOL_CMD 是否存在或有执行权限。${NOCOLOR}"
        return 1
    fi

    echo -e "${LIGHTPURPLE}解包结束: $pak_file -> $UNPACK_DIR${NOCOLOR}"

    # --- 自动导出解包结果 ---
    echo -e "${CYAN}>>> 正在自动导出解包文件到共享目录...${NOCOLOR}"
    rm -rf "$SHARED_UNPACK_DIR"/* 2>/dev/null
    mv "$UNPACK_DIR"/* "$SHARED_UNPACK_DIR"/ 2>/dev/null
    echo -e "${GREEN}文件已成功导出到 $SHARED_UNPACK_DIR${NOCOLOR}"
    sleep 1
}

# ================================================================
# 打包函数
# ================================================================
function db_simplified {
    local TOOL_CMD="./uexp打包.bms" 

    select_pak_file || return 1

    local pak_full_path
    local pak_file
    
    # *** 修复读取路径：使用 $TEMP_DIR ***
    pak_full_path=$(cat "$TEMP_DIR/selected_pak_path.txt")
    pak_file=$(cat "$TEMP_DIR/selected_pak_name.txt")

    echo -e "${YELLOW}当前PAK文件（作为打包模板）：$pak_file${NOCOLOR}"
    echo -e "${YELLOW}请确保 $PACK_DIR 目录下已放置需要打包的 .dat 文件。${NOCOLOR}"

    echo -e "${LIGHTPURPLE}开始打包文件：$pak_file${NOCOLOR}"
    if ! "$TOOL_CMD" -a -r "$pak_full_path" "$PACK_DIR"; then
        echo -e "${RED}打包失败，请检查 $TOOL_CMD 是否存在或有执行权限。${NOCOLOR}"
        return 1
    fi

    echo -e "${LIGHTPURPLE}打包结束: $pak_file -> $PACK_DIR${NOCOLOR}"

    # --- 自动导出打包结果 ---
    echo -e "${CYAN}>>> 正在自动导出打包后的 PAK 文件到共享目录...${NOCOLOR}"
    rm -rf "$SHARED_PAK_DIR"/* 2>/dev/null
    mv "$PAK_DIR"/*.pak "$SHARED_PAK_DIR"/ 2>/dev/null
    echo -e "${GREEN}打包后的 PAK 文件已成功导出到 $SHARED_PAK_DIR${NOCOLOR}"
    sleep 1
}

# ================================================================
# 主菜单
# ================================================================
clear
PS3="请选择工具类型: "
options=("[⚡]uexp解包[✨]" "[⚡]uexp打包[✨]" "退出")

select opt in "${options[@]}"
do
 case $opt in
    "[⚡]uexp解包[✨]")
        jb_simplified
    ;;
    "[⚡]uexp打包[✨]")
        db_simplified
    ;;
    "退出")
        echo -e "${CYAN}再见！${NOCOLOR}"
        exit
    ;;
    *)
        echo -e "${RED}无效选项 $REPLY${NOCOLOR}"
    ;;
 esac
done