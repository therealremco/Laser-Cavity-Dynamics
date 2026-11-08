# cavity-dynamics

A one-dimensional, time-dependent simulation of a short laser cavity with a
spatially inhomogeneous gain medium. The C++17 core has no third-party
dependencies and can be used independently of the command-line interface.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

Generate a starter configuration and run it:

```sh
build/cavity-dynamics defaults > config.txt
build/cavity-dynamics run config.txt -o output.dat
```

The simulation writes tab-separated time-series data suitable for gnuplot,
Origin, or Python. If snapshots are enabled in the configuration, a companion
`output.snapshots.dat` file contains the spatial profiles. All parameters use
SI units; see [CONTEXT.md](CONTEXT.md) for the physical model and parameter
definitions.
