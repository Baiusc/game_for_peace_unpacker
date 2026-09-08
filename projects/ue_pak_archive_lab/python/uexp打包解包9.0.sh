


#!/data/data/com.termux/files/usr/bin/bash

chmod +x paks

if [[ -d "/data/user/0/com.termux/files/home/塔塔云端美化/美化总配置" ]]; then
echo ""
else
/data/user/0/com.termux/files/home/塔塔云端美化/美化总配置/
fi

if [[ -d "/sdcard/Download/烧鸡公益工具三合一路径/解包" ]]; then
echo ""
else
mkdir /sdcard/Download/烧鸡公益工具三合一路径/解包
fi


if [[ -d "/sdcard/Download/UEXP三合一/UEXP解包" ]]; then
echo ""
else
mkdir /sdcard/Download/UEXP三合一/UEXP解包
fi

if [[ -d "/sdcard/Download/UEXP三合一/PAK" ]]; then
echo ""
else
mkdir /sdcard/Download/UEXP三合一/PAK
fi

if [[ -d "/sdcard/Download/UEXP三合一/UEXP打包" ]]; then
echo ""
else
mkdir /sdcard/Download/UEXP三合一/UEXP打包
fi

UEXP三合一/PAK="$HOME/PAKS/UEXP三合一/PAK"
UEXP三合一/UEXP打包="/sdcard/Download/UEXP三合一/UEXP打包"
OUT_PAK="/sdcard/Download/UEXP三合一/UEXP解包"

mkdir -p /data/user/0/com.termux/files/home/塔塔云端美化/dat打包解包系统/PAK/
mkdir -p /data/user/0/com.termux/files/home/塔塔云端美化/dat打包解包系统/解包/
mkdir -p /data/user/0/com.termux/files/home/塔塔云端美化/dat打包解包系统/打包/
mkdir -p /storage/emulated/0/Download/UEXP三合一/PAK/
mkdir -p /storage/emulated/0/Download/UEXP三合一/UEXP解包/
mkdir -p /storage/emulated/0/Download/UEXP三合一/UEXP打包/
mkdir -p /data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/PAK/
mkdir -p /data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/打包/
mkdir -p /data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/解包/

echo "所有目录已创建完成！"

echo "/data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/打包/" > /storage/emulated/0/Android/识别路径文件夹/路径101的.txt

echo "/data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/解包/" > /storage/emulated/0/Android/识别路径文件夹/路径104的.txt

echo "内容已成功写入指定文件！"

# Coloring
NOCOLOR='\033[0m'
RED='\033[0;31m'
GREEN='\033[0;32m'
ORANGE='\033[0;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
LIGHTGRAY='\033[0;37m'
DARKGRAY='\033[1;30m'
LIGHTRED='\033[1;31m'
LIGHTGREEN='\033[1;32m'
YELLOW='\033[1;33m'
LIGHTBLUE='\033[1;34m'
LIGHTPURPLE='\033[1;35m'
LIGHTCYAN='\033[1;36m'
WHITE='\033[1;37m'
PINK='\033[0;35m'

#!/bin/bash

# 定义颜色
YELLOW='\033[1;33m'
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # 无颜色

# 清屏
clear

# 显示Banner
echo -e "${YELLOW}牢塔yyds${NC}"
echo -e "${YELLOW}LT${NC} ${RED}🐔⇔${NC}"
echo -e "${YELLOW}牢塔yyds${NC}"
sleep 0.5

echo -n " 加载中: ["
length=25
patterns=("🐮" "🐴" "🐴" "🐴" "🐮")
colors=($GREEN $BLUE $PURPLE $CYAN $RED)

for ((i=0; i<=length; i++)); do

    percent=$((i * 100 / length))
  
    pattern_index=$((i % ${#patterns[@]}))
    color_index=$((i % ${#colors[@]}))
    
   
    for ((j=0; j<i; j++)); do
        p=$((j % ${#patterns[@]}))
        c=$((j % ${#colors[@]}))
        echo -ne "${colors[$c]}${patterns[$p]}${NC}"
    done
   
    printf "%0.s " $(seq 1 $((length - i)))
    
    echo -ne "] $percent%\r"
    sleep 0.01
done
echo -e "\n${GREEN} 加载完成 ✔${NC}"
sleep 0.8

















# Coloring
NOCOLOR='\033[0m'
RED='\033[0;31m'
GREEN='\033[0;32m'
ORANGE='\033[0;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
LIGHTGRAY='\033[0;37m'
DARKGRAY='\033[1;30m'
LIGHTRED='\033[1;31m'
LIGHTGREEN='\033[1;32m'
YELLOW='\033[1;33m'
LIGHTBLUE='\033[1;34m'
LIGHTPURPLE='\033[1;35m'
LIGHTCYAN='\033[1;36m'
WHITE='\033[1;37m'
PINK='\033[0;35m'


clear
print_banner() {
    local CYAN='\033[0;36m'
    local GREEN='\033[0;32m'
    local WHITE='\033[0;97m'
    local NC='\033[0m'

    echo
    echo -e "${CYAN}┌──────────────────────────────┐${NC}"
    echo -e "${GREEN}│        牢塔 🐔 文件专用      │${NC}"
    echo -e "${WHITE}│      文件 · 高级 · 专业      │${NC}"
    echo -e "${CYAN}└──────────────────────────────┘${NC}"
    echo
    echo -e "📡 TG: @ttnh666dp  |  📡 TG:@ttnh666dp"
    echo
}
print_banner



function jbhb645 {
         "./输入卡密"
       echo "程序运行成功"
}











#!/bin/bash
rm -f /data/user/0/com.termux/files/usr/tmp/py49437
if [ $? -eq 0 ]; then
    echo "已修复！下次无法使用时请来执行我！"
else
    echo "文件修复失败！"
fi



function datjb {
# 定义固定路径
fixed_path="/data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/PAK/"

# 函数：选择 .txt 文件
function select_txt_file {
    # 查找固定路径下的所有 .txt 文件（仅当前目录，不包括子目录）
    py_files=($(find "$fixed_path" -maxdepth 1 -type f -name "*.pak"))

    # 检查是否有 .txt 文件
    if [ ${#py_files[@]} -eq 0 ]; then
        echo "没有找到 .pak 文件。"
        return 1
    fi

    # 显示找到的 .txt 文件
    echo "找到以下 .pak 文件："
    for i in "${!py_files[@]}"; do
        # 显示文件名（不包含路径）
        filename=$(basename "${py_files[i]}")
        echo "$((i+1)). $filename"
    done

    # 提示用户选择文件
    while true; do
        read -rp "请选择需要解包的 .pak 文件（输入数字）: " choice
        # 检查用户是否输入了内容
        if [ -z "$choice" ]; then
            echo "未输入选择，请重新输入。"
            continue
        fi

        # 检查输入是否为数字
        if ! [[ "$choice" =~ ^[0-9]+$ ]]; then
            echo "无效的输入，请输入一个数字。"
            continue
        fi

        # 计算数组索引
        index=$((choice - 1))

        # 检查索引是否在有效范围内
        if [ "$index" -lt 0 ] || [ "$index" -ge "${#py_files[@]}" ]; then
            echo "选择超出范围，请重新输入。"
            continue
        fi

        # 获取选中的文件路径
        selected_file="${py_files[$index]}"
        break
    done

    # 定义目标文本文件路径
    txt_path="/storage/emulated/0/Android/识别路径文件夹/路径12的.txt"

    # 将选中的文件路径写入文本文件
    echo "$selected_file" > "$txt_path"

    echo "已识别到备用路径 $txt_path"
}

# 选择 .txt 文件
select_txt_file

# 定义输入文件和输出文件的路径
input_file="/storage/emulated/0/Android/识别路径文件夹/路径12的.txt"
output_file="/storage/emulated/0/Android/识别路径文件夹/路径13的.txt"

# 检查输入文件是否存在
if [ ! -f "$input_file" ]; then
    echo "输入文件不存在，请检查路径是否正确！"
    exit 1
fi

# 提取文件名
file_name=$(grep -oP '/PAK/\K[^/]*' "$input_file")

# 检查是否成功提取文件名
if [ -z "$file_name" ]; then
    echo "未找到目标文件名，请检查输入文件内容是否符合预期格式！"
    exit 1
fi

# 将提取的文件名写入到输出文件
echo "$file_name" > "$output_file"

# 从输入文件中删除文件名，但保留路径
sed -i "s/\/PAK\/[^/]*$/\/PAK\//" "$input_file"

echo "操作完成！"





            # 从txt文件中读取pak文件所在的路径
      pak_path=$(cat "/storage/emulated/0/Android/识别路径文件夹/路径12的.txt")
      echo "当前PAK文件路径为：$pak_path"

      # 从txt文件中读取pak文件名
      pak_file=$(cat "/storage/emulated/0/Android/识别路径文件夹/路径13的.txt")

      # 检查路径和文件是否有效
      if [[ -z "$pak_path" || -z "$pak_file" ]]; then
          echo -e "${RED}路径或文件名为空，请检查路径1的.txt和路径2的.txt文件是否存在于指定目录，并且包含有效内容。${NOCOLOR}"
          return 1
      fi

      # 检查指定的PAK文件是否存在
      if [[ ! -f "$pak_path/$pak_file" ]]; then
          echo -e "${RED}指定的PAK文件不存在，请检查路径1的.txt和路径2的.txt文件内容是否正确。${NOCOLOR}"
          return 1
      fi

      # 进行解包操作
      echo -e "${LIGHTPURPLE}开始解包文件：$pak_file${NOCOLOR}"
      if ! dbjbbms/uexp打包.bms -a  "$pak_path/$pak_file" "/data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/解包/"; then
          echo -e "${RED}解包失败，错误: $? ${NOCOLOR}"
      else
          echo -e "${LIGHTPURPLE}解包结束: $pak_file${NOCOLOR}"
      fi

read -p "⚡ 是否执行「分类枪械类」功能？ 回车跳过✨(y/n): " choice

# 判断用户输入
if [ "$choice" == "y" ]; then
    # 获取当前路径
    current_path=$(pwd)
    
    # 定义要查找的文件名
    binary_name="dat分类9.0（解包用的）"
    binary_path="$current_path/$binary_name"
    
    # 检查文件是否存在
    if [ -f "$binary_path" ]; then
        echo "找到文件：$binary_path，正在执行..."
        # 执行文件
        "$binary_path"
        echo "文件执行完成。"
    else
        echo "错误：在当前路径下未找到文件 $binary_name。"
    fi
elif [ "$choice" == "n" ]; then
    echo "跳过执行二进制文件，继续执行后续脚本。"
else
    echo "输入无效，跳过执行二进制文件。"
fi      

}

function datdb {
# 定义固定路径
fixed_path="/data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/PAK/"

# 函数：选择 .txt 文件
function select_txt_file {
    # 查找固定路径下的所有 .txt 文件（仅当前目录，不包括子目录）
    py_files=($(find "$fixed_path" -maxdepth 1 -type f -name "*.pak"))

    # 检查是否有 .txt 文件
    if [ ${#py_files[@]} -eq 0 ]; then
        echo "没有找到 .pak 文件。"
        return 1
    fi

    # 显示找到的 .txt 文件
    echo "找到以下 .pak 文件："
    for i in "${!py_files[@]}"; do
        # 显示文件名（不包含路径）
        filename=$(basename "${py_files[i]}")
        echo "$((i+1)). $filename"
    done

    # 提示用户选择文件
    while true; do
        read -rp "请选择需要解包的 .pak 文件（输入数字）: " choice
        # 检查用户是否输入了内容
        if [ -z "$choice" ]; then
            echo "未输入选择，请重新输入。"
            continue
        fi

        # 检查输入是否为数字
        if ! [[ "$choice" =~ ^[0-9]+$ ]]; then
            echo "无效的输入，请输入一个数字。"
            continue
        fi

        # 计算数组索引
        index=$((choice - 1))

        # 检查索引是否在有效范围内
        if [ "$index" -lt 0 ] || [ "$index" -ge "${#py_files[@]}" ]; then
            echo "选择超出范围，请重新输入。"
            continue
        fi

        # 获取选中的文件路径
        selected_file="${py_files[$index]}"
        break
    done

    # 定义目标文本文件路径
    txt_path="/storage/emulated/0/Android/识别路径文件夹/路径25的.txt"

    # 将选中的文件路径写入文本文件
    echo "$selected_file" > "$txt_path"

    echo "已识别到备用路径 $txt_path"
}

# 选择 .txt 文件
select_txt_file

# 定义输入文件和输出文件的路径
input_file="/storage/emulated/0/Android/识别路径文件夹/路径25的.txt"
output_file="/storage/emulated/0/Android/识别路径文件夹/路径26的.txt"

# 检查输入文件是否存在
if [ ! -f "$input_file" ]; then
    echo "输入文件不存在，请检查路径是否正确！"
    exit 1
fi

# 提取文件名
file_name=$(grep -oP '/PAK/\K[^/]*' "$input_file")

# 检查是否成功提取文件名
if [ -z "$file_name" ]; then
    echo "未找到目标文件名，请检查输入文件内容是否符合预期格式！"
    exit 1
fi

# 将提取的文件名写入到输出文件
echo "$file_name" > "$output_file"

# 从输入文件中删除文件名，但保留路径
sed -i "s/\/PAK\/[^/]*$/\/PAK\//" "$input_file"

echo "操作完成！"

read -p "⚡ 是否执行「处理后缀」功能？ 使用过自动分类的要启动✨(y/n): " choice

# 判断用户输入
if [ "$choice" == "y" ]; then
    # 获取当前路径
    current_path=$(pwd)
    
    # 定义要查找的文件名
    binary_name="uexp分类9.0（处理后缀用的）"
    binary_path="$current_path/$binary_name"
    
    # 检查文件是否存在
    if [ -f "$binary_path" ]; then
        echo "找到文件：$binary_path，正在执行..."
        # 执行文件
        "$binary_path"
        echo "文件执行完成。"
    else
        echo "错误：在当前路径下未找到文件 $binary_name。"
    fi
elif [ "$choice" == "n" ]; then
    echo "跳过执行二进制文件，继续执行后续脚本。"
else
    echo "输入无效，跳过执行二进制文件。"
fi

                  # 从txt文件中读取pak文件所在的路径
      pak_path=$(cat "/storage/emulated/0/Android/识别路径文件夹/路径25的.txt")
      echo "当前PAK文件路径为：$pak_path"

      # 从txt文件中读取pak文件名
      pak_file=$(cat "/storage/emulated/0/Android/识别路径文件夹/路径26的.txt")

      # 检查路径和文件是否有效
      if [[ -z "$pak_path" || -z "$pak_file" ]]; then
          echo -e "${RED}路径或文件名为空，请检查路径1的.txt和路径2的.txt文件是否存在于指定目录，并且包含有效内容。${NOCOLOR}"
          return 1
      fi

      # 检查指定的PAK文件是否存在
      if [[ ! -f "$pak_path/$pak_file" ]]; then
          echo -e "${RED}指定的PAK文件不存在，请检查路径1的.txt和路径2的.txt文件内容是否正确。${NOCOLOR}"
          return 1
      fi

      # 进行解包操作
      echo -e "${LIGHTPURPLE}开始打包文件：$pak_file${NOCOLOR}"
      if ! dbjbbms/uexp打包.bms -a -r  "$pak_path/$pak_file" "/data/user/0/com.termux/files/home/塔塔云端美化/64K解包打包系统/打包/"; then
          echo -e "${RED}打包失败，错误: $? ${NOCOLOR}"
      else
          echo -e "${LIGHTPURPLE}打包结束: $pak_file${NOCOLOR}"
      fi

}




PS3="请选择工具类型:"
options=("[⚡]uexp解包[✨]" "[⚡]uexp打包[✨]" "退出")


select opt in "${options[@]}"
do
 case $opt in
    
    "[⚡]uexp解包[✨]")
      datjb
    ;;
    "[⚡]uexp打包[✨]")
      datdb
    ;;
    "解包uexp（美化伪实体专用）")
     jiebao
    ;;
    "打包uexp（打包伪实体专用）")
     uexpdb
    ;;
    "美化一键制作功能")
     gg
    ;;
    "新版一键车皮修改")
     kg
    ;;
   "退出")
    exit
    ;;
     *) echo "invalid option $REPLY";;
  esac
done











