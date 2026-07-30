# templed

An experimental LED command line hardware monitor project for Raspberry Pi, written in C++.

## About
Simply strobes 4 LEDS, Blue, Green, Yellow, Red, depending on SOC temperature.
Also prints the temperature to the console

## Usage

```bash
templed -t 2 # Refresh every 2 seconds (default 1 sec.)
templed -f   # Display temperatures in Fahrenheit
templed -v   # Show program version
templed -h   # Show help.
```

## Building

templed supports regular [`make`](./Makefile), as well as [`cmake`](./CMakeLists.txt) and [GN/Ninja](./BUILD.gn).

CMake requires version 3.10+, GN/Ninja requires my [gn-legacy](https://github.com/Alex313031/gn-legacy) repo.

```bash

make -j 4 # make with 4 jobs

make IS_DEBUG=1 # make a debug build

mkdir out && cd out && cmake ../ # CMake build

ninja -C out/Default templed # Ninja build
```

## License
This repository is licensed under the [BSD-3 Clause License](./LICENSE.md).
