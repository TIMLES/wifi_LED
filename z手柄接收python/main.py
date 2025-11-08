import pygame
import socket
import time

# ESP8266/ESP32 的IP和端口（注意IP和硬件实际一致）
ESP_IP = '192.168.137.198'
ESP_PORT = 4210

# 准备UDP通信
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# 初始化pygame和手柄
pygame.init()
pygame.joystick.init()

count = pygame.joystick.get_count()
if count == 0:
    print("未检测到手柄，请插入 Xbox 手柄或重试。")
    exit(1)

# 支持多手柄，第一个为主
j = pygame.joystick.Joystick(0)
j.init()
print(f"已连接手柄: {j.get_name()}")

clock = pygame.time.Clock()
FPS = 30    # 期望帧率（每秒发送 60 次）
last_x, last_y = None, None
try:
    while True:
        pygame.event.pump()
        # 读取左摇杆
        x = int(-j.get_axis(0) * 5+5)
        y = int(-j.get_axis(1) * 5+5)
        if 1:
            print(f"发送指令：x={x}, y={y}")
            last_x, last_y = x, y
            
        msg = f"{x},{y}".encode()
        sock.sendto(msg, (ESP_IP, ESP_PORT))
        # 控制循环速率
        clock.tick(FPS)   # 保证每秒循环 FPS 次（恒定帧率）
        
except KeyboardInterrupt:
    print("程序已退出。")
finally:
    pygame.joystick.quit()
    pygame.quit()
    sock.close()