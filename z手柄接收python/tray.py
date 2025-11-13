# tray.py
import functools
import pystray
from pystray import MenuItem as item
from PIL import Image
import sys, os
import webbrowser
import time
import threading
from pathlib import Path
FPS_OPTIONS = [30, 60, 90, 180, 240, 600]
def get_resource_path(relpath):
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relpath)
    return os.path.join(os.path.dirname(__file__), relpath)


def icon_for_tray(path, size=(64,64)):
    im = Image.open(path).convert("RGBA")
    # 按内容比例缩放到最长边=size
    ratio = min(size[0]/im.width, size[1]/im.height)
    new_size = (int(im.width*ratio), int(im.height*ratio))
    im_resized = im.resize(new_size, Image.LANCZOS)
    # 新建正方形底，透明
    bg = Image.new("RGBA", size, (0,0,0,0))
    bg.paste(im_resized, ((size[0]-new_size[0])//2, (size[1]-new_size[1])//2), im_resized)
    return bg

def icon_running():
    return icon_for_tray(get_resource_path("resource/Xbox_on.png"))


def icon_not_connected():
    return icon_for_tray(get_resource_path("resource/Xbox_None.png"))


def icon_disconnected():
    return icon_for_tray(get_resource_path("resource/Xbox_off.png"))


class TrayApp:
    def __init__(self, main_app):
        self.main_app = main_app
        self.icon = pystray.Icon("JoystickSender")
        self.update_icon()
        self.icon.title = "手柄UDP状态"
        self.icon.menu = self.build_menu()

    def update_icon(self):
        status = self.main_app.status
        if status == "running":
            self.icon.icon = icon_running()
            self.icon.title = f"手柄已连接"
        elif status == "not_connected":
            self.icon.icon = icon_not_connected()
            self.icon.title = "未检测到手柄"
        elif status == "disconnected":
            self.icon.icon = icon_disconnected()
            self.icon.title = "已断开手柄"
        else:
            self.icon.icon = icon_not_connected()
            self.icon.title = "未知状态"

    def build_menu(self):
        def get_connect_label(icon):  # 动态显示
            if self.main_app.status == "running":
                return f"🟢 运行中 - {self.main_app.FPS}FPS"
            elif self.main_app.status == "not_connected":
                return "⚪ 未连接"
            elif self.main_app.status == "disconnected":
                return "🔶 已断开"
            return "❓ 未知"

        style_menu = pystray.Menu(
            *(item(
                s,
                functools.partial(self.set_style, s),
                checked=(lambda i, s2=s: self.main_app.style == s2)
            ) for s in self.main_app.STYLES)
        )
        # 构建FPS子菜单
        fps_menu = pystray.Menu(
            *(item(
                f"{fps} FPS",
                functools.partial(self.set_fps, fps),
                checked=(lambda icon, fps2=fps: self.main_app.FPS == fps2)
            ) for fps in FPS_OPTIONS)
        )
        return pystray.Menu(
            item(get_connect_label, lambda *args: None),  # 状态显示
            item("🎮 手柄风格", style_menu),
            item("🎞️ 帧率(FPS)", fps_menu),
            item("ℹ️ 在线调试", self.about),
            item("📴 退出", self.on_exit)
        )

    def set_style(self, style, icon, item):
        self.main_app.style = style
        self.icon.menu = self.build_menu()

    def set_fps(self, fps, icon, item):
        print(f"托盘选择FPS: {fps}")
        self.main_app.FPS = fps
        # 重新构建菜单，切换选中项
        self.icon.menu = self.build_menu()

    def about(self, icon=None, item=None):
        path = get_resource_path("resource/display_Xbox.html")
        url = Path(path).absolute().as_uri()
        webbrowser.open(url)

    def on_exit(self, icon, item):
        print("托盘退出事件，关闭主程序...")
        self.main_app.running = False
        icon.stop()

    def run(self):
        def refresh():
            last_status = None
            last_style = None
            while self.main_app.running:
                # 只有状态/风格有变化才刷新menu，否则只刷新icon
                if self.main_app.status != last_status or self.main_app.style != last_style:
                    self.update_icon()
                    self.icon.menu = self.build_menu()
                    last_status = self.main_app.status
                    last_style = self.main_app.style
                else:
                    self.update_icon()  # 通常这里只刷新icon，极少用menu
                time.sleep(1)
        t = threading.Thread(target=refresh, daemon=True)
        t.start()
        self.icon.run()