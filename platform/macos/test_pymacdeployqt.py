#!/usr/bin/env python3

"""Regression tests for the macOS Python runtime bundle validation."""

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

MODULE_PATH = Path(__file__).with_name("pymacdeployqt.py")
SPEC = importlib.util.spec_from_file_location("pymacdeployqt", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
PYMACDEPLOYQT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PYMACDEPLOYQT)


class PythonRuntimePackageValidationTest(unittest.TestCase):
    """Cover the CPack layout used by flat vcpkg Python installations."""

    def test_accepts_runtime_packages_in_frameworks(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            frameworks_dir = Path(temporary_directory) / "Contents" / "Frameworks"
            for package in PYMACDEPLOYQT.PYTHON_RUNTIME_PACKAGES:
                package_dir = frameworks_dir / package
                package_dir.mkdir(parents=True, exist_ok=True)
                (package_dir / "__init__.py").touch()

            PYMACDEPLOYQT.validate_python_runtime_packages(str(frameworks_dir))

    def test_accepts_runtime_packages_in_bundled_site_packages(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            frameworks_dir = Path(temporary_directory) / "Contents" / "Frameworks"
            site_packages = frameworks_dir / "lib" / "python3.12" / "site-packages"
            for package in PYMACDEPLOYQT.PYTHON_RUNTIME_PACKAGES:
                package_dir = site_packages / package
                package_dir.mkdir(parents=True, exist_ok=True)
                (package_dir / "__init__.py").touch()

            PYMACDEPLOYQT.validate_python_runtime_packages(str(frameworks_dir))

    def test_rejects_missing_runtime_package(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            frameworks_dir = Path(temporary_directory) / "Contents" / "Frameworks"
            site_packages = frameworks_dir / "lib" / "python3.12" / "site-packages"
            for package in PYMACDEPLOYQT.PYTHON_RUNTIME_PACKAGES[:-1]:
                package_dir = site_packages / package
                package_dir.mkdir(parents=True, exist_ok=True)
                (package_dir / "__init__.py").touch()

            with self.assertRaisesRegex(RuntimeError, "osgeo"):
                PYMACDEPLOYQT.validate_python_runtime_packages(str(frameworks_dir))

    def test_reuses_already_staged_python_stdlib(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            frameworks_dir = Path(temporary_directory) / "Contents" / "Frameworks"
            python_library = frameworks_dir / "libpython3.12.dylib"
            stdlib_dir = frameworks_dir / "lib" / "python3.12"
            python_library.parent.mkdir(parents=True, exist_ok=True)
            python_library.touch()
            (stdlib_dir / "traceback.py").parent.mkdir(parents=True, exist_ok=True)
            (stdlib_dir / "traceback.py").touch()

            self.assertEqual(
                PYMACDEPLOYQT.stage_python_stdlib(
                    str(python_library), str(frameworks_dir)
                ),
                stdlib_dir,
            )

    def test_discovers_staged_python_site_packages_without_running_python(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            frameworks_dir = Path(temporary_directory) / "Contents" / "Frameworks"
            python_library = frameworks_dir / "libpython3.12.dylib"
            site_packages = frameworks_dir / "lib" / "python3.12" / "site-packages"
            python_library.parent.mkdir(parents=True, exist_ok=True)
            python_library.touch()
            site_packages.mkdir(parents=True)
            (site_packages.parent / "traceback.py").touch()

            self.assertEqual(
                PYMACDEPLOYQT.python_site_packages_path(str(python_library)),
                site_packages.resolve(),
            )


class QtRuntimeDiscoveryTest(unittest.TestCase):
    """Cover flat vcpkg Qt resources and already-staged CPack resources."""

    def test_finds_flat_vcpkg_qt_resources(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefix = Path(temporary_directory) / "vcpkg"
            qt_core = prefix / "lib" / "libQt6Core.6.dylib"
            qt_svg = prefix / "lib" / "libQt6Svg.6.dylib"
            cocoa = prefix / "plugins" / "platforms" / "libqcocoa.dylib"
            svg = prefix / "plugins" / "imageformats" / "libqsvg.dylib"
            qml_manifest = (
                prefix / "share" / "qt" / "qml" / "QtQuick" / "Controls" / "qmldir"
            )
            for path in (qt_core, qt_svg, cocoa, svg, qml_manifest):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()

            self.assertEqual(
                PYMACDEPLOYQT.qt_plugin_directory([str(qt_core)]),
                prefix / "plugins",
            )
            self.assertEqual(PYMACDEPLOYQT.qt_svg_plugin([str(qt_svg)]), svg)
            self.assertEqual(
                PYMACDEPLOYQT.qt_qml_directory([str(qt_core)]),
                prefix / "share" / "qt" / "qml",
            )

    def test_finds_framework_qt_resources(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefix = Path(temporary_directory) / "qt"
            qt_core = prefix / "lib" / "QtCore.framework" / "Versions" / "A" / "QtCore"
            cocoa = (
                prefix / "share" / "qt" / "plugins" / "platforms" / "libqcocoa.dylib"
            )
            for path in (qt_core, cocoa):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()

            self.assertEqual(
                PYMACDEPLOYQT.qt_plugin_directory([str(qt_core)]),
                prefix / "share" / "qt" / "plugins",
            )

    def test_reuses_staged_qt_resources(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            app_bundle = Path(temporary_directory) / "Strata.app"
            cocoa = (
                app_bundle / "Contents" / "PlugIns" / "platforms" / "libqcocoa.dylib"
            )
            svg = app_bundle / "Contents" / "PlugIns" / "imageformats" / "libqsvg.dylib"
            qml_manifest = (
                app_bundle
                / "Contents"
                / "Qt6"
                / "qml"
                / "QtQuick"
                / "Controls"
                / "qmldir"
            )
            qml_plugin = qml_manifest.with_name("libqtquickcontrols2plugin.dylib")
            for path in (cocoa, svg, qml_manifest, qml_plugin):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()

            self.assertEqual(PYMACDEPLOYQT.stage_qt_plugins(str(app_bundle), []), [])
            self.assertEqual(
                PYMACDEPLOYQT.stage_qt_qml_imports(str(app_bundle), []), []
            )
            self.assertTrue(
                (
                    app_bundle
                    / "Contents"
                    / "Resources"
                    / "qml"
                    / "QtQuick"
                    / "Controls"
                    / "qmldir"
                ).is_file()
            )

    def test_does_not_copy_staged_plugins_onto_themselves(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            app_bundle = Path(temporary_directory) / "Strata.app"
            plugins = app_bundle / "Contents" / "PlugIns"
            cocoa = plugins / "platforms" / "libqcocoa.dylib"
            svg = plugins / "imageformats" / "libqsvg.dylib"
            for path in (cocoa, svg):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()

            source_plugins = Path(temporary_directory) / "plugins-alias"
            source_plugins.symlink_to(plugins, target_is_directory=True)
            with (
                patch.object(
                    PYMACDEPLOYQT, "qt_plugin_directory", return_value=source_plugins
                ),
                patch.object(
                    PYMACDEPLOYQT,
                    "qt_svg_plugin",
                    return_value=source_plugins / "imageformats" / "libqsvg.dylib",
                ),
            ):
                self.assertEqual(
                    PYMACDEPLOYQT.stage_qt_plugins(str(app_bundle), []), []
                )

    def test_removes_only_reported_duplicate_rpath(self):
        changes = [
            ("-change", "old", "new"),
            ("-add_rpath", "@loader_path/."),
            ("-add_rpath", "@loader_path/../Frameworks"),
        ]
        stderr = (
            "install_name_tool: option '-add_rpath @loader_path/.' would duplicate "
            "path, file already has LC_RPATH for: @loader_path/."
        )
        self.assertEqual(
            PYMACDEPLOYQT.without_duplicate_rpath_command(changes, stderr),
            [
                ("-change", "old", "new"),
                ("-add_rpath", "@loader_path/../Frameworks"),
            ],
        )

    def test_removes_lone_duplicate_rpath(self):
        self.assertEqual(
            PYMACDEPLOYQT.without_duplicate_rpath_command(
                [("-add_rpath", "@loader_path/.")],
                "install_name_tool: option '-add_rpath @loader_path/.' would "
                "duplicate path, file already has LC_RPATH for: @loader_path/.",
            ),
            [],
        )


if __name__ == "__main__":
    unittest.main()
