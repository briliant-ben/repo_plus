# rpp

A pure C Linux tool built with Autotools, serving as a lightweight reimplementation of the Android `repo` tool.

## Installation

### From Source
It is recommended to use a separate build directory to keep the source tree clean.

```bash
autoreconf -i
mkdir -p build && cd build
../configure
make
sudo make install
```

### From Debian Package
```bash
sudo dpkg -i rpp_*.deb
```

## Testing
The project includes a comprehensive integration test suite that validates commands like `init`, `sync`, `status`, `forall`, and `grep` against a local mock Git server.

To run the tests:
1. Ensure the project is built (see Installation).
2. Execute the test runner from the root directory:
```bash
./test/run_tests.sh
```

## Usage
Run `rpp --help` for a list of available commands.