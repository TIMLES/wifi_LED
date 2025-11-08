import pygame
import socket
import time
# ESP8266/ESP32 的IP和端口
ESP_IP = '192.168.137.51'
ESP_PORT = 4210

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
pygame.init()
pygame.joystick.init()
count = pygame.joystick.get_count()
if count == 0:
    print("未检测到手柄，请插入 Xbox 手柄或重试。")
    exit(1)
j = pygame.joystick.Joystick(0)
j.init()
print(f"已连接手柄: {j.get_name()}")

clock = pygame.time.Clock()
FPS = 30    # 期望帧率（每秒发送 60 次）

def clip255(val):
    return max(0, min(int(val), 255))

last_packet=None
frame_count = 0       # 计帧数
last_fps_print = time.time()
try:
    while True:
        pygame.event.pump()

        # 左摇杆
        lx = clip255((j.get_axis(0) + 1) * 128);lx=128 if abs(lx-128)<10 else lx
        ly = clip255((-j.get_axis(1) + 1) * 128);ly=128 if abs(ly-128)<10 else ly
        # 右摇杆
        rx = clip255((j.get_axis(2) + 1) * 128);rx=128 if abs(rx-128)<10 else rx
        ry = clip255((-j.get_axis(3) + 1) * 128);ry=128 if abs(ry-128)<10 else ry
        # 扳机
        lt = clip255((j.get_axis(4) + 1) * 128);lt=0 if abs(lt-0)<2 else lt
        rt = clip255((j.get_axis(5) + 1) * 128);rt=0 if abs(rt-0)<2 else rt
        # print(f"摇杆数据: lx={lx}, ly={ly}, rx={rx}, ry={ry}, lt={lt}, rt={rt}")


        # 十字键（hat），返回(x, y)，x左-1右+1，y上+1下-1
        hat = j.get_hat(0)
        dpad_left  = int(hat[0] == -1)
        dpad_right = int(hat[0] == +1)
        dpad_up    = int(hat[1] == +1)
        dpad_down  = int(hat[1] == -1)

        # 按钮
        btn_a = int(j.get_button(0))
        btn_b = int(j.get_button(1))
        btn_x = int(j.get_button(2))
        btn_y = int(j.get_button(3))

        # 总共 6 + 4 + 4 = 14 字节
        packet = bytes([
            lx, ly, rx, ry, lt, rt,
            dpad_up, dpad_down, dpad_left, dpad_right,
            btn_a, btn_b, btn_x, btn_y
        ])
        # 实时打印当前状态（你可以随便操作，马上看到变化）：
        if (packet!=last_packet) & True:  # 只在数据变化时打印，避免刷屏    
            last_packet=packet
            print(
                f"左摇杆:({lx},{ly}) 右摇杆:({rx},{ry}) "
                f"LT:{lt} RT:{rt} | 十字键:(上:{dpad_up} 下:{dpad_down} 左:{dpad_left} 右:{dpad_right}) "
                f"A:{btn_a} B:{btn_b} X:{btn_x} Y:{btn_y}"
            )

        sock.sendto(packet, (ESP_IP, ESP_PORT))
        frame_count += 1
        now = time.time()
        if now - last_fps_print >= 1.0:
            fps = frame_count
            print(f"当前FPS: {fps}")
            frame_count = 0
            last_fps_print = now
        clock.tick(FPS)
except KeyboardInterrupt:
    print("程序已退出。")
finally:
    pygame.joystick.quit()
    pygame.quit()
    sock.close()