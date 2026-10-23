"""Tests exercise binary bytes and real temporary packages, not source text."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_artifacts import VERSION, inspect_dll, verify_package, verify_zip

PAYLOAD = ("ContainerPreview.dll", "config.json", "config.schema.json", "mod.json",
           "manifest.json", "README.md", "THIRD_PARTY_NOTICES.txt")


def dll_fixture():
    data = bytearray(0x1200)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x84, 0x8664, 1)
    struct.pack_into("<HH", data, 0x94, 240, 0x2022)
    struct.pack_into("<H", data, 0x98, 0x20B)
    struct.pack_into("<I", data, 0x98 + 60, 0x200)
    struct.pack_into("<I", data, 0x98 + 108, 16)
    struct.pack_into("<II", data, 0x98 + 112, 0x1000, 0x80)
    struct.pack_into("<II", data, 0x98 + 120, 0x1200, 40)
    data[0x188:0x190] = b".rdata\0\0"
    struct.pack_into("<IIII", data, 0x190, 0x1000, 0x1000, 0x1000, 0x200)
    struct.pack_into("<I", data, 0x188 + 36, 0x40000040)

    def off(rva):
        return rva - 0x1000 + 0x200

    struct.pack_into("<IIIIII", data, off(0x1000) + 16, 1, 2, 2, 0x1080, 0x1090, 0x10A0)
    struct.pack_into("<II", data, off(0x1080), 0x1800, 0x1810)
    struct.pack_into("<II", data, off(0x1090), 0x10C0, 0x10E0)
    struct.pack_into("<HH", data, off(0x10A0), 0, 1)
    for rva, value in ((0x10C0, b"ContainerPreviewInit\0"),
                       (0x10E0, b"ContainerPreviewStatus\0"),
                       (0x1300, b"KERNEL32.dll\0"), (0x1400, b"\0\0GetTickCount64\0")):
        data[off(rva):off(rva) + len(value)] = value
    struct.pack_into("<IIIII", data, off(0x1200), 0x1250, 0, 0, 0x1300, 0x1260)
    struct.pack_into("<QQ", data, off(0x1250), 0x1400, 0)
    return data


def checksums(path):
    lines = [hashlib.sha256((path / name).read_bytes()).hexdigest().upper() + "  " + name
             for name in sorted(PAYLOAD)]
    (path / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="ascii")


def package_fixture(path, version=VERSION):
    (path / "ContainerPreview.dll").write_bytes(dll_fixture())
    values = {
        "config.json": {"enabled": True, "holdKey": "LALT"},
        "config.schema.json": {"type": "object", "properties": {
            "enabled": {"type": "boolean"}, "holdKey": {"type": "string"}}},
        "mod.json": {"id": "container-preview", "name": "ContainerPreview", "version": version,
                     "dll": "ContainerPreview.dll", "config": "config.json", "schema": "config.schema.json",
                     "supportedGameVersions": ["1.26.45.01", "1.26.51.01", "1.26.52.03"]},
        "manifest.json": {"name": "ContainerPreview", "entry": "ContainerPreview.dll",
                          "version": version, "type": "preload-native"},
    }
    for name, value in values.items():
        (path / name).write_text(json.dumps(value), encoding="utf-8")
    (path / "README.md").write_text("# ContainerPreview\nLocal test.\n", encoding="utf-8")
    (path / "THIRD_PARTY_NOTICES.txt").write_text("MinHook / nlohmann JSON\n", encoding="utf-8")
    checksums(path)


class ArtifactTests(unittest.TestCase):
    def test_stable_cli_accepts_exact_retained_dll_and_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root, version="0.1.0")
            digest = hashlib.sha256((root / "ContainerPreview.dll").read_bytes()).hexdigest().upper()
            result = subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / "tools" / "verify_artifacts.py"),
                                     "--package", str(root), "--required-dll-sha256", digest],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["sha256"], digest)

    def test_rechecksummed_replacement_dll_cannot_pass_provenance_pin(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root, version="0.1.0")
            data = bytearray((root / "ContainerPreview.dll").read_bytes())
            accepted = hashlib.sha256(data).hexdigest().upper()
            data[-1] ^= 1  # PE still valid; only independent binary provenance differs.
            (root / "ContainerPreview.dll").write_bytes(data)
            checksums(root)
            archive = root.parent / (root.name + "-payload.zip")
            try:
                with zipfile.ZipFile(archive, "w") as out:
                    for item in root.iterdir():
                        out.write(item, item.name)
                for kind, path in (("--package", root), ("--zip", archive)):
                    with self.subTest(kind=kind):
                        result = subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / "tools" / "verify_artifacts.py"),
                                                 kind, str(path), "--required-dll-sha256", accepted],
                                                capture_output=True, text=True)
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("DLL provenance mismatch", result.stderr)
            finally:
                archive.unlink(missing_ok=True)

    def test_older_package_requires_explicit_expected_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root, version="0.1.0-test31")
            command = [sys.executable, str(Path(__file__).resolve().parents[1] / "tools" / "verify_artifacts.py"),
                       "--package", str(root)]
            refused = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(refused.returncode, 0)
            accepted = subprocess.run(command + ["--expected-version", "0.1.0-test31"], capture_output=True, text=True)
            self.assertEqual(accepted.returncode, 0, accepted.stderr)

    def test_inspects_pe_without_loading_or_executing(self):
        info = inspect_dll(dll_fixture())
        self.assertEqual(info.get("machine"), "AMD64")
        self.assertEqual(info.get("exports"), ["ContainerPreviewInit", "ContainerPreviewStatus"])
        self.assertEqual(info.get("imports"), ["kernel32.dll"])

    def test_wrong_machine_refused(self):
        data = dll_fixture()
        struct.pack_into("<H", data, 0x84, 0x14C)
        with self.assertRaises(ValueError):
            inspect_dll(data)

    def test_executable_without_dll_flag_refused(self):
        data = dll_fixture()
        struct.pack_into("<H", data, 0x96, 0x22)
        with self.assertRaises(ValueError):
            inspect_dll(data)

    def test_missing_export_refused(self):
        data = dll_fixture()
        data[0x2C0] = ord("X")
        with self.assertRaises(ValueError):
            inspect_dll(data)

    def test_unknown_dependency_refused(self):
        data = dll_fixture()
        data[0x500:0x50D] = b"evilhook.dll\0"
        with self.assertRaises(ValueError):
            inspect_dll(data)

    def test_delayed_import_cannot_bypass_dependency_checks(self):
        data = dll_fixture()
        struct.pack_into("<II", data, 0x98 + 112 + 13 * 8, 0x1500, 32)
        with self.assertRaises(ValueError):
            inspect_dll(data)

    def test_truncated_headers_and_unbacked_rva_refused(self):
        for data in (b"", b"MZ", dll_fixture()[:400]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                inspect_dll(data)
        data = dll_fixture()
        struct.pack_into("<I", data, 0x98 + 112, 0xFFFFFFF0)
        with self.assertRaises(ValueError):
            inspect_dll(data)

    def test_complete_package_and_zip_have_eight_flat_members(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "payload"
            root.mkdir()
            package_fixture(root)
            self.assertEqual(verify_package(root).get("files"), sorted(PAYLOAD + ("SHA256SUMS.txt",)))
            archive = Path(tmp) / "test.zip"
            with zipfile.ZipFile(archive, "w") as out:
                for item in root.iterdir():
                    out.write(item, item.name)
            self.assertEqual(verify_zip(archive).get("files"), sorted(PAYLOAD + ("SHA256SUMS.txt",)))

    def test_changed_file_fails_checksum(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root)
            (root / "README.md").write_text("Changed", encoding="utf-8")
            with self.assertRaises(ValueError):
                verify_package(root)

    def test_version_mismatch_fails_even_with_fresh_checksum(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root)
            value = json.loads((root / "manifest.json").read_text())
            value["version"] = "0.9.0"
            (root / "manifest.json").write_text(json.dumps(value))
            checksums(root)
            with self.assertRaises(ValueError):
                verify_package(root)

    def test_numeric_enabled_is_not_a_boolean_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root)
            (root / "config.json").write_text('{"enabled":1,"holdKey":"LALT"}')
            checksums(root)
            with self.assertRaises(ValueError):
                verify_package(root)

    def test_package_cannot_claim_unadmitted_game_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root)
            value = json.loads((root / "mod.json").read_text())
            value["supportedGameVersions"].append("1.26.53.01")
            (root / "mod.json").write_text(json.dumps(value))
            checksums(root)
            with self.assertRaisesRegex(ValueError, "supported game versions"):
                verify_package(root)

    def test_extra_file_and_missing_checksum_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root)
            (root / "private.log").write_text("private")
            with self.assertRaises(ValueError):
                verify_package(root)
            (root / "private.log").unlink()
            (root / "SHA256SUMS.txt").unlink()
            with self.assertRaises(ValueError):
                verify_package(root)

    def test_private_path_refused_even_with_correct_checksum(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_fixture(root)
            (root / "README.md").write_text(r"C:\Users\example\private", encoding="utf-8")
            checksums(root)
            with self.assertRaises(ValueError):
                verify_package(root)

    def test_archive_traversal_and_duplicate_members_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "test.zip"
            with zipfile.ZipFile(archive, "w") as out:
                out.writestr("../ContainerPreview.dll", b"not a DLL")
            with self.assertRaises(ValueError):
                verify_zip(archive)
            with zipfile.ZipFile(archive, "w") as out:
                out.writestr("a", b"1")
                import warnings
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    out.writestr("a", b"2")
            with self.assertRaises(ValueError):
                verify_zip(archive)


if __name__ == "__main__":
    unittest.main()
