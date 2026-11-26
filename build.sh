#!/bin/sh
set -e
cd "$(dirname "$0")"
L="${LLVM:-/c/Program Files/LLVM/bin}"
mkdir -p build
"$L/llvm-dlltool.exe" -m i386:x86-64 -d kernel32.def -l build/kernel32.lib
"$L/clang-cl.exe" -nologo -c -O2 -GS- -Zl -W4 -Wno-unused-parameter -Wno-incompatible-library-redeclaration -fno-builtin --target=x86_64-pc-windows-msvc -Fobuild/dxgi_proxy.obj dxgi_proxy.c
"$L/lld-link.exe" -nologo -dll -nodefaultlib -entry:DllMain -machine:x64 -brepro -def:dxgi.def -out:build/dxgi.dll build/dxgi_proxy.obj build/kernel32.lib
