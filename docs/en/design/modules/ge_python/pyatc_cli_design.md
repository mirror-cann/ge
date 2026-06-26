# pyatc CLI Design Document

## 1. Background

ATC is GE offline model compilation tool, existing entry is `api/atc/atc` shell wrapper. This method starts ATC in new process, Python interpreter used by TBE and custom Python pass is parsed by ATC process itself, may be inconsistent with `python` selected by user in terminal.

This design adds `ge.pyatc` in `ge-py` distributed with CANN, uses same command line parameters as ATC, enters original `ge::main_impl()` main flow in **current process**, makes TBE/Python pass consistent with interpreter selected by user.

## 2. Goals and Scope

### 2.1 Goals

- Provide equivalent CLI entry: `python3 -m ge.pyatc` and `<cann_install>/bin/pyatc`.
- ATC parameters, help, stdout/stderr, exit code consistent with native ATC behavior; not do parameter validation in Python layer or independently maintain help text.
- Reuse `ge::main_impl()` and `atc_static`, not add second ATC main flow.
- OM/OM2/exeom product format unchanged.

### 2.2 Non-goals

- Not redo ATC parameter parsing and compilation logic in Python layer.
- Not promise single process multiple reentrant calls, thread safety or concurrent calls.
- Not make `main()` as stable external Python API.

## 3. User Experience

Execute `source <cann_install>/set_env.sh` before using CANN environment (makes `pyatc`, `ge` package and `libpyatc_wrapper.so` loadable by current interpreter). Then:

```bash
pyatc <parameters same as atc>
or python3 -m ge.pyatc <parameters same as atc>
```

- Command line parameters passed to ATC unchanged.
- `pyatc --help` and `atc --help` parameter content consistent, usage, examples and failure prompt entry name displayed as `pyatc`.
- `python3 -m ge.pyatc` is recommended writing for module entry; users can also use other Python 3 interpreters to execute `-m ge.pyatc` according to environment needs.
- `pyatc` is shell wrapper installed under `<cann_install>/bin`, default uses `python3` parsed by current `PATH`, can explicitly specify interpreter through `PYTHON=/path/to/python pyatc ...`.

## 4. Overall Architecture

- **Thin entry**: Python side only does parameter forwarding and calling convention adaptation; ATC business logic still in `main_impl` and various existing modules.
- **CANN bin wrapper**: `<cann_install>/bin/pyatc` only responsible for supplementing CANN `PYTHONPATH`/`LD_LIBRARY_PATH`, and passes wrapper absolute path as original `argv[0]` through Python entry function parameters; does not depend on wheel `console_scripts`, avoids binding Python interpreter when installing wheel.
- **Independent `libpyatc_wrapper.so`**: Links `atc_static`, enters `ge::main_impl` through thin C encapsulation; separates from `libgraph_wrapper.so`, `libge_runtime_wrapper.so`, `liboffline_compile_wrapper.so`, avoids ATC global state affecting other ge-py usage.

## 5. Compatibility

- Original `atc`/`atc.bin` and existing ge-py API behavior unchanged.
- Old version OM execution under new version not affected; `pyatc` is new entry, depends on new version CANN `bin/pyatc` wrapper, `libpyatc_wrapper.so` and module code in ge-py wheel.
- ge-py wheel does not declare `console_scripts` entry, avoids generating scripts that cannot be discovered by `PATH` or bind wrong interpreter when `pip install -t <cann_install>/python/site-packages`.

## 6. Acceptance Criteria

1. `pyatc --xxx`, `python3 -m ge.pyatc --xxx` and `atc --xxx` output equivalent.
2. `pyatc --help` (`<cann_install>/bin/pyatc` shell wrapper) behavior consistent with module entry.
3. Typical model compilation parameters can be transparently passed and produce OM consistent with `atc`.
4. Process exit code consistent with ATC main flow when failing.
5. TBE/custom Python pass path priority uses current process interpreter.
