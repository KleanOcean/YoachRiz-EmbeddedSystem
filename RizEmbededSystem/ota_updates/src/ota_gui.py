#!/usr/bin/env python3
"""
OTA GUI 更新系统 - 主界面
用于固件编译、设备连接和OTA更新的图形界面应用
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import threading
import queue
import subprocess
import os
import sys
import json
import time
from datetime import datetime
from pathlib import Path

# 添加蓝牙模块路径
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from ble_manager import BLEManager
from firmware_compiler import FirmwareCompiler
from ota_uploader import OTAUploader

class OTAGUI:
    """OTA GUI主应用类"""

    def __init__(self, root):
        self.root = root
        self.root.title("Riz OTA 更新系统 v1.0.0")
        self.root.geometry("1000x700")

        # 初始化组件
        self.ble_manager = BLEManager()
        self.firmware_compiler = FirmwareCompiler()
        self.ota_uploader = OTAUploader()

        # 状态变量
        self.selected_device = None
        self.firmware_path = None
        self.is_connected = False
        self.update_in_progress = False

        # 消息队列（用于线程间通信）
        self.message_queue = queue.Queue()

        # 设置界面
        self.setup_ui()

        # 启动消息处理
        self.process_messages()

        # 自动开始扫描设备
        self.start_device_scan()

    def setup_ui(self):
        """设置用户界面"""

        # 创建主框架
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # 配置网格权重
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)

        # ===== 顶部工具栏 =====
        toolbar_frame = ttk.LabelFrame(main_frame, text="控制面板", padding="5")
        toolbar_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))

        # 编译按钮
        self.compile_btn = ttk.Button(
            toolbar_frame,
            text="📦 编译固件",
            command=self.compile_firmware,
            width=15
        )
        self.compile_btn.grid(row=0, column=0, padx=5)

        # 选择固件按钮
        self.select_fw_btn = ttk.Button(
            toolbar_frame,
            text="📁 选择固件",
            command=self.select_firmware,
            width=15
        )
        self.select_fw_btn.grid(row=0, column=1, padx=5)

        # 扫描设备按钮
        self.scan_btn = ttk.Button(
            toolbar_frame,
            text="🔍 扫描设备",
            command=self.start_device_scan,
            width=15
        )
        self.scan_btn.grid(row=0, column=2, padx=5)

        # 连接按钮
        self.connect_btn = ttk.Button(
            toolbar_frame,
            text="🔗 连接设备",
            command=self.connect_device,
            width=15,
            state=tk.DISABLED
        )
        self.connect_btn.grid(row=0, column=3, padx=5)

        # 断开连接按钮
        self.disconnect_btn = ttk.Button(
            toolbar_frame,
            text="🔌 断开连接",
            command=self.disconnect_device,
            width=15,
            state=tk.DISABLED
        )
        self.disconnect_btn.grid(row=0, column=4, padx=5)

        # 测试按钮
        self.test_btn = ttk.Button(
            toolbar_frame,
            text="🧪 发送测试",
            command=self.send_test_signal,
            width=15,
            state=tk.DISABLED
        )
        self.test_btn.grid(row=0, column=5, padx=5)

        # 更新按钮
        self.update_btn = ttk.Button(
            toolbar_frame,
            text="🚀 开始更新",
            command=self.start_ota_update,
            width=15,
            state=tk.DISABLED
        )
        self.update_btn.grid(row=0, column=6, padx=5)

        # ===== 左侧设备列表 =====
        device_frame = ttk.LabelFrame(main_frame, text="设备列表", padding="5")
        device_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 5))

        # 设备树形视图
        self.device_tree = ttk.Treeview(
            device_frame,
            columns=("name", "rssi", "status"),
            show="tree headings",
            height=15,
            selectmode='browse'  # 单选模式
        )
        self.device_tree.heading("#0", text="MAC地址")
        self.device_tree.heading("name", text="设备名称")
        self.device_tree.heading("rssi", text="信号强度")
        self.device_tree.heading("status", text="连接状态")

        self.device_tree.column("#0", width=150)
        self.device_tree.column("name", width=100)
        self.device_tree.column("rssi", width=80)
        self.device_tree.column("status", width=80)

        # 配置标签样式
        self.device_tree.tag_configure('connected', background='#90EE90')  # 浅绿色
        self.device_tree.tag_configure('selected', background='#ADD8E6')   # 浅蓝色
        self.device_tree.tag_configure('normal', background='white')

        self.device_tree.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # 设备列表滚动条
        device_scroll = ttk.Scrollbar(device_frame, orient=tk.VERTICAL, command=self.device_tree.yview)
        device_scroll.grid(row=0, column=1, sticky=(tk.N, tk.S))
        self.device_tree.configure(yscrollcommand=device_scroll.set)

        # 绑定选择事件
        self.device_tree.bind("<<TreeviewSelect>>", self.on_device_select)

        # ===== 右侧信息面板 =====
        info_frame = ttk.LabelFrame(main_frame, text="设备信息", padding="5")
        info_frame.grid(row=1, column=1, sticky=(tk.W, tk.E, tk.N, tk.S))

        # 设备信息标签
        info_labels = [
            ("设备名称:", "device_name"),
            ("MAC地址:", "device_mac"),
            ("固件版本:", "firmware_version"),
            ("连接状态:", "connection_status"),
            ("固件路径:", "firmware_path"),
            ("固件大小:", "firmware_size")
        ]

        self.info_vars = {}
        for i, (label, var_name) in enumerate(info_labels):
            ttk.Label(info_frame, text=label).grid(row=i, column=0, sticky=tk.W, pady=2)
            self.info_vars[var_name] = tk.StringVar(value="--")
            ttk.Label(info_frame, textvariable=self.info_vars[var_name]).grid(
                row=i, column=1, sticky=tk.W, padx=(10, 0), pady=2
            )

        # ===== 底部日志和进度 =====
        bottom_frame = ttk.Frame(main_frame)
        bottom_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(10, 0))
        bottom_frame.columnconfigure(0, weight=1)
        bottom_frame.rowconfigure(0, weight=1)

        # 日志框架
        log_frame = ttk.LabelFrame(bottom_frame, text="操作日志", padding="5")
        log_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        # 日志文本框
        self.log_text = scrolledtext.ScrolledText(
            log_frame,
            height=10,
            wrap=tk.WORD,
            font=("Consolas", 9)
        )
        self.log_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # 配置日志颜色标签
        self.log_text.tag_config("INFO", foreground="black")
        self.log_text.tag_config("SUCCESS", foreground="green")
        self.log_text.tag_config("WARNING", foreground="orange")
        self.log_text.tag_config("ERROR", foreground="red")

        # 进度条框架
        progress_frame = ttk.Frame(bottom_frame)
        progress_frame.grid(row=1, column=0, sticky=(tk.W, tk.E), pady=(10, 0))
        progress_frame.columnconfigure(1, weight=1)

        # 进度标签
        self.progress_label = ttk.Label(progress_frame, text="就绪")
        self.progress_label.grid(row=0, column=0, padx=(0, 10))

        # 进度条
        self.progress_bar = ttk.Progressbar(
            progress_frame,
            mode='determinate',
            maximum=100
        )
        self.progress_bar.grid(row=0, column=1, sticky=(tk.W, tk.E))

        # 进度百分比
        self.progress_percent = ttk.Label(progress_frame, text="0%")
        self.progress_percent.grid(row=0, column=2, padx=(10, 0))

        # 状态栏
        self.status_bar = ttk.Label(
            self.root,
            text="准备就绪",
            relief=tk.SUNKEN,
            anchor=tk.W
        )
        self.status_bar.grid(row=1, column=0, sticky=(tk.W, tk.E))

    def log(self, message, level="INFO"):
        """写入日志"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        log_entry = f"[{timestamp}] {message}\n"

        # 添加到消息队列（线程安全）
        self.message_queue.put(("log", log_entry, level))

    def process_messages(self):
        """处理消息队列（在主线程中）"""
        try:
            while True:
                msg_type, *args = self.message_queue.get_nowait()

                if msg_type == "log":
                    log_entry, level = args
                    self.log_text.insert(tk.END, log_entry, level)
                    self.log_text.see(tk.END)

                elif msg_type == "progress":
                    value, label = args
                    self.progress_bar["value"] = value
                    self.progress_percent.config(text=f"{value}%")
                    if label:
                        self.progress_label.config(text=label)

                elif msg_type == "status":
                    self.status_bar.config(text=args[0])

                elif msg_type == "devices":
                    self.update_device_list(args[0])

        except queue.Empty:
            pass

        # 继续处理消息
        self.root.after(100, self.process_messages)

    def start_device_scan(self):
        """开始扫描设备"""
        self.log("开始扫描BLE设备...")
        self.scan_btn.config(state=tk.DISABLED)

        def scan_thread():
            devices = self.ble_manager.scan_devices()
            self.message_queue.put(("devices", devices))
            self.message_queue.put(("log", f"发现 {len(devices)} 个设备", "SUCCESS"))
            self.scan_btn.config(state=tk.NORMAL)

        threading.Thread(target=scan_thread, daemon=True).start()

    def update_device_list(self, devices):
        """更新设备列表显示"""
        # 保存当前连接的设备
        connected_address = self.selected_device['address'] if self.is_connected and self.selected_device else None

        # 清空现有列表
        for item in self.device_tree.get_children():
            self.device_tree.delete(item)

        # 添加新设备
        for device in devices:
            if device['name'].startswith('PRO-'):
                status = "已连接" if connected_address == device['address'] else "未连接"
                tag = 'connected' if connected_address == device['address'] else 'normal'

                self.device_tree.insert(
                    "",
                    "end",
                    text=device['address'],
                    values=(device['name'], f"{device['rssi']} dBm", status),
                    tags=(tag,)
                )

    def on_device_select(self, event):
        """处理设备选择事件"""
        selection = self.device_tree.selection()
        if selection:
            item = self.device_tree.item(selection[0])
            self.selected_device = {
                'address': item['text'],
                'name': item['values'][0]
            }

            # 更新信息显示
            self.info_vars['device_name'].set(self.selected_device['name'])
            self.info_vars['device_mac'].set(self.selected_device['address'])

            # 启用连接按钮
            self.connect_btn.config(state=tk.NORMAL)

            self.log(f"选中设备: {self.selected_device['name']}")

    def connect_device(self):
        """连接到选中的设备"""
        if not self.selected_device:
            return

        self.log(f"正在连接到 {self.selected_device['name']}...")
        self.connect_btn.config(state=tk.DISABLED)

        def connect_thread():
            success = self.ble_manager.connect(self.selected_device['address'])

            if success:
                self.is_connected = True
                self.message_queue.put(("log", "连接成功", "SUCCESS"))
                self.info_vars['connection_status'].set("已连接")

                # 启用断开按钮，禁用连接按钮
                self.disconnect_btn.config(state=tk.NORMAL)
                self.connect_btn.config(state=tk.DISABLED)

                # 启用测试和更新按钮
                self.test_btn.config(state=tk.NORMAL)
                if self.firmware_path:
                    self.update_btn.config(state=tk.NORMAL)
            else:
                self.message_queue.put(("log", "连接失败", "ERROR"))
                self.connect_btn.config(state=tk.NORMAL)

        threading.Thread(target=connect_thread, daemon=True).start()

    def disconnect_device(self):
        """断开设备连接"""
        if not self.is_connected:
            return

        self.log(f"正在断开与 {self.selected_device['name']} 的连接...")
        self.disconnect_btn.config(state=tk.DISABLED)

        def disconnect_thread():
            success = self.ble_manager.disconnect()

            if success:
                self.is_connected = False
                self.message_queue.put(("log", "已断开连接", "SUCCESS"))
                self.info_vars['connection_status'].set("未连接")

                # 禁用测试和更新按钮
                self.test_btn.config(state=tk.DISABLED)
                self.update_btn.config(state=tk.DISABLED)

                # 启用连接按钮，禁用断开按钮
                self.connect_btn.config(state=tk.NORMAL)
                self.disconnect_btn.config(state=tk.DISABLED)

                # 更新设备列表显示
                self.update_device_list(self.ble_manager.scan_devices())
            else:
                self.message_queue.put(("log", "断开连接失败", "ERROR"))
                self.disconnect_btn.config(state=tk.NORMAL)

        threading.Thread(target=disconnect_thread, daemon=True).start()

    def compile_firmware(self):
        """编译固件"""
        self.log("开始编译固件...")
        self.compile_btn.config(state=tk.DISABLED)
        self.message_queue.put(("progress", 0, "编译中..."))

        def compile_thread():
            try:
                # 调用编译器
                result = self.firmware_compiler.compile(
                    progress_callback=lambda p: self.message_queue.put(("progress", p, None))
                )

                if result['success']:
                    self.firmware_path = result['output_path']
                    self.message_queue.put(("log", f"编译成功: {self.firmware_path}", "SUCCESS"))

                    # 如果有归档文件，优先使用归档文件
                    if result.get('archived_path'):
                        archived_path = result['archived_path']
                        self.message_queue.put(("log", f"固件已归档: {os.path.basename(archived_path)}", "SUCCESS"))

                        # 显示归档文件信息
                        size_kb = os.path.getsize(archived_path) / 1024
                        self.info_vars['firmware_path'].set(os.path.basename(archived_path))
                        self.info_vars['firmware_size'].set(f"{size_kb:.1f} KB")

                        # 使用归档文件作为固件路径
                        self.firmware_path = archived_path
                    else:
                        # 更新固件信息
                        size_kb = os.path.getsize(self.firmware_path) / 1024
                        self.info_vars['firmware_path'].set(os.path.basename(self.firmware_path))
                        self.info_vars['firmware_size'].set(f"{size_kb:.1f} KB")

                    # 如果已连接，启用更新按钮
                    if self.is_connected:
                        self.update_btn.config(state=tk.NORMAL)
                else:
                    self.message_queue.put(("log", f"编译失败: {result['error']}", "ERROR"))

            except Exception as e:
                self.message_queue.put(("log", f"编译异常: {str(e)}", "ERROR"))

            finally:
                self.compile_btn.config(state=tk.NORMAL)
                self.message_queue.put(("progress", 100, "就绪"))

        threading.Thread(target=compile_thread, daemon=True).start()

    def select_firmware(self):
        """选择固件文件"""
        # 默认打开固件归档文件夹
        initial_dir = self.firmware_compiler.firmware_archive_dir

        # 如果有最新的固件，打开其所在文件夹
        latest_firmware = self.firmware_compiler.get_latest_firmware()
        if latest_firmware:
            initial_dir = os.path.dirname(latest_firmware)

        filename = filedialog.askopenfilename(
            title="选择固件文件",
            initialdir=initial_dir,
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )

        if filename:
            self.firmware_path = filename
            size_kb = os.path.getsize(filename) / 1024

            self.info_vars['firmware_path'].set(os.path.basename(filename))
            self.info_vars['firmware_size'].set(f"{size_kb:.1f} KB")

            self.log(f"选择固件: {os.path.basename(filename)}")

            # 如果已连接，启用更新按钮
            if self.is_connected:
                self.update_btn.config(state=tk.NORMAL)

    def send_test_signal(self):
        """发送测试信号"""
        self.log("发送测试信号...")

        def test_thread():
            # 发送开启模式命令让LED闪烁
            success = self.ble_manager.send_command("11,999,999,999,999,999,999,999")

            if success:
                self.message_queue.put(("log", "测试信号发送成功，设备LED应该闪烁", "SUCCESS"))
            else:
                self.message_queue.put(("log", "测试信号发送失败", "ERROR"))

        threading.Thread(target=test_thread, daemon=True).start()

    def start_ota_update(self):
        """开始OTA更新"""
        if not self.firmware_path or not self.is_connected:
            return

        # 确认对话框
        if not messagebox.askyesno(
            "确认更新",
            f"确定要将固件更新到设备 {self.selected_device['name']} 吗？\n\n"
            f"固件文件: {os.path.basename(self.firmware_path)}\n"
            f"警告: 更新过程中请勿断开连接！"
        ):
            return

        self.log("开始OTA更新...")
        self.update_btn.config(state=tk.DISABLED)
        self.update_in_progress = True
        self.message_queue.put(("progress", 0, "准备更新..."))

        def update_thread():
            try:
                # 执行OTA更新
                result = self.ota_uploader.upload(
                    self.ble_manager,
                    self.firmware_path,
                    progress_callback=lambda p, msg: self.message_queue.put(("progress", p, msg))
                )

                if result['success']:
                    self.message_queue.put(("log", "OTA更新成功！设备将重启。", "SUCCESS"))
                    messagebox.showinfo("更新成功", "固件更新成功！\n设备将自动重启。")
                else:
                    self.message_queue.put(("log", f"OTA更新失败: {result['error']}", "ERROR"))
                    messagebox.showerror("更新失败", f"固件更新失败:\n{result['error']}")

            except Exception as e:
                self.message_queue.put(("log", f"OTA更新异常: {str(e)}", "ERROR"))
                messagebox.showerror("更新异常", f"更新过程出现异常:\n{str(e)}")

            finally:
                self.update_in_progress = False
                self.update_btn.config(state=tk.NORMAL)
                self.message_queue.put(("progress", 100, "就绪"))

        threading.Thread(target=update_thread, daemon=True).start()

def main():
    """主函数"""
    root = tk.Tk()
    app = OTAGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()