#!/usr/bin/env python3
"""Lists or extracts files of the game's disc image (ISO 9660) and of its archive (CRASH6/CRASH.BD, by CRASH.BH's table: the
levels, the texts, the fonts...), for making mods (native/WINDOWS.md, "Mods").

    iso_extract.py IMAGE --list [FOLDER]           the disc's files (FMV/H01_A.PSS...)
    iso_extract.py IMAGE --archive --list [FOLDER] the archive's files (Levels/Earth/Hub/beach.rm2...)
    iso_extract.py IMAGE [--archive] PATH [OUT]

PATH in any case, with / or backslashes; OUT defaults to the file's name. A file put in the settings folder's mods folder at the same path
(%APPDATA%/Crash Twinsanity/mods/Levels/Earth/Hub/beach.rm2, mods/FMV/H01_A.PSS) is read by the game instead of the original.
"""
import argparse
import struct
import sys
from pathlib import Path

SECTOR = 2048


def records(image, sector, size):
    """A directory's records: (name, sector, size, is directory)."""
    image.seek(sector * SECTOR)
    data = image.read(size)
    at = 0
    while at < len(data):
        length = data[at]
        if length == 0:
            # The rest of the sector is padding: the next record starts at the next sector
            at = (at // SECTOR + 1) * SECTOR
            continue
        record = data[at:at + length]
        extent = struct.unpack_from("<I", record, 2)[0]
        data_size = struct.unpack_from("<I", record, 10)[0]
        flags = record[25]
        name_length = record[32]
        name = record[33:33 + name_length].decode("latin-1")
        if name not in ("\x00", "\x01"):
            name = name.split(";")[0].rstrip(".")
            yield name, extent, data_size, bool(flags & 2)
        at += length


def root(image):
    image.seek(16 * SECTOR)
    descriptor = image.read(SECTOR)
    if descriptor[1:6] != b"CD001":
        sys.exit("not an ISO 9660 image")
    return struct.unpack_from("<I", descriptor, 156 + 2)[0], struct.unpack_from("<I", descriptor, 156 + 10)[0]


def find(image, path):
    sector, size = root(image)
    directory = True
    for part in [p for p in path.replace("\\", "/").split("/") if p]:
        if not directory:
            return None
        for name, extent, data_size, is_directory in records(image, sector, size):
            if name.upper() == part.upper():
                sector, size, directory = extent, data_size, is_directory
                break
        else:
            return None
    return sector, size, directory


def walk(image, sector, size, prefix):
    for name, extent, data_size, is_directory in records(image, sector, size):
        if is_directory:
            yield from walk(image, extent, data_size, f"{prefix}{name}/")
        else:
            yield f"{prefix}{name}", data_size


def archive_files(image):
    """The archive's files: path -> (offset in the image, size)."""
    table = find(image, "CRASH6/CRASH.BH")
    data = find(image, "CRASH6/CRASH.BD")
    if table is None or data is None:
        sys.exit("no CRASH6/CRASH.BH and CRASH.BD on the disc")
    image.seek(table[0] * SECTOR)
    header = image.read(table[1])
    files, at = {}, 4
    while at + 4 <= len(header):
        length = struct.unpack_from("<I", header, at)[0]
        if length == 0 or at + 4 + length + 8 > len(header):
            break
        name = header[at + 4:at + 4 + length].decode("latin-1").replace("\\", "/")
        offset, size = struct.unpack_from("<II", header, at + 4 + length)
        files[name] = (data[0] * SECTOR + offset, size)
        at += 4 + length + 8
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image")
    parser.add_argument("path", nargs="?")
    parser.add_argument("out", nargs="?")
    parser.add_argument("--list", nargs="?", const="", metavar="FOLDER")
    parser.add_argument("--archive", action="store_true", help="the archive's files rather than the disc's")
    arguments = parser.parse_args()
    with open(arguments.image, "rb") as image:
        if arguments.archive:
            files = archive_files(image)
            if arguments.list is not None:
                prefix = arguments.list.replace("\\", "/").strip("/").upper()
                for name, (_, size) in files.items():
                    if name.upper().startswith(prefix):
                        print(f"{size:10}  {name}")
                return
            if arguments.path is None:
                parser.error("a path or --list")
            wanted = arguments.path.replace("\\", "/").strip("/").upper()
            match = [name for name in files if name.upper() == wanted]
            if not match:
                sys.exit(f"no file {arguments.path} in the archive")
            offset, size = files[match[0]]
            image.seek(offset)
            out = Path(arguments.out or Path(match[0]).name)
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(image.read(size))
            print(f"{match[0]}: {size} bytes -> {out}")
            return
        if arguments.list is not None:
            found = find(image, arguments.list) if arguments.list else (*root(image), True)
            if found is None or not found[2]:
                sys.exit(f"no folder {arguments.list}")
            for path, size in walk(image, found[0], found[1], arguments.list.strip("/") + "/" if arguments.list else ""):
                print(f"{size:10}  {path}")
            return
        if arguments.path is None:
            parser.error("a path or --list")
        found = find(image, arguments.path)
        if found is None or found[2]:
            sys.exit(f"no file {arguments.path}")
        sector, size, _ = found
        image.seek(sector * SECTOR)
        out = Path(arguments.out or Path(arguments.path.replace("\\", "/")).name)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(image.read(size))
        print(f"{arguments.path}: {size} bytes -> {out}")


if __name__ == "__main__":
    main()
