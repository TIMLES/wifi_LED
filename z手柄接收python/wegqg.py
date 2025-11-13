import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk
import os

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

class IconConverterApp:
    def __init__(self, master):
        self.master = master
        master.title("PNG 转 ICO 工具")
        master.geometry("320x420")
        self.png_path = None
        self.img_preview = None

        self.label = tk.Label(master, text="选择 PNG 文件", font=("微软雅黑", 14))
        self.label.pack(pady=12)

        self.preview_label = tk.Label(master, text="预览", font=("微软雅黑", 11))
        self.preview_label.pack()

        self.canvas = tk.Canvas(master, width=80, height=80, bg='#dddddd')
        self.canvas.pack(padx=10, pady=10)

        self.choose_btn = tk.Button(master, text="浏览 PNG 文件", command=self.choose_png)
        self.choose_btn.pack(pady=6)
        self.ico_size_entry = tk.Entry(master)
        self.ico_size_entry.insert(0, "64")  # 默认64
        tk.Label(master, text="ICO尺寸(px):").pack()
        self.ico_size_entry.pack(pady=2)
        self.save_btn = tk.Button(master, text="保存为 ICO", command=self.save_ico, state=tk.DISABLED)
        self.save_btn.pack(pady=14)

    def choose_png(self):
        ftypes = [("PNG图片", "*.png"), ("所有文件", "*.*")]
        path = filedialog.askopenfilename(title="选择PNG文件", filetypes=ftypes)
        if path:
            try:
                size = int(self.ico_size_entry.get())
            except:
                size = 64
            img = icon_for_tray(path, (size,size))
            self.img_preview = ImageTk.PhotoImage(img.resize((80, 80), Image.LANCZOS))
            self.canvas.delete("all")
            self.canvas.create_image(40, 40, image=self.img_preview)
            self.png_path = path
            self.save_btn.config(state=tk.NORMAL)
        else:
            self.save_btn.config(state=tk.DISABLED)

    def save_ico(self):
        if not self.png_path:
            messagebox.showwarning("未选择PNG", "请先选择PNG文件")
            return
        try:
            size = int(self.ico_size_entry.get())
        except:
            size = 64
        ico_path = filedialog.asksaveasfilename(
            title="保存ICO文件",
            defaultextension=".ico",
            filetypes=[("ICO图标", "*.ico")])
        if ico_path:
            img = icon_for_tray(self.png_path, (size,size))
            img.save(ico_path, format="ICO")
            messagebox.showinfo("保存成功", f"ICO已保存为\n{ico_path}")

if __name__ == "__main__":
    root = tk.Tk()
    app = IconConverterApp(root)
    root.mainloop()