# Style Guide
## C++

Rules:
- `auto` always
- East `const` e.g. (`auto const val = ...`, `auto const& ref = ...`)
- For naming, I use: [the google c++ style](https://google.github.io/styleguide/cppguide.html#Naming). Ignore the file naming rules, I use `.cpp/.hpp` extensions.
- Separate private interfaces from public interfaces.
    - Put library public interface header files `.hpp` in a`<library_src_dir>/include` subdirectory. `#include` these headers with angle brackets `#include <>`
    - Put library private interface headers next to the `.cpp` files, and include them with quotes `#include ""` 
- Don't create a new feature without tests
- You can only test the public interface of a library. If you find yourself wanting to test something that is not part of the public interface, you probably need to split the component into another library that has the feature in its public interface.
- Don't use output parameters in functions. If you want to return multiple things from a function, use c++ destructuring e.g. `auto [val1, val2] = MultiReturnFunc();`
- Put whatever possible in `.cpp` files. Only put things in `.hpp` files if the client needs the implementation along with interface e.g. templates, constexpr functions
- Don't copy and paste. Refactor if you have to.
- Don't `#include <iostream>` or use `std::cout` for logging. Use `spdlog` for logging and `fmt` for formatting strings.
- If you want to make a note to implement or fix something create a github issue, don't leave inline comments `//TODO:`. If you find TODO comments that I put in there you can make an issue and delete them if you want.

## cmake

Control the visibility of dependencies using `PRIVATE`, `INTERFACE`, and `PUBLIC` specifiers.

## clang-format

Set up your editor to use clang-format.

## git
- Don't commit broken code
- Don't commit code that doesn't pass tests
- try to break up commits into single features

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
