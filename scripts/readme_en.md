# Graph Engine Personal Developer Toolchain Usage Guide

GE Developer Toolchain is an automated script toolchain in graph engine designed for individual developers.

Currently supports a series of common developer functions including container-based development environment preparation, automatic download/installation/configuration of build dependencies, code formatting, compilation, testing, code coverage checking, document generation, etc.

## Prerequisites

Below are the manual prerequisites needed for using GE developer toolchain;

The following are verified best performance recommended configurations:

1. Operating System, choose one of the following:
    - Native Linux operating system, such as ubuntu;
    - Windows operating system, recommended to install WSL ubuntu system, strongly suggest downloading code directly inside WSL, do not mount volumes (poor build performance)!
    - MAC OS;

2. Docker installation:
    - Docker installed successfully, and relevant mirror sources configured correctly, can normally download external images.

3. OS-supported command line tools: Native Linux or WSL shell;

Available but not recommended configurations:

- Install docker directly in windows, use Linux-like bash (Cygwin, minGW etc.) to execute GE toolchain;
  (Using this method can also execute all GE toolchain operations, but due to Windows and container heterogeneous kernel file access limitations, build speed will be relatively slow)

## Quick Start

GE toolchain corresponding scripts are under scripts, can execute according to following flow:

1. Enter scripts directory:

    ```sh
    $ cd ./scripts
    ```

2. Execute `ge env` to automatically download container environment, and login to environment

    ```sh
    $ ./ge.sh env
    ```

3. Download and install external libraries required for building

    ```sh
    $ ge update
    ```

    (Note: After entering container, `ge` command already automatically registered into system, so container内 doesn't need to write script full name)

4. Execute testing, default executes unit test cases, `ge test` will automatically trigger build

    ```sh
    $ ge test
    ```

## Detailed Usage

Under scripts directory, run ./ge.sh -h to view all subcommand集合.

```sh
$ ./ge.sh -h

Usage: ge  COMMANDS

Run ge commands

Commands:
    env         Prepare docker env
    config      Config dependencies server
    update      Update dependencies
    format      Format code
    lint        Static verify
    build       Build code
    test        Run test of UT/ST
    cov         Run Coverage
    docs        Generate documents
    clean       Clean
```

Each subcommand built into script represents an independent function; each subcommand also provides secondary parameters for flexible specification of execution方式.

Each subcommand can view supported configurable parameters through `-h`.

For example querying `env` subcommand parameters, can use following command:

```sh
$  ./ge.sh env -h
```

Each subcommand has a default behavior when不带参数.

### `ge env`

This command用于 prepares container environment used for building and testing, specifically contains following parameters:

```sh
$  ./ge.sh env -h

Usage: ge env [OPTIONS]

Prepare for docker env for build and test

Options:
    -b, --build  Build docker image
    -p, --pull   Pull  docker image
    -e, --enter  Enter container
    -r, --reset  Reset container
    -h, --help
```

Parameter detailed explanation:

- `-b  -- build`: Generate required running container image based on "scripts/env/Dockerfile";
- `-p  -- pull`: Pull required container image from locally configured container central repository;
- `-e  -- enter`: Login to container runtime environment前提 existing local container image;
- `-r  -- reset`: Delete local running container image environment;

Default: Pull corresponding container image from central container repository, run instance and login.

### `ge config`

Configure external dependency server, specific parameters如下:

```sh
$ ge config -h

Usage: ge config [OPTIONS]

update server config for ge, you need input all config info (ip, user, password)

Options:
    -i, --ip           Config ip config
    -u, --user         Config user name
    -p, --password     Config password
    -h, --help

Example: ge config -i=<ip-adress> -u=<username> -p=<password> (Need add escape character \ before special character $、#、!)
```

Parameter detailed explanation:

- `-i,  --ip`          : Configure dependency library server IP address;
- `-u,  --usr`         : Configure dependency library server username;
- `-p,  --password`    : Configure dependency library address;

Default: Print help information.

### `ge update`

Install external dependency libraries required for graph engine building, specific parameters如下:

```sh
$ ge update -h

Usage: ge update [OPTIONS]

update dependencies of build and test

Options:
    -p, --public       Download dependencies from community
    -d, --download     Download dependencies
    -i, --install      Install dependencies
    -c, --clear        Clear dependencies
    -h, --help
```

Parameter detailed explanation:

- `-p,  --public`   : Download and install dependency libraries from community;
- `-d,  --download` : Download external dependency libraries needed for building;
- `-i,  --install`  : Install external dependency packages to corresponding locations;
- `-c,  --clear`    : Clear downloaded external dependency packages;

Default: Download external dependency libraries according to "scripts/update/deps_config.sh" configuration and install to corresponding directories.
(Note: Please ensure server address, username, password in "scripts/update/server_config.sh" already configured)

### `ge format`

Use clang-format for code formatting, specific parameters如下:

```sh
$ ge format -h
Options:
    -a format of all files
    -c format of the files changed compared to last commit, default case
    -l format of the files changed in last commit
    -h Print usage
```

Parameter detailed explanation:

- `-a`: Format all code;
- `-c`: Only format code modified this time;
- `-l`: Format code from last commit;

Default: Format code modified this time.

### `ge build`

Execute build (Note: Calls original build.sh script, under modification...);

### `ge test`

Build and run test cases, currently supports following parameters:

```sh
$ ge test -h

Usage: ge test [OPTIONS]

Options:
    -u, --unit          Run unit Test
    -c, --component     Run component Test
    -h, --help
```

Parameter detailed explanation:

- `-u, --unit`      : Execute unit testing
- `-c, --component` : Execute component testing

Default: Execute unit testing.

### `ge cov`

Execute code coverage checking, supports full coverage and increment coverage functions, this command需要 already ran test cases, currently supports following parameters:

```sh
$ ge cov -h

Usage: ge cov [OPTIONS]

Options:
    -a, --all          Full coverage
    -i, --increment    Increment coverage
    -d, --directory    Coverage of directory
    -h, --help
```

Parameter detailed explanation:

- `-a, --all`       : Execute full coverage rate statistics;
- `-i, --increment` : Execute increment coverage checking, default analyzes uncommitted modified code coverage (if new git untracked files exist, need先 git add to add in);
- `-d, --directory` : Code path for increment coverage checking, supports passing path parameters;

Default: Execute increment coverage checking;

Below command demonstrates how to check all code's increment coverage under ge directory:

```sh
$ ge cov -d=ge
```

### `ge docs`

Doxygen document generation, includes code logic and physical structure and relationships, convenient for reading and understanding code; currently supports following parameters:

```sh
$ ge docs -h

Usage: ge docs [OPTIONS]

Options:
    -b, --brief  Build brief docs
    -a, --all    Build all docs
    -h, --help
```

Parameter detailed explanation:

- `-b, --brief`: Generate brief documents, ignore部分 relationship diagram generation, fast speed;
- `-a, --all`: Generate full documents, includes various code relationship diagrams, relatively slow speed;

Default: Generate full code documents.

### `ge clean`

Clean various downloaded or generated intermediate files, currently supported parameters如下:

```sh
$ ge clean -h

Usage: ge clean [OPTIONS]

Options:
    -b, --build         Clean build dir
    -d, --docs          Clean generate docs
    -i, --install       Clean dependencies
    -a, --all           Clean all
    -h, --help
```

Parameter detailed explanation:

- `-b, --build`: Clean generated compilation build temporary files;
- `-d, --docs`: Clean generated document temporary files;
- `-i, --install`: Clean installed dependency files;
- `-a, --all`: Clean all downloaded and generated temporary files;

Default: Clean temporary files generated by compilation build.

## Follow us

Toolchain functionality还在 continuously improving, please submit issue if problems, thank you!
