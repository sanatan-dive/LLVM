# PARALLDISCOVER - An LLVM-based ILP Discovery Framework

PARALLDISCOVER is a framework built on LLVM for discovering and exploiting instruction-level parallelism (ILP) in sequential code. This tool consists of multiple LLVM passes that analyze dependency chains, identify parallelizable instruction sequences, and generate optimized IR that exposes additional parallelism opportunities.

## Table of Contents

1. [Overview](#overview)
2. [Requirements](#requirements)
3. [Building the Project](#building-the-project)
4. [Usage Instructions](#usage-instructions)
5. [Pass Descriptions](#pass-descriptions)
6. [Runtime Profiler](#runtime-profiler)
7. [Command Line Options](#command-line-options)
8. [Example Workflow](#example-workflow)
9. [Troubleshooting](#troubleshooting)
10. [License](#license)

## Overview

PARALLDISCOVER consists of the following components:

- `EnhancedDependencyAnalysis`: A novel dependency analysis framework that builds on LLVM's existing capabilities but provides more precise memory dependency information
- `ILPExposureTransform`: A transformation pass that reorders instructions and restructures code to expose ILP
- `SpeculativeExecutionGuard`: A pass that adds runtime checks for speculative code regions
- `InstrumentationPass`: A pass that adds profiling hooks for runtime feedback collection
- Runtime profiling library for collecting execution data and guiding optimizations

## Requirements

- LLVM 14.0.0 or newer
- CMake 3.13.4 or newer
- C++17 compatible compiler (GCC 7+ or Clang 6+)
- Python 3.6+ (for analysis scripts)

## Building the Project

Follow these steps to build PARALLDISCOVER:

1. Clone the repository:
   ```bash
   git clone https://github.com/username/paralldiscover.git
   cd paralldiscover
   ```

2. Create a build directory and navigate to it:
   ```bash
   mkdir build
   cd build
   ```

3. Configure with CMake:
   ```bash
   cmake -DLLVM_DIR=/path/to/llvm/cmake/modules ..
   ```

4. Build the project:
   ```bash
   make -j$(nproc)
   ```

5. (Optional) Run tests:
   ```bash
   make check
   ```

## Usage Instructions

### Basic Usage

To use PARALLDISCOVER with an existing C/C++ project:

1. Compile your source to LLVM IR:
   ```bash
   clang -S -emit-llvm -O1 -Xclang -disable-llvm-passes input.c -o input.ll
   ```

2. Run PARALLDISCOVER on the IR:
   ```bash
   opt -load ./lib/libParallDiscover.so -paralldiscover input.ll -o optimized.ll
   ```

3. Compile the optimized IR to an executable:
   ```bash
   clang optimized.ll -o output
   ```

### With Profiling

To enable profile-guided optimization:

1. First, compile with profiling instrumentation:
   ```bash
   clang -S -emit-llvm -O1 -Xclang -disable-llvm-passes input.c -o input.ll
   opt -load ./lib/libParallDiscover.so -paralldiscover -paralldiscover-profile input.ll -o instrumented.ll
   clang instrumented.ll ./lib/libRuntimeProfiler.so -o instrumented_binary
   ```

2. Run the instrumented binary with representative inputs:
   ```bash
   ./instrumented_binary < input_data
   ```

3. Use the generated profile data for optimization:
   ```bash
   opt -load ./lib/libParallDiscover.so -paralldiscover -paralldiscover-use-profile=paralldiscover_profile.dat input.ll -o optimized.ll
   clang optimized.ll -o output
   ```

## Pass Descriptions

### EnhancedDependencyAnalysis

This analysis pass extends LLVM's dependency analysis with:

- More precise alias analysis
- Cross-iteration dependency tracking
- Critical path identification

Example usage:
```bash
opt -load ./lib/libParallDiscover.so -enhanced-dep-analysis -analyze input.ll
```

### ILPExposureTransform

This transformation pass applies optimizations to expose ILP:

- Instruction reordering
- Loop restructuring
- Partial loop unrolling optimized for target architecture

Example usage:
```bash
opt -load ./lib/libParallDiscover.so -ilp-exposure input.ll -o output.ll
```

### SpeculativeExecutionGuard

This pass adds runtime checks for speculative execution:

- Identifies branches with high predictability
- Inserts guard conditions
- Creates recovery paths

Example usage:
```bash
opt -load ./lib/libParallDiscover.so -spec-guard input.ll -o output.ll
```

### InstrumentationPass

This pass adds profiling instrumentation:

- Basic block execution counters
- Memory access tracking
- Branch prediction tracking

Example usage:
```bash
opt -load ./lib/libParallDiscover.so -instrumentation input.ll -o output.ll
```

## Runtime Profiler

The runtime profiler collects execution data to guide optimizations:

- Block execution frequencies
- Memory access patterns
- Branch prediction accuracy

The profile data is written to `paralldiscover_profile.dat` when the program exits.

## Command Line Options

PARALLDISCOVER supports the following command-line options:

- `-paralldiscover-profile`: Enable profiling instrumentation
- `-paralldiscover-use-profile=<file>`: Use profile data from the specified file
- `-paralldiscover-speculate`: Enable speculative execution transformations (default: enabled)
- `-paralldiscover-unroll-factor=<n>`: Set the loop unrolling factor (default: 4)
- `-paralldiscover-debug`: Enable debug output

## Example Workflow

Here's a complete workflow example using matrix multiplication:

1. Save the following code as `mm.c`:
   ```c
   #include <stdio.h>
   #include <stdlib.h>
   #include <time.h>

   #define N 1000

   double a[N][N], b[N][N], c[N][N];

   void initialize() {
     for (int i = 0; i < N; i++) {
       for (int j = 0; j < N; j++) {
         a[i][j] = ((double) rand() / RAND_MAX);
         b[i][j] = ((double) rand() / RAND_MAX);
         c[i][j] = 0.0;
       }
     }
   }

   void multiply() {
     for (int i = 0; i < N; i++) {
       for (int j = 0; j < N; j++) {
         for (int k = 0; k < N; k++) {
           c[i][j] += a[i][k] * b[k][j];
         }
       }
     }
   }

   int main() {
     srand(time(NULL));
     initialize();
     
     clock_t start = clock();
     multiply();
     clock_t end = clock();
     
     double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
     printf("Execution time: %f seconds\n", time_spent);
     
     return 0;
   }
   ```

2. Compile to LLVM IR:
   ```bash
   clang -S -emit-llvm -O1 -Xclang -disable-llvm-passes mm.c -o mm.ll
   ```

3. Run PARALLDISCOVER:
   ```bash
   opt -load ./lib/libParallDiscover.so -paralldiscover mm.ll -o mm_opt.ll
   ```

4. Compile and run the optimized code:
   ```bash
   clang mm_opt.ll -o mm_opt
   ./mm_opt
   ```

5. Compare with baseline:
   ```bash
   clang -O3 mm.c -o mm_baseline
   ./mm_baseline
   ```

## Troubleshooting

### Common Issues

1. **Pass not registered**: Make sure you're using the correct path to the shared library.

   Solution: Verify the path using `find . -name "libParallDiscover.so"`.

2. **Segmentation fault during profiling**: Ensure the runtime library is properly linked.

   Solution: Link with `-Wl,-rpath,/path/to/lib` to specify the runtime path.

3. **Build errors**: Check LLVM version compatibility.

   Solution: Ensure your LLVM version matches the one supported by the project.

### Debugging

Enable debug output with:
```bash
opt -load ./lib/libParallDiscover.so -paralldiscover -debug-only=paralldiscover input.ll -o output.ll
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.