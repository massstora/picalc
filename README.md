# picalc

`picalc` is a command-line pi calculator with a NASM x86-64 entry point and
GMP-backed arbitrary-precision arithmetic. It uses the Chudnovsky algorithm with
binary splitting and internal guard digits so the requested decimal places are
stable before output is truncated to the requested length.

On Linux, `picalc` detects unique physical CPU cores from `/sys` topology data
and uses that many worker threads for the Chudnovsky binary-splitting phase. SMT
/ hyperthread siblings are counted as one physical core.

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
