from conans import ConanFile


class FAM(ConanFile):
    options = {
    }
    name = "FAM"
    version = "0.1"
    requires = (
        "catch2/2.13.7",
        "fmt/8.0.1",
        "spdlog/1.9.2",
        "boost/1.77.0",
        "grpc/1.50.0",
        "range-v3/0.12.0",
    )
    generators = "cmake", "gcc", "txt", "cmake_find_package"
