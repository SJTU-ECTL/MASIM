# MASIM
A multi-array scheduler for in-memory SIMD computation targeting at minimizing the number of copy instructions.

## Dependencies
In order to use the scheduler, you will need a Windows machine with:

Mockturtle(https://github.com/lsils/mockturtle)

Please add all the include directories of the tool into your project's additional include directories.

Note that you may need to use C++17 Language Standard and also update C++ Preprocessor setting in your project to use Mockturle.

Please make sure you can run Mockturle before using our scheduler.

## Run
To apply the scheduler on an XMG netlist, please:

1) put the .v file of the benchmark into the OPP folder

2) put the name of the benchmark and the number of rows in the array in benchmarks.txt.
