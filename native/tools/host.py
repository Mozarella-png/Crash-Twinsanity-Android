"""The machine the native build is made on: its clang, the include folders of the libraries the native platform layer uses
(SDL3, FFmpeg) and the PS2SDK toolchain's folder (tools/local_config.py), for the native tools that run clang themselves.

    TWIN_CLANG            the host's clang++ (native/CMakeLists.txt passes its own compiler), else clang++ on the PATH
    TWIN_HOST_INCLUDES    the libraries' include folders, separated by | (native/CMakeLists.txt) or the system's path
                          separator, else Homebrew's
    TWIN_HOST_LIBS        the libraries the build links (make_stubs.py on Windows: what they define is never stood in for)
"""
import hashlib
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import local_config  # noqa: E402

WINDOWS = os.name == "nt"
CLANG = os.environ.get("TWIN_CLANG") or "clang++"


def split_list(value):
    return [part for chunk in value.split("|") for part in chunk.split(os.pathsep) if part]


def native_flags():
    """The flags the native build compiles with that change layouts (native/CMakeLists.txt): MinGW lays bitfields out the
    Microsoft way unless told not to, where the PS2's GCC and every other host lay them out the same as each other"""
    return ["-mno-ms-bitfields"] if WINDOWS else []


def host_includes():
    value = os.environ.get("TWIN_HOST_INCLUDES")
    folders = split_list(value) if value else ["/opt/homebrew/include"]
    return ["-I" + folder for folder in folders]


def host_libs():
    return split_list(os.environ.get("TWIN_HOST_LIBS", ""))


def ps2dev():
    return local_config.ps2dev()


def ps2sdk():
    return local_config.ps2sdk()


def ee_gxx():
    return local_config.ee_tool("g++")


HERE = Path(__file__).resolve().parents[2]


def headers_digest():
    """A digest of every header the native build's C++ includes (the game's, the platforms', the stand-ins of PS2SDK's): a
    change to any of them changes every file's results"""
    digest = hashlib.sha1()
    for root in ("include", "native/include", "src"):
        for path in sorted((HERE / root).rglob("*")):
            if path.suffix in (".h", ".inc", ".inl", ".hpp"):
                digest.update(path.relative_to(HERE).as_posix().encode())
                digest.update(path.read_bytes())
    return digest.hexdigest()


class FileCache:
    """What a tool made of each source file, kept between runs (build/native/<name>.json): a file's result is used again while
    the file, the headers, the tool and its arguments are the same. The tools run clang on hundreds of files; a change to one
    .cpp file then reruns clang on that one alone"""

    def __init__(self, name, salt):
        self.path = HERE / "build" / "native" / f"{name}.json"
        self.salt = hashlib.sha1((headers_digest() + "\0" + salt).encode()).hexdigest()
        try:
            self.entries = json.loads(self.path.read_text()) if self.path.exists() else {}
        except ValueError:
            self.entries = {}
        if self.entries.get("salt") != self.salt:
            self.entries = {"salt": self.salt, "files": {}}

    def key(self, source):
        return hashlib.sha1(Path(source).read_bytes()).hexdigest()

    def get(self, source):
        entry = self.entries["files"].get(str(source))
        return entry["result"] if entry is not None and entry["key"] == self.key(source) else None

    def put(self, source, result):
        self.entries["files"][str(source)] = {"key": self.key(source), "result": result}

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self.entries))
