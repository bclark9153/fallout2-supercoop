#!/usr/bin/env bash
# Builds the Windows release zip: installer + co-op engine + launchers.
#
#   scripts/package-windows.sh <dir with f2_server.exe and fallout2-ce.exe> [out dir]
#
# Produces Fallout2-SuperCoop-<version>-windows.zip (version from
# dist/windows/VERSION.txt). The Restoration Project is not bundled: the
# installer downloads the pinned release and checks its hash.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
exe_dir="${1:?usage: package-windows.sh <exe dir> [out dir]}"
out_dir="${2:-$root/out}"
version="$(head -n1 "$root/dist/windows/VERSION.txt" | tr -d '\r\n ')"
name="Fallout2-SuperCoop-$version"

for exe in f2_server.exe fallout2-ce.exe; do
    [[ -f "$exe_dir/$exe" ]] || { echo "missing $exe_dir/$exe" >&2; exit 1; }
done

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
pkg="$stage/$name"
mkdir -p "$pkg/files"

cp "$root/dist/windows/install.cmd" "$root/dist/windows/install.ps1" "$root/dist/windows/VERSION.txt" "$pkg/"
cp "$root/dist/windows/QUICKSTART.txt" "$pkg/README.txt"
cp "$root/LICENSE.md" "$root/AUTHORS.md" "$pkg/"
cp "$exe_dir/f2_server.exe" "$exe_dir/fallout2-ce.exe" "$pkg/files/"
cp "$root/dist/windows/start-server.cmd" "$root/dist/windows/join.cmd" "$root/dist/windows/QUICKSTART.txt" "$pkg/files/"

# Smaller download; the engine keeps working the same.
if command -v x86_64-w64-mingw32-strip >/dev/null 2>&1; then
    x86_64-w64-mingw32-strip "$pkg/files/"*.exe
elif command -v strip >/dev/null 2>&1; then
    strip "$pkg/files/"*.exe 2>/dev/null || true
fi

mkdir -p "$out_dir"
zip_path="$out_dir/$name-windows.zip"
rm -f "$zip_path"
(cd "$stage" && zip -qr9 "$zip_path" "$name")
echo "$zip_path"
