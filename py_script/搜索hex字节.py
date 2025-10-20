
import os
import re
from typing import List, Dict, Tuple

# 硬编码配置
SEARCH_DIR = "./tool/天线补丁包/天线0922"  # 搜索目录
SEARCH_DIR = "./paks/dat_temp"
OUTPUT_DIR = SEARCH_DIR          # 输出目录
CONTEXT_BYTES = 32               # 上下文字节数

TARGET_PATTERNS = [
    "34303332353100892041B507", # 改前 2次  343033323531 = 403251  小白衣  【  403251--403251--T恤(白)(新)(创建角色使  】偏移量 0xB0A8 
    "3431333439340653079ACE07", # 改后 0次  343133343934 = 413494  青焰关公【  413494--413494--角色-青焰关公  】偏移量 0x1D930 
    
    "3431333439340053079ACE07", # 改前 3次  343133343934 = 413494  青焰关公

    "343035303131000473C04C07", # 改前 3次  343035303131 = 405011  棕色高帮运动鞋  【  405011--405011--棕色高帮运动鞋-创建角  】偏移量 0x11408 
    "3431333635370062A4A4EA07", # 改后 0次  343133363537 = 413657  宇宙意志-塞卢姆 【  413657--413657--宇宙意志-塞卢姆  】偏移量 0x1E5C0 

    "3431333635370002A4A4EA07", # 改前 3次  343133363537 = 413657  宇宙意志-塞卢姆 


]
# "343133343936" # 银枪赵云 【  413496--413496--角色-银枪赵云  】偏移量 0x1D960 
TARGET_PATTERNS = [
    "215" # dat搜索关键词
]
# 名称 id 偏移 指针
MY_MAP = {
    "405010": {
        "name": "蓝色高帮运动鞋",
        "asc": "343035303130",
        "pointer": "00b36ea52b07",
        "offset": "0x113F0"
    },
    "403251": {
        "name": "小白T恤(白)(新)(创建角色使用)",
        "asc": "343033323531", 
        "pointer": "00892041b507",
        "offset": "0xB0A8"
    }
}

def print_match_context(file_path: str, hex_content: str, pattern: str, match_pos: int):
    """打印匹配项的上下文到控制台"""
    pattern_len = len(re.sub(r'\s', '', pattern))
    start = max(0, match_pos - CONTEXT_BYTES * 2)
    end = min(len(hex_content), match_pos + pattern_len + CONTEXT_BYTES * 2)
    
    # 格式化上下文显示
    context = hex_content[start:end]
    marked_context = (
        context[:match_pos-start] + 
        f"【{context[match_pos-start:match_pos-start+pattern_len]}】" + 
        context[match_pos-start+pattern_len:]
    )
    
    print(f"\n▼ 匹配内容: {pattern.upper()}")
    print(f"▼ 文件位置: {file_path}")
    print(f"▼ 偏移地址: 0x{match_pos//2:08X}")
    print("▼ 上下文内容:")
    print("-" * 80)
    # 每行显示64个字符（32字节）
    for i in range(0, len(marked_context), 64):
        print(marked_context[i:i+64])
    print("-" * 80)

def export_hex_with_matches(file_path: str, patterns: List[str]):
    """处理匹配文件并输出"""
    try:
        with open(file_path, 'rb') as f:
            hex_content = f.read().hex()
        
        # 创建输出目录
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        
        # 生成输出文件名
        base_name = os.path.basename(file_path)
        output_path = os.path.join(OUTPUT_DIR, f"{base_name}_hexdump.txt")
        
        # 查找所有匹配项
        found = False
        for pattern in patterns:
            clean_p = re.sub(r'\s', '', pattern.lower())
            for match in re.finditer(clean_p, hex_content):
                if not found:
                    print(f"\n▷ 发现匹配文件: {file_path}")
                    found = True
                print_match_context(file_path, hex_content, pattern, match.start())
        
        # 如果找到匹配项，输出完整文件
        if found:
            with open(output_path, 'w', encoding='utf-8') as f:
                # 标记所有匹配项
                marked_content = hex_content
                for p in patterns:
                    clean_p = re.sub(r'\s', '', p.lower())
                    marked_content = re.sub(clean_p, f"【{clean_p.upper()}】", marked_content)
                
                # 每行128字符（64字节）
                for i in range(0, len(marked_content), 128):
                    f.write(marked_content[i:i+128] + '\n')
            
            print(f"✔ 已导出完整内容到: {output_path}")
            
    except Exception as e:
        print(f"【错误】处理文件 {file_path} 失败: {str(e)}")

def scan_files():
    """主扫描函数"""
    print(f"正在扫描目录 [{SEARCH_DIR}]...")
    
    # 验证Hex模式
    valid_patterns = [re.sub(r'\s', '', p.lower()) 
                     for p in TARGET_PATTERNS 
                     if re.fullmatch(r'^[0-9a-fA-F]+$', re.sub(r'\s', '', p))]
    
    if not valid_patterns:
        print("【错误】无有效Hex模式")
        return
    
    # 扫描文件
    match_count = 0
    for root, _, files in os.walk(SEARCH_DIR):
        for file in files:
            file_path = os.path.join(root, file)
            with open(file_path, 'rb') as f:
                if any(re.search(p, f.read().hex()) for p in valid_patterns):
                    export_hex_with_matches(file_path, valid_patterns)
                    match_count += 1
    
    print("\n扫描完成")
    print(f"共找到 {match_count} 个文件包含匹配内容")

if __name__ == "__main__":
    scan_files()