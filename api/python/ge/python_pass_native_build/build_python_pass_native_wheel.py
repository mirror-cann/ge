#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""Build the Python pass native artifact wheel."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path


PACKAGE_NAME = "ge-py-pass-bridge"
DIST_NAME = "ge_py_pass_bridge"
COMPAT_NATIVE_MODULE = "_ge_pass_native.so"


def _copy_artifact_files(artifact_dir: Path, artifact_root: Path) -> None:
    for file_path in sorted(artifact_dir.rglob("*")):
        if file_path.is_file():
            target_path = artifact_root / file_path.relative_to(artifact_dir)
            target_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file_path, target_path)


def _copy_compat_native_module(artifact_dir: Path, project_dir: Path) -> None:
    native_module = artifact_dir / COMPAT_NATIVE_MODULE
    if not native_module.is_file():
        raise RuntimeError(f"Cannot find native module for compatibility import: {native_module}")
    compat_module = project_dir / "ge" / "passes" / COMPAT_NATIVE_MODULE
    compat_module.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(native_module, compat_module)


def _write_setup_py(project_dir: Path, version: str) -> None:
    setup_content = f"""
import os

from setuptools import find_namespace_packages
from setuptools import setup
from setuptools.dist import Distribution
from wheel.bdist_wheel import bdist_wheel


class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        return True


class NativeBdistWheel(bdist_wheel):
    def get_tag(self):
        python_tag, abi_tag, platform_tag = super().get_tag()
        forced_tag = os.environ.get("GE_PY_PASS_WHEEL_PYTHON_TAG")
        if forced_tag:
            return forced_tag, forced_tag, platform_tag
        return python_tag, abi_tag, platform_tag


setup(
    name="{PACKAGE_NAME}",
    version="{version}",
    description="GraphEngine Python pass native artifacts",
    packages=find_namespace_packages(include=["ge", "ge.*"]),
    include_package_data=True,
    package_data={{"ge.passes": ["{COMPAT_NATIVE_MODULE}", "python_pass_artifacts/*/*"]}},
    distclass=BinaryDistribution,
    cmdclass={{"bdist_wheel": NativeBdistWheel}},
    zip_safe=False,
)
"""
    (project_dir / "setup.py").write_text(textwrap.dedent(setup_content).lstrip(), encoding="utf-8")


def _run_bdist_wheel(project_dir: Path, output_dir: Path, python_tag: str, wheel_platform: str) -> None:
    env = os.environ.copy()
    env["GE_PY_PASS_WHEEL_PYTHON_TAG"] = python_tag
    command = [
        sys.executable,
        "setup.py",
        "bdist_wheel",
        "--python-tag",
        python_tag,
        "--plat-name",
        wheel_platform,
        "--dist-dir",
        os.fspath(output_dir),
    ]
    subprocess.run(command, cwd=os.fspath(project_dir), env=env, check=True)


def build_wheel(args: argparse.Namespace) -> Path:
    artifact_dir = Path(args.artifact_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    artifact_set_name = f"{args.python_tag}-{args.platform_tag}"
    project_dir = output_dir.parent / f"_build_{DIST_NAME}_{artifact_set_name}"
    if project_dir.exists():
        shutil.rmtree(project_dir)
    artifact_root = project_dir / "ge" / "passes" / "python_pass_artifacts" / artifact_set_name
    artifact_root.mkdir(parents=True, exist_ok=True)
    _copy_artifact_files(artifact_dir, artifact_root)
    _copy_compat_native_module(artifact_dir, project_dir)
    _write_setup_py(project_dir, args.version)
    try:
        _run_bdist_wheel(project_dir, output_dir, args.python_tag, args.wheel_platform)
    finally:
        shutil.rmtree(project_dir, ignore_errors=True)

    wheel_name = f"{DIST_NAME}-{args.version}-{args.python_tag}-{args.python_tag}-{args.wheel_platform}.whl"
    wheel_path = output_dir / wheel_name
    if not wheel_path.is_file():
        raise RuntimeError(f"Cannot find generated native wheel: {wheel_path}")
    return wheel_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", required=True)
    parser.add_argument("--python-tag", required=True)
    parser.add_argument("--platform-tag", required=True)
    parser.add_argument("--wheel-platform", required=True)
    parser.add_argument("--version", default="0.0.1")
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args()


def main() -> None:
    wheel_path = build_wheel(parse_args())
    print(os.fspath(wheel_path))


if __name__ == "__main__":
    main()
