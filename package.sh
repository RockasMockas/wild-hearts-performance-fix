#!/bin/sh
# Stage the release artifacts from build/dxgi.dll.
#
#   build/release/dxgi.dll        standalone install, next to the game exe
#   build/release/OptiScaler.zip  OptiScaler install, unzip into the game folder
#
set -e
cd "$(dirname "$0")"

OUT=build
REL="$OUT/release"

[ -f "$OUT/dxgi.dll" ] || {
  echo "error: $OUT/dxgi.dll is missing, run ./build.sh first" >&2
  exit 1
}
command -v zip >/dev/null || {
  echo "error: 'zip' is required to build OptiScaler.zip" >&2
  exit 1
}

rm -rf "$REL"
mkdir -p "$REL/OptiScaler/plugins"
cp "$OUT/dxgi.dll" "$REL/dxgi.dll"
cp "$OUT/dxgi.dll" "$REL/OptiScaler/plugins/dxgi.dll"

echo "==> $REL/OptiScaler.zip"
(cd "$REL" && rm -f OptiScaler.zip && zip -q -X OptiScaler.zip OptiScaler/plugins/dxgi.dll)
rm -rf "$REL/OptiScaler"

echo
echo "Release artifacts:"
echo "  $REL/dxgi.dll        standalone: goes next to the game exe"
echo "  $REL/OptiScaler.zip  with OptiScaler: unzip to <game>\\OptiScaler\\plugins\\dxgi.dll"
