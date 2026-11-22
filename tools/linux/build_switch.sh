#!/usr/bin/env bash
# S4ep2-XBLA-nx (2026-11-21): the Switch NRO in a cloud session, from LoS1-nx's tools/linux/build_switch.sh (Mark:
# "in literally every other port project, you can build NROs in the cloud"). devkitPro's Windows devkitA64 from Mark's
# C:\devkitPro runs under Wine (devkitPro's package server is not reachable from the cloud); the GCC wrappers rewrite
# the dependency files' Z:/ paths for ninja.
#
#   bash tools/linux/build_switch.sh setup <folder with dkp_a64.tar and dkp_rest.tar>   (once a session)
#   bash tools/linux/build_switch.sh inputs <folder with Mark's out\renut_switch files> <shader_cache.cpp of out\pc>
#                                                       <folder of the deko3d host shader headers>
#   bash tools/linux/build_switch.sh configure
#   bash tools/linux/build_switch.sh build        (JOBS=2 by default)
#   bash tools/linux/build_switch.sh package      (out/sd/switch/s4ep2-xbla-nx: s4ep2.nro, renut.toml, shaders; the
#                                                  ELF kept as out/nx/elf/renut_<sha12>.elf)
# The game code comes from the cloud codegen (tools/preflight.py --codegen); the game shader package and the shader
# tables from Mark's PC build (they change only when the shader dump does).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
W=/opt/devkitpro-win
L=/opt/devkitpro
export DEVKITPRO=$L DEVKITA64=$L/devkitA64 WINEDEBUG=-all

case "${1:-}" in
setup)
  src="${2:?folder with dkp_a64.tar and dkp_rest.tar}"
  command -v wine >/dev/null || { apt-get update && apt-get install -y wine64 zstd; }
  rm -rf $W && mkdir -p $W && tar -xf "$src/dkp_a64.tar" -C $W && tar -xf "$src/dkp_rest.tar" -C $W
  rm -rf $L && mkdir -p $L/devkitA64/bin $L/tools/bin
  for d in libnx portlibs cmake; do ln -s $W/$d $L/$d; done
  for d in $W/devkitA64/*; do n=$(basename "$d"); [ "$n" = bin ] || ln -s "$d" $L/devkitA64/$n; done
  for f in $W/devkitA64/bin/*.exe $W/tools/bin/*.exe; do
    n=$(basename "$f" .exe); dir=$L/devkitA64/bin; [[ "$f" == $W/tools/* ]] && dir=$L/tools/bin
    printf '#!/bin/sh\nexport WINEDEBUG=-all\nexec /usr/lib/wine/wine64 %s "$@"\n' "$f" > $dir/$n; chmod +x $dir/$n
  done
  for n in aarch64-none-elf-gcc aarch64-none-elf-g++ aarch64-none-elf-c++; do
    cat > $L/devkitA64/bin/$n <<WRAP
#!/bin/bash
export WINEDEBUG=-all
mf=""; prev=""
for a in "\$@"; do [ "\$prev" = "-MF" ] && mf="\$a"; prev="\$a"; done
/usr/lib/wine/wine64 $W/devkitA64/bin/$n.exe "\$@"; rc=\$?
[ -n "\$mf" ] && [ -f "\$mf" ] && sed -i 's#Z:/#/#g' "\$mf"
exit \$rc
WRAP
    chmod +x $L/devkitA64/bin/$n
  done
  $L/devkitA64/bin/aarch64-none-elf-gcc --version | head -1
  ;;
inputs)
  mkdir -p "$ROOT/out/renut_switch" "$ROOT/out/pc/renut_gpu" "$ROOT/out/renut_switch_host"
  cp "${2:?}"/* "$ROOT/out/renut_switch/"
  cp "${3:?}" "$ROOT/out/pc/renut_gpu/shader_cache.cpp"
  cp "${4:?}"/*.deko3d.h "$ROOT/out/renut_switch_host/"
  # shader_cache_switch.cpp comes with Mark's out\renut_switch (his build made it from the same shader dump).
  sed -i "s#C:/S4ep2-XBLA-nx#$ROOT#g" "$ROOT/out/renut_switch/deko3d_glsl_switch.cpp" "$ROOT/out/renut_switch/shader_cache_switch.cpp"
  ;;
configure)
  cd "$ROOT"
  cmake -S app -B out/nx -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/switch/cmake/switch-devkitA64.cmake" -DDEVKITPRO=$L -DREXSDK_DIR="$ROOT/sdk" \
    -DBANJO_RENUT_RENDERER=ON -DBANJO_SWITCH_RENDERER_LINES=ON -DBANJO_RENUT_SHADER_DIR="$ROOT/out/renut_switch" \
    -DBANJO_DEKO3D_HOST_SHADERS_PREBUILT="$ROOT/out/renut_switch_host" -DBANJO_GLSLANG_VALIDATOR=/usr/bin/glslangValidator \
    -DPYTHON_EXECUTABLE=/usr/bin/python3 -DPython3_EXECUTABLE=/usr/bin/python3 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "${@:2}"
  ;;
build)
  ninja -C "$ROOT/out/nx" -k 0 -j "${JOBS:-2}"
  ;;
package)
  cd "$ROOT"
  nro=out/nx/renut.nro; elf=out/nx/renut
  sha=$(sha256sum "$nro" | cut -c1-64)
  sd=out/sd/switch/s4ep2-xbla-nx; mkdir -p "$sd" out/nx/elf
  cp "$nro" "$sd/s4ep2.nro"; echo "$sha  s4ep2.nro" > "$sd/s4ep2.nro.sha256"  # Mark: the release name, never renut.nro
  cp app/s4_switch_test.toml "$sd/renut.toml"
  cp out/renut_switch/deko3d_shaders.bin out/renut_switch/deko3d_variants.txt "$sd/"
  [ -f "$elf" ] && cp "$elf" "out/nx/elf/renut_${sha:0:12}.elf"
  ls -la "$sd"; echo "renut.nro sha256 $sha"
  ;;
*)
  sed -n 2,16p "$0"; exit 1;;
esac
