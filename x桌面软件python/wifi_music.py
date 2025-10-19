
from tkinter import messagebox      # 界面弹窗（保留，但后面用托盘替代）
import threading                   # 线程支持（为托盘和主线程分离）
from tray import TrayApp            # 托盘管理类

from audio import MusicColorVisualizerNoGUI_UDP



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