# picalc

`picalc` is a command-line pi calculator with a NASM x86-64 entry point and
GMP-backed arbitrary-precision arithmetic. It uses the Chudnovsky algorithm with
binary splitting and internal guard digits so the requested decimal places are
stable before output is truncated to the requested length.

On Linux, `picalc` detects unique physical CPU cores from `/sys` topology data
and uses that many worker threads for the Chudnovsky binary-splitting phase. SMT
/ hyperthread siblings are counted as one physical core. Worker threads are
pinned to one representative logical CPU for each physical core when Linux
affinity APIs are available.

The implementation keeps GMP values thread-local, combines completed worker
chunks as they are joined, clears large temporaries as soon as they are no longer
needed, and streams decimal output instead of first creating one full output
string in memory. Per-thread task/result headers are cache-line aligned to avoid
false sharing in the program's own worker metadata.

## Build

Requirements:

- `nasm`
- `gcc`
- GMP development headers and library

```sh
make
```

## Usage

```sh
./picalc DIGITS
./picalc DIGITS -o FILE
```

`DIGITS` is the number of digits after the decimal point.

Examples:

```sh
./picalc 300
./picalc 300 -o pi.txt
```

## Test

```sh
make test
```
