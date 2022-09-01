# Installation

### conan

We use conan for installing C++ dependencies. To install conan:

```shell
pip3 install --user conan
conan --version #check if conan executable is on path. May need PATH=$PATH:$HOME/.local/bin
```

### Mellanox OFED

MOFED is already installed on aries.

### tbb

tbb is already installed on aries. You need to run the environment setup script to make sure the libraries, headers, and
executables are on the correct paths:

```shell
source /opt/intel/oneapi/setvars.sh
```

## Network Configuration
Our setup assumes an RDMA device with IPoIB addr: 192.168.12.1. You can change IPoIB add

## Running Tests

To run tests that involve RDMA, you need to start a server process.

```shell
./src/server #start server on loopback
make test #run all tests (except large graph tests)
```

Then to run large graph tests:

```shell
./test/large_graph
```
