import functools
import webbrowser
import pystray
from pystray import MenuItem as item
from PIL import Image

import sys, os

def get_resource_path(relpath):
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relpath)
    return os.path.join(os.path.dirname(__file__), relpath)


# ==== 托盘部分 ====
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
        self.icon.icon = create_color_icon()
        self.icon.title = "节奏灯运行中"
        self.icon.menu = self.build_menu()

    def build_menu(self):
        return pystray.Menu(
            item(lambda i: "⏸️  暂停" if not self.main_app.paused else "▶️   运行", self.toggle_pause),
            item(
                "🎶  风格选择",
                pystray.Menu(
                    item(
                        "动态",
                        functools.partial(self.set_style, "dynamic"),
                        checked=lambda i: self.main_app.style == "dynamic"
                    ),
                    item(
                        "全亮",
                        functools.partial(self.set_style, "full_on"),
                        checked=lambda i: self.main_app.style == "full_on"
                    )
                )
            ),
            item("ℹ️  关于", self.about),
            item("📴  退出", self.on_exit)
        )

    def set_style(self, style, icon, item):
        self.main_app.style = style
        self.icon.menu = self.build_menu()

    def toggle_pause(self, icon, item):
        self.main_app.paused = not self.main_app.paused
        self.icon.menu = self.build_menu()
        if self.main_app.paused:
            self.icon.icon = create_paused_icon()
        else:
            self.icon.icon = create_color_icon()
    def about(self, icon=None, item=None):
        webbrowser.open("http://139.196.234.229:1234")  # 这里换成你的实际URL


    def on_exit(self, icon, item):
        print("托盘退出事件，关闭主程序...")
        self.main_app.running = False
        icon.stop()

    def run(self):
        self.icon.run()