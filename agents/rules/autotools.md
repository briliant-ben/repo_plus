---
description: Autotools Framework Rules
---
# Autotools Build Rules

1. **Framework**: This project uses the `autotools` build system (autoconf, automake, libtool if needed).
2. **Key Files**:
    * `configure.ac`: Define configuration paths, checks for programs, libraries, and headers here.
    * `Makefile.am`: Define macros for building the executables and libraries here.
3. **Usage**:
    * Always regenerate the build system by running `autoreconf -fi` when modifying `configure.ac` or `Makefile.am`.
    * Build with `./configure && make`.
4. **Debian Packaging**: Ensure that standard targets (`make install`, `make dist`) work, allowing easy `apt` integration eventually through Debian `rules`.
