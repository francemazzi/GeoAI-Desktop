#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from functools import cache
from pathlib import Path

# System paths that should be excluded from copying
SYSTEM_PATHS = [
    "/usr/lib",
    "/System/Library",
    "/Library/Frameworks",
]

PYTHON_RUNTIME_PACKAGES = ("packaging", "jinja2", "markupsafe", "psycopg2", "osgeo")


@dataclass
class Library:
    path: str
    install_name: str
    dependencies: list[str]
    rpaths: list[str]


@cache
def get_macho_info(path: str) -> bytes:
    """Run otool -l and cache the output."""
    result = subprocess.run(["otool", "-l", path], capture_output=True, check=True)
    return result.stdout


def parse_macho_info(path: str) -> Library:
    """Parse otool -l output to extract all needed information."""
    output = get_macho_info(path).decode("utf-8").split("\n")

    install_name = path
    rpaths = []
    dependencies = []

    i = 0
    while i < len(output):
        line = output[i].strip()

        # Look for load command type
        if "cmd LC_" not in line:
            i += 1
            continue

        cmd_type = line.split()[-1]

        if cmd_type == "LC_ID_DYLIB":
            # Next line is cmdsize, name is in the line after
            if i + 2 < len(output):
                name_line = output[i + 2].strip()
                if name_line.startswith("name"):
                    install_name = name_line.split()[1]

        elif cmd_type == "LC_LOAD_DYLIB" or cmd_type == "LC_LOAD_WEAK_DYLIB":
            # Next line is cmdsize, name is in the line after
            if i + 2 < len(output):
                name_line = output[i + 2].strip()
                if name_line.startswith("name"):
                    dep_path = name_line.split()[1]
                    if dep_path != install_name:
                        dependencies.append(dep_path)

        elif cmd_type == "LC_RPATH":
            # Next line is cmdsize, path is in the line after
            if i + 2 < len(output):
                path_line = output[i + 2].strip()
                if path_line.startswith("path"):
                    rpaths.append(path_line.split()[1])

        i += 1

    return Library(path, install_name, dependencies, rpaths)


def is_system_path(path: str) -> bool:
    """Check if the path is a system path that should be excluded."""
    return any(path.startswith(sys_path) for sys_path in SYSTEM_PATHS)


def find_library(lib_name: str, search_paths: list[str]) -> str:
    """Find library in search paths."""
    for path in search_paths:
        full_path = os.path.join(path, lib_name)
        if os.path.exists(full_path):
            return full_path
        framework_path = os.path.join(
            path, f"{lib_name}.framework", "Versions", "A", lib_name
        )
        if os.path.exists(framework_path):
            return framework_path
    return ""


def framework_details(path: str) -> tuple[Path, Path] | None:
    """Return a framework's root and binary-relative path when *path* is in one."""
    library_path = Path(path)
    for candidate in (library_path, *library_path.parents):
        if candidate.name.endswith(".framework"):
            return candidate, library_path.relative_to(candidate)
    return None


def macos_development_library_dirs() -> list[str]:
    """Return Homebrew library directories available on the packaging host.

    Formulae such as ``icu4c@78`` are intentionally not linked into the
    global prefix. Qt records them as ``@rpath`` dependencies, so make their
    opt-prefix lib directories available while resolving the deployment graph.
    Every resolved dependency is copied into the app bundle.
    """
    directories = []
    for opt_root in (Path("/opt/homebrew/opt"), Path("/usr/local/opt")):
        if not opt_root.is_dir():
            continue
        directories.extend(
            str(path / "lib") for path in opt_root.iterdir() if (path / "lib").is_dir()
        )
    return directories


def resolve_at_path(dep_path: str, binary_path: str, rpaths: list[str]) -> str:
    """
    Resolve a path that starts with @rpath, @executable_path, or @loader_path
    Returns resolved absolute path or empty string if not found
    """
    if dep_path.startswith("@rpath/"):
        lib_name = dep_path[len("@rpath/") :]
        # Try all rpaths
        for rpath in rpaths:
            # Handle nested @ paths in rpaths
            if rpath.startswith("@"):
                rpath = resolve_at_path(rpath, binary_path, [])
                if not rpath:
                    continue
            full_path = os.path.join(rpath, lib_name)
            if os.path.exists(full_path):
                return full_path
    elif dep_path.startswith("@executable_path/"):
        lib_name = dep_path[len("@executable_path/") :]
        exe_dir = os.path.dirname(binary_path)
        full_path = os.path.join(exe_dir, lib_name)
        if os.path.exists(full_path):
            return full_path
    elif dep_path.startswith("@loader_path/"):
        lib_name = dep_path[len("@loader_path/") :]
        loader_dir = os.path.dirname(binary_path)
        full_path = os.path.join(loader_dir, lib_name)
        if os.path.exists(full_path):
            return full_path
    return ""


def collect_dependencies(
    binary_path: str, lib_dirs: list[str], processed: set[str]
) -> dict[str, Library]:
    """Recursively collect all dependencies for a binary."""
    result = {}
    search_paths = lib_dirs.copy()

    def process_binary(path: str) -> None:
        if path in processed:
            return

        processed.add(path)
        real_path, _ = resolve_symlink(path)
        lib_info = parse_macho_info(real_path)
        result[path] = lib_info

        # Add library's directory to search paths if it's not a system path
        lib_dir = os.path.dirname(real_path)
        if lib_dir not in search_paths and not is_system_path(lib_dir):
            search_paths.append(lib_dir)

        # Process dependencies
        for dep in lib_info.dependencies:
            if dep.startswith("@"):
                # If we couldn't resolve it earlier, try again with updated search paths
                resolved_path = resolve_at_path(dep, path, lib_info.rpaths)
                if resolved_path:
                    dep = resolved_path

            if not os.path.isabs(dep):
                dep = find_library(os.path.basename(dep), search_paths)

            if dep and os.path.exists(dep):
                process_binary(dep)

    process_binary(binary_path)
    return result


def resolve_symlink(path: str) -> tuple[str, list[str]]:
    """
    Resolve a symlink chain to its final destination and return the real file path
    along with the chain of symlinks that led to it.
    """
    symlink_chain = []
    current_path = path

    while os.path.islink(current_path):
        symlink_chain.append(os.path.basename(current_path))
        current_path = os.path.realpath(current_path)

    return current_path, symlink_chain


def is_macho(filepath: str) -> bool:
    """
    Checks if a file is a Mach-O binary by reading the first 4 bytes.

    Args:
        filepath: Path to the file to check

    Returns:
        True if it is a Mach-O file
    """
    # Mach-O magic numbers
    MAGIC_64 = 0xCFFAEDFE  # 64-bit mach-o
    MAGIC_32 = 0xCEFAEDFE  # 32-bit mach-o

    try:
        # Open file in binary mode and read first 4 bytes
        with open(filepath, "rb") as f:
            magic = int.from_bytes(f.read(4), byteorder="big")

        if magic in (MAGIC_64, MAGIC_32):
            return True
        else:
            return False

    except OSError:
        return False


def collect_macho_files(root: str) -> list[str]:
    """Return every non-symlink Mach-O file below *root*."""
    binaries = []
    for directory, _, files in os.walk(root):
        for file in files:
            path = os.path.join(directory, file)
            if not os.path.islink(path) and is_macho(path):
                binaries.append(path)
    return binaries


def python_stdlib_path(python_library: str) -> Path | None:
    """Locate the standard library associated with a Python.framework binary."""
    library_path = Path(python_library).resolve()
    framework_root = next(
        (
            path
            for path in (library_path, *library_path.parents)
            if path.name == "Python.framework"
        ),
        None,
    )
    if framework_root is None:
        return None

    versions_dir = framework_root / "Versions"
    if not versions_dir.is_dir():
        return None

    for version_dir in sorted(versions_dir.iterdir()):
        if (
            version_dir.name == "Current"
            or version_dir.is_symlink()
            or not version_dir.is_dir()
        ):
            continue
        for stdlib_dir in sorted((version_dir / "lib").glob("python*")):
            if (stdlib_dir / "traceback.py").is_file():
                return stdlib_dir
    return None


def stage_python_stdlib(python_library: str, frameworks_dir: str) -> Path:
    """Copy Python's relocatable stdlib while excluding developer site-packages."""
    source_stdlib = python_stdlib_path(python_library)
    if source_stdlib is None:
        raise RuntimeError(
            f"Could not locate the Python standard library for {python_library}"
        )

    destination_stdlib = Path(frameworks_dir) / "lib" / source_stdlib.name
    print(f"Deploy Python standard library: {source_stdlib} -> {destination_stdlib}")
    shutil.copytree(
        source_stdlib,
        destination_stdlib,
        symlinks=True,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("site-packages", "__pycache__", "*.pyc"),
    )
    if not (destination_stdlib / "traceback.py").is_file():
        raise RuntimeError(
            f"Python standard library deployment is incomplete: {destination_stdlib / 'traceback.py'} is missing"
        )
    return destination_stdlib


def python_site_packages_path(python_library: str) -> Path:
    """Locate the runtime site-packages for the embedded Python framework."""
    source_stdlib = python_stdlib_path(python_library)
    if source_stdlib is None:
        raise RuntimeError(
            f"Could not locate the Python standard library for {python_library}"
        )

    version_dir = source_stdlib.parent.parent
    version = source_stdlib.name.removeprefix("python")
    candidates = [
        version_dir / "bin" / f"python{version}",
        version_dir / "bin" / "python3",
        version_dir / "bin" / "python",
    ]
    python_executable = next((path for path in candidates if path.is_file()), None)
    if python_executable is None:
        raise RuntimeError(
            f"Could not locate the Python executable for {python_library}"
        )

    environment = os.environ.copy()
    environment.pop("PYTHONHOME", None)
    environment.pop("PYTHONPATH", None)
    result = subprocess.run(
        [
            str(python_executable),
            "-c",
            "import sysconfig; print(sysconfig.get_path('platlib'))",
        ],
        check=True,
        capture_output=True,
        text=True,
        env=environment,
    )
    site_packages = Path(result.stdout.strip())
    if not site_packages.is_dir():
        raise RuntimeError(
            f"Could not locate Python runtime site-packages for {python_library}: {site_packages}"
        )
    return site_packages


def stage_python_runtime_packages(
    python_library: str, frameworks_dir: str
) -> list[str]:
    """Stage the PyQt runtime required by PyQGIS without development packages."""
    source_package = python_site_packages_path(python_library) / "PyQt6"
    if not source_package.is_dir():
        raise RuntimeError(
            f"Required PyQt6 runtime package is missing: {source_package}"
        )

    destination_package = Path(frameworks_dir) / "PyQt6"
    print(f"Deploy PyQt6 runtime: {source_package} -> {destination_package}")
    shutil.copytree(
        source_package,
        destination_package,
        # Homebrew's public site-packages tree is mostly links into its Cellar.
        # Resolve them while staging so the distributed bundle has no external
        # Python-package dependency.
        symlinks=False,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
    )
    if not any(destination_package.glob("QtCore*.so")):
        raise RuntimeError(
            f"PyQt6 runtime deployment is incomplete: QtCore extension missing in {destination_package}"
        )
    return collect_macho_files(str(destination_package))


def validate_python_runtime_packages(frameworks_dir: str) -> None:
    """Ensure CPack staged the explicit Python runtime package allow-list."""
    missing = [
        package
        for package in PYTHON_RUNTIME_PACKAGES
        if not (Path(frameworks_dir) / package / "__init__.py").is_file()
    ]
    if missing:
        raise RuntimeError(
            "Bundled Python runtime packages are missing: " + ", ".join(missing)
        )


def qt_plugin_directory(library_paths: list[str]) -> Path | None:
    """Locate the Qt plugin directory associated with a QtCore framework."""
    for library_path in library_paths:
        framework = framework_details(library_path)
        if framework is None or framework[0].name != "QtCore.framework":
            continue
        for parent in framework[0].parents:
            plugins_dir = parent / "share" / "qt" / "plugins"
            if (plugins_dir / "platforms" / "libqcocoa.dylib").is_file():
                return plugins_dir
    return None


def qt_svg_plugin(library_paths: list[str]) -> Path | None:
    """Locate the SVG image plugin belonging to the bundled QtSvg framework."""
    for library_path in library_paths:
        framework = framework_details(library_path)
        if framework is None or framework[0].name != "QtSvg.framework":
            continue
        for parent in framework[0].parents:
            svg_plugin = (
                parent / "share" / "qt" / "plugins" / "imageformats" / "libqsvg.dylib"
            )
            if svg_plugin.is_file():
                return svg_plugin
    return None


def qt_qml_directory(library_paths: list[str]) -> Path | None:
    """Locate Qt's QML import tree from the QtCore framework dependency."""
    candidates = []
    for library_path in library_paths:
        framework = framework_details(library_path)
        if framework is None or framework[0].name != "QtCore.framework":
            continue
        for parent in framework[0].parents:
            qml_dir = parent / "share" / "qt" / "qml"
            if (qml_dir / "QtQuick" / "Controls" / "qmldir").is_file():
                candidates.append(qml_dir)

    if not candidates:
        return None

    # Prefer the aggregate import tree for the same reason as the plugin tree:
    # it includes imports contributed by Qt modules outside QtDeclarative.
    return max(candidates, key=lambda path: sum(1 for _ in path.rglob("*")))


def stage_qt_plugins(app_bundle: str, library_paths: list[str]) -> list[str]:
    """Deploy Qt plugins and configure Qt to load them from Contents/PlugIns."""
    source_plugins = qt_plugin_directory(library_paths)
    if source_plugins is None:
        raise RuntimeError("Could not locate Qt's Cocoa platform plugin")

    contents_dir = Path(app_bundle) / "Contents"
    destination_plugins = contents_dir / "PlugIns"
    print(f"Deploy Qt plugins: {source_plugins} -> {destination_plugins}")
    shutil.copytree(
        source_plugins,
        destination_plugins,
        # Materialize links so the final app cannot load a plugin from the
        # packager machine.
        symlinks=False,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("objects-*", "*.o", "*.prl"),
    )

    # Qt's base plugin tree does not contain the SVG image handler, but the
    # bundled welcome screen renders SVG resources through it. Stage precisely
    # that runtime plugin instead of the complete aggregate plugin tree, which
    # also contains tooling and development-only plugins.
    source_svg_plugin = qt_svg_plugin(library_paths)
    if source_svg_plugin is None:
        raise RuntimeError("Could not locate Qt's SVG image plugin")
    destination_svg_plugin = destination_plugins / "imageformats" / "libqsvg.dylib"
    destination_svg_plugin.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_svg_plugin, destination_svg_plugin)
    (contents_dir / "Resources" / "qt.conf").write_text(
        "[Paths]\nPrefix = ..\nPlugins = PlugIns\nQmlImports = Resources/qml\n",
        encoding="utf-8",
    )
    return collect_macho_files(str(destination_plugins))


def stage_qt_qml_imports(app_bundle: str, library_paths: list[str]) -> list[str]:
    """Deploy the Qt QML import tree required by the bundled welcome screen."""
    source_qml = qt_qml_directory(library_paths)
    if source_qml is None:
        raise RuntimeError("Could not locate Qt's QML import tree")

    destination_qml = Path(app_bundle) / "Contents" / "Resources" / "qml"
    print(f"Deploy Qt QML imports: {source_qml} -> {destination_qml}")
    # The Homebrew QML tree contains links into its Cellar. Materialize those
    # links so the distributed app is self-contained.
    shutil.copytree(
        source_qml,
        destination_qml,
        symlinks=False,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("objects-*", "*.o", "*.prl"),
    )

    controls_dir = destination_qml / "QtQuick" / "Controls"
    if not (controls_dir / "qmldir").is_file() or not any(
        controls_dir.glob("*qtquickcontrols2plugin.*")
    ):
        raise RuntimeError(
            "Qt QML deployment is incomplete: QtQuick.Controls plugin is missing"
        )

    return collect_macho_files(str(destination_qml))


def stage_qca_plugins(app_bundle: str, library_paths: list[str]) -> list[str]:
    """Deploy QCA crypto plugins so they do not fall back to Homebrew."""
    source_plugins = None
    for library_path in library_paths:
        framework = framework_details(library_path)
        if framework is None or framework[0].name != "qca-qt6.framework":
            continue
        for parent in framework[0].parents:
            plugins_dir = parent / "lib" / "qt" / "plugins"
            if (plugins_dir / "crypto" / "libqca-botan.dylib").is_file():
                source_plugins = plugins_dir
                break
        if source_plugins is not None:
            break

    if source_plugins is None:
        return []

    contents_dir = Path(app_bundle) / "Contents"
    destination_plugins = contents_dir / "PlugIns"
    source_plugin_binaries = [
        binary
        for binary in collect_macho_files(str(source_plugins))
        if Path(binary).suffix in (".dylib", ".so")
    ]
    print(f"Deploy QCA plugins: {source_plugins} -> {destination_plugins}")
    shutil.copytree(
        source_plugins,
        destination_plugins,
        symlinks=True,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("objects-*", "*.o", "*.prl"),
    )
    return [
        str(destination_plugins / Path(binary).relative_to(source_plugins))
        for binary in source_plugin_binaries
    ]


def stage_openssl_modules(app_bundle: str, library_paths: list[str]) -> list[str]:
    """Deploy OpenSSL providers used dynamically by QCA's OpenSSL plugin."""
    source_modules = None
    for library_path in library_paths:
        resolved_library = Path(library_path).resolve()
        if not resolved_library.name.startswith("libcrypto"):
            continue
        modules_dir = resolved_library.parent / "ossl-modules"
        if modules_dir.is_dir():
            source_modules = modules_dir
            break

    if source_modules is None:
        return []

    destination_modules = (
        Path(app_bundle) / "Contents" / "Resources" / "openssl-modules"
    )
    print(f"Deploy OpenSSL providers: {source_modules} -> {destination_modules}")
    # Formulae commonly use links into their Cellar here. Resolve them so the
    # providers do not load a development libcrypto at runtime.
    shutil.copytree(
        source_modules,
        destination_modules,
        symlinks=False,
        dirs_exist_ok=True,
    )
    return collect_macho_files(str(destination_modules))


def handle_resources_binaries(app_bundle: str) -> None:
    """
    Move Mach-O files from Contents/Resources to Contents/PlugIns/_Resources
    and replace them with symlinks.
    """
    resources_dir = os.path.join(app_bundle, "Contents", "Resources")
    plugins_resources_dir = os.path.join(
        app_bundle, "Contents", "PlugIns", "_Resources"
    )

    if not os.path.exists(resources_dir):
        return

    # Find all Mach-O files in Resources
    for root, _, files in os.walk(resources_dir):
        for file in files:
            path = os.path.join(root, file)
            try:
                if is_macho(file):
                    # Calculate relative path from Resources root
                    rel_path = os.path.relpath(path, resources_dir)
                    new_path = os.path.join(plugins_resources_dir, rel_path)

                    # Create directory structure in PlugIns/_Resources
                    os.makedirs(os.path.dirname(new_path), exist_ok=True)

                    # Move the file and create symlink
                    shutil.move(path, new_path)
                    relative_target = os.path.relpath(new_path, os.path.dirname(path))
                    os.symlink(relative_target, path)
            except subprocess.CalledProcessError:
                continue


def deploy_libraries(app_bundle: str, lib_dirs: list[str]) -> None:
    """Deploy all libraries to the app bundle."""
    frameworks_dir = os.path.join(app_bundle, "Contents", "Frameworks")
    os.makedirs(frameworks_dir, exist_ok=True)

    print("Handle resources binaries")
    # Handle Resources binaries first
    handle_resources_binaries(app_bundle)

    print("Handle main binaries")
    # Find all binaries in the app bundle
    binaries = collect_macho_files(app_bundle)
    # Signing the app executable also validates nested code.  Therefore sign
    # libraries, plugins and Python extension modules before Contents/MacOS.
    binaries.sort(key=lambda path: "/Contents/MacOS/" in path)

    processed_libs = set()
    all_dependencies = {}

    # Collect all dependencies
    for binary in binaries:
        print(f"Analyzing {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    python_libraries = []
    for library_path in all_dependencies:
        real_library_path, _ = resolve_symlink(library_path)
        if python_stdlib_path(real_library_path) is not None:
            python_libraries.append(real_library_path)

    if not python_libraries:
        raise RuntimeError(
            "Could not locate a Python.framework dependency to deploy its standard library"
        )

    validate_python_runtime_packages(frameworks_dir)

    qt_plugin_binaries = stage_qt_plugins(app_bundle, list(all_dependencies))
    binaries.extend(qt_plugin_binaries)
    for binary in qt_plugin_binaries:
        print(f"Analyzing Qt plugin {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    qt_qml_binaries = stage_qt_qml_imports(app_bundle, list(all_dependencies))
    binaries.extend(qt_qml_binaries)
    for binary in qt_qml_binaries:
        print(f"Analyzing Qt QML plugin {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    qca_plugin_binaries = stage_qca_plugins(app_bundle, list(all_dependencies))
    binaries.extend(qca_plugin_binaries)
    for binary in qca_plugin_binaries:
        print(f"Analyzing QCA plugin {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    openssl_module_binaries = stage_openssl_modules(app_bundle, list(all_dependencies))
    binaries.extend(openssl_module_binaries)
    for binary in openssl_module_binaries:
        print(f"Analyzing OpenSSL provider {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    python_stdlib = stage_python_stdlib(python_libraries[0], frameworks_dir)
    python_extension_binaries = collect_macho_files(str(python_stdlib / "lib-dynload"))
    binaries.extend(python_extension_binaries)
    for binary in python_extension_binaries:
        print(f"Analyzing Python extension {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    pyqt_extension_binaries = stage_python_runtime_packages(
        python_libraries[0], frameworks_dir
    )
    binaries.extend(pyqt_extension_binaries)
    for binary in pyqt_extension_binaries:
        print(f"Analyzing PyQt6 extension {binary}")
        deps = collect_dependencies(binary, lib_dirs, processed_libs)
        all_dependencies.update(deps)

    # Copy libraries and prepare install_name_tool commands
    commands = {}  # path -> list of changes
    lib_mapping = {}  # old_install_name -> new_install_name
    deployed_paths = {}  # source dependency path -> copied app-bundle path

    # First pass: copy libraries and record their new install names
    for lib_path, lib_info in all_dependencies.items():
        if lib_path.startswith(app_bundle):
            continue

        # Skip system libraries
        if is_system_path(lib_path):
            continue

        # Resolve symlinks to get real file
        real_lib_path, symlink_chain = resolve_symlink(lib_path)

        # Skip if the real file is in a system path
        if is_system_path(real_lib_path):
            continue

        framework = framework_details(lib_path)
        # Python's shared library remains flat: its stdlib is staged separately
        # under Frameworks/lib/pythonX.Y.  Qt and other frameworks must retain
        # their framework directory structure for CFBundle/QLibraryInfo.
        is_framework = framework is not None and framework[0].name != "Python.framework"
        if is_framework:
            framework_root, framework_relative_path = framework
            destination_framework_root = Path(frameworks_dir) / framework_root.name
            new_path = str(destination_framework_root / framework_relative_path)
            new_install_name = (
                f"@rpath/{framework_root.name}/{framework_relative_path.as_posix()}"
            )
        else:
            lib_name = os.path.basename(real_lib_path)
            new_path = os.path.join(frameworks_dir, lib_name)
            new_install_name = f"@rpath/{lib_name}"
        deployed_paths[lib_path] = new_path
        deployed_paths[real_lib_path] = new_path

        # Record the mapping from old install name to new install name
        lib_mapping[lib_info.install_name] = new_install_name
        lib_mapping[lib_path] = new_install_name
        lib_mapping[real_lib_path] = new_install_name
        if is_framework:
            lib_mapping[
                f"@rpath/{framework_root.name}/{framework_relative_path.as_posix()}"
            ] = new_install_name
            if not destination_framework_root.exists():
                shutil.copytree(
                    framework_root,
                    destination_framework_root,
                    symlinks=True,
                )
        else:
            # Copy the real file if not already present
            if not os.path.exists(new_path):
                shutil.copy2(real_lib_path, new_path)

            # Recreate symlink chain
            current_name = lib_name
            for link_name in reversed(symlink_chain):
                link_path = os.path.join(frameworks_dir, link_name)
                if not os.path.exists(link_path):
                    os.symlink(current_name, link_path)
                current_name = link_name

        # Prepare commands for the library itself
        if new_path not in commands:
            commands[new_path] = []

            # Set its own install name
            commands[new_path].append(("-id", new_install_name))

    # Second pass: update each binary's direct dependencies
    for binary_path, lib_info in all_dependencies.items():
        deployed_binary_path = deployed_paths.get(binary_path, binary_path)
        if deployed_binary_path not in commands:
            commands[deployed_binary_path] = []

        # Update only the direct dependencies of this binary
        for dep in lib_info.dependencies:
            if dep in lib_mapping:
                commands[deployed_binary_path].append(
                    ("-change", dep, lib_mapping[dep])
                )
    frameworks_dir = os.path.join(app_bundle, "Contents", "Frameworks")

    def calculate_relative_frameworks_path(binary_path: str) -> str:
        """Calculate relative path from binary to Frameworks directory."""
        binary_dir = os.path.dirname(binary_path)
        rel_path = os.path.relpath(frameworks_dir, binary_dir)
        return rel_path

    # A framework is copied as a directory, including helpers such as
    # QtWebEngineProcess. Those nested Mach-O executables are not necessarily
    # represented by the original dependency graph after the framework copy,
    # so make one final pass over the deployed bundle. This also handles
    # framework dependencies recorded with an absolute Homebrew install name.
    for binary in collect_macho_files(app_bundle):
        lib_info = parse_macho_info(binary)
        changes = commands.setdefault(binary, [])
        for dep in lib_info.dependencies:
            replacement = lib_mapping.get(dep)
            if replacement is None:
                framework = framework_details(dep)
                if framework is not None:
                    framework_root, framework_relative_path = framework
                    deployed_framework_binary = (
                        Path(frameworks_dir)
                        / framework_root.name
                        / framework_relative_path
                    )
                    if deployed_framework_binary.exists():
                        replacement = (
                            f"@rpath/{framework_root.name}/"
                            f"{framework_relative_path.as_posix()}"
                        )
            if replacement is None or replacement == dep:
                continue
            change = ("-change", dep, replacement)
            if change not in changes:
                changes.append(change)

    # Remove developer rpaths from both the original bundle binaries and every
    # dependency copied into Frameworks. The latter are not necessarily in the
    # initial binary scan, but may otherwise retain a Homebrew Cellar rpath.
    for binary in set(binaries).union(commands):
        if binary not in commands:
            commands[binary] = []

        # Get existing rpaths
        lib_info = parse_macho_info(binary)
        # Delete absolute rpaths
        for rpath in lib_info.rpaths:
            if rpath.startswith("/"):
                commands[binary].append(("-delete_rpath", rpath))

        # Add proper search path for all executables
        rel_frameworks_path = calculate_relative_frameworks_path(binary)
        new_path = f"@loader_path/{rel_frameworks_path}"
        if new_path not in lib_info.rpaths:
            commands[binary].append(("-add_rpath", new_path))

    # Execute install_name_tool commands
    for path, changes in commands.items():
        print(f"Changing {path}")
        cmd = ["install_name_tool"]
        if not changes:
            continue
        print(f"  {changes}")
        for command_tuple in changes:
            cmd.extend(command_tuple)
        print(f"Executing {cmd} {path}")
        try:
            result = subprocess.run(
                cmd + [path], check=True, capture_output=True, text=True
            )
            print(result.stdout)
            print(result.stderr)
        except subprocess.CalledProcessError as e:
            print(f"Command failed with exit code {e.returncode}")
            print("stdout:")
            print(e.stdout)
            print("stderr:")
            print(e.stderr)
            raise


def sign_bundle(app_bundle: str, identity: str) -> None:
    """Sign every executable code object after install-name changes.

    The embedded stdlib intentionally lives in ``Frameworks/lib/pythonX.Y``.
    ``codesign --deep`` treats that dotted directory as a nested bundle and
    rejects the outer app. Signing each Mach-O object avoids that heuristic
    while still replacing every signature invalidated by relocation. The app
    executables are signed from temporary standalone copies: signing them in
    ``Contents/MacOS`` would otherwise validate the entire app and reject the
    dotted stdlib directory.
    """
    binaries = collect_macho_files(app_bundle)
    macos_dir = os.path.join(app_bundle, "Contents", "MacOS")
    print(f"Signing {len(binaries)} Mach-O files with identity: {identity}")
    for binary in binaries:
        if os.path.dirname(binary) == macos_dir:
            with tempfile.TemporaryDirectory() as temporary_directory:
                standalone_binary = os.path.join(
                    temporary_directory, os.path.basename(binary)
                )
                shutil.copy2(binary, standalone_binary)
                subprocess.run(
                    ["codesign", "--force", "--sign", identity, standalone_binary],
                    check=True,
                )
                shutil.copy2(standalone_binary, binary)
            continue
        subprocess.run(
            ["codesign", "--force", "--sign", identity, binary],
            check=True,
        )


def main():
    parser = argparse.ArgumentParser(description="Enhanced macdeployqt implementation")
    parser.add_argument("app_bundle", help="Path to the app bundle")
    parser.add_argument(
        "--libdir",
        action="append",
        default=[],
        help="Additional library search directories",
    )
    parser.add_argument(
        "-codesign",
        dest="codesign_identity",
        default="-",
        help="Code-signing identity; defaults to ad-hoc signing for local bundles",
    )
    parser.add_argument(
        "-sign-for-notarization",
        dest="notarization_identity",
        default=None,
        help="Compatibility option supplied by CPack's release-signing flow",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "-skip-sign",
        action="store_true",
        help="Relocate libraries without signing; used for pre-sign packaging checks",
    )
    mode.add_argument(
        "-sign-only",
        action="store_true",
        help="Sign an already relocated bundle without changing its contents",
    )

    args = parser.parse_args()

    lib_dirs = (
        args.libdir
        + [os.path.join(args.app_bundle, "Contents", "Frameworks")]
        + macos_development_library_dirs()
    )
    if args.sign_only:
        sign_bundle(args.app_bundle, args.codesign_identity)
    else:
        deploy_libraries(args.app_bundle, lib_dirs)
        if not args.skip_sign:
            sign_bundle(args.app_bundle, args.codesign_identity)


if __name__ == "__main__":
    main()
