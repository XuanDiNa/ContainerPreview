"""Bounded, offline DLL/package verifier. Never loads or executes the DLL."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

VERSION = "0.1.0"
SUPPORTED_GAMES = ["1.26.45.01", "1.26.51.01", "1.26.52.03"]
PAYLOAD = frozenset(("ContainerPreview.dll", "config.json", "config.schema.json", "mod.json",
                     "manifest.json", "README.md", "THIRD_PARTY_NOTICES.txt"))
MEMBERS = PAYLOAD | {"SHA256SUMS.txt"}
EXPORTS = {"ContainerPreviewInit", "ContainerPreviewStatus"}
SYSTEM_IMPORTS = {"kernel32.dll", "user32.dll", "bcrypt.dll"}
MAX_FILE = 8 * 1024 * 1024


class PE:
    def __init__(self, data):
        self.data = data
        if len(data) < 64 or data[:2] != b"MZ":
            raise ValueError("Missing DOS header")
        pe = self.unpack("<I", 60)[0]
        if self.take(pe, 4) != b"PE\0\0":
            raise ValueError("Missing PE header")
        machine, count = self.unpack("<HH", pe + 4)
        optional, flags = self.unpack("<HH", pe + 20)
        self.opt = pe + 24
        if machine != 0x8664 or not flags & 0x2000 or optional != 240 or not 1 <= count <= 96:
            raise ValueError("Not an AMD64 PE32+ DLL")
        self.take(self.opt, optional + count * 40)
        if self.unpack("<H", self.opt)[0] != 0x20B or self.unpack("<I", self.opt + 108)[0] != 16:
            raise ValueError("Unsupported optional header")
        self.headers = self.unpack("<I", self.opt + 60)[0]
        if self.headers > len(data):
            raise ValueError("Unbacked headers")
        self.sections = []
        for n in range(count):
            vs, rva, size, raw = self.unpack("<IIII", self.opt + optional + n * 40 + 8)
            self.take(raw, size)
            self.sections.append((rva, size, raw))

    def take(self, offset, size):
        if offset < 0 or size < 0 or offset > len(self.data) or size > len(self.data) - offset:
            raise ValueError("Truncated PE structure")
        return self.data[offset:offset + size]

    def unpack(self, fmt, offset):
        return struct.unpack(fmt, self.take(offset, struct.calcsize(fmt)))

    def offset(self, rva, size=1):
        if 0 <= rva < self.headers and size <= self.headers - rva:
            return rva
        for begin, length, raw in self.sections:
            if begin <= rva and rva - begin < length and size <= length - (rva - begin):
                return raw + rva - begin
        raise ValueError("RVA has no complete file-backed range")

    def at(self, fmt, rva):
        return self.unpack(fmt, self.offset(rva, struct.calcsize(fmt)))

    def string(self, rva):
        value = bytearray()
        for n in range(256):
            b = self.data[self.offset(rva + n)]
            if b == 0:
                try:
                    return value.decode("ascii")
                except UnicodeDecodeError as error:
                    raise ValueError("Non-ASCII PE identifier") from error
            value.append(b)
        raise ValueError("Unterminated PE identifier")

    def directory(self, index):
        return self.unpack("<II", self.opt + 112 + index * 8)


def inspect_dll(data, required_dll_sha256=None):
    digest = hashlib.sha256(data).hexdigest().upper()
    if required_dll_sha256 is not None:
        if not re.fullmatch(r"[0-9a-fA-F]{64}", required_dll_sha256):
            raise ValueError("Invalid required DLL SHA256")
        if digest != required_dll_sha256.upper():
            raise ValueError("DLL provenance mismatch")
    pe = PE(data)
    exports = []
    rva, size = pe.directory(0)
    if not rva or size < 40:
        raise ValueError("Missing export directory")
    pe.offset(rva, size)
    base, functions, count, targets, names, ordinals = pe.at("<IIIIII", rva + 16)
    if not 1 <= count <= 32 or functions > 32:
        raise ValueError("Invalid export count")
    for n in range(count):
        name = pe.string(pe.at("<I", names + n * 4)[0])
        ordinal = pe.at("<H", ordinals + n * 2)[0]
        if ordinal >= functions:
            raise ValueError("Invalid export ordinal")
        target = pe.at("<I", targets + ordinal * 4)[0]
        pe.offset(target)
        if rva <= target < rva + size:
            raise ValueError("Forwarded plugin entry is not supported")
        exports.append(name)
    if len(exports) != len(EXPORTS) or set(exports) != EXPORTS:
        raise ValueError("Unexpected or missing plugin exports")
    if any(pe.directory(13)):
        raise ValueError("Delayed imports are not permitted")
    imports = []
    import_symbols = {}
    rva, size = pe.directory(1)
    if not rva or not 20 <= size <= 4096:
        raise ValueError("Missing or invalid import directory")
    pe.offset(rva, size)
    terminated = False
    for n in range(size // 20):
        fields = pe.at("<IIIII", rva + n * 20)
        if not any(fields):
            terminated = True
            break
        thunk, _, _, name_rva, iat = fields
        name = pe.string(name_rva).lower()
        if name not in SYSTEM_IMPORTS or name in imports:
            raise ValueError("Unexpected dependency: " + name)
        imports.append(name)
        symbols = []
        for slot in range(4096):
            value = pe.at("<Q", (thunk or iat) + slot * 8)[0]
            if not value:
                break
            symbols.append("#" + str(value & 65535) if value & (1 << 63) else pe.string(value + 2))
        else:
            raise ValueError("Unterminated import thunk")
        import_symbols[name] = symbols
    if not terminated or not imports:
        raise ValueError("Unterminated or empty import directory")
    return {"machine": "AMD64", "exports": sorted(exports), "imports": sorted(imports),
            "import_symbols": import_symbols, "sha256": digest}


def verify_contents(contents, expected_version=VERSION, required_dll_sha256=None):
    if set(contents) != MEMBERS:
        raise ValueError("Package must contain exactly the eight allowed flat members")
    expected = {}
    try:
        for line in contents["SHA256SUMS.txt"].decode("ascii").splitlines():
            digest, name = line.split("  ", 1)
            if name not in PAYLOAD or name in expected or not re.fullmatch(r"[0-9a-fA-F]{64}", digest):
                raise ValueError("Invalid checksum entry")
            expected[name] = digest.upper()
        if set(expected) != PAYLOAD:
            raise ValueError("Incomplete checksum coverage")
        for name in PAYLOAD:
            if hashlib.sha256(contents[name]).hexdigest().upper() != expected[name]:
                raise ValueError("Checksum mismatch: " + name)
        config = json.loads(contents["config.json"])
        mod = json.loads(contents["mod.json"])
        manifest = json.loads(contents["manifest.json"])
        schema = json.loads(contents["config.schema.json"])
        if not isinstance(config, dict) or type(config.get("enabled")) is not bool or config != {"enabled": True, "holdKey": "LALT"}:
            raise ValueError("Wrong packaged defaults")
        for field, value in {"id": "container-preview", "name": "ContainerPreview", "version": expected_version,
                             "dll": "ContainerPreview.dll", "config": "config.json", "schema": "config.schema.json"}.items():
            if mod.get(field) != value:
                raise ValueError("Invalid mod metadata: " + field)
        if manifest != {"name": "ContainerPreview", "entry": "ContainerPreview.dll", "version": expected_version, "type": "preload-native"}:
            raise ValueError("Invalid loader manifest")
        if mod.get("supportedGameVersions") != SUPPORTED_GAMES:
            raise ValueError("Invalid supported game versions")
        if schema.get("type") != "object" or schema.get("properties", {}).get("enabled", {}).get("type") != "boolean" or schema.get("properties", {}).get("holdKey", {}).get("type") != "string":
            raise ValueError("Missing configuration schema fields")
        for name in PAYLOAD - {"ContainerPreview.dll"}:
            text = contents[name].decode("utf-8")
            if re.search(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/]|\\\\[A-Za-z0-9_.-]+\\|gh[pousr]_[A-Za-z0-9]{20,}", text):
                raise ValueError("Private path or credential-like value in " + name)
    except (UnicodeError, json.JSONDecodeError, TypeError, AttributeError) as error:
        raise ValueError("Invalid package text/JSON") from error
    result = inspect_dll(contents["ContainerPreview.dll"], required_dll_sha256)
    result["files"] = sorted(contents)
    result["file_hashes"] = expected
    return result


def verify_package(path, expected_version=VERSION, required_dll_sha256=None):
    contents = {}
    for item in Path(path).iterdir():
        stat = item.lstat()
        if item.is_symlink() or getattr(stat, "st_file_attributes", 0) & 0x400 or not item.is_file() or stat.st_size > MAX_FILE:
            raise ValueError("Package contains a directory, link or oversized file")
        if item.name not in MEMBERS:
            raise ValueError("Non-whitelisted file: " + item.name)
        contents[item.name] = item.read_bytes()
    return verify_contents(contents, expected_version, required_dll_sha256)


def verify_zip(path, expected_version=VERSION, required_dll_sha256=None):
    contents = {}
    with zipfile.ZipFile(path) as archive:
        for entry in archive.infolist():
            if entry.filename not in MEMBERS or entry.filename in contents or entry.is_dir() or entry.file_size > MAX_FILE or entry.external_attr >> 16 & 0o170000 == 0o120000:
                raise ValueError("Unexpected, duplicate or unsafe ZIP member")
            contents[entry.filename] = archive.read(entry)
    return verify_contents(contents, expected_version, required_dll_sha256)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--dll", type=Path)
    group.add_argument("--package", type=Path)
    group.add_argument("--zip", type=Path)
    parser.add_argument("--expected-version", choices=(VERSION, "0.1.0-test31"), default=VERSION,
                        help="Use the explicit historical version only when auditing its retained package")
    parser.add_argument("--required-dll-sha256", help="Independently pin the gameplay-tested binary, not just its package checksums")
    args = parser.parse_args()
    try:
        result = (inspect_dll(args.dll.read_bytes(), args.required_dll_sha256) if args.dll else
                  verify_package(args.package, args.expected_version, args.required_dll_sha256) if args.package else
                  verify_zip(args.zip, args.expected_version, args.required_dll_sha256))
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, "Verification failed: " + str(error) + "\n")


if __name__ == "__main__":
    main()
