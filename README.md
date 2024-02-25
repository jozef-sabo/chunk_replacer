# Minecraft Chunk Replacer

This is a Minecraft Chunk Replacer, which gives you an opportunity to replace single chunks in minecraft worlds. The chunk to replace is given only by players actual coordinations.

## How to compile
```shell
# First compile using
gcc main.c -o chunk_replacer -std=c99 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L

# or use Cmake and then make
cmake CmakeListst.txt
make
```

## How to run
```shell
./chunk_replacer <original_dir> <replacement_dir> <x_coord> <y_coord>
```
where:
 - `original_dir` - directory with original `.mca` files
 - `replacement_dir` - directory with `.mca` files from which to take chunks
 - `x_coord` - X coordination of player standing within the chunk
 - `y_coord` - Y coordination of player standing within the chunk

## Compliance
This code is POSIX-compliant