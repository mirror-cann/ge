# ES (Eager Style) Generator

## Prerequisites
1. Properly install `toolkit` package through [Installation Guide](../../../quick_install.md), and **correctly configure environment variables** according to the guide
2. Properly install operator `ops` package through [Installation Guide](../../../quick_install.md) (ES depends on operator prototypes for API generation), and **correctly configure environment variables** according to the guide

## Environment Variable Requirements
List of environment variables required by gen_esb:
- ASCEND_OPP_PATH: Points to the opp path under installation directory
- LD_LIBRARY_PATH: Environment variable specifying dynamic library search path

Note: The above environment variables don't need and are not recommended to be configured separately, the environment variables already configured in [Prerequisites](#prerequisites) by default meet the requirements

## Functionality Explanation
### This program supports two generation modes
1. Code Generation Mode
  Generate ES graph builder C, C++, Python code, including:
  - C interfaces for all supported operators (ops)
  - C++ interfaces for all supported operators
  - Python interfaces for all supported operators
  - Aggregation header files for convenient one-time inclusion of all operators
  - Aggregation Python files for convenient one-time import of all operators
2. Historical Prototype Library Generation Mode
  Generate historical prototype structured data, including:
  - Version index
  - Version metadata
  - Operator prototype data for that version

## Usage Methods
### Code Generation Mode
```bash
gen_esb [--output_dir=DIR] [--module_name=NAME] [--h_guard_prefix=PREFIX] [--exclude_ops=OP_TYPE1,OP_TYPE2] [--history_registry=PKG_DIR] [--release_version=VER]
```
### Historical Prototype Library Generation Mode
```bash
gen_esb --es_mode=extract_history --release_version=VER [--output_dir=DIR] [--release_date=YYYY-MM-DD] [--branch_name=BRANCH]
```

Note: Because environment variables have already been configured in [Prerequisites](#prerequisites), `gen_esb` has been added to `PATH` environment variable, so can be executed directly

### Parameter Explanation
- --es_mode: Optional parameter, specify generation mode, supports `codegen` and `extract_history`
  If not specified, defaults to codegen
- --output_dir: Optional parameter, specify target directory for generation
  If not specified, defaults to output to current directory
- --module_name: Optional parameter, control aggregation header file naming
  - "math" -> es_math_ops_c.h, es_math_ops.h, es_math_ops.py
  - "all" -> es_all_ops_c.h, es_all_ops.h, es_all_ops.py
  - Not passed -> defaults to "all"
- --h_guard_prefix: Optional parameter, control generated header file guard macro prefix, used for possible internal/external operator same-name situation distinction
  - If not specified, use default prefix
  - When specified, concatenate with default prefix
  - python files don't recognize this parameter, same-name scenarios avoid conflict through different paths
- --exclude_ops: Optional parameter, control operators to exclude from code generation
  - Separate operator names by `,`
- --history_registry: Optional parameter, specify historical prototype library directory for code generation
  - If not specified, historical prototype library is not enabled by default
  - When specified, generated C++ interface will include compatible version information in historical prototype library
- --release_version:
  - Code generation mode: Optional parameter, used with `--history_registry`, specify current version number, generated C++ interface includes compatible version information for this version; if not specified, generate compatible historical versions based on current date
  - Historical prototype library generation mode: Required parameter, specify version number corresponding to current historical prototype data
- --release_date: Optional parameter, control release date of historical prototype structured data, format `YYYY-MM-DD`
  - If not specified, use current date
- --branch_name: Optional parameter, control release branch name of historical prototype structured data

### Output File Explanation
#### Code Generation Mode Output
- es_<module>_ops_c.h: C interface aggregation header file
- es_<module>_ops.h: C++ interface aggregation header file
- es_<module>_ops.py: Python interface aggregation file
- es_<op_type>_c.h: Single operator C interface header file
- es_<op_type>.cpp: Single operator C interface implementation file
- es_<op_type>.h: Single operator C++ interface header file
- es_<op_type>.py: Single operator Python interface file

#### Historical Prototype Library Generation Mode Output
- index.json: Version index
- registry/<ver>/metadata.json: Version metadata
- registry/<ver>/operators.json: Operator prototype data for that version

## Usage Examples
### Generate code to current directory, use default module name "all", default guard macro prefix
`gen_esb`

### Generate code to specified directory, use default module name "all", default guard macro prefix
`gen_esb --output_dir=./output`

### Generate code to specified directory, use "math" module name, default guard macro prefix
`gen_esb --output_dir=./output --module_name=math`

### Generate code to specified directory, use "all" module name, default guard macro prefix
`gen_esb --output_dir=./output --module_name=all`

### Generate code to specified directory, use "math" module name, custom guard macro prefix "MY_CUSTOM"
`gen_esb --output_dir=./output --module_name=math --h_guard_prefix=MY_CUSTOM`

### Generate code to specified directory, use "math" module name, custom guard macro prefix "MY_CUSTOM", and exclude Add operator generation
`gen_esb --output_dir=./output --module_name=math --h_guard_prefix=MY_CUSTOM --exclude_ops=Add`

### Generate code to specified directory, use "math" module name, default guard macro prefix, generated C++ interface will include compatible version information filtered based on current date in math historical prototype directory
`./gen_esb --output_dir=./output --module_name=math --history_registry=/${CANN_INSTALL_PATH}/cann/opp/history_registry/math`

### Generate code to specified directory, use "math" module name, default guard macro prefix, generated C++ interface will include historical version information compatible with "8.0.RC2" version in math historical prototype directory
