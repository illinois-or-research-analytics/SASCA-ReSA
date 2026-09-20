# Parquet input and output MWE

This example starts from committed toy Parquet nodelist and edgelist files, runs five simulation cycles, and writes Parquet output for both tables. The small CSV files alongside the Parquet inputs show the source records. The three distribution CSVs are a small sample of the existing project inputs.

Requirements: the project dependencies listed in the root README, plus Arrow C++ and Parquet C++ development packages. CMake finds the latter with `find_package(Arrow CONFIG)` and `find_package(Parquet CONFIG)`. To regenerate the toy Parquet inputs, install Python `pandas` and `pyarrow` and run `python scripts/make_toy_parquet.py` from the repository root.

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
mkdir -p tests/parquet_toy/output
./build/bin/abm --config tests/parquet_toy/test_parquet_config.ini
```

The run creates `tests/parquet_toy/output/edgelist.parquet` and `nodelist.parquet`. The output directory is ignored by Git. With Python `pandas` and `pyarrow`, the output can be inspected using `pd.read_parquet(...)`. The simulation's existing CSV input and output paths remain available by specifying `.csv` file names in a config. Parquet checkpoint input is not implemented.
