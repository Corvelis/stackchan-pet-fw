from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class TimekeeperControllerTest(unittest.TestCase):
    def test_host_state_machine(self) -> None:
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("no host C++ compiler is available")

        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "timekeeper_controller_test"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "scripts/tests/timekeeper_controller_test.cpp"),
                    str(ROOT / "src/TimekeeperController.cpp"),
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
