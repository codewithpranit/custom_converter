import subprocess
import os
import sys
import tempfile
import shutil
from pathlib import Path

class ConverterWrapper:
    def __init__(self):
        self.package_root = Path(__file__).parent
        self.c_source = self.package_root / "resources" / "converter.c"
        # Store the binary in a platform-specific location or temp dir
        self.binary_path = self.package_root / "resources" / ("converter" + (".exe" if os.name == "nt" else ""))
        self._ensure_binary()

    def _ensure_binary(self):
        if not self.binary_path.exists():
            print("First run: Compiling C converter...")
            try:
                subprocess.check_call(["gcc", "-o", str(self.binary_path), str(self.c_source)])
                print("Compilation successful.")
            except subprocess.CalledProcessError as e:
                print(f"Error compiling C source: {e}")
                sys.exit(1)
            except FileNotFoundError:
                print("Error: 'gcc' not found. Please install a C compiler.")
                sys.exit(1)

    def run(self, cmd, arg):
        try:
            result = subprocess.run([str(self.binary_path), cmd, arg], capture_output=True, text=True, check=False)
            return result.stdout + result.stderr
        except Exception as e:
            return f"Error running converter: {str(e)}"

converter = ConverterWrapper()
