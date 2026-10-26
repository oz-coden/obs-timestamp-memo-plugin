"""Exercise distribution layouts, version checks and complete release collection."""

import contextlib
import hashlib
import importlib.util
import io
import json
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("release.py")
loader = importlib.util.spec_from_file_location("release", SCRIPT)
release = importlib.util.module_from_spec(loader)
loader.loader.exec_module(release)
NAME = "obs-timestamp-memo"


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.set_version("1.2.3-beta.1")
        for path in ("LICENSE", "README.md", "SPEC.md", "CHANGELOG.md", "docs/RELEASING.md"):
            self.write(path, "Documentation\n")
        installed = "release/Release"
        self.write(f"{installed}/{NAME}/bin/64bit/{NAME}.dll", "Windows binary")
        self.write(f"{installed}/{NAME}/bin/64bit/{NAME}.pdb", "Do not distribute in install ZIP")
        self.write(f"{installed}/{NAME}.plugin/Contents/MacOS/{NAME}", "macOS binary")
        (self.root / installed / f"{NAME}.plugin/Contents/MacOS/{NAME}").chmod(0o755)
        self.write(f"{installed}/lib/obs-plugins/{NAME}.so", "Linux binary")
        for data in (f"{NAME}/data", f"{NAME}.plugin/Contents/Resources", f"share/obs/obs-plugins/{NAME}"):
            for locale in ("en-US", "ja-JP"):
                self.write(f"{installed}/{data}/locale/{locale}.ini", "Plugin locale\n")

    def write(self, path, content):
        destination = self.root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(content, encoding="utf-8")
        return destination

    def set_version(self, version):
        self.spec = {
            "name": NAME, "version": version, "website": "https://example.com",
            "author": "Test", "email": "test@example.com",
            "platformConfig": {"macos": {"bundleId": "com.example.test"}},
        }
        self.write("buildspec.json", json.dumps(self.spec))

    def archive(self, platform):
        with contextlib.redirect_stdout(io.StringIO()):
            return release.create_archive(self.root, platform, "Release")

    def artifacts(self):
        artifacts = self.root / "artifacts"
        artifacts.mkdir()
        for platform in release.PLATFORMS:
            target = artifacts / platform
            target.mkdir()
            shutil.copy2(self.archive(platform), target)
        prefix = f"{NAME}-{self.spec['version']}-source"
        source = self.root / "source-tree"
        for path in ("CMakeLists.txt", "src/plugin-main.cpp", "LICENSE"):
            file = source / prefix / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("source\n", encoding="utf-8")
        with tarfile.open(artifacts / f"{prefix}.tar.xz", "w:xz") as archive:
            archive.add(source / prefix, arcname=prefix)
        return artifacts

    def collect(self, artifacts):
        output = self.root / "distribution"
        with contextlib.redirect_stdout(io.StringIO()):
            release.collect(self.root, self.spec["version"], artifacts, output)
        return output

    def test_version_validation(self):
        for version in ("0.0.0", "1.2.3", "65535.0.1", "1.0.0-beta1", "1.0.0-rc.1", "1.0.0-BETA.1"):
            with self.subTest(version=version):
                self.set_version(version)
                self.assertEqual(release.metadata(self.root, version)["version"], version)
        for version in ("v1.0.0", "1.0", "01.0.0", "65536.0.0", "1.0.0-01", "1.0.0-rc..1",
                        "1.0.0+build", "1.0.0\nother=true", "$(command)"):
            with self.subTest(version=version):
                self.set_version(version)
                with self.assertRaises(ValueError):
                    release.metadata(self.root, version)
        self.set_version("1.2.3")
        with self.assertRaisesRegex(ValueError, "update and commit"):
            release.metadata(self.root, "1.2.4")

    def test_windows_archive_is_obs_root_layout(self):
        with zipfile.ZipFile(self.archive("windows-x64")) as archive:
            self.assertEqual(archive.read(f"obs-plugins/64bit/{NAME}.dll"), b"Windows binary")
            self.assertIn(f"data/obs-plugins/{NAME}/locale/ja-JP.ini", archive.namelist())
            self.assertIn("docs/RELEASING.md", archive.namelist())
            self.assertFalse(any(path.endswith(".pdb") for path in archive.namelist()))

    def test_macos_bundle_and_executable_permissions(self):
        with tarfile.open(self.archive("macos-universal")) as archive:
            member = archive.getmember(f"{NAME}.plugin/Contents/MacOS/{NAME}")
            self.assertTrue(member.mode & 0o111)
            self.assertEqual(archive.extractfile(member).read(), b"macOS binary")

    def test_linux_copies_resolved_versioned_library(self):
        library = self.root / f"release/Release/lib/obs-plugins/{NAME}.so"
        versioned = library.with_name(f"{NAME}.so.1.2.3")
        library.rename(versioned)
        try:
            library.symlink_to(versioned.name)
        except OSError as error:
            self.skipTest(f"Creating symlinks is unavailable: {error}")
        with tarfile.open(self.archive("ubuntu-24.04-x86_64")) as archive:
            member = archive.getmember(f"{NAME}/bin/64bit/{NAME}.so")
            self.assertTrue(member.isfile())
            self.assertEqual(archive.extractfile(member).read(), b"Linux binary")

    def test_invalid_payload_keeps_previous_archive(self):
        output = self.archive("windows-x64")
        previous = output.read_bytes()
        locale = self.root / f"release/Release/{NAME}/data/locale/ja-JP.ini"
        locale.write_text("", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "non-empty"):
            self.archive("windows-x64")
        self.assertEqual(output.read_bytes(), previous)
        self.assertFalse(output.with_name(output.name + ".pending").exists())
        (self.root / f"release/Release/{NAME}/bin/64bit/{NAME}.dll").unlink()
        with self.assertRaisesRegex(ValueError, "Missing build output"):
            self.archive("windows-x64")

    def test_collect_checksums_match_all_four_archives(self):
        output = self.collect(self.artifacts())
        lines = (output / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(lines), 4)
        self.assertEqual(len(list(output.iterdir())), 5)
        for line in lines:
            digest, filename = line.split("  ")
            self.assertIn("1.2.3-beta.1", filename)
            self.assertEqual(digest, hashlib.sha256((output / filename).read_bytes()).hexdigest())

    def test_collect_rejects_missing_duplicate_or_broken_artifacts(self):
        artifacts = self.artifacts()
        source = next(artifacts.glob("*-source.tar.xz"))
        source_bytes = source.read_bytes()
        source.unlink()
        with self.assertRaisesRegex(ValueError, "found 0"):
            self.collect(artifacts)
        source.write_bytes(source_bytes)
        duplicate = artifacts / "duplicate"
        duplicate.mkdir()
        shutil.copy2(source, duplicate)
        with self.assertRaisesRegex(ValueError, "found 2"):
            self.collect(artifacts)
        shutil.rmtree(duplicate)
        with tarfile.open(source, "w:xz"):
            pass
        with self.assertRaisesRegex(ValueError, "missing a non-empty"):
            self.collect(artifacts)
        self.assertFalse(any((self.root / "distribution").iterdir()))

    @unittest.skipUnless(shutil.which("cmake"), "CMake is not installed")
    def test_prerelease_bootstrap_can_configure_cmake_project(self):
        # Exercise the actual bootstrap with inert platform modules, without
        # downloading OBS or requiring the host's compiler/Qt installation.
        common = self.root / "cmake/common"
        common.mkdir(parents=True)
        shutil.copy2(SCRIPT.parents[2] / "cmake/common/bootstrap.cmake", common)
        for module in ("osconfig", "buildnumber"):
            (common / f"{module}.cmake").write_text("# Test fixture\n", encoding="utf-8")
        self.write("CMakeLists.txt", '''cmake_minimum_required(VERSION 3.28)
include(cmake/common/bootstrap.cmake)
project(version_probe VERSION ${PLUGIN_VERSION_NUMERIC} LANGUAGES NONE)
if(NOT PROJECT_VERSION STREQUAL "1.2.3" OR NOT PLUGIN_VERSION STREQUAL "1.2.3-beta.1")
  message(FATAL_ERROR "Prerelease version was not preserved correctly")
endif()
''')
        result = subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.root / "build")],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
