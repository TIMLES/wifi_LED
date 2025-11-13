# joystick_sender.py
import pygame
import socket
import time
import os
from FPScontrol import FPSLimiter
class JoystickUDPController:
    """
    Xbox手柄UDP实时控制主类。用于向ESP8266/ESP32发送控制包。
    """
    STATUS_NOT_CONNECTED = "not_connected"
    STATUS_RUNNING = "running"
    STATUS_DISCONNECTED = "disconnected"  # 运行后断开
    STYLES = ["Xbox", "Switch", "PS"]

    def __init__(self, addressesList, fps=30):

        self.ESP_ADDRESSES = addressesList
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        pygame.init()
        pygame.joystick.init()
        self.clock = pygame.time.Clock()
        self.FPS = fps
        self.running = True
        self.paused = False         # 状态已由status替代
        self.status = self.STATUS_NOT_CONNECTED  # not_connected/running/disconnected
        self.style = "Xbox"
        self.frame_count = 0

        self.fps = 0  #实测fps
        self.last_fps_print = time.time()
        self.last_packet = None
        self.j = None             # 当前手柄对象
        self.jid = None           # 当前手柄 id
        self._joystick_count = 0  # 上次已知手柄总数
        self._last_poll_time = 0  # 轮询保险计时器

    def check_connection(self):
        """
        自动检测手柄连接和断开，只有断开时做pygame.joystick.quit/init，
        有手柄时不会踢掉对象，支持热插拔。
        """
        joystick_count = pygame.joystick.get_count()
        if joystick_count == 0:
            # 所有手柄都离线
            if self.j is not None:
                print("[轮询] 检测到手柄已断开")
                self.j.quit()
                self.j = None
                self.jid = None
                self.status = self.STATUS_DISCONNECTED 
        elif self.j is None:
            # 主动连接新的手柄（只选第一个，扩展多手柄可遍历 range(joystick_count)）
            try:
                joystick = pygame.joystick.Joystick(0)
                joystick.init()
                self.j = joystick
                self.jid = 0
                self.status = self.STATUS_RUNNING
                print(f"[轮询] 检测到手柄已连接 id=0, 名称={joystick.get_name()}")
            except Exception as e:
                print(f"保险新建手柄对象失败：{e}")
        
    @staticmethod
    def clip255(val):
        return max(0, min(int(val), 255))
    def get_packet(self):
        # 摇杆/扳机转换，同原版
        j = self.j
        lx = self.clip255((j.get_axis(0) + 1) * 128); lx=128 if abs(lx-128)<10 else lx
        ly = self.clip255((-j.get_axis(1) + 1) * 128); ly=128 if abs(ly-128)<10 else ly
        rx = self.clip255((j.get_axis(2) + 1) * 128); rx=128 if abs(rx-128)<10 else rx
        ry = self.clip255((-j.get_axis(3) + 1) * 128); ry=128 if abs(ry-128)<10 else ry
        lt = self.clip255((j.get_axis(4) + 1) * 128); lt=0 if abs(lt-0)<2 else lt
        rt = self.clip255((j.get_axis(5) + 1) * 128); rt=0 if abs(rt-0)<2 else rt
        hat = j.get_hat(0)
        #十字键
        dpad_left  = int(hat[0] == -1)
        dpad_right = int(hat[0] == +1)
        dpad_up    = int(hat[1] == +1)
        dpad_down  = int(hat[1] == -1)
        # 按键
        btn_a = int(j.get_button(0))
        btn_b = int(j.get_button(1))
        btn_x = int(j.get_button(2))
        btn_y = int(j.get_button(3))
        #左右肩键
        btn_lb = int(j.get_button(4))
        btn_rb = int(j.get_button(5))

        if self.style == "Xbox":
            pass  # 默认Xbox风格，无需转换
        elif self.style == "Switch":
            # Switch风格转换
            btn_a, btn_b = btn_b, btn_a
            btn_x, btn_y = btn_y, btn_x
        elif self.style == "PS":
            # PS风格转换
            btn_a, btn_b = btn_b, btn_a
            btn_x, btn_y = btn_y, btn_x     

        packet = bytes([
            lx, ly, rx, ry, lt, rt,
            dpad_up, dpad_down, dpad_left, dpad_right,
            btn_a, btn_b, btn_x, btn_y,btn_lb, btn_rb
        ])
        return packet, dict(
            lx=lx, ly=ly, rx=rx, ry=ry, lt=lt, rt=rt,
            dpad_up=dpad_up, dpad_down=dpad_down, dpad_left=dpad_left, dpad_right=dpad_right,
            btn_a=btn_a, btn_b=btn_b, btn_x=btn_x, btn_y=btn_y, btn_lb=btn_lb, btn_rb=btn_rb
        )

    def send_packet(self, packet):
        if self.paused:
            return
        for ip, port in self.ESP_ADDRESSES:
            try:
                self.sock.sendto(packet, (ip, port))
            except Exception as e:
                print(f"UDP发送异常 [{ip}:{port}]:", e)

    def run_loop(self):
        """
        持续检测手柄,连接则发送,断开则等待,状态由托盘可见.
        """
        limiter = FPSLimiter()
        try:
            while self.running:
                now = time.time()
                # 连接检测（1秒检查一次，节省资源）
                if now - self._last_poll_time > 1:
                    self._last_poll_time = now
                    self.check_connection()

                # 2. 数据采集与发包
                if self.status == self.STATUS_RUNNING and self.j is not None:
                    try:
                        pygame.event.pump()
                        
                        packet, info = self.get_packet()
                        self.send_packet(packet)
                        if (packet != self.last_packet):
                            self.last_packet = packet
                            print(
                                f"左摇杆:({info['lx']},{info['ly']}) 右摇杆:({info['rx']},{info['ry']}) "
                                f"LT:{info['lt']} RT:{info['rt']} | 十字键:(上:{info['dpad_up']} 下:{info['dpad_down']} 左:{info['dpad_left']} 右:{info['dpad_right']}) "
                                f"A:{info['btn_a']} B:{info['btn_b']} X:{info['btn_x']} Y:{info['btn_y']} LB:{info['btn_lb']} RB:{info['btn_rb']}"
                            )
                        self.frame_count += 1
                        nowf = time.time()
                        if nowf - self.last_fps_print >= 1.0:
                            self.fps = self.frame_count
                            print(f"FPS:{self.fps}")
                            self.frame_count = 0
                            self.last_fps_print = nowf

                        # self.clock.tick(self.FPS)  # 控制帧率
                        limiter.wait(self.FPS)              # 控制FPS
                    except pygame.error:
                        # 手柄突然断开
                        self.status = self.STATUS_DISCONNECTED
                        self.j = None
                else:
                    # 间隔等待，节省CPU
                        # 间隔等待，节省CPU
                    for _ in range(10):
                        if not self.running:
                            break
                        time.sleep(0.02)  # 每次只睡20ms，总200ms，但能及时响应退出
        except KeyboardInterrupt:
            print("程序已退出。")
            self.running = False
        finally:
            print("[JoystickUDPController] 清理开始")
            try: pygame.joystick.quit(); print("pygame.joystick.quit() ok")
            except Exception as e: print("joystick quit err:", e)
            try: pygame.quit(); print("pygame.quit() ok")
            except Exception as e: print("pygame quit err:", e)
            try: self.sock.close(); print("sock.close() ok")
            except Exception as e: print("sock close err:", e)
            print("[JoystickUDPController] 清理收尾结束，线程即将 return")
            os._exit(0)  # 强制退出所有线程，避免pygame线程卡住
    def start(self):
        """供线程启动"""
        self.run_loop()



        