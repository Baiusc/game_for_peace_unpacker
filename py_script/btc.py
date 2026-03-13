#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BTC 永续合约监控版 (UTC+8) - 窗口缩放优化版
- 优化：使用 MaxNLocator 自动控制标签数量，防止重叠
- 优化：使用 autofmt_xdate 自动旋转和对齐标签
- 优化：手动调整边距，确保小窗口下标签不被遮挡
"""

import ccxt
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.ticker as ticker  # 引入刻度管理器
from matplotlib.animation import FuncAnimation
import warnings
from datetime import datetime, timedelta
import pytz

# === 配置区域 ===
SYMBOL = 'BTC/USDT:USDT'
TIMEFRAME = '1m'
HOURS_TO_SHOW = 6
LIMIT = 400
PROXY_URL = 'http://127.0.0.1:7890'
REFRESH_RATE = 2000

warnings.filterwarnings("ignore")
plt.style.use('dark_background')

class BTCSwapMonitorPro:
    def __init__(self):
        self.exchange = ccxt.binance({
            'enableRateLimit': True,
            'options': {'defaultType': 'swap'},
            'proxies': {'http': PROXY_URL, 'https': PROXY_URL}
        })
        
        # 增加 figsize 比例，适应更灵活的缩放
        self.fig, self.ax = plt.subplots(figsize=(10, 5))
        self.tz_bj = pytz.timezone('Asia/Shanghai')
        self.last_stored_price = None

    def fetch_data(self):
        try:
            ohlcv = self.exchange.fetch_ohlcv(SYMBOL, timeframe=TIMEFRAME, limit=LIMIT)
            if not ohlcv: return pd.DataFrame()
            
            df = pd.DataFrame(ohlcv, columns=['ts', 'open', 'high', 'low', 'close', 'vol'])
            df['dt_bj'] = pd.to_datetime(df['ts'], unit='ms', utc=True).dt.tz_convert(self.tz_bj)
            df['ma20'] = df['close'].rolling(window=20).mean()
            return df
        except Exception as e:
            print(f"⚠️ 数据获取错误: {e}")
            return pd.DataFrame()

    def update_chart(self, frame):
        df = self.fetch_data()
        if df.empty: return

        now = datetime.now(self.tz_bj)
        cutoff = now - timedelta(hours=HOURS_TO_SHOW)
        df_view = df.loc[df['dt_bj'] >= cutoff]
        if df_view.empty: return

        self.ax.clear()

        dates = df_view['dt_bj']
        closes = df_view['close']
        last_price = closes.iloc[-1]
        last_date = dates.iloc[-1]

        # 1. 绘图
        self.ax.plot(dates, closes, color='#00E5FF', linewidth=1.2, label='Price (1m)')
        self.ax.fill_between(dates, df_view['low'], closes, color='#00E5FF', alpha=0.1)
        self.ax.plot(dates, df_view['ma20'], color='#FFD700', linewidth=0.8, linestyle='--', label='MA20')
        self.ax.plot(last_date, last_price, marker='o', color='red', markersize=6, zorder=5)

        # 2. 标题栏
        price_color = '#00ff00' if self.last_stored_price is None or last_price >= self.last_stored_price else '#ff4444'
        self.last_stored_price = last_price
        self.ax.set_title(f"{SYMBOL} (6H)", loc='left', fontsize=10, pad=15)
        self.ax.set_title(f"LIVE: ${last_price:,.2f}", loc='right', color=price_color, fontsize=12, fontweight='bold', pad=15)

        # 3. 坐标轴与刻度优化 (核心解决遮挡代码)
        self.ax.yaxis.tick_right()
        self.ax.yaxis.set_label_position("right")
        self.ax.set_xlim(cutoff, now + timedelta(minutes=5))
        
        # 3.1 时间格式
        self.ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M', tz=self.tz_bj))
        
        # 3.2 智能限制标签数量：MaxNLocator 会根据窗口大小自动决定显示几个时间点
        # nbins=8 表示即使窗口很大，最多也就显示 8 个时间标签
        self.ax.xaxis.set_major_locator(ticker.MaxNLocator(nbins=8, prune='both'))
        
        # 3.3 自动倾斜标签并对齐，防止水平重叠
        self.fig.autofmt_xdate(rotation=30, ha='right')

        # 4. 辅助线与网格
        self.ax.grid(True, linestyle=':', alpha=0.15)
        self.ax.legend(loc='upper left', frameon=False, fontsize='x-small')
        
        # 5. 手动锁定边距 (核心解决被窗口切掉代码)
        # 预留足够的 bottom 空间给 X 轴标签，预留 top 给标题，预留 right 给价格轴
        plt.subplots_adjust(bottom=0.18, top=0.88, right=0.92, left=0.08)

    def start(self):
        print(f"🚀 监控启动: {SYMBOL} (窗口缩放优化版)")
        ani = FuncAnimation(self.fig, self.update_chart, interval=REFRESH_RATE, cache_frame_data=False)
        plt.show()

if __name__ == "__main__":
    monitor = BTCSwapMonitorPro()
    monitor.start()