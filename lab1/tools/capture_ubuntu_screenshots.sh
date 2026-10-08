#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
image_dir="$project_root/images"
work_dir="$(mktemp -d)"
mkdir -p "$image_dir"

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT

cat >"$work_dir/twmrc" <<'EOF'
RandomPlacement
NoGrabServer
TitleFont "-misc-fixed-bold-r-normal--15-140-75-75-c-90-iso8859-1"
MenuFont "-misc-fixed-medium-r-normal--14-130-75-75-c-70-iso8859-1"
EOF

cat >"$work_dir/run-qemu.sh" <<EOF
#!/usr/bin/env bash
cd "$project_root"
clear
printf '\033[1;32m%s@%s\033[0m:\033[1;34m~/labcodes/lab1\033[0m\$ grep PRETTY_NAME /etc/os-release\n' "\$(whoami)" "\$(hostname)"
grep '^PRETTY_NAME=' /etc/os-release
printf '\n\033[1;32m%s@%s\033[0m:\033[1;34m~/labcodes/lab1\033[0m\$ make clean && make\n' "\$(whoami)" "\$(hostname)"
make clean && make
printf '\n\033[1;32m%s@%s\033[0m:\033[1;34m~/labcodes/lab1\033[0m\$ make qemu\n' "\$(whoami)" "\$(hostname)"
: >"$work_dir/qemu-live.log"
(
    while ! grep -q '(THU.CST) os is loading' "$work_dir/qemu-live.log"; do
        sleep 0.2
    done
    touch "$work_dir/qemu-ready"
) &
make qemu 2>&1 | tee "$work_dir/qemu-live.log"
EOF

cat >"$work_dir/run-gdb.sh" <<EOF
#!/usr/bin/env bash
cd "$project_root"
clear
printf '\033[1;32m%s@%s\033[0m:\033[1;34m~/labcodes/lab1\033[0m\$ grep PRETTY_NAME /etc/os-release\n' "\$(whoami)" "\$(hostname)"
grep '^PRETTY_NAME=' /etc/os-release
printf '\n\033[1;32m%s@%s\033[0m:\033[1;34m~/labcodes/lab1\033[0m\$ bash tools/trace_boot.sh\n' "\$(whoami)" "\$(hostname)"
bash tools/trace_boot.sh
touch "$work_dir/gdb-ready"
sleep 60
EOF
chmod +x "$work_dir/run-qemu.sh" "$work_dir/run-gdb.sh"

capture_terminal() {
    local display_number="$1"
    local title="$2"
    local runner="$3"
    local marker="$4"
    local output="$5"
    local display=":$display_number"

    Xvfb "$display" -screen 0 1800x1600x24 -nolisten tcp >"$work_dir/xvfb-$display_number.log" 2>&1 &
    local xvfb_pid=$!
    sleep 1
    DISPLAY="$display" twm -f "$work_dir/twmrc" >"$work_dir/twm-$display_number.log" 2>&1 &
    local twm_pid=$!
    sleep 1
    DISPLAY="$display" xterm \
        -title "$title" \
        -fa 'DejaVu Sans Mono' -fs 13 \
        -geometry 157x84+16+16 \
        -bg '#0b0e14' -fg '#e6e6e6' \
        -bd '#5c6370' -bw 2 \
        -xrm 'XTerm*faceName: DejaVu Sans Mono' \
        -xrm 'XTerm*faceSize: 13' \
        -e bash "$runner" >"$work_dir/xterm-$display_number.log" 2>&1 &
    local xterm_pid=$!

    for _ in $(seq 1 40); do
        if [[ -f "$marker" ]]; then
            break
        fi
        sleep 1
    done
    if [[ ! -f "$marker" ]]; then
        echo "Timed out waiting for terminal command: $title" >&2
        kill "$xterm_pid" "$twm_pid" "$xvfb_pid" 2>/dev/null || true
        return 1
    fi

    sleep 1
    DISPLAY="$display" import -window root "$output"
    convert "$output" -trim +repage "$output"

    kill "$xterm_pid" "$twm_pid" "$xvfb_pid" 2>/dev/null || true
    wait "$xterm_pid" "$twm_pid" "$xvfb_pid" 2>/dev/null || true
}

capture_terminal 97 \
    'Ubuntu 22.04 — Lab 1 build and QEMU run' \
    "$work_dir/run-qemu.sh" \
    "$work_dir/qemu-ready" \
    "$image_dir/qemu_result_actual.png"

capture_terminal 98 \
    'Ubuntu 22.04 — Lab 1 GDB boot trace' \
    "$work_dir/run-gdb.sh" \
    "$work_dir/gdb-ready" \
    "$image_dir/gdb_trace_actual.png"

identify "$image_dir/qemu_result_actual.png" "$image_dir/gdb_trace_actual.png"
