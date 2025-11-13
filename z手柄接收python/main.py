# main.py
from tkinter import messagebox      # 界面弹窗（保留，但后面用托盘替代）
import threading
from tray import TrayApp
from joystick_sender import JoystickUDPController
import pystray
import time
from ws_udp_server import run_ws_udp_server  # 导入你的封装函数
# main.py
if __name__ == '__main__':
    ESP_ADDRESSES = [
        ('192.168.137.51', 4210),
        ('127.0.0.1', 4210),      # 再加一个，举例
    ]
    FPS = 60
    try:
        messagebox.showinfo("提示", "手柄程序已在后台运行。")
    except Exception as e:
        print("弹窗异常:", e)
    print("[main] 初始化 JoystickUDPController")
    main_app = JoystickUDPController(ESP_ADDRESSES, FPS)
    t_worker = threading.Thread(target=main_app.start)
    t_worker.daemon = True
    print("[main] 启动 worker 线程")
    t_worker.start()

    # 启动后端服务（建议加停止控制）
    backend_stop_event = threading.Event()
    t_backend = threading.Thread(target=run_ws_udp_server, args=(backend_stop_event,), daemon=True)
    print("[main] 启动后端服务线程")
    t_backend.start()

    print("[main] 初始化 TrayApp")
    tray = TrayApp(main_app)

    # 如果需要退出时关掉后端服务
    try:
        tray.run()
    finally:
        print("[main] TrayApp 退出，准备关闭后端服务")
        backend_stop_event.set()
        t_backend.join(timeout=5)