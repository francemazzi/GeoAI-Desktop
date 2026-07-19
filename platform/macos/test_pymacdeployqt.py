#!/usr/bin/env python3

"""Regression tests for the macOS Python runtime bundle validation."""

import importlib.util
import tempfile
import unittest
from pathlib import Path

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


if __name__ == "__main__":
    unittest.main()
