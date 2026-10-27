#!/usr/bin/env python3
"""Capture actual X terminals running commands or displaying named saved logs."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT.parent / "report"


def execute(command, log=None, timeout=100):
    print("$ " + " ".join(command), flush=True)
    process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, start_new_session=True)
    try:
        text = process.communicate(timeout=timeout)[0]
        status = process.returncode
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=4)
    print(text, end="", flush=True)
    if log:
        (REPORT / "logs" / log).write_text("$ " + " ".join(command) + "\n" + text)
    if status:
        raise RuntimeError(f"command failed with exit {status}")
    return text


def worker(mode, marker):
    print("Nankai OS 2026 - Lab3 | actual Ubuntu terminal", flush=True)
    try:
        if mode == "boot":
            execute(["make", "clean"])
            execute(["make", "-j2"], "final-default-build.log")
            output = execute(["make", "qemu", "QEMU=/usr/bin/qemu-system-riscv64"],
                             "final-boot.log", timeout=24)
            assert output.count("100 ticks") == 10
            print("Actual QEMU exited successfully after 10 tick lines.", flush=True)
        elif mode == "grade":
            output = execute(["make", "grade", "QEMU=/usr/bin/qemu-system-riscv64"],
                             "make-grade-final.log")
            assert "Total Score: 100/100" in output
            execute(["grep", "-E", "PASS|Total Score", "../report/logs/make-grade-final.log"])
            execute(["make", "-j2"], "final-default-build.log")
        elif mode == "extra":
            print("Saved raw QEMU run from final qemu6 verification:", flush=True)
            execute(["sed", "-n", "/DTB Init/,$p", "../report/logs/qemu6-extra-first_fit.log"])
        elif mode == "gdb":
            print("Saved raw GDB session from final qemu6 verification:", flush=True)
            execute(["cat", "../report/logs/qemu6-gdb-trap.log"])
        elif mode == "matrix":
            print("Reading the actual verification JSON files:", flush=True)
            for label in ("qemu6", "qemu7"):
                state = json.loads((REPORT / "logs" / f"verification-{label}.json").read_text())
                print(f"\n{state['qemu_version']} | {state['qemu']}", flush=True)
                for result in state["results"]:
                    print(f"  {'PASS' if result['passed'] else 'FAIL'} {result['name']}", flush=True)
                assert len(state["results"]) == 6 and state["passed"]
                print("  6/6 checks; official grade uses documented payload adapter.", flush=True)
            print("\nOriginal unadapted loader failure is retained separately.", flush=True)
        marker.write_text(json.dumps({"passed": True}))
    except BaseException as error:
        print(f"EVIDENCE FAILURE: {error}", flush=True)
        marker.write_text(json.dumps({"passed": False, "error": str(error)}))
    time.sleep(20)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker")
    parser.add_argument("--marker", type=Path)
    args = parser.parse_args()
    if args.worker:
        return worker(args.worker, args.marker)
    images = REPORT / "images"
    images.mkdir(exist_ok=True)
    modes = [("boot", 68), ("grade", 42), ("extra", 66), ("gdb", 54), ("matrix", 34)]
    with tempfile.TemporaryDirectory(prefix="lab3-evidence-") as temporary:
        temporary = Path(temporary)
        number = 300 + os.getpid() % 500
        while Path(f"/tmp/.X11-unix/X{number}").exists():
            number += 1
        display = f":{number}"
        env = dict(os.environ, DISPLAY=display)
        xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", "1900x1800x24", "-nolisten", "tcp"],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            time.sleep(0.8)
            for index, (mode, rows) in enumerate(modes, 1):
                print(f"Capturing actual terminal: {mode}", flush=True)
                marker = temporary / f"{mode}.json"
                title = f"Lab3 - {mode}"
                terminal = subprocess.Popen(["xterm", "-fa", "DejaVu Sans Mono", "-fs", "12",
                    "-geometry", f"160x{rows}+8+8", "-bg", "#0b0e14", "-fg", "#e6e6e6",
                    "-title", title, "-e", sys.executable, str(Path(__file__).resolve()),
                    "--worker", mode, "--marker", str(marker)], env=env,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
                try:
                    deadline = time.monotonic() + 115
                    while not marker.exists():
                        if terminal.poll() is not None or time.monotonic() >= deadline:
                            raise RuntimeError(f"{mode}: terminal worker did not finish")
                        time.sleep(0.25)
                    time.sleep(0.3)
                    status = json.loads(marker.read_text())
                    windows = subprocess.check_output(["xwininfo", "-display", display, "-root", "-tree"], text=True)
                    match = re.search(r'(0x[0-9a-f]+)\s+"' + re.escape(title) + '"', windows)
                    if not match:
                        raise RuntimeError("terminal window not found")
                    destination = images / f"{index:02}-{mode}.png"
                    subprocess.run(["import", "-display", display, "-window", match.group(1), str(destination)], check=True)
                    if not status["passed"]:
                        raise RuntimeError(status)
                    print(f"Saved {destination.name}", flush=True)
                finally:
                    if terminal.poll() is None:
                        os.killpg(terminal.pid, signal.SIGTERM)
                    terminal.wait(timeout=5)
        finally:
            xvfb.terminate()
            xvfb.wait(timeout=5)
    print("Captured 5 actual terminal screenshots.", flush=True)


if __name__ == "__main__":
    main()
