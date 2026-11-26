#!/bin/sh
# Builds fixed Wild Hearts dxgi.dll.
# Usage: ./build.sh [1|2]     1 = native Linux, 2 = WSL
#          ./build.sh -h      help
# Override the toolchain dir with: LLVM=/path/to/llvm/bin ./build.sh
set -e
cd "$(dirname "$0")"

SRC=src
OUT=build

usage() {
  cat <<'EOF'
Build fixed dxgi.dll.

  ./build.sh          prompt for environment, then build
  ./build.sh 1        build with the native Linux LLVM toolchain (from PATH)
  ./build.sh 2        build with LLVM installed on the Windows side (WSL)
  ./build.sh -h       this help

Environment variables:
  LLVM   directory holding clang-cl / lld-link / llvm-dlltool.
         Defaults to the PATH (Linux) or "C:\Program Files\LLVM\bin" (WSL).

Output: build/dxgi.dll
EOF
}

# --- pick the environment ------------------------------------------------
choice="${1:-}"
case "$choice" in
-h | --help | help)
  usage
  exit 0
  ;;
esac

if [ "$choice" != 1 ] && [ "$choice" != 2 ]; then
  echo "Wild Hearts - build fixed dxgi.dll"
  echo
  echo "  1) Linux   (native LLVM toolchain, taken from PATH)"
  echo "  2) WSL     (LLVM installed on the Windows side)"
  echo
  printf "Where are you building from? [1/2]: "
  read -r choice
  echo
fi

case "$choice" in
1)
  env_name="Linux (native)"
  EXT=""
  LLVM_DIR="${LLVM:-}"
  hint="install LLVM (e.g. sudo apt install lld clang) or set LLVM=/path/to/llvm/bin"
  ;;
2)
  env_name="WSL (Windows LLVM)"
  EXT=".exe"
  LLVM_DIR="${LLVM:-/c/Program Files/LLVM/bin}"
  hint="install LLVM on Windows (https://releases.llvm.org/download.html) or set LLVM=/mnt/c/.../bin"
  ;;
*)
  echo "error: expected 1 or 2, got '$choice'" >&2
  exit 2
  ;;
esac

# --- locate the toolchain ------------------------------------------------
resolve() {
  if [ -n "$LLVM_DIR" ]; then
    found="$LLVM_DIR/$1$EXT"
    [ -f "$found" ] || {
      echo "error: '$found' not found." >&2
      echo "       $hint" >&2
      exit 3
    }
  else
    found="$(command -v "$1$EXT" 2>/dev/null || true)"
    [ -n "$found" ] || {
      echo "error: '$1$EXT' not found in PATH." >&2
      echo "       $hint" >&2
      exit 3
    }
  fi
  printf '%s' "$found"
}

DLLTOOL=$(resolve llvm-dlltool)
CLANG=$(resolve clang-cl)
LINK=$(resolve lld-link)

echo "==> environment : $env_name"
echo "==> toolchain   : $(dirname "$DLLTOOL")"
echo "==> sources     : $SRC/"
echo "==> output      : $OUT/"
echo

[ -f "$SRC/dxgi_proxy.c" ] || {
  echo "error: $SRC/dxgi_proxy.c is missing" >&2
  exit 4
}
mkdir -p "$OUT"

echo "==> import library ($OUT/kernel32.lib)"
"$DLLTOOL" -m i386:x86-64 -d "$SRC/kernel32.def" -l "$OUT/kernel32.lib"

echo "==> compiling    ($OUT/dxgi_proxy.obj)"
"$CLANG" -nologo -c -O2 -GS- -Zl -W4 -Wno-unused-parameter \
  -Wno-incompatible-library-redeclaration -fno-builtin \
  --target=x86_64-pc-windows-msvc \
  -Fo"$OUT/dxgi_proxy.obj" "$SRC/dxgi_proxy.c"

echo "==> linking      ($OUT/dxgi.dll)"
"$LINK" -nologo -dll -nodefaultlib -entry:DllMain -machine:x64 -brepro \
  -def:"$SRC/dxgi.def" \
  -out:"$OUT/dxgi.dll" "$OUT/dxgi_proxy.obj" "$OUT/kernel32.lib"

echo
echo "Built $OUT/dxgi.dll ($(wc -c <"$OUT/dxgi.dll") bytes)"
echo "Drop it next to the game executable to install the fix."
