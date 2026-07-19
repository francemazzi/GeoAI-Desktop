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


if __name__ == "__main__":
    unittest.main()
