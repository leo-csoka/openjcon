#!/usr/bin/env bash
set -e

# build script for pico-switch-controller firmware
# requirements: pico-sdk installed at $HOME/.pico-sdk
SDK_DIR="$HOME/.pico-sdk"
TOOLCHAIN="$SDK_DIR/toolchain/15_2_Rel1"
SDK="$SDK_DIR/sdk/2.3.1"

export PATH="$TOOLCHAIN/bin:$PATH"

cd "$(dirname "$0")"
cmake -S . -B build-cli \
    -DPICO_TOOLCHAIN_PATH="$TOOLCHAIN" \
    -DPICO_SDK_PATH="$SDK"
cmake --build build-cli -j8

cp build-cli/pico-switch-controller.uf2 .
echo "done: $(pwd)/pico-switch-controller.uf2"
