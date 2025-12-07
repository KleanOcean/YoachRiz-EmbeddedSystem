#!/usr/bin/env python3
"""
固件编译器 - 调用PlatformIO编译固件
"""

import os
import subprocess
import json
import shutil
import hashlib
from pathlib import Path
from datetime import datetime
from typing import Dict, Optional, Callable

class FirmwareCompiler:
    """固件编译器类"""

    def __init__(self):
        # 获取项目根目录（EmbededSystem文件夹）
        self.project_root = Path(__file__).parent.parent.parent
        self.platformio_ini = self.project_root / "platformio.ini"
        self.build_dir = self.project_root / ".pio" / "build"

        # 创建固件存档文件夹
        self.firmware_archive_dir = self.project_root / "ota_updates" / "firmware_archive"
        self.firmware_archive_dir.mkdir(exist_ok=True, parents=True)

        # 检查PlatformIO是否安装
        self.pio_executable = self._find_platformio()

    def _find_platformio(self) -> Optional[str]:
        """
        查找PlatformIO可执行文件

        Returns:
            PlatformIO可执行文件路径，如果未找到返回None
        """
        # 尝试不同的可能路径
        possible_commands = ['pio', 'platformio', '~/.platformio/penv/bin/pio']

        for cmd in possible_commands:
            expanded_cmd = os.path.expanduser(cmd)
            if shutil.which(expanded_cmd):
                return expanded_cmd

        # 检查是否在PATH中
        try:
            result = subprocess.run(
                ['pio', '--version'],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                return 'pio'
        except:
            pass

        print("警告: 未找到PlatformIO，将使用模拟编译")
        return None

    def check_environment(self) -> Dict:
        """
        检查编译环境

        Returns:
            环境检查结果
        """
        result = {
            'platformio_found': self.pio_executable is not None,
            'project_found': self.platformio_ini.exists(),
            'environments': []
        }

        if result['platformio_found'] and result['project_found']:
            # 读取可用的编译环境
            try:
                with open(self.platformio_ini, 'r') as f:
                    content = f.read()
                    # 简单解析环境名称
                    import re
                    envs = re.findall(r'\[env:(\w+)\]', content)
                    result['environments'] = envs
            except Exception as e:
                print(f"读取platformio.ini失败: {e}")

        return result

    def compile(
        self,
        environment: str = "esp32doit-devkit-v1",
        progress_callback: Optional[Callable[[int], None]] = None
    ) -> Dict:
        """
        编译固件

        Args:
            environment: 编译环境名称
            progress_callback: 进度回调函数

        Returns:
            编译结果，包含 success, output_path, error
        """
        result = {
            'success': False,
            'output_path': None,
            'error': None
        }

        # 检查环境
        env_check = self.check_environment()

        if not env_check['platformio_found']:
            # 模拟编译
            return self._mock_compile(progress_callback)

        if not env_check['project_found']:
            result['error'] = f"未找到项目文件: {self.platformio_ini}"
            return result

        # 执行编译
        try:
            # 清理之前的构建
            if progress_callback:
                progress_callback(10)

            clean_cmd = [self.pio_executable, 'run', '--target', 'clean', '-e', environment]
            subprocess.run(
                clean_cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=30
            )

            if progress_callback:
                progress_callback(20)

            # 编译固件
            compile_cmd = [self.pio_executable, 'run', '-e', environment]

            print(f"执行编译命令: {' '.join(compile_cmd)}")
            print(f"工作目录: {self.project_root}")

            # 使用Popen以获取实时输出
            process = subprocess.Popen(
                compile_cmd,
                cwd=self.project_root,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )

            # 读取输出并更新进度
            output_lines = []
            progress = 20

            for line in process.stdout:
                output_lines.append(line)
                print(line.strip())  # 打印到控制台

                # 根据输出更新进度
                if "Compiling" in line:
                    progress = min(progress + 2, 80)
                    if progress_callback:
                        progress_callback(progress)
                elif "Linking" in line:
                    if progress_callback:
                        progress_callback(85)
                elif "Building" in line:
                    if progress_callback:
                        progress_callback(90)

            # 等待进程结束
            process.wait(timeout=120)

            if process.returncode == 0:
                # 查找生成的固件文件
                firmware_path = self.build_dir / environment / "firmware.bin"

                if firmware_path.exists():
                    # 保存带时间戳的固件副本
                    archived_path = self._archive_firmware(firmware_path)

                    result['success'] = True
                    result['output_path'] = str(firmware_path)
                    result['archived_path'] = str(archived_path) if archived_path else None

                    if progress_callback:
                        progress_callback(100)

                    print(f"编译成功: {firmware_path}")
                    if archived_path:
                        print(f"固件已归档: {archived_path}")
                else:
                    result['error'] = "编译完成但未找到固件文件"
            else:
                result['error'] = f"编译失败，返回码: {process.returncode}"

        except subprocess.TimeoutExpired:
            result['error'] = "编译超时"
        except Exception as e:
            result['error'] = f"编译异常: {str(e)}"

        return result

    def _mock_compile(self, progress_callback: Optional[Callable[[int], None]] = None) -> Dict:
        """
        模拟编译（用于测试）

        Args:
            progress_callback: 进度回调函数

        Returns:
            模拟的编译结果
        """
        import time

        # 模拟编译进度
        for progress in [10, 30, 50, 70, 90, 100]:
            if progress_callback:
                progress_callback(progress)
            time.sleep(0.5)

        # 创建模拟固件文件
        mock_firmware = self.project_root / "ota_updates" / "test" / "mock_firmware.bin"
        mock_firmware.parent.mkdir(exist_ok=True, parents=True)

        # 写入一些模拟数据
        with open(mock_firmware, 'wb') as f:
            f.write(b'MOCK_FIRMWARE_v0.0.1' + b'\x00' * 1024 * 100)  # 100KB

        return {
            'success': True,
            'output_path': str(mock_firmware),
            'error': None
        }

    def _calculate_hash(self, file_path: Path, algorithm: str = 'md5') -> str:
        """
        计算文件哈希值

        Args:
            file_path: 文件路径
            algorithm: 哈希算法 ('md5' 或 'sha256')

        Returns:
            哈希值的十六进制字符串
        """
        if algorithm == 'sha256':
            hash_obj = hashlib.sha256()
        else:
            hash_obj = hashlib.md5()

        try:
            with open(file_path, 'rb') as f:
                # 分块读取以处理大文件
                for chunk in iter(lambda: f.read(4096), b''):
                    hash_obj.update(chunk)
            return hash_obj.hexdigest()
        except Exception as e:
            print(f"计算哈希失败: {e}")
            return ""

    def _archive_firmware(self, firmware_path: Path) -> Optional[Path]:
        """
        归档固件文件，添加时间戳和哈希值

        Args:
            firmware_path: 原始固件文件路径

        Returns:
            归档后的文件路径，如果失败返回None
        """
        try:
            # 计算文件哈希（取前8位）
            file_hash = self._calculate_hash(firmware_path, 'md5')[:8]

            # 生成时间戳
            now = datetime.now()
            date_str = now.strftime("%m%d")  # 月日，如1123
            time_str = now.strftime("%H%M")  # 时分，如1430

            # 创建今天的子文件夹
            today_folder = self.firmware_archive_dir / now.strftime("%Y%m%d")
            today_folder.mkdir(exist_ok=True)

            # 生成新文件名：firmware_[hash]_[date]_[time].bin
            # 例如：firmware_a1b2c3d4_1123_1430.bin
            new_filename = f"firmware_{file_hash}_{date_str}_{time_str}.bin"
            archived_path = today_folder / new_filename

            # 复制文件
            shutil.copy2(firmware_path, archived_path)

            # 创建信息文件
            info_file = archived_path.with_suffix('.json')
            info = {
                'original_path': str(firmware_path),
                'archived_path': str(archived_path),
                'file_hash_md5': self._calculate_hash(firmware_path, 'md5'),
                'file_hash_sha256': self._calculate_hash(firmware_path, 'sha256'),
                'file_size': firmware_path.stat().st_size,
                'compile_time': now.isoformat(),
                'timestamp': now.timestamp()
            }

            with open(info_file, 'w') as f:
                json.dump(info, f, indent=2, ensure_ascii=False)

            print(f"固件归档完成: {new_filename}")
            return archived_path

        except Exception as e:
            print(f"归档固件失败: {e}")
            return None

    def get_archived_firmwares(self) -> list:
        """
        获取所有归档的固件文件列表

        Returns:
            固件文件信息列表
        """
        firmwares = []

        try:
            # 遍历所有日期文件夹
            for date_folder in sorted(self.firmware_archive_dir.iterdir(), reverse=True):
                if date_folder.is_dir():
                    # 遍历文件夹中的固件文件
                    for firmware_file in sorted(date_folder.glob("*.bin"), reverse=True):
                        # 尝试读取对应的信息文件
                        info_file = firmware_file.with_suffix('.json')

                        if info_file.exists():
                            with open(info_file, 'r') as f:
                                info = json.load(f)
                        else:
                            # 如果没有信息文件，生成基本信息
                            stat = firmware_file.stat()
                            info = {
                                'archived_path': str(firmware_file),
                                'file_size': stat.st_size,
                                'compile_time': datetime.fromtimestamp(stat.st_mtime).isoformat()
                            }

                        # 添加文件名和日期文件夹
                        info['filename'] = firmware_file.name
                        info['date_folder'] = date_folder.name

                        firmwares.append(info)

        except Exception as e:
            print(f"获取归档固件列表失败: {e}")

        return firmwares

    def get_latest_firmware(self) -> Optional[str]:
        """
        获取最新的归档固件文件路径

        Returns:
            最新固件文件路径，如果没有则返回None
        """
        firmwares = self.get_archived_firmwares()
        if firmwares:
            return firmwares[0].get('archived_path')
        return None

    def get_firmware_info(self, firmware_path: str) -> Dict:
        """
        获取固件信息

        Args:
            firmware_path: 固件文件路径

        Returns:
            固件信息，包含大小、修改时间等
        """
        info = {
            'path': firmware_path,
            'size': 0,
            'size_human': '0 B',
            'modified': None,
            'version': 'Unknown'
        }

        try:
            path = Path(firmware_path)
            if path.exists():
                stat = path.stat()
                info['size'] = stat.st_size
                info['modified'] = stat.st_mtime

                # 人类可读的大小
                size = stat.st_size
                for unit in ['B', 'KB', 'MB']:
                    if size < 1024:
                        info['size_human'] = f"{size:.1f} {unit}"
                        break
                    size /= 1024

                # 尝试从文件名或内容提取版本
                # 这里可以根据实际固件格式解析版本信息

        except Exception as e:
            print(f"获取固件信息失败: {e}")

        return info

# 测试代码
if __name__ == "__main__":
    compiler = FirmwareCompiler()

    print("检查编译环境...")
    env = compiler.check_environment()
    print(f"PlatformIO: {'已安装' if env['platformio_found'] else '未安装'}")
    print(f"项目文件: {'找到' if env['project_found'] else '未找到'}")
    print(f"可用环境: {env['environments']}")

    print("\n开始编译...")

    def progress_callback(percent):
        print(f"进度: {percent}%")

    result = compiler.compile(progress_callback=progress_callback)

    if result['success']:
        print(f"\n编译成功!")
        print(f"固件路径: {result['output_path']}")

        info = compiler.get_firmware_info(result['output_path'])
        print(f"固件大小: {info['size_human']}")
    else:
        print(f"\n编译失败: {result['error']}")