# Security Statement

## Running User Recommendation

From a security perspective, it is not recommended to use root or other administrator-type accounts to execute any commands. Follow the principle of minimum privileges.

## File Permission Control

- We recommend users set the running system umask value to 0027 or higher on hosts (including host machines) and containers to ensure new folders have default maximum permissions of 750 and new files have default maximum permissions of 640.
- We recommend users implement permission control and other security measures for sensitive content such as personal privacy data, commercial assets, source files, and various files saved during operator development. For example, permission control for this project installation directory, input public data file permission control. For recommended permissions, please refer to [A-File (Folder) Permission Control Recommended Maximum Values for Various Scenarios](#a-file-folder-permission-control-recommended-maximum-values-for-various-scenarios).
- Users need to implement proper permission control during installation and usage. Please refer to [A-File (Folder) Permission Control Recommended Maximum Values for Various Scenarios](#a-file-folder-permission-control-recommended-maximum-values-for-various-scenarios) for file permission settings.

## Build Security Statement

When compiling and installing this project from source code, you need to compile it yourself. The compilation process generates some intermediate files. We recommend implementing permission control for intermediate files after compilation completion to ensure file security.

## Runtime Security Statement

When runtime exceptions occur, the process will exit and print error information. We recommend locating specific error causes based on error prompts.

## Public Network Address Statement

Public network addresses included in this project code are declared as follows:

|      Type      |                                           Open Source Code Address                                           |                            File Name                             |             Public IP Address/Public URL Address/Domain/Email Address/Compressed File Address             |                   Usage Description                    |
| :------------: |:------------------------------------------------------------------------------------------:|:----------------------------------------------------------| :---------------------------------------------------------- |:-----------------------------------------|
|  Dependency  | Not applicable  | cmake/third_party/makeself-fetch.cmake | https://gitcode.com/cann-src-third-party/makeself/releases/download/release-2.5.0-patch1.0/makeself-release-2.5.0-patch1.tar.gz | Download makeself source code from gitcode as build dependency |
|  Dependency  | Not applicable  | cmake/third_party/json.cmake | https://gitcode.com/cann-src-third-party/json/releases/download/v3.11.3/include.zip | Download json source code from gitcode as build dependency |
|  Dependency  | Not applicable  | cmake/third_party/gtest.cmake | https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz | Download googletest source code from gitcode as build dependency |
| Dependency | Not applicable | cmake/third_party/pybind11.cmake | <https://gitcode.com/cann-src-third-party/pybind11/releases/download/v2.13.6/pybind11-2.13.6.tar.gz> | Download pybind11 source code from gitcode as build dependency |
|  Dependency  | Not applicable  | cmake | https://apt.kitware.com/keys/kitware-archive-latest.asc | Download cmake software from kitware as build dependency |
|  Dependency  | Not applicable  | cmake | https://apt.kitware.com/ubuntu/ | Download cmake software from kitware as build dependency |

## Vulnerability Mechanism Description

[Vulnerability Management](https://gitcode.com/cann/community/blob/master/security/security.md)

## Appendix

### A-File (Folder) Permission Control Recommended Maximum Values for Various Scenarios

| Type           | Linux Permission Reference Maximum Value |
| -------------- | --------------  |
| User home directory                        |   750 (rwxr-x---)            |
| Program files (including script files, library files, and so on)       |   550 (r-xr-x---)             |
| Program file directory                      |   550 (r-xr-x---)            |
| Configuration file                          |  640 (rw-r-----)             |
| Configuration file directory                      |   750 (rwxr-x---)            |
| Log files (completed recording or archived)        |  440 (r--r-----)             |
| Log files (currently recording)                |    640 (rw-r-----)           |
| Log file directory                      |   750 (rwxr-x---)            |
| Debug files                         |  640 (rw-r-----)         |
| Debug file directory                     |   750 (rwxr-x---)  |
| Temporary file directory                      |   750 (rwxr-x---)   |
| Maintenance upgrade file directory                  |   770 (rwxrwx---)    |
| Business data files                      |   640 (rw-r-----)    |
| Business data file directory                  |   750 (rwxr-x---)      |
| Key components, private keys, certificates, ciphertext file directory    |  700 (rwx------)      |
| Key components, private keys, certificates, encrypted ciphertext        | 600 (rw-------)      |
| Encryption/decryption interfaces, encryption/decryption scripts            |   500 (r-x------)        |
