#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
make >/dev/null

qemu_log=$(mktemp)
qemu-system-riscv64 -machine virt -nographic -bios default \
    -kernel bin/ucore.img -gdb tcp:127.0.0.1:1234 -S \
    >"$qemu_log" 2>&1 &
qemu_pid=$!
trap 'kill "$qemu_pid" 2>/dev/null || true; wait "$qemu_pid" 2>/dev/null || true; rm -f "$qemu_log"' EXIT

sleep 1
gdb-multiarch -q -batch bin/kernel \
    -ex 'set pagination off' \
    -ex 'target remote 127.0.0.1:1234' \
    -ex 'printf "=== Reset PC ===\n"' \
    -ex 'p/x $pc' \
    -ex 'x/6i $pc' \
    -ex 'x/4gx 0x1018' \
    -ex 'printf "=== Kernel preloaded before execution ===\n"' \
    -ex 'x/4i 0x80200000' \
    -ex 'si' \
    -ex 'printf "=== After first instruction ===\n"' \
    -ex 'p/x $pc' \
    -ex 'break *0x80000000' \
    -ex 'continue' \
    -ex 'printf "=== OpenSBI entry ===\n"' \
    -ex 'p/x $pc' \
    -ex 'x/4i $pc' \
    -ex 'break *0x80200000' \
    -ex 'continue' \
    -ex 'printf "=== Kernel entry ===\n"' \
    -ex 'info registers pc sp a0 a1' \
    -ex 'x/6i $pc' \
    -ex 'p/x &bootstacktop' \
    -ex 'si' \
    -ex 'si' \
    -ex 'printf "=== After stack setup ===\n"' \
    -ex 'info registers pc sp' \
    -ex 'break kern_init' \
    -ex 'continue' \
    -ex 'printf "=== C entry ===\n"' \
    -ex 'p/x $pc' \
    -ex 'bt'
