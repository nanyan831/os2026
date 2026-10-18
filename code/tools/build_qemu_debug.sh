#!/usr/bin/env bash
# Independent source build; intentionally no make install.
set -euo pipefail
lab2_qemu_parent="${LAB2_QEMU_PARENT:-$HOME/oslab2}"
lab2_qemu_source="$lab2_qemu_parent/qemu-4.1.1"
mkdir -p "$lab2_qemu_parent"
lab2_archive="${1:-$lab2_qemu_parent/qemu-4.1.1.tar.xz}"
if [[ ! -f "$lab2_archive" ]]; then
    curl --fail --location https://download.qemu.org/qemu-4.1.1.tar.xz --output "$lab2_archive"
fi
sha256sum "$lab2_archive"
if [[ ! -d "$lab2_qemu_source" ]]; then
    mkdir "$lab2_qemu_source"
    tar -xJf "$lab2_archive" -C "$lab2_qemu_source" --strip-components=1
fi
mkdir -p "$lab2_qemu_source/build-debug"
cd "$lab2_qemu_source/build-debug"
if [[ ! -f config-host.mak ]]; then
    ../configure --target-list=riscv64-softmmu --enable-debug --disable-werror \
        --disable-docs --disable-gtk --disable-sdl --python=/usr/bin/python3
fi
make -j"${LAB2_BUILD_JOBS:-4}"
./riscv64-softmmu/qemu-system-riscv64 --version
