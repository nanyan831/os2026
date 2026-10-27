#!/usr/bin/env python3
"""Run unchanged course tests and independent Lab3 regressions in temp trees."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT.parent / "report" / "logs"
OFFICIAL = ["grade.sh", "inject.py", "test_lab3.c", "test_lab3.h",
            "test_lab3_context.S"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_manifest():
    paths = [p for top in ("kern", "libs") for p in (ROOT / top).rglob("*")
             if p.is_file() and p.suffix in (".c", ".h", ".S")]
    paths += [ROOT / x for x in ("Makefile", "tools/function.mk", "tools/kernel.ld")]
    return {str(p.relative_to(ROOT)): sha(p) for p in sorted(paths)}


def run(command, cwd, log, env=None, timeout=60):
    start = time.monotonic()
    try:
        proc = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, errors="replace", timeout=timeout)
        code, output, timed_out = proc.returncode, proc.stdout, False
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        code, timed_out = 124, True
    record = {"command": [str(x) for x in command], "exit_code": code,
              "timed_out": timed_out, "seconds": round(time.monotonic() - start, 3),
              "log": str(log.relative_to(LOGS.parent))}
    log.write_text("$ " + " ".join(map(str, command)) + "\n" + output
                   + f"\n[runner exit={code}, timed_out={timed_out}]\n")
    return record, output


def snapshot(parent):
    target = parent / "code"
    shutil.copytree(ROOT, target, ignore=shutil.ignore_patterns(
        "bin", "obj", "__pycache__", ".git", ".qemu*.out", ".gdb.in"))
    return target


def inject_extra(tree):
    target = tree / "kern" / "extra"
    target.mkdir()
    for name in ("test_lab3_extra.c", "test_lab3_extra_context.S"):
        shutil.copyfile(tree / "tools" / name, target / name)
    makefile = tree / "Makefile"
    text = makefile.read_text()
    index = text.index("KCFLAGS\t")
    text = text[:index] + "KINCLUDE += kern/extra\nKSRCDIR += kern/extra\n\n" + text[index:]
    makefile.write_text(text)
    init = tree / "kern/init/init.c"
    text = init.read_text()
    text = text.replace("int kern_init(void)", "void lab3_extra_check(void);\n\nint kern_init(void)", 1)
    anchor = text.rfind("    idt_init();")
    if anchor < 0:
        raise RuntimeError("extra injection: idt_init anchor not found")
    insert = text.index("\n", anchor) + 1
    text = text[:insert] + "    lab3_extra_check();\n" + text[insert:]
    init.write_text(text)


def build(tree, pmm, log):
    record, output = run(["make", "-j2", f"PMM={pmm}"], tree, log, timeout=90)
    if record["exit_code"]:
        raise RuntimeError(f"build failed: {log.name}\n{output[-1500:]}")
    return record


def qemu_command(qemu, tree):
    return [qemu, "-machine", "virt", "-nographic", "-bios", "default",
            "-kernel", tree / "bin/ucore.img", "-monitor", "none", "-no-reboot"]


def grade(qemu, label):
    with tempfile.TemporaryDirectory(prefix="nku-lab3-grade-") as temporary:
        tree = snapshot(Path(temporary))
        record, output = run(["python3", ROOT / "tools/inject.py", tree], tree,
                             LOGS / f"{label}-grade-inject.log")
        if record["exit_code"]:
            raise RuntimeError("course test injection failed")
        env = dict(os.environ, LAB3_QEMU_REAL=qemu)
        env["PATH"] = str(tree / "tools/qemu_compat") + os.pathsep + env["PATH"]
        record, output = run(["sh", "tools/grade.sh"], tree,
                             LOGS / f"{label}-official-grade.log", env, timeout=120)
        for raw in tree.glob(".qemu.*.out"):
            shutil.copyfile(raw, LOGS / f"{label}-{raw.name.lstrip('.')}")
        record.update(name="official-grade-with-payload-adapter",
                      passed=record["exit_code"] == 0 and "Total Score: 100/100" in output,
                      score=re.findall(r"Total Score: (\d+/100)", output)[-1:],
                      adaptation="raw loader -> -kernel; official test bytes unchanged")
        return record


def boot(qemu, label, pmm):
    with tempfile.TemporaryDirectory(prefix="nku-lab3-boot-") as temporary:
        tree = snapshot(Path(temporary))
        build(tree, pmm, LOGS / f"{label}-boot-{pmm}-build.log")
        record, output = run(qemu_command(qemu, tree), tree,
                             LOGS / f"{label}-boot-{pmm}.log", timeout=22)
        ticks = len(re.findall(r"^100 ticks\r?$", output, re.M))
        manager = "default_pmm_manager" if pmm == "first_fit" else "best_fit_pmm_manager"
        record.update(name=f"normal-boot-{pmm}", tick_lines=ticks,
                      passed=record["exit_code"] == 0 and not record["timed_out"]
                      and ticks == 10 and f"memory management: {manager}" in output
                      and "check_alloc_page() succeeded!" in output,
                      kernel_sha256=sha(tree / "bin/kernel"))
        return record


def extra(qemu, label, pmm):
    with tempfile.TemporaryDirectory(prefix="nku-lab3-extra-") as temporary:
        tree = snapshot(Path(temporary))
        inject_extra(tree)
        build(tree, pmm, LOGS / f"{label}-extra-{pmm}-build.log")
        dump = subprocess.check_output(["riscv64-unknown-elf-objdump", "-d", "bin/kernel"],
                                       cwd=tree, text=True)
        snippets = []
        for symbol in ("lab3_break16_site", "lab3_break32_site", "lab3_illegal16_site",
                       "lab3_illegal32_site"):
            match = re.search(r"(?m)^.*<" + symbol + r">:\n(?:.*\n){1,3}", dump)
            if match:
                snippets.append(match.group())
        (LOGS / f"{label}-extra-{pmm}-instructions.log").write_text("\n".join(snippets))
        record, output = run(qemu_command(qemu, tree), tree,
                             LOGS / f"{label}-extra-{pmm}.log", timeout=8)
        required = ["PMM tail insertion", "all writable GPRs, original SP, GP/TP",
                    "breakpoint16 return", "breakpoint32 return", "illegal16 return",
                    "illegal32 return", "SIE preservation and nested save/restore",
                    "suite complete"]
        checks = {name: f"[Extra] {name}: PASS" in output for name in required}
        record.update(name=f"extra-{pmm}", checks=checks,
                      passed=record["exit_code"] == 0 and all(checks.values()),
                      kernel_sha256=sha(tree / "bin/kernel"))
        return record


def gdb_trace(qemu, label):
    with tempfile.TemporaryDirectory(prefix="nku-lab3-gdb-") as temporary:
        tree = snapshot(Path(temporary))
        build(tree, "first_fit", LOGS / f"{label}-gdb-build.log")
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        script = f"""set pagination off
set confirm off
set remotetimeout 5
file bin/kernel
set arch riscv:rv64
target remote 127.0.0.1:{port}
tbreak *__alltraps
continue
set $original_sp = $sp
set $original_epc = $sepc
printf "ENTRY sp=0x%lx sepc=0x%lx scause=0x%lx sstatus=0x%lx\\n", $sp, $sepc, $scause, $sstatus
if ($scause & 0xff) != 5
  quit 1
end
tbreak *trap
continue
set $frame = (struct trapframe *)$a0
printf "FRAME a0=0x%lx sp=0x%lx bytes=%lu saved_sp=0x%lx epc=0x%lx status=0x%lx\\n", $a0, $sp, sizeof(struct trapframe), $frame->gpr.sp, $frame->epc, $frame->status
if $a0 != $sp || sizeof(struct trapframe) != 288 || $frame->gpr.sp != $original_sp || $frame->epc != $original_epc
  quit 1
end
if ($frame->status & 0x120) != 0x120 || ($frame->status & 2) != 0
  quit 1
end
tbreak *__trapret
continue
printf "RETURN frame_sp=0x%lx epc=0x%lx\\n", $sp, $frame->epc
if $sp != (unsigned long)$frame || $frame->epc != $original_epc
  quit 1
end
tbreak *$original_epc
continue
printf "RESUMED pc=0x%lx sp=0x%lx sstatus=0x%lx\\n", $pc, $sp, $sstatus
if $sp != $original_sp || $pc != $original_epc || ($sstatus & 2) == 0
  quit 1
end
printf "GDB Trap roundtrip: PASS\\n"
detach
quit 0
"""
        gdbfile = Path(temporary) / "trace.gdb"
        gdbfile.write_text(script)
        qlog = LOGS / f"{label}-gdb-qemu.log"
        with qlog.open("w") as stream:
            process = subprocess.Popen(qemu_command(qemu, tree) +
                                       ["-S", "-gdb", f"tcp:127.0.0.1:{port}"],
                                       cwd=tree, stdout=stream, stderr=subprocess.STDOUT)
            try:
                time.sleep(0.35)
                record, output = run(["gdb-multiarch", "-q", "-batch", "-x", gdbfile],
                                     tree, LOGS / f"{label}-gdb-trap.log", timeout=25)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=4)
        record.update(name="gdb-trap-roundtrip", passed=record["exit_code"] == 0
                      and "GDB Trap roundtrip: PASS" in output)
        return record


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", choices=["all", "grade", "boot", "extra", "gdb"], default="all")
    parser.add_argument("--qemu", default="/usr/bin/qemu-system-riscv64")
    parser.add_argument("--label", default="qemu6")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9_-]+", args.label):
        parser.error("label must contain only letters, digits, underscore or hyphen")
    qemu = shutil.which(args.qemu)
    if not qemu:
        parser.error("QEMU not found")
    LOGS.mkdir(parents=True, exist_ok=True)
    destination = LOGS / f"verification-{args.label}.json"
    current = source_manifest()
    previous = json.loads(destination.read_text()) if destination.exists() else {}
    harness = {name: sha(ROOT / "tools" / name) for name in
               ("test_lab3_extra.c", "test_lab3_extra_context.S", "qemu_compat/qemu-system-riscv64")}
    results = previous.get("results", []) if (previous.get("source_sha256") == current
              and previous.get("harness_sha256") == harness) else []
    steps = []
    if args.stage in ("all", "grade"):
        steps.append(lambda: grade(qemu, args.label))
    if args.stage in ("all", "boot"):
        for pmm in ("first_fit", "best_fit"):
            steps.append(lambda pmm=pmm: boot(qemu, args.label, pmm))
    if args.stage in ("all", "extra"):
        for pmm in ("first_fit", "best_fit"):
            steps.append(lambda pmm=pmm: extra(qemu, args.label, pmm))
    if args.stage in ("all", "gdb"):
        steps.append(lambda: gdb_trace(qemu, args.label))
    state = {"timestamp_utc": datetime.now(timezone.utc).isoformat(), "qemu": qemu,
             "qemu_version": subprocess.check_output([qemu, "--version"], text=True).splitlines()[0],
             "source_sha256": current, "harness_sha256": harness,
             "official_test_sha256": {name: sha(ROOT / "tools" / name) for name in OFFICIAL},
             "results": results}
    for step in steps:
        record = step()
        results[:] = [r for r in results if r["name"] != record["name"]]
        results.append(record)
        state["passed"] = all(r["passed"] for r in results)
        destination.write_text(json.dumps(state, ensure_ascii=False, indent=2) + "\n")
        print(f"{'PASS' if record['passed'] else 'FAIL'} {record['name']} ({record['seconds']}s)", flush=True)
    print(f"Results: {sum(r['passed'] for r in results)}/{len(results)}", flush=True)
    if not state.get("passed"):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
