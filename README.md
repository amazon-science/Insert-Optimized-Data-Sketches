# Insert-Optimized Implementation of Streaming Data Sketches
This repository contains the implementations accompanying our paper "Insert-Optimized Implementation of Streaming Data Sketches."

The codebase includes our optimized implementations alongside baseline versions and Apache DataSketches implementations. You'll also find the microbenchmarking code, results from our experiments, and visualization scripts for analyzing the results.

*Note: Due to licensing restrictions, implementations from compared papers are not included in this repository.*

## Build
To build the project, simply run:
```bash
./build.sh
```
This script uses CMake with GCC in release mode  by default, and creates all build artifacts in the `cmake-build-release`.

## Run Benchmarks
After building, execute the following commands to run the benchmarks:

```
cmake-build-release/bm_insert --benchmark_out="results/bm_insert.json" --benchmark_min_time=10s

cmake-build-release/bm_hash --benchmark_out="results/bm_hash.json" --benchmark_min_time=10s

cmake-build-release/bm_hash_insert --benchmark_out="results/bm_hash_insert.json" --benchmark_min_time=10s
```

## Plot
Execute the Jupyter notebook `analysis.ipynb` to generate the plots from the paper, which will be saved in the `figures/` directory.

## Benchmarking Your Own Sketch Implementation
1. Add your implementation to the `src` directory
2. Integrate it into one of the existing benchmarks in `bench/`
3. Ensure your implementation uses these sketch parameters:
   - Count Sketch: `t = 2048`, `d = 5`
   - SpaceSaving: `k = 96`
   - Karnin-Lang-Liberty: `k = 200`
4. Follow the build, run, and plot instructions above

# Citation

Our paper is available [here](https://www.amazon.science/) (TODO).
```
TODO
```

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This project is licensed under the Apache-2.0 License.

