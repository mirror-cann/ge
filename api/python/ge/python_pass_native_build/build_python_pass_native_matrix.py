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

"""Build Python pass native wheels for all supported Python versions found in PATH or Conda envs."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import textwrap
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

SUPPORTED_TAGS = ("cp39", "cp310", "cp311", "cp312", "cp313", "cp314")
BRIDGE_SOURCE = "compiler/graph/fusion/pass/python_pass_pybind_bridge.cc"
NATIVE_SOURCES = (
    "api/python/ge/ge/passes/native_bindings/module.cc",
    "api/python/ge/ge/passes/native_bindings/pass_context_binding.cc",
    "api/python/ge/ge/passes/native_bindings/pattern_binding.cc",
    "api/python/ge/ge/passes/native_bindings/match_result_binding.cc",
    "api/python/ge/ge/passes/native_bindings/pattern_matcher_config_binding.cc",
    "api/python/ge/ge/passes/native_bindings/graph_handle_binding.cc",
    "api/python/ge/ge/passes/native_bindings/graph_rewriter_binding.cc",
)


@dataclass
class TargetBuildInfo:
    cwd: Path
    defines: List[str]
    includes: List[str]
    flags: List[str]
    link_args: List[str]


@dataclass
class PythonBuildInfo:
    tag: str
    executable: str
    version: str
    include_dir: Path
    library: Optional[Path]
    pybind_include: Optional[Path]


def _repo_root() -> Path:
    for parent in Path(__file__).resolve().parents:
        if (parent / "api" / "python" / "ge").is_dir() and (parent / "CMakeLists.txt").is_file():
            return parent
    raise RuntimeError("Cannot locate repository root")


def _run(
    command: Sequence[str],
    cwd: Optional[Path] = None,
    extra_env: Optional[Dict[str, str]] = None,
) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    return subprocess.run(
        command,
        cwd=os.fspath(cwd) if cwd is not None else None,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def _python_tag(python: str) -> Optional[Tuple[str, str]]:
    command = [
        python,
        "-c",
        "import sys; print('cp%d%d' % sys.version_info[:2]); print(sys.executable)",
    ]
    completed = _run(command)
    if completed.returncode != 0:
        return None
    lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
    if len(lines) < 2:
        return None
    return lines[0], lines[1]


def _candidate_python_commands() -> Iterable[str]:
    for tag in SUPPORTED_TAGS:
        yield f"python{tag[2]}.{tag[3:]}"
    yield "python3"
    yield "python"


def _python_in_env(env_dir: Path) -> Path:
    return env_dir / "bin" / "python"


def _append_conda_python(candidates: List[str], env_dir: Path) -> None:
    python = _python_in_env(env_dir)
    if python.is_file():
        candidates.append(os.fspath(python))


def _conda_executable_candidates() -> Iterable[str]:
    conda_exe = os.environ.get("CONDA_EXE")
    if conda_exe:
        yield conda_exe
    yield "conda"


def _conda_envs_from_command() -> List[str]:
    for conda in _conda_executable_candidates():
        conda_path = shutil.which(conda)
        if conda_path is None:
            conda_file = Path(conda)
            if not conda_file.is_file():
                continue
            conda_path = os.fspath(conda_file)
        completed = _run([conda_path, "env", "list", "--json"])
        if completed.returncode != 0:
            continue
        try:
            envs = json.loads(completed.stdout).get("envs", [])
        except json.JSONDecodeError:
            continue
        return [env for env in envs if isinstance(env, str)]
    return []


def _add_conda_env_root(roots: List[Path], root: Path) -> None:
    if root.is_dir() and root not in roots:
        roots.append(root)


def _conda_env_roots() -> List[Path]:
    roots: List[Path] = []
    conda_prefix = os.environ.get("CONDA_PREFIX")
    if conda_prefix:
        prefix = Path(conda_prefix)
        if prefix.parent.name == "envs":
            _add_conda_env_root(roots, prefix.parent)
        else:
            _add_conda_env_root(roots, prefix / "envs")

    conda_exe = os.environ.get("CONDA_EXE")
    if conda_exe:
        exe = Path(conda_exe)
        if len(exe.parents) >= 2:
            _add_conda_env_root(roots, exe.parents[1] / "envs")

    home = Path.home()
    for root in (
        home / "miniconda3" / "envs",
        home / "anaconda3" / "envs",
        home / ".conda" / "envs",
    ):
        _add_conda_env_root(roots, root)
    return roots


def _conda_python_candidates() -> Iterable[str]:
    candidates: List[str] = []
    for env in _conda_envs_from_command():
        _append_conda_python(candidates, Path(env))
    for root in _conda_env_roots():
        for env_dir in sorted(root.iterdir()):
            if env_dir.is_dir():
                _append_conda_python(candidates, env_dir)
    return candidates


def _discover_pythons(explicit_pythons: Sequence[str], requested_tags: Sequence[str]) -> Dict[str, str]:
    requested = set(requested_tags or SUPPORTED_TAGS)
    discovered: Dict[str, str] = {}
    seen = set()
    for python in [
        *explicit_pythons,
        *_candidate_python_commands(),
        *_conda_python_candidates(),
    ]:
        python_path = shutil.which(python)
        if python_path is None:
            candidate_path = Path(python)
            if not candidate_path.is_file():
                continue
            python_path = os.fspath(candidate_path)
        real_path = os.path.realpath(python_path)
        if real_path in seen:
            continue
        seen.add(real_path)
        tag_and_executable = _python_tag(python_path)
        if tag_and_executable is None:
            continue
        tag, executable = tag_and_executable
        if tag in requested and tag in SUPPORTED_TAGS and tag not in discovered:
            discovered[tag] = executable
    return discovered


def _query_python_build_info(tag: str, python: str) -> Optional[PythonBuildInfo]:
    query = r"""
import json
import os
import sys
import sysconfig

try:
    import pybind11
    pybind_include = pybind11.get_include()
except Exception:
    pybind_include = ""

version = sysconfig.get_config_var("VERSION") or f"{sys.version_info.major}.{sys.version_info.minor}"
libdir = sysconfig.get_config_var("LIBDIR") or ""
candidates = [
    sysconfig.get_config_var("LDLIBRARY"),
    sysconfig.get_config_var("INSTSONAME"),
    sysconfig.get_config_var("LIBRARY"),
    f"libpython{version}.so.1.0" if version else "",
    f"libpython{version}.so" if version else "",
]
seen = []
for candidate in candidates:
    if candidate and candidate not in seen:
        seen.append(candidate)
matches = [os.path.join(libdir, item) for item in seen if libdir and os.path.exists(os.path.join(libdir, item))]
shared = next((item for item in matches if ".so" in os.path.basename(item)), "")
print(json.dumps({
    "version": sys.version.split()[0],
    "include": sysconfig.get_path("include") or sysconfig.get_config_var("INCLUDEPY") or "",
    "library": shared or (matches[0] if matches else ""),
    "pybind_include": pybind_include,
    "executable": sys.executable,
}))
"""
    completed = _run([python, "-c", textwrap.dedent(query)])
    if completed.returncode != 0:
        return None
    try:
        info = json.loads(completed.stdout)
    except json.JSONDecodeError:
        return None
    include_dir = Path(info.get("include", ""))
    library_value = info.get("library", "")
    library = Path(library_value) if library_value else None
    pybind_include = Path(info["pybind_include"]) if info.get("pybind_include") else None
    if not include_dir.is_dir():
        return None
    if library is not None and not library.is_file():
        library = None
    if pybind_include is not None and not pybind_include.is_dir():
        pybind_include = None
    return PythonBuildInfo(
        tag=tag,
        executable=info.get("executable", python),
        version=info.get("version", ""),
        include_dir=include_dir,
        library=library,
        pybind_include=pybind_include,
    )


def _read_make_variable(path: Path, name: str) -> List[str]:
    prefix = f"{name} = "
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            return shlex.split(line[len(prefix) :])
    return []


def _load_target_build_info(build_dir: Path, relative_dir: str, target_name: str) -> TargetBuildInfo:
    target_dir = build_dir / relative_dir / "CMakeFiles" / f"{target_name}.dir"
    flags_path = target_dir / "flags.make"
    link_path = target_dir / "link.txt"
    if not flags_path.is_file() or not link_path.is_file():
        raise RuntimeError(f"Cannot find generated build metadata for target {target_name} under {target_dir}")
    return TargetBuildInfo(
        cwd=build_dir / relative_dir,
        defines=_read_make_variable(flags_path, "CXX_DEFINES"),
        includes=_read_make_variable(flags_path, "CXX_INCLUDES"),
        flags=_read_make_variable(flags_path, "CXX_FLAGS"),
        link_args=shlex.split(link_path.read_text(encoding="utf-8").strip()),
    )


def _python_include_args(_target: TargetBuildInfo, python_info: PythonBuildInfo) -> List[str]:
    pybind_include = python_info.pybind_include
    if pybind_include is None:
        raise RuntimeError(
            f"Cannot find pybind11 include for {python_info.executable}. Please install pybind11 for this Python."
        )
    # Keep the selected Python/pybind11 includes ahead of the parent build includes. Otherwise a matrix build can
    # accidentally pick the parent build Python headers when compiling another cp tag.
    return ["-I", os.fspath(python_info.include_dir), "-I", os.fspath(pybind_include)]


def _is_python_or_pybind_include(path: str) -> bool:
    parts = Path(path).parts
    return any(part.startswith("python3.") for part in parts) or ("pybind11" in parts)


def _filter_python_related_includes(tokens: Sequence[str]) -> List[str]:
    filtered: List[str] = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token in ("-I", "-isystem") and index + 1 < len(tokens):
            path = tokens[index + 1]
            if _is_python_or_pybind_include(path):
                index += 2
                continue
            filtered.extend([token, path])
            index += 2
            continue
        if token.startswith("-I") and len(token) > 2 and _is_python_or_pybind_include(token[2:]):
            index += 1
            continue
        filtered.append(token)
        index += 1
    return filtered


def _compile_base_args(target: TargetBuildInfo, python_info: PythonBuildInfo) -> List[str]:
    return (
        target.defines
        + _python_include_args(target, python_info)
        + _filter_python_related_includes(target.includes)
        + target.flags
    )


def _is_werror_flag(flag: str) -> bool:
    return flag == "-Werror" or flag.startswith("-Werror=")


def _without_werror(target: TargetBuildInfo) -> Tuple[TargetBuildInfo, List[str]]:
    removed = [flag for flag in target.flags if _is_werror_flag(flag)]
    if not removed:
        return target, []
    return TargetBuildInfo(
        cwd=target.cwd,
        defines=target.defines,
        includes=target.includes,
        flags=[flag for flag in target.flags if not _is_werror_flag(flag)],
        link_args=target.link_args,
    ), removed


def _version_from_tag(tag: str) -> Tuple[int, int]:
    version = tag[2:]
    return int(version[0]), int(version[1:])


def _compiler_command(args: argparse.Namespace) -> List[str]:
    command: List[str] = []
    if args.cxx_compiler_launcher:
        command.append(args.cxx_compiler_launcher)
    command.append(args.cxx_compiler)
    return command


def _can_compile_python_header(
    args: argparse.Namespace,
    target: TargetBuildInfo,
    output_dir: Path,
    python_info: PythonBuildInfo,
) -> Tuple[bool, str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    source = output_dir / "check_python_header.cc"
    obj = output_dir / "check_python_header.o"
    major, minor = _version_from_tag(python_info.tag)
    source.write_text(
        textwrap.dedent(f"""
        #include <Python.h>
        #if PY_MAJOR_VERSION != {major} || PY_MINOR_VERSION != {minor}
        #error "Python header version does not match target tag"
        #endif
        int ge_python_header_probe() {{ return PY_MAJOR_VERSION; }}
        """),
        encoding="utf-8",
    )
    command = (
        _compiler_command(args)
        + _compile_base_args(target, python_info)
        + ["-c", os.fspath(source), "-o", os.fspath(obj)]
    )
    completed = _run(command, cwd=target.cwd)
    if completed.returncode == 0:
        return True, ""
    return False, completed.stdout.strip()


def _compile_sources(
    args: argparse.Namespace,
    target: TargetBuildInfo,
    sources: Sequence[Path],
    output_dir: Path,
    python_info: PythonBuildInfo,
) -> List[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    temp_dir = output_dir / "tmp"
    cache_dir = output_dir / "ccache"
    temp_dir.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(parents=True, exist_ok=True)
    compile_env = {
        "CCACHE_DIR": os.fspath(cache_dir),
        "CCACHE_TEMPDIR": os.fspath(temp_dir),
        "TMPDIR": os.fspath(temp_dir),
    }
    objects: List[Path] = []
    base_args = _compile_base_args(target, python_info)
    for index, source in enumerate(sources):
        obj = output_dir / f"{index}_{source.stem}.o"
        command = _compiler_command(args) + base_args + ["-c", os.fspath(source), "-o", os.fspath(obj)]
        completed = _run(command, cwd=target.cwd, extra_env=compile_env)
        if completed.returncode != 0:
            raise RuntimeError(f"Compile failed for {source} with {python_info.executable}:\n{completed.stdout}")
        objects.append(obj)
    return objects


def _is_object_arg(token: str) -> bool:
    return token.endswith(".o") or token.endswith(".obj")


def _is_libpython_arg(token: str) -> bool:
    return "libpython" in os.path.basename(token) or token.startswith("-lpython")


def _looks_like_python_lib_dir(path: str) -> bool:
    normalized = path.replace("\\", "/").lower()
    if ("/python-" in normalized or "/python3." in normalized) and normalized.endswith("/lib"):
        return True
    path_obj = Path(path)
    return path_obj.is_dir() and any(path_obj.glob("libpython*.so*"))


def _filter_rpath(token: str, removed_paths: List[str]) -> Optional[str]:
    prefix = "-Wl,-rpath,"
    if not token.startswith(prefix):
        return token
    kept_paths = []
    for path in token[len(prefix) :].split(":"):
        if _looks_like_python_lib_dir(path):
            removed_paths.append(path)
        else:
            kept_paths.append(path)
    if not kept_paths:
        return None
    return prefix + ":".join(kept_paths)


@dataclass
class LinkRewriteReport:
    replaced_libpython_args: List[str]
    removed_python_paths: List[str]


def _rewrite_link_args(
    target: TargetBuildInfo,
    output: Path,
    objects: Sequence[Path],
    python_info: PythonBuildInfo,
) -> Tuple[List[str], LinkRewriteReport]:
    args = target.link_args[1:] if target.link_args else []
    rewritten: List[str] = []
    replaced_libpython_args: List[str] = []
    removed_python_paths: List[str] = []
    objects_inserted = False
    index = 0
    while index < len(args):
        token = args[index]
        if token == "-o" and index + 1 < len(args):
            rewritten.extend(["-o", os.fspath(output)])
            index += 2
            continue
        if _is_object_arg(token):
            if not objects_inserted:
                rewritten.extend(os.fspath(obj) for obj in objects)
                objects_inserted = True
            index += 1
            continue
        if _is_libpython_arg(token):
            if python_info.library is not None:
                rewritten.append(os.fspath(python_info.library))
                replaced_libpython_args.append(token)
            index += 1
            continue
        if token == "-L" and index + 1 < len(args):
            path = args[index + 1]
            if _looks_like_python_lib_dir(path):
                removed_python_paths.append(path)
                index += 2
                continue
            rewritten.extend([token, path])
            index += 2
            continue
        if token.startswith("-L") and len(token) > 2 and _looks_like_python_lib_dir(token[2:]):
            removed_python_paths.append(token[2:])
            index += 1
            continue
        if token.startswith("-Wl,-rpath,"):
            rewritten_rpath = _filter_rpath(token, removed_python_paths)
            if rewritten_rpath is not None:
                rewritten.append(rewritten_rpath)
            index += 1
            continue
        rewritten.append(token)
        index += 1
    if not objects_inserted:
        rewritten.extend(os.fspath(obj) for obj in objects)
    return rewritten, LinkRewriteReport(replaced_libpython_args, removed_python_paths)


def _format_list(values: Sequence[str]) -> str:
    return ",".join(values) if values else "none"


def _format_optional_path(path: Optional[Path]) -> str:
    return os.fspath(path) if path is not None else "not-found"


def _link_command_has_python(command: Sequence[str]) -> bool:
    return (
        any(_is_libpython_arg(token) for token in command)
        or any(token.startswith("-L") and _looks_like_python_lib_dir(token[2:]) for token in command if len(token) > 2)
        or any(
            token.startswith("-Wl,-rpath,")
            and any(_looks_like_python_lib_dir(path) for path in token[len("-Wl,-rpath,") :].split(":"))
            for token in command
        )
    )


def _link_shared(
    args: argparse.Namespace,
    target: TargetBuildInfo,
    output: Path,
    objects: Sequence[Path],
    python_info: PythonBuildInfo,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    link_args, report = _rewrite_link_args(target, output, objects, python_info)
    command = [args.cxx_compiler] + link_args
    completed = _run(command, cwd=target.cwd)
    if completed.returncode != 0:
        raise RuntimeError(f"Link failed for {output} with {python_info.executable}:\n{completed.stdout}")
    print(
        f"Linked {python_info.tag} {output.name}: output[{output}], "
        f"python_link[{'yes' if _link_command_has_python(command) else 'no'}], "
        f"selected_libpython[{_format_optional_path(python_info.library)}], "
        f"replaced_libpython[{_format_list(report.replaced_libpython_args)}], "
        f"stripped_python_paths[{_format_list(report.removed_python_paths)}]"
    )


def _write_manifest(artifact_dir: Path, python_info: PythonBuildInfo, platform_tag: str, bridge_abi: int) -> None:
    manifest = {
        "python_tag": python_info.tag,
        "python_version": python_info.version,
        "platform": platform_tag,
        "bridge_abi": bridge_abi,
        "build_python": {
            "executable": python_info.executable,
            "version": python_info.version,
            "include": os.fspath(python_info.include_dir),
            "libpython": _format_optional_path(python_info.library),
            "pybind11_include": _format_optional_path(python_info.pybind_include),
            "link_python": True,
        },
        "artifacts": {
            "bridge": "libge_python_pass_bridge.so",
            "native": "_ge_pass_native.so",
        },
    }
    (artifact_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def _log_python_build_info(python_info: PythonBuildInfo) -> None:
    print(
        f"Build {python_info.tag} native artifacts with python[{python_info.executable}], "
        f"version[{python_info.version}], include[{python_info.include_dir}], "
        f"libpython[{_format_optional_path(python_info.library)}], "
        f"pybind11[{_format_optional_path(python_info.pybind_include)}], link_python[yes], rpath_python[no]"
    )


def _strip_optional_werror(
    tag: str, bridge_target: TargetBuildInfo, native_target: TargetBuildInfo
) -> Tuple[TargetBuildInfo, TargetBuildInfo]:
    bridge_target, removed_bridge_flags = _without_werror(bridge_target)
    native_target, removed_native_flags = _without_werror(native_target)
    removed_flags = removed_bridge_flags + removed_native_flags
    print(f"Build optional {tag} native artifacts without Werror: stripped_flags[{_format_list(removed_flags)}]")
    return bridge_target, native_target


def _build_native_artifacts(
    args: argparse.Namespace, python_info: PythonBuildInfo, is_current_tag: bool
) -> Optional[Path]:
    tag_work_dir = args.work_dir / python_info.tag
    if args.fresh and tag_work_dir.exists():
        shutil.rmtree(tag_work_dir)
    artifact_dir = tag_work_dir / "artifact"
    artifact_dir.mkdir(parents=True, exist_ok=True)

    bridge_target = _load_target_build_info(args.build_dir, "compiler", "ge_python_pass_bridge")
    native_target = _load_target_build_info(args.build_dir, "api/python/ge/ge/passes", "_ge_pass_native")
    if not is_current_tag:
        bridge_target, native_target = _strip_optional_werror(python_info.tag, bridge_target, native_target)
    header_ok, header_error = _can_compile_python_header(
        args, bridge_target, tag_work_dir / "header_probe", python_info
    )
    if not header_ok:
        print(
            f"Skip {python_info.tag}: Python development headers are not usable for {python_info.executable}:\n"
            f"{header_error}",
            file=sys.stderr,
        )
        return None
    bridge_objects = _compile_sources(
        args,
        bridge_target,
        [args.source_dir / BRIDGE_SOURCE],
        tag_work_dir / "bridge_obj",
        python_info,
    )
    native_sources = [args.source_dir / source for source in NATIVE_SOURCES]
    native_objects = _compile_sources(args, native_target, native_sources, tag_work_dir / "native_obj", python_info)

    _link_shared(
        args,
        bridge_target,
        artifact_dir / "libge_python_pass_bridge.so",
        bridge_objects,
        python_info,
    )
    _link_shared(
        args,
        native_target,
        artifact_dir / "_ge_pass_native.so",
        native_objects,
        python_info,
    )
    _write_manifest(artifact_dir, python_info, args.platform_tag, args.bridge_abi)
    return artifact_dir


def _build_wheel(args: argparse.Namespace, tag: str, artifact_dir: Path) -> Path:
    script = Path(__file__).resolve().with_name("build_python_pass_native_wheel.py")
    command = [
        sys.executable,
        os.fspath(script),
        "--artifact-dir",
        os.fspath(artifact_dir),
        "--python-tag",
        tag,
        "--platform-tag",
        args.platform_tag,
        "--wheel-platform",
        args.wheel_platform,
        "--output-dir",
        os.fspath(args.dist_dir),
    ]
    completed = _run(command)
    if completed.returncode != 0:
        raise RuntimeError(f"Build native wheel failed for {tag}:\n{completed.stdout}")
    wheels = sorted(args.dist_dir.glob(f"ge_py_pass_bridge-*-{tag}-{tag}-*.whl"))
    if not wheels:
        raise RuntimeError(f"No native wheel generated for {tag} under {args.dist_dir}")
    return wheels[-1]


def _copy_current_wheel(args: argparse.Namespace, tag: str) -> Optional[Path]:
    if tag != args.current_tag or not args.current_wheel or not args.current_wheel.is_file():
        return None
    args.dist_dir.mkdir(parents=True, exist_ok=True)
    target = args.dist_dir / args.current_wheel.name
    shutil.copy2(args.current_wheel, target)
    return target


def _build_one(args: argparse.Namespace, tag: str, python: str, is_current_tag: bool) -> List[Path]:
    if args.dry_run:
        print(f"{tag}: {python} -> {args.work_dir / tag}")
        return []
    python_info = _query_python_build_info(tag, python)
    if python_info is None:
        print(
            f"Skip {tag}: cannot resolve Python development include for {python}",
            file=sys.stderr,
        )
        return []
    if python_info.library is None:
        print(
            f"Skip {tag}: cannot resolve libpython for {python_info.executable}",
            file=sys.stderr,
        )
        return []
    if python_info.pybind_include is None:
        print(
            f"Skip {tag}: cannot resolve pybind11 include for {python_info.executable}. "
            f"Please install pybind11 for this Python.",
            file=sys.stderr,
        )
        return []
    _log_python_build_info(python_info)
    copied = _copy_current_wheel(args, tag)
    if copied is not None:
        print(f"Reuse current native wheel for {tag}: wheel[{copied}], link_result[parent ge_python_native_wheel]")
        return [copied]
    artifact_dir = _build_native_artifacts(args, python_info, is_current_tag)
    if artifact_dir is None:
        return []
    return [_build_wheel(args, tag, artifact_dir)]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=_repo_root())
    parser.add_argument(
        "--build-dir",
        type=Path,
        required=True,
        help="Parent CMake build directory. The matrix builder reuses its generated flags/link data.",
    )
    parser.add_argument("--work-dir", type=Path, default=None)
    parser.add_argument("--dist-dir", type=Path, default=None)
    parser.add_argument(
        "--python",
        action="append",
        default=[],
        help="Optional Python executable to include before PATH and Conda probing.",
    )
    parser.add_argument(
        "--tag",
        action="append",
        choices=SUPPORTED_TAGS,
        default=[],
        help="Optional supported Python tag filter.",
    )
    parser.add_argument("--platform-tag", required=True)
    parser.add_argument("--wheel-platform", required=True)
    parser.add_argument("--bridge-abi", type=int, required=True)
    parser.add_argument("--current-tag", default="")
    parser.add_argument("--current-wheel", type=Path, default=None)
    parser.add_argument("--cxx-compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--cxx-compiler-launcher", default="")
    parser.add_argument("--fresh", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    args.source_dir = args.source_dir.resolve()
    args.build_dir = args.build_dir.resolve()
    args.work_dir = (args.work_dir or (args.build_dir / "python_pass_native_matrix_build")).resolve()
    args.dist_dir = (args.dist_dir or (args.work_dir / "dist")).resolve()
    args.current_wheel = args.current_wheel.resolve() if args.current_wheel is not None else None
    return args


def main() -> None:
    args = parse_args()
    pythons = _discover_pythons(args.python, args.tag)
    if not pythons:
        raise RuntimeError("No supported Python interpreter found for native wheel matrix")
    if not args.dry_run:
        args.dist_dir.mkdir(parents=True, exist_ok=True)
        for wheel in args.dist_dir.glob("ge_py_pass_bridge-*.whl"):
            wheel.unlink()
        for build_dir in args.dist_dir.glob("_build_ge_py_pass_bridge_*"):
            if build_dir.is_dir():
                shutil.rmtree(build_dir)
    built_wheels: List[Path] = []
    built_tags: List[str] = []
    for tag in SUPPORTED_TAGS:
        python = pythons.get(tag)
        if python is None:
            continue
        is_current_tag = tag == args.current_tag
        try:
            tag_wheels = _build_one(args, tag, python, is_current_tag)
        except Exception as err:
            if is_current_tag:
                raise
            print(
                f"Skip optional {tag}: native wheel build failed for {python}:\n{err}",
                file=sys.stderr,
            )
            continue
        built_wheels.extend(tag_wheels)
        if tag_wheels:
            built_tags.append(tag)
    for wheel in built_wheels:
        print(os.fspath(wheel))
    print(f"GE python pass native wheel matrix built {len(built_tags)} tag(s): {', '.join(built_tags)}")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(err, file=sys.stderr)
        sys.exit(1)
