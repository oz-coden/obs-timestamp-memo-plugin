"""Run the standalone suites using the Qt dependencies from the plugin build."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--config", default="RelWithDebInfo")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
configure = ["cmake", "-S", str(root / "tests"), "-B", str(root / "build_tests")]
if os.name == "nt":
    configure += ["-A", "x64"]
else:
    configure += [f"-DCMAKE_BUILD_TYPE={args.config}"]
qt_config = next((root / ".deps").glob("**/Qt6Config.cmake"), None)
if qt_config:
    configure += [f"-DQt6_DIR={qt_config.parent}",
                  f"-DCMAKE_PREFIX_PATH={qt_config.parent.parents[2]}"]
for command in (configure,
                ["cmake", "--build", str(root / "build_tests"), "--config", args.config, "--parallel", "4"],
                ["ctest", "--test-dir", str(root / "build_tests"), "-C", args.config, "--output-on-failure"]):
    print("Running:", command, flush=True)
    subprocess.run(command, cwd=root, check=True)
