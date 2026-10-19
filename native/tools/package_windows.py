#!/usr/bin/env python3
"""The Windows release (native/WINDOWS.md): the built program with what it needs beside it, made into
build/native/dist/Crash Twinsanity/ and build/native/dist/Crash-Twinsanity-Windows.zip.

    .venv\\Scripts\\python.exe native\\tools\\package_windows.py [--build build/native/cmake]

The folder holds CrashTwinsanity.exe, SDL's and FFmpeg's DLLs, the UI's fonts, the libraries' licences and the player's guide
(native/PLAYING_WINDOWS.md as README.txt). Nothing of the game's: the player picks their own disc image.
"""
import argparse
import shutil
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default=str(HERE / "build" / "native" / "cmake"))
    parser.add_argument("--deps", default=str(HERE / "build" / "native" / "deps"),
                        help="where build_windows.py put SDL3 and FFmpeg (for their licences)")
    arguments = parser.parse_args()
    build = Path(arguments.build)
    exe = build / "CrashTwinsanity.exe"
    if not exe.exists():
        sys.exit(f"{exe} isn't built: native/tools/build_windows.py")

    dist = HERE / "build" / "native" / "dist"
    folder = dist / "Crash Twinsanity"
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)

    shutil.copy2(exe, folder / exe.name)
    dlls = sorted(build.glob("*.dll"))
    for dll in dlls:
        shutil.copy2(dll, folder / dll.name)

    fonts = folder / "fonts"
    fonts.mkdir()
    for name in ("LuckiestGuy-Regular.ttf", "Fredoka-SemiBold.ttf", "Fredoka-Bold.ttf"):
        shutil.copy2(HERE / "native" / "third_party" / "fonts" / name, fonts / name)

    licenses = folder / "licenses"
    licenses.mkdir()
    for name in ("LICENSE-LuckiestGuy.txt", "OFL-Fredoka.txt", "README.md"):
        shutil.copy2(HERE / "native" / "third_party" / "fonts" / name, licenses / f"fonts-{name}")
    shutil.copy2(HERE / "native" / "third_party" / "stb" / "LICENSE.txt", licenses / "stb-LICENSE.txt")
    # The port's own licence (what it covers is in it)
    shutil.copy2(HERE / "LICENSE", licenses / "Crash-Twinsanity-Windows-LICENSE.txt")
    deps = Path(arguments.deps)
    found = sorted(deps.glob("SDL3-*/LICENSE.txt"))
    if found:
        shutil.copy2(found[-1], licenses / "SDL3-LICENSE.txt")
    else:
        print(f"warning: no SDL3-*/LICENSE.txt in {deps}", file=sys.stderr)
    # FFmpeg's licence and how to replace its DLLs (the LGPL's relinking clause): native/third_party/ffmpeg's, for our own
    # build (build_ffmpeg.sh); a prebuilt one's own licence when that's what was linked
    ffmpeg = licenses / "ffmpeg"
    ffmpeg.mkdir()
    for path in (HERE / "native" / "third_party" / "ffmpeg").iterdir():
        shutil.copy2(path, ffmpeg / path.name)
    for prebuilt in sorted(deps.glob("ffmpeg-*/LICENSE.txt")):
        if any(build.glob("swresample-*.dll")):
            shutil.copy2(prebuilt, ffmpeg / "LICENSE-prebuilt.txt")

    shutil.copy2(HERE / "native" / "PLAYING_WINDOWS.md", folder / "README.txt")

    archive = dist / "Crash-Twinsanity-Windows.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(folder.rglob("*")):
            z.write(path, Path("Crash Twinsanity") / path.relative_to(folder))

    size = archive.stat().st_size / (1024 * 1024)
    print(f"{folder} ({len(dlls)} DLLs), {archive} ({size:.0f} MB)")


if __name__ == "__main__":
    main()
