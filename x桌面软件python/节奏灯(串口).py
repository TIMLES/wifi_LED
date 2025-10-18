import sounddevice as sd         # 用于音频实时采集
import numpy as np              # 音频和FFT等科学计算
import colorsys                 # HSV转RGB的色彩工具
import serial                   # 电脑串口通信
import time                     # 睡眠与时间控制
from tkinter import messagebox
import serial.tools.list_ports  # 导入串口端口枚举模块

# 串口参数（按实际硬件设置修改）
SERIAL_BAUDRATE = 115200    # 串口波特率
SAMPLERATE = 44100          # 音频采样率，标准为44100Hz
WINDOW_SIZE = 1024          # 每帧处理音频样本数，越小刷新越快

def find_audio_input_device():
    """
    自动查找并选择电脑可用的音频输入设备。
    优先使用虚拟声卡或Stereo Mix（录制电脑播放的声音）。
    如果没检测到则手动输入编号。
    """
    devices = sd.query_devices()
    # 首先查找“虚拟声卡”或“CABLE Output”
    for i, d in enumerate(devices):
        if 'CABLE Output' in d['name'] and d['max_input_channels'] > 0:
            print(f"自动选择虚拟声卡: {i}: {d['name']}")
            return i
    # 其次查找“Stereo Mix”（立体声混音）
    for i, d in enumerate(devices):
        if (
            ('Stereo Mix' in d['name'] or '立体声' in d['name']) and
            d['max_input_channels'] > 0
        ):
            print(f"自动选择立体声混音: {i}: {d['name']}")
            return i
    # 如果自动未找到，列出所有可用设备供手动选择
    print("未检测到虚拟声卡或Stereo Mix，请手动输入编号。设备列表如下:")
    for i, d in enumerate(devices):
        print(f"{i}: {d['name']} (max_input_channels={d['max_input_channels']})")
    try:
        return int(input("请输入使用的设备编号："))
    except Exception as e:
        print("设备选择无效:", e)
        return None
# 获得用户选择的音频设备编号
DEVICE = find_audio_input_device()


#获取串口设备。握手
def handshake_with_esp32():
    while(1):
        # 枚举本机所有可用串口设备，提取设备名称（如'COM3'或'/dev/ttyUSB0'等）
        ports = [p.device for p in serial.tools.list_ports.comports()]
        for port in ports:  # 依次遍历每一个可用串口
            try:
                # 尝试用指定端口名打开串口，波特率115200，读操作超时0秒
                s = serial.Serial(port, SERIAL_BAUDRATE, timeout=0.1)
                # 向设备写入特定握手信号（字节类型，末尾加换行）
                s.write(b'HELLO-ESP32\n')
                # 等待0.5秒，留给ESP32处理并响应的时间
                time.sleep(0.2)
                # 从串口读取一行数据，字节转字符串，并去掉两端换行和空格
                rsp = s.readline().decode().strip()
                # 判定是否收到预期的握手应答内容
                if rsp == 'HELLO-PC':
                    print(f'ESP32连接成功: {port}')  # 打印成功信息和端口名
                    return s  # 返回找到的端口名，结束函数
                else:
                    s.close()
            except:  # 若出现异常（如端口不可用、串口出错等），忽略并继续尝试下一个
                pass
        # 若所有端口都尝试过仍未找到符合条件的ESP32，则打印提示信息
        print('未发现ESP32')
        time.sleep(0.5)



class MusicColorVisualizerNoGUI:
    """
    主控制类。实现音频分析和串口RGB发送，没有界面，只后台运行和调试打印
    """
    def __init__(self):
        self.last_hue = 0.0       # 上一次色相值，用于平滑过渡
        self.last_val = 0.2       # 上一次亮度值
        self.running = True       # 是否运行标志
        # 串口初始化，并尝试打开端口
        try:
            self.serial = handshake_with_esp32()
        except Exception as e:
            print(f'串口打开失败: {e}')
            self.serial = None

    def send_rgb_to_serial(self, rgb):
        """
        向串口发送RGB三色数据作为字符串，如 '128,220,12\n'
        """
        if self.serial and self.serial.is_open:
            packet = f"{rgb[0]},{rgb[1]},{rgb[2]}\n"
            try:
                self.serial.write(packet.encode())
                # print("[RGB]", rgb)
            except Exception as e:
                self.serial.close()
                print("串口发送异常：", e)
                print("正在重连...")
                self.serial = handshake_with_esp32();

                
    def process_audio(self, indata):
        """
        对每1帧音频数据做频谱处理、提取特征
        人声节奏突出版本：亮度动态与中频能量突变增强绑定，色相跟随频谱分布
        """
        # 对双通道（立体声）取均值变单声道
        mono = indata.mean(axis=1)
        # 进行快速傅里叶变换（FFT），计算频域能量
        fft = np.abs(np.fft.rfft(mono))
        freqs = np.fft.rfftfreq(len(mono), 1/SAMPLERATE)
        # 分频段统计能量（低频/中频/高频）
        low = np.mean(fft[(freqs>20) & (freqs<400)])
        mid = np.mean(fft[(freqs>=400) & (freqs<4000)]) * 5
        high = np.mean(fft[(freqs>=4000) & (freqs<20000)]) * 10
        # 计算整体音量（均方根RMS）
        volume = np.sqrt(np.mean(mono ** 2))

        # 动态音量归一化（滑动窗口最大值）
        if not hasattr(self, 'vol_history'):
            self.vol_history = []
        self.vol_history.append(volume)
        if len(self.vol_history) > 1000:
            self.vol_history.pop(0)

        vol_max = np.percentile(self.vol_history, 95)
        vol_norm = volume / (vol_max + 1e-6) if volume>1e-3 else 0
        vol_norm = np.clip(vol_norm, 0, 1)

        # -------- 节奏增强：分析中频能量突变 --------
        # 用于突出人声节奏（主要在中频）
        mid_energy = np.mean(fft[(freqs>=400) & (freqs<4000)])
        if not hasattr(self, 'rhythm_history'):
            self.rhythm_history = []
        self.rhythm_history.append(mid_energy)
        if len(self.rhythm_history) > 10:
            prev_mid = np.mean(self.rhythm_history[-10:-1])
        else:
            prev_mid = self.rhythm_history[0]
        mid_rhythm_delta = mid_energy - prev_mid
        mid_rhythm_delta = max(mid_rhythm_delta, 0)  # 只用能量上升部分

        # -------- 改进亮度算法，将节奏与普通能量结合 --------
        # 节奏增强gain可以调节，建议取值在0~0.3之间
        gain = np.clip(mid_rhythm_delta * 2e-3, 0, 0.2)  # 参数可酌情调试
        val_base = vol_norm * 0.75 + gain                 # 亮度由音量和节奏共同决定

        # attack/release平滑（亮度不乱跳）
        if not hasattr(self, 'last_val'):
            self.last_val = 0.2
        if val_base > self.last_val:
            alpha_up = 0.7
            val = self.last_val * (1-alpha_up) + val_base * alpha_up 
        else:
            alpha_down = 0.2
            val = self.last_val * (1-alpha_down) + val_base * alpha_down
        self.last_val = val

        # -------- 色相算法（保持你的设定） --------
        total = low + mid + high + 1e-8
        hue_raw = (mid + 2*high) / total if total > 0 else 0.0
        if not hasattr(self, 'last_hue'):
            self.last_hue = hue_raw
        hue = self.last_hue * 0.7 + hue_raw * 0.3
        self.last_hue = hue
        sat = 0.9

        # HSV->RGB：得到适合灯光的三色值
        rgb = colorsys.hsv_to_rgb(hue, sat, val)
        rgb_disp = tuple(int(x * 255) for x in rgb)

        # # 打印调试信息
        print(f"[音量] 最大:{vol_max:.2f}, 当前:{volume:.2f}, 占比:{vol_norm*100:.2f}%")
        # print(f"[节奏] delta:{mid_rhythm_delta:.2f}, gain:{gain:.2f}, final_val:{val:.2f}")


        # 直接发送RGB到下位机（如ESP32）
        self.send_rgb_to_serial(rgb_disp)
    def audio_callback(self, indata, frames, time_, status):
        """
        音频采集回调函数。每到一帧数据自动被调用
        """
        if not self.running:
            return
        self.process_audio(indata)
    def start(self):
        """
        主控制流程。初始化音频流并循环等待，直到Ctrl+C终止
        """
        if DEVICE is None:
            print("未选择可用设备。程序终止。")
            return
        try:
            # 声卡流和串口、主循环同时运行
            with sd.InputStream(
                samplerate=SAMPLERATE,
                channels=2,                 # 双声道
                blocksize=WINDOW_SIZE,      # 每帧采样点数（决定刷新率）
                device=DEVICE,              # 使用的声卡编号
                callback=self.audio_callback, # 音频数据到来就进行分析和发送
                latency='low'               # 优先低延迟
            ):
                print("正在运行，Ctrl+C退出。")
                while self.running:
                    time.sleep(0.1)         # 主线程小休，让回调有机会运行
        except KeyboardInterrupt:
            print("程序终止。")
            self.running = False
        # 程序退出时安全关闭串口
        if self.serial:
            self.serial.close()
if __name__ == '__main__':
    messagebox.showinfo("提示", "节奏灯已在后台运行。")
    MusicColorVisualizerNoGUI().start()