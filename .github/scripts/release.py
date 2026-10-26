"""Validate manually selected versions and prepare installable release archives."""

import argparse
import hashlib
import json
import re
import shutil
import tarfile
import tempfile
import zipfile
from pathlib import Path


PLATFORMS = ("windows-x64", "macos-universal", "ubuntu-24.04-x86_64")
DOCUMENTS = ("LICENSE", "README.md", "SPEC.md", "CHANGELOG.md", "docs")
VERSION = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?")


def metadata(root, version=None):
    spec = json.loads((root / "buildspec.json").read_text(encoding="utf-8"))
    selected = version if version is not None else spec["version"]
    match = VERSION.fullmatch(selected)
    if not match or any(int(part) > 65535 for part in match.groups()[:3]):
        raise ValueError("Use MAJOR.MINOR.PATCH with optional -beta1 / -beta.1 / -rc.1 suffix; components must be <= 65535")
    if match[4] and any(part.isdigit() and len(part) > 1 and part[0] == "0" for part in match[4].split(".")):
        raise ValueError("Numeric prerelease identifiers must not contain leading zeroes")
    if selected != spec["version"]:
        raise ValueError(f"Requested {selected}, but buildspec.json contains {spec['version']}; update and commit it first")
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", spec["name"]):
        raise ValueError("Invalid product filename")
    return spec


def filename(spec, platform):
    extension = ".zip" if platform == "windows-x64" else ".tar.xz"
    return f"{spec['name']}-{spec['version']}-{platform}{extension}"


def required_members(name, platform):
    if platform == "windows-x64":
        return (f"obs-plugins/64bit/{name}.dll", f"data/obs-plugins/{name}/locale/en-US.ini",
                f"data/obs-plugins/{name}/locale/ja-JP.ini", "LICENSE", "README.md", "SPEC.md")
    if platform == "macos-universal":
        prefix = f"{name}.plugin/Contents"
        return (f"{prefix}/MacOS/{name}", f"{prefix}/Resources/locale/en-US.ini",
                f"{prefix}/Resources/locale/ja-JP.ini", "LICENSE", "README.md", "SPEC.md")
    return (f"{name}/bin/64bit/{name}.so", f"{name}/data/locale/en-US.ini",
            f"{name}/data/locale/ja-JP.ini", "LICENSE", "README.md", "SPEC.md")


def verify_archive(path, members, is_zip=None):
    if is_zip is None:
        is_zip = path.suffix == ".zip"
    if is_zip:
        with zipfile.ZipFile(path) as archive:
            entries = {item.filename: item.file_size for item in archive.infolist()}
            if archive.testzip() is not None:
                raise ValueError(f"Corrupt ZIP: {path.name}")
    else:
        with tarfile.open(path, "r:xz") as archive:
            entries = {item.name.removeprefix("./"): item.size for item in archive if item.isfile()}
    for member in members:
        if entries.get(member, 0) == 0:
            raise ValueError(f"{path.name} is missing a non-empty {member}")


def copy_file(source, destination):
    if not source.is_file() or source.stat().st_size == 0:
        raise ValueError(f"Missing build output: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def create_archive(root, platform, configuration):
    spec = metadata(root)
    name = spec["name"]
    installed = root / "release" / configuration
    output = root / "release" / filename(spec, platform)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        staging = Path(temporary)
        if platform == "windows-x64":
            copy_file(installed / name / "bin/64bit" / f"{name}.dll", staging / "obs-plugins/64bit" / f"{name}.dll")
            shutil.copytree(installed / name / "data", staging / "data/obs-plugins" / name)
        elif platform == "macos-universal":
            shutil.copytree(installed / f"{name}.plugin", staging / f"{name}.plugin", symlinks=True)
        else:
            libraries = list((installed / "lib").rglob(f"{name}.so"))
            if len(libraries) != 1:
                raise ValueError(f"Expected one installed {name}.so; found {len(libraries)}")
            copy_file(libraries[0], staging / name / "bin/64bit" / f"{name}.so")
            shutil.copytree(installed / "share/obs/obs-plugins" / name, staging / name / "data")
        for document in DOCUMENTS:
            source = root / document
            if source.is_dir():
                shutil.copytree(source, staging / document)
            else:
                copy_file(source, staging / document)
        # Replace only after the complete archive passes the payload check.
        pending = output.with_name(output.name + ".pending")
        try:
            if platform == "windows-x64":
                with zipfile.ZipFile(pending, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                    for file in sorted(staging.rglob("*")):
                        if file.is_file():
                            archive.write(file, file.relative_to(staging).as_posix())
            else:
                with tarfile.open(pending, "w:xz") as archive:
                    for file in sorted(staging.iterdir()):
                        archive.add(file, arcname=file.name)
            verify_archive(pending, required_members(name, platform), is_zip=platform == "windows-x64")
            pending.replace(output)
        finally:
            pending.unlink(missing_ok=True)
    print(output)
    return output


def collect(root, version, artifacts, output):
    spec = metadata(root, version)
    if output.exists() and any(output.iterdir()):
        raise ValueError("Distribution output must be empty")
    output.mkdir(parents=True, exist_ok=True)
    expected = [filename(spec, platform) for platform in PLATFORMS]
    expected.append(f"{spec['name']}-{version}-source.tar.xz")
    selected = []
    for index, name in enumerate(expected):
        matches = list(artifacts.rglob(name))
        if len(matches) != 1:
            raise ValueError(f"Expected exactly one {name}; found {len(matches)}")
        required = required_members(spec["name"], PLATFORMS[index]) if index < 3 else (
            f"{spec['name']}-{version}-source/CMakeLists.txt",
            f"{spec['name']}-{version}-source/src/plugin-main.cpp",
            f"{spec['name']}-{version}-source/LICENSE",
        )
        verify_archive(matches[0], required)
        selected.append(matches[0])
    checksums = []
    for source in selected:
        copy_file(source, output / source.name)
        with source.open("rb") as stream:
            checksums.append(f"{hashlib.file_digest(stream, 'sha256').hexdigest()}  {source.name}\n")
    (output / "SHA256SUMS.txt").write_text("".join(checksums), encoding="utf-8")
    print(f"Validated {len(selected)} release archives in {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate")
    validate.add_argument("--version", required=True)
    validate.add_argument("--github-output", type=Path)
    archive = commands.add_parser("archive")
    archive.add_argument("--platform", choices=PLATFORMS, required=True)
    archive.add_argument("--config", choices=("Debug", "RelWithDebInfo", "Release", "MinSizeRel"), required=True)
    gather = commands.add_parser("collect")
    gather.add_argument("--version", required=True)
    gather.add_argument("--artifacts", type=Path, required=True)
    gather.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "validate":
        spec = metadata(args.root, args.version)
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8") as stream:
                stream.write(f"version={spec['version']}\nprerelease={str('-' in spec['version']).lower()}\n")
    elif args.command == "archive":
        create_archive(args.root, args.platform, args.config)
    else:
        collect(args.root, args.version, args.artifacts, args.output)


if __name__ == "__main__":
    main()
