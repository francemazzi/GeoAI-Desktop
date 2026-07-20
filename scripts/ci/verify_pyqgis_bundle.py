import argparse
import fnmatch
import os
import subprocess
import sys
import zipfile
from pathlib import Path

FORBIDDEN_MACOS_REFERENCES = (
    "/opt/homebrew/",
    "/usr/local/",
    "Library/Python/",
)

CHECKS = {
    "macos": (
        (
            "qgispython support library",
            lambda p: fnmatch.fnmatch(Path(p).name, "*qgispython*"),
        ),
        (
            "PyQGIS qgis Python package",
            lambda p: Path(p).name == "__init__.py" and "qgis" in Path(p).parts,
        ),
        ("PyQGIS core module", lambda p: fnmatch.fnmatch(Path(p).name, "_core*.so")),
        (
            "Python runtime",
            lambda p: (
                fnmatch.fnmatch(Path(p).name, "python3*")
                or Path(p).name == "Python"
                or "Python.framework" in Path(p).parts
            ),
        ),
        ("PyQt6 package", lambda p: "PyQt6" in Path(p).parts),
        ("PyQt6 Qsci module", lambda p: fnmatch.fnmatch(Path(p).name, "*Qsci*.so")),
        (
            "packaging Python runtime package",
            lambda p: Path(p).name == "__init__.py" and "packaging" in Path(p).parts,
        ),
        (
            "Jinja2 Python runtime package",
            lambda p: Path(p).name == "__init__.py" and "jinja2" in Path(p).parts,
        ),
        (
            "MarkupSafe Python runtime package",
            lambda p: Path(p).name == "__init__.py" and "markupsafe" in Path(p).parts,
        ),
        (
            "psycopg2 Python runtime package",
            lambda p: Path(p).name == "__init__.py" and "psycopg2" in Path(p).parts,
        ),
        (
            "GDAL osgeo Python runtime package",
            lambda p: Path(p).name == "__init__.py" and "osgeo" in Path(p).parts,
        ),
        (
            "QtQuick.Controls QML module manifest",
            lambda p: p.endswith("Contents/Resources/qml/QtQuick/Controls/qmldir"),
        ),
        (
            "QtQuick.Controls QML plugin",
            lambda p: (
                "QtQuick" in Path(p).parts
                and "Controls" in Path(p).parts
                and "qtquickcontrols2plugin" in Path(p).name
            ),
        ),
        (
            "Qt SVG image plugin",
            lambda p: p.endswith("Contents/PlugIns/imageformats/libqsvg.dylib"),
        ),
    ),
    "windows": (
        (
            "qgispython support library",
            lambda p: fnmatch.fnmatch(Path(p).name, "*qgispython*.dll"),
        ),
        (
            "PyQGIS qgis Python package",
            lambda p: Path(p).name == "__init__.py" and "qgis" in Path(p).parts,
        ),
        ("PyQGIS core module", lambda p: fnmatch.fnmatch(Path(p).name, "_core*.pyd")),
        (
            "Python runtime",
            lambda p: (
                Path(p).name.lower() == "python.exe"
                or fnmatch.fnmatch(Path(p).name.lower(), "python3*.dll")
            ),
        ),
        ("PyQt6 package", lambda p: "PyQt6" in Path(p).parts),
        ("PyQt6 Qsci module", lambda p: fnmatch.fnmatch(Path(p).name, "*Qsci*.pyd")),
    ),
}


def iter_artifact_paths(path):
    if path.is_dir():
        for root, dirs, files in os.walk(path, followlinks=False):
            root_path = Path(root)
            for name in dirs + files:
                yield (root_path / name).relative_to(path).as_posix()
        return

    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for name in archive.namelist():
                yield name
        return

    raise SystemExit(f"Unsupported artifact path: {path}")


def is_macho(path):
    try:
        with path.open("rb") as handle:
            magic = int.from_bytes(handle.read(4), byteorder="big")
    except OSError:
        return False
    return magic in (0xCFFAEDFE, 0xCEFAEDFE)


def has_forbidden_macos_reference(value):
    return any(reference in value for reference in FORBIDDEN_MACOS_REFERENCES)


def verify_macos_references(artifact):
    """Return external Homebrew or user-Python references in an app bundle."""
    if not artifact.is_dir():
        return []

    failures = []
    otool = "/usr/bin/otool"
    for root, dirs, files in os.walk(artifact, followlinks=False):
        root_path = Path(root)
        for name in dirs + files:
            path = root_path / name
            if path.is_symlink():
                target = os.readlink(path)
                resolved = os.path.realpath(path)
                if has_forbidden_macos_reference(
                    target
                ) or has_forbidden_macos_reference(resolved):
                    failures.append(f"external symlink {path}: {target}")
                continue

            if not path.is_file() or not is_macho(path):
                continue

            linked = subprocess.run(
                [otool, "-L", str(path)], capture_output=True, text=True, check=True
            )
            load_commands = subprocess.run(
                [otool, "-l", str(path)], capture_output=True, text=True, check=True
            )
            if has_forbidden_macos_reference(linked.stdout + load_commands.stdout):
                failures.append(f"external Mach-O reference in {path}")
    return failures


def main():
    parser = argparse.ArgumentParser(
        description="Verify a packaged Strata artifact contains PyQGIS runtime files."
    )
    parser.add_argument("--platform", choices=sorted(CHECKS), required=True)
    parser.add_argument("artifact", type=Path)
    args = parser.parse_args()

    if not args.artifact.exists():
        raise SystemExit(f"Artifact does not exist: {args.artifact}")

    paths = list(iter_artifact_paths(args.artifact))
    failed = False
    for label, predicate in CHECKS[args.platform]:
        match = next((path for path in paths if predicate(path)), None)
        if match is None:
            print(f"Missing {label} in {args.artifact}", file=sys.stderr)
            failed = True
        else:
            print(f"{label}: {match}")

    if args.platform == "macos":
        for failure in verify_macos_references(args.artifact):
            print(failure, file=sys.stderr)
            failed = True

    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
