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

"""Generate runtime fallback resources for GE Python pass."""

import argparse
import base64
import gzip
import json
import shlex
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


def _collect_files(files: Iterable[Path], subdir: str) -> List[Tuple[str, bytes]]:
    resources: List[Tuple[str, bytes]] = []
    for src in files:
        src_path = src.resolve()
        rel_path = f"{subdir}/{src_path.name}"
        resources.append((rel_path, src_path.read_bytes()))
    return resources


def _split_encoded_content(encoded_content: str) -> str:
    chunks = [encoded_content[index : index + 76] for index in range(0, len(encoded_content), 76)]
    return "\n".join(f'        "{chunk}"' for chunk in chunks)


def _build_resource_module(resources: Dict[str, bytes]) -> str:
    lines = [
        "#!/usr/bin/env python3",
        "# -*- coding: utf-8 -*-",
        "# -----------------------------------------------------------------------------------------------------------",
        "# Copyright (c) 2026 Huawei Technologies Co., Ltd.",
        "# This program is free software, you can redistribute it and/or modify it under the terms and conditions of",
        '# CANN Open Software License Agreement Version 2.0 (the "License").',
        "# Please refer to the License for details. You may not use this file except in compliance with the License.",
        '# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,',
        "# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.",
        "# See LICENSE in the root of the software repository for the full text of the License.",
        "# -----------------------------------------------------------------------------------------------------------",
        "",
        '"""Generated fallback source resources for GE Python pass."""',
        "",
        "import base64",
        "import gzip",
        "from pathlib import Path",
        "",
        "_RESOURCES = {",
    ]
    for rel_path, content in sorted(resources.items()):
        encoded_content = base64.b64encode(gzip.compress(content, mtime=0)).decode("ascii")
        lines.append(f"    {rel_path!r}: (")
        lines.append(_split_encoded_content(encoded_content))
        lines.append("    ),")
    lines.extend(
        [
            "}",
            "",
            "",
            "def materialize(output_root: Path) -> None:",
            "    output_root = Path(output_root)",
            "    for rel_path, encoded_content in _RESOURCES.items():",
            "        rel = Path(rel_path)",
            '        if rel.is_absolute() or ".." in rel.parts:',
            '            raise RuntimeError(f"Invalid fallback resource path: {rel_path}")',
            "        dst = output_root / rel",
            "        dst.parent.mkdir(parents=True, exist_ok=True)",
            "        dst.write_bytes(gzip.decompress(base64.b64decode(encoded_content)))",
            "",
        ]
    )
    return "\n".join(lines)


def _read_make_variable(makefile: Path, name: str) -> List[str]:
    prefix = f"{name} = "
    for line in makefile.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            return shlex.split(line[len(prefix) :])
    return []


def _normalized(path: Path) -> Path:
    return path.expanduser().resolve(strict=False)


def _relative_parts(path: Path, root: Path) -> Optional[Tuple[str, ...]]:
    try:
        return tuple(path.relative_to(root).parts)
    except ValueError:
        return None


def _join_placeholder(root: str, suffix: Sequence[str]) -> str:
    return root if not suffix else f"{root}/{'/'.join(suffix)}"


def _map_include_path(include_path: str, cann_root: Path) -> Optional[str]:
    raw_path = Path(include_path)
    if not raw_path.is_absolute():
        return None
    path = _normalized(raw_path)
    parts = path.parts
    if "pybind11" in parts:
        return "@PYBIND11_INCLUDE@"
    if any(part.startswith("python3.") for part in parts):
        return "@PYTHON_INCLUDE@"

    rel = _relative_parts(path, _normalized(cann_root / "include"))
    if rel is not None:
        return _join_placeholder("@CANN_INCLUDE_DIR@", rel)
    return None


def _rewrite_include_args(include_args: Sequence[str], cann_root: Path) -> List[str]:
    rewritten: List[str] = []
    index = 0
    while index < len(include_args):
        arg = include_args[index]
        if arg in ("-I", "-isystem", "-iquote"):
            if index + 1 >= len(include_args):
                break
            mapped = _map_include_path(include_args[index + 1], cann_root)
            if mapped is not None:
                rewritten.extend([arg, mapped])
            index += 2
            continue
        if arg.startswith("-I") and len(arg) > 2:
            mapped = _map_include_path(arg[2:], cann_root)
            if mapped is not None:
                rewritten.extend(["-I", mapped])
            index += 1
            continue
        index += 1
    rewritten.extend(["-I", "@CANN_PKG_INC@", "-I", "@CANN_PKG_INC@/base"])
    return rewritten


def _map_library_dir(path_arg: str, cann_root: Path) -> Optional[str]:
    raw_path = Path(path_arg)
    if not raw_path.is_absolute():
        return None
    path = _normalized(raw_path)
    rel = _relative_parts(path, _normalized(cann_root / "lib64"))
    if rel is not None:
        return _join_placeholder("@CANN_LIB64@", rel)
    return None


def _rewrite_link_path_list(arg: str, prefix: str, cann_root: Path) -> Optional[str]:
    if not arg.startswith(prefix):
        return arg
    kept: List[str] = []
    for path_item in arg[len(prefix) :].split(":"):
        mapped = _map_library_dir(path_item, cann_root)
        if mapped is not None and mapped not in kept:
            kept.append(mapped)
    return f"{prefix}{':'.join(kept)}" if kept else None


def _is_object_arg(arg: str) -> bool:
    return arg.endswith((".o", ".obj"))


def _is_libpython_arg(arg: str) -> bool:
    if arg.startswith("-lpython"):
        return True
    return Path(arg).name.startswith("libpython")


def _library_file_info(arg: str, cwd: Path) -> Optional[Tuple[str, Path]]:
    if arg.startswith("-"):
        return None
    path = Path(arg)
    if not path.is_absolute():
        path = cwd / path
    path = _normalized(path)
    name = path.name
    if not name.startswith("lib"):
        return None
    for suffix in (".so", ".a"):
        suffix_index = name.find(suffix)
        if suffix_index > 3:
            return name[3:suffix_index], path
    return None


def _next_link_index_if_skipped(args: Sequence[str], index: int) -> Optional[int]:
    arg = args[index]
    if arg == "-shared":
        return index + 1
    if arg == "-o":
        return index + 2
    if arg.startswith("-Wl,--dependency-file="):
        return index + 1
    if arg == "-Wl,--dependency-file":
        return index + 2
    if _is_object_arg(arg):
        return index + 1
    return None


def _rewrite_rpath_arg(arg: str, cann_root: Path) -> Optional[List[str]]:
    for prefix in ("-Wl,-rpath,", "-Wl,-rpath-link,"):
        if arg.startswith(prefix):
            mapped = _rewrite_link_path_list(arg, prefix, cann_root)
            return [mapped] if mapped is not None else []
    return None


def _rewrite_library_arg(arg: str, cwd: Path, cann_lib_dir: Path) -> Optional[List[str]]:
    if _is_libpython_arg(arg):
        return ["@PYTHON_LIBRARY@"]
    library_info = _library_file_info(arg, cwd)
    if library_info is None:
        return None
    library_name, library_path = library_info
    if (cann_lib_dir / library_path.name).exists() or "/tests/depends/" not in library_path.as_posix():
        return [f"-l{library_name}"]
    return []


def _rewrite_library_dir_arg(args: Sequence[str], index: int, cann_root: Path) -> Optional[Tuple[List[str], int]]:
    def mapped_arg(path_arg: str) -> List[str]:
        mapped = _map_library_dir(path_arg, cann_root)
        if mapped is None:
            return []
        return [f"-L{mapped}"]

    arg = args[index]
    if arg == "-L":
        if index + 1 >= len(args):
            return [], len(args)
        return mapped_arg(args[index + 1]), index + 2
    if arg.startswith("-L") and len(arg) > 2:
        return mapped_arg(arg[2:]), index + 1
    return None


def _rewrite_link_args(link_args: Sequence[str], cwd: Path, cann_root: Path) -> List[str]:
    rewritten: List[str] = ["-L@CANN_LIB64@"]
    cann_lib_dir = _normalized(cann_root / "lib64")
    args = list(link_args[1:]) if link_args else []
    index = 0
    while index < len(args):
        arg = args[index]
        next_index = _next_link_index_if_skipped(args, index)
        if next_index is not None:
            index = next_index
            continue

        rpath_result = _rewrite_rpath_arg(arg, cann_root)
        if rpath_result is not None:
            rewritten.extend(rpath_result)
            index += 1
            continue

        library_result = _rewrite_library_arg(arg, cwd, cann_lib_dir)
        if library_result is not None:
            rewritten.extend(library_result)
            index += 1
            continue

        library_dir_result = _rewrite_library_dir_arg(args, index, cann_root)
        if library_dir_result is not None:
            mapped_args, next_index = library_dir_result
            rewritten.extend(mapped_args)
            index = next_index
            continue

        rewritten.append(arg)
        index += 1

    return rewritten


@dataclass(frozen=True)
class _FallbackTargetSpec:
    relative_dir: str
    target_name: str
    output_name: str
    local_include_dir: str


def _load_target_config(build_dir: Path, cann_root: Path, spec: _FallbackTargetSpec) -> dict:
    target_dir = build_dir / spec.relative_dir / "CMakeFiles" / f"{spec.target_name}.dir"
    flags_make = target_dir / "flags.make"
    link_txt = target_dir / "link.txt"
    if not flags_make.is_file() or not link_txt.is_file():
        raise RuntimeError(f"Cannot find generated build metadata for target {spec.target_name} under {target_dir}")

    cxx_defines = _read_make_variable(flags_make, "CXX_DEFINES")
    cxx_includes = [
        "-I",
        f"@FALLBACK_ROOT@/{spec.local_include_dir}",
    ] + _rewrite_include_args(_read_make_variable(flags_make, "CXX_INCLUDES"), cann_root)
    cxx_flags = _read_make_variable(flags_make, "CXX_FLAGS")
    link_args = _rewrite_link_args(
        shlex.split(link_txt.read_text(encoding="utf-8")),
        build_dir / spec.relative_dir,
        cann_root,
    )
    return {
        "output": spec.output_name,
        "cxx_defines": cxx_defines,
        "cxx_includes": cxx_includes,
        "cxx_flags": cxx_flags,
        "link_args": link_args,
    }


def _build_config(args: argparse.Namespace) -> dict:
    build_dir = args.build_dir
    cann_root = args.cann_root
    return {
        "bridge_abi": args.bridge_abi,
        "targets": {
            "bridge": _load_target_config(
                build_dir,
                cann_root,
                _FallbackTargetSpec(
                    "compiler",
                    "ge_python_pass_bridge",
                    "libge_python_pass_bridge.so",
                    "include/bridge",
                ),
            ),
            "native": _load_target_config(
                build_dir,
                cann_root,
                _FallbackTargetSpec(
                    "api/python/ge/ge/passes",
                    "_ge_pass_native",
                    "_ge_pass_native.so",
                    "include/native",
                ),
            ),
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cann-root", type=Path, required=True)
    parser.add_argument("--bridge-abi", type=int, required=True)
    parser.add_argument("--bridge-source", type=Path, nargs="+", required=True)
    parser.add_argument("--bridge-header", type=Path, nargs="+", required=True)
    parser.add_argument("--native-source", type=Path, nargs="+", required=True)
    parser.add_argument("--native-header", type=Path, nargs="+", required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    resources = dict(
        _collect_files(args.bridge_source, "src/bridge")
        + _collect_files(args.bridge_header, "include/bridge")
        + _collect_files(args.native_source, "src/native")
        + _collect_files(args.native_header, "include/native")
    )

    config = _build_config(args)
    config_bytes = json.dumps(config, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    (output_dir / "build_config.json").write_bytes(config_bytes)
    (output_dir / "_sources.py").write_text(_build_resource_module(resources), encoding="utf-8")
    (output_dir / "__init__.py").write_text("", encoding="utf-8")


if __name__ == "__main__":
    main()
