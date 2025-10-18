import sounddevice as sd           # 实时音频采集
import numpy as np                 # 科学计算与FFT
import colorsys                    # HSV转RGB
import socket                      # UDP通信
import time                        # 主循环休眠控制
from tkinter import messagebox      # 界面弹窗（保留，但后面用托盘替代）
import threading                   # 线程支持（为托盘和主线程分离）
import pystray                     # 托盘功能库
from PIL import Image, ImageDraw   # 托盘图标支持
import sys;import os

ESP32_IP = '192.168.137.50'        # ESP32的IP地址，根据实际情况填写
ESP32_PORT = 8888                  # ESP32监听UDP端口
SAMPLERATE = 44100                 # 音频采样率，标准44.1kHz
WINDOW_SIZE = 512                  # 每帧采样窗口

# 访问目标资源，使得能在打包后还能获取
def get_resource_path(relpath):
    # relpath 例如: 'resource/xxx.png'
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relpath)
    return os.path.join(os.path.dirname(__file__), relpath)  # 纯源码情况

def find_audio_input_device():
    """
    自动查找电脑上的音频输入设备（双通道），优先虚拟声卡或混音
    """
    devices = sd.query_devices()
    # 优先虚拟声卡
    for i, d in enumerate(devices):
        if 'CABLE Output' in d['name'] and d['max_input_channels'] > 0:
            print(f"自动选择虚拟声卡: {i}: {d['name']}")
            return i
    # 其次混音
    for i, d in enumerate(devices):
        if (('Stereo Mix' in d['name'] or '立体声' in d['name']) and d['max_input_channels'] > 0):
            print(f"自动选择立体声混音: {i}: {d['name']}")
            return i
    print("未检测到虚拟声卡或Stereo Mix，请手动输入编号。设备列表如下:")
    for i, d in enumerate(devices):
        print(f"{i}: {d['name']} (max_input_channels={d['max_input_channels']})")
    try:
        return int(input("请输入使用的设备编号："))
    except Exception as e:
        print("设备选择无效:", e)
        return None

DEVICE = find_audio_input_device()

class MusicColorVisualizerNoGUI_UDP:
    """
    主过程控制类。实现实时音频采集、分析，UDP发送RGB到ESP32。
    关键稳健性增强见注释。
    """
    def __init__(self):
        self.last_hue = 0.0           # 平滑上一次色相
        self.last_val = 0.2           # 亮度
        self.running = True           # 运行标志
        self.paused = False        # 是否暂停发送
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)    # 创建UDP SOCKET

    def send_rgb_udp(self, rgb):
        """
        向ESP32通过UDP发送RGB三色二进制包，长度3字节
        ===============================
        稳健性增强：
          1. 自动clip保证rgb各值在0~255范围，防止bytes转换异常
          2. 异常捕获，防止UDP网络异常导致崩溃
        ===============================
        """
        if getattr(self, "paused", False): #是否暂停
            return
        safe_rgb = [max(0, min(255, int(x))) for x in rgb]    # clip范围
        packet = bytes(safe_rgb)
        try:
            self.udp.sendto(packet, (ESP32_IP, ESP32_PORT))
        except Exception as e:
            print("UDP发送异常：", e)

    def process_audio(self, indata):
        """
        主音频分析逻辑，周期性被回调，提取音频特征并转为RGB灯色
        ===============================
        稳健性增强：
          1. 关键计算过程加try-except防止分析崩溃
          2. RGB数值强制clip，防止传递给bytes出错
        ===============================
        """
        try:
            # 立体声取平均变单通道
            mono = indata.mean(axis=1)
            # 频谱分析 (FFT)
            fft = np.abs(np.fft.rfft(mono))
            freqs = np.fft.rfftfreq(len(mono), 1/SAMPLERATE)
            low = np.mean(fft[(freqs>20) & (freqs<400)])
            mid = np.mean(fft[(freqs>=400) & (freqs<4000)]) * 5
            high = np.mean(fft[(freqs>=4000) & (freqs<20000)]) * 10

            # 全局音量分析
            volume = np.sqrt(np.mean(mono ** 2))
            if not hasattr(self, 'vol_history'):
                self.vol_history = []
            self.vol_history.append(volume)
            if len(self.vol_history) > 1000:
                self.vol_history.pop(0)
            try:
                vol_max = np.percentile(self.vol_history, 95)
            except Exception:
                vol_max = 1.0
            vol_norm = volume / (vol_max + 1e-6) if volume > 1e-3 else 0
            vol_norm = np.clip(vol_norm, 0, 1)

            # 中频突变增强节奏
            mid_energy = np.mean(fft[(freqs>=400) & (freqs<4000)])
            if not hasattr(self, 'rhythm_history'):
                self.rhythm_history = []
            self.rhythm_history.append(mid_energy)
            if len(self.rhythm_history) > 10:
                prev_mid = np.mean(self.rhythm_history[-10:-1])
            else:
                prev_mid = self.rhythm_history[0]
            mid_rhythm_delta = mid_energy - prev_mid
            mid_rhythm_delta = max(mid_rhythm_delta, 0)
            gain = np.clip(mid_rhythm_delta * 2e-3, 0, 0.2)

            val_base = vol_norm * 0.75 + gain           # 亮度来自音量+节奏
            if not hasattr(self, 'last_val'):
                self.last_val = 0.2
            if val_base > self.last_val:
                alpha_up = 0.7
                val = self.last_val * (1-alpha_up) + val_base * alpha_up
            else:
                alpha_down = 0.2
                val = self.last_val * (1-alpha_down) + val_base * alpha_down
            self.last_val = val

            total = low + mid + high + 1e-8
            hue_raw = (mid + 2*high) / total if total > 0 else 0.0
            if not hasattr(self, 'last_hue'):
                self.last_hue = hue_raw
            hue = self.last_hue * 0.7 + hue_raw * 0.3
            self.last_hue = hue
            sat = 0.9

            rgb = colorsys.hsv_to_rgb(hue, sat, val)
            # 修正：保证RGB整数且[0,255]范围，防止崩溃
            rgb_disp = tuple(max(0, min(int(x * 255), 255)) for x in rgb)
            # print(f"[音量] 最大:{vol_max:.2f}, 当前:{volume:.2f}, 占比:{vol_norm*100:.2f}%")
            self.send_rgb_udp(rgb_disp)

        except Exception as e:
            print("音频处理异常：", e)
            # 可选：self.running = False

    def audio_callback(self, indata, frames, time_, status):
        """
        声卡数据到来时自动调用。
        ===============================
        稳健性增强：
          1. 如果status中有错误，主动提示
          2. try-except避免异常致使回调奔溃（PortAudio的异常不易捕获，但可以做提示）
        ===============================
        """
        if status:
            print("音频流状态提示:", status)
        if not self.running:  
            return
        try:
            self.process_audio(indata)
        except Exception as e:
            print("声音分析回调异常", e)

    def start(self):
        """
        主控制流程。开启音频流，同时进入主循环。
        ===============================
        稳健性增强：
          1. 捕获全部异常：音频流启动、主循环异常/设备丢失
          2. 退出前关闭UDP socket资源，避免死锁
        ===============================
        """
        if DEVICE is None:
            print("未选择可用设备。程序终止。")
            return
        try:
            with sd.InputStream(
                samplerate=SAMPLERATE,
                channels=2,
                blocksize=WINDOW_SIZE,
                device=DEVICE,
                callback=self.audio_callback,
                latency='low'
            ):
                print("正在运行，Ctrl+C退出。")
                while self.running:
                    time.sleep(0.01)
        except KeyboardInterrupt:
            print("程序终止。")
            self.running = False
        except Exception as e:
            print("音频主流程异常:", e)
            self.running = False
        finally:
            try:
                self.send_rgb_udp((0, 0, 0))
            except Exception:
                pass
            self.udp.close()

# ==== 托盘部分 新增 ====
def create_color_icon():
    im = Image.open(get_resource_path("resource\ICON_running.png"))
    im = im.resize((64, 64))
    return im
def create_paused_icon():
    im = Image.open(get_resource_path("resource\ICON_paused.png"))
    im = im.resize((64, 64))
    return im

class TrayApp:
    def __init__(self, main_app):
        self.main_app = main_app
        self.icon = pystray.Icon("MusicLight")
        self.icon.icon = create_color_icon()  # 初始化为运行状态图标
        self.icon.title = "节奏灯运行中"
        self.icon.menu = pystray.Menu(
            pystray.MenuItem(
                lambda item: '暂停' if not self.main_app.paused else '运行',
                self.toggle_pause
            ),
            pystray.MenuItem(
                "退出", self.on_exit
            )
        )
    def toggle_pause(self, icon, item):
        self.main_app.paused = not self.main_app.paused
        # 立即刷新菜单显示
        icon.menu = pystray.Menu(
            pystray.MenuItem(
                lambda item: '暂停' if not self.main_app.paused else '运行',
                self.toggle_pause
            ),
            pystray.MenuItem(
                "退出", self.on_exit
            )
        )
                # 切换托盘图标
        if self.main_app.paused:
            self.icon.icon = create_paused_icon()
        else:
            self.icon.icon = create_color_icon()

    def on_exit(self, icon, item):
        print("托盘退出事件，关闭主程序...")
        self.main_app.running = False
        icon.stop()
    def run(self):
        self.icon.run()

if __name__ == '__main__':
    # 保留原 messagebox 弹窗提示
    try:
        messagebox.showinfo("提示", "UDP节奏灯已在后台运行。")
    except Exception as e:
        print("弹窗异常:", e)
    # === 用托盘方式管理主程序 ===
    main_app = MusicColorVisualizerNoGUI_UDP()
    t_audio = threading.Thread(target=main_app.start)
    t_audio.daemon = True
    t_audio.start()
    tray = TrayApp(main_app)
    tray.run()