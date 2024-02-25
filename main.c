#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#define CHUNKS_IN_REGION 1024

#define CHUNKS_PER_REGION 32
#define BLOCKS_PER_CHUNK 16
#define KIB 1024
#define FOUR_KIB (4 * KIB)

int open_actdir_or_die() {
    int dir_fd = open(".", O_DIRECTORY | O_RDONLY);
    if (dir_fd < 0) {
        err(-1, "Cannot open actual folder");
    }

    return dir_fd;
}

int open_or_die(int dir_fd, char* filename, int oflag) {
    int fd = openat(dir_fd, filename, oflag, 0644);
    if (fd < 0) {
        err(-1, "Cannot open file %s", filename);
    }

    return fd;
}

struct chunk {
    unsigned char* address;
    uint8_t sectors;
    uint32_t timestamp;
} typedef chunk;


uint32_t load_3bytes_bigend(const unsigned char* memstart) {
    uint32_t number = 0;
    unsigned char act_byte;
    for (int byte_n = 0; byte_n < 3; ++byte_n) {
        number <<= 8;
        act_byte = *(memstart + byte_n);
        number |= act_byte;
    }
    return number;
}

void store_3bytes_bigend(uint32_t value, unsigned char* memstart) {
    *(memstart + 2) = (char) (value & 0xff);
    *(memstart + 1) = (char) ((value >> 8) & 0xff);
    *(memstart) = (char) ((value >> 16) & 0xff);
}

/**
 * Loading chunks region using Region file format
 * https://minecraft.fandom.com/wiki/Region_file_format
 * Working in big endian
 *
 * File structure:
 * 0x0000 - 0x0FFF locations   - 1024 entries (4 bytes)
 * 0x1000 - 0x1FFF timestamps  - 1024 entries (4 bytes)
 * 0x2000 -        chunks
 *
 * Locations entry:
 * byte 0-2 - offset
 * byte   3 - sector count
 * @param memory
 * @param chunks
 */
void load_chunks(unsigned char *memory, chunk *chunks) {
    for (int index = 0; index < CHUNKS_IN_REGION; ++index) {
        uint32_t offset = load_3bytes_bigend(memory + index * 4);
        chunks[index].address = memory + FOUR_KIB * offset;

        chunks[index].sectors = *(memory + index * 4 + 3);

        // byte order does not matter, we won't change the value
        chunks[index].timestamp =  *((uint32_t *) memory + index + CHUNKS_IN_REGION);
    }
}

void store_chunks(unsigned char *memory, chunk *chunks) {
    ssize_t counter = 2;
    for (int index = 0; index < CHUNKS_IN_REGION; ++index) {
        store_3bytes_bigend(counter, memory + index * 4);
        if (chunks[index].sectors != 0) {
            memcpy(memory + FOUR_KIB * counter, chunks[index].address,
                   FOUR_KIB * chunks[index].sectors);
            counter += chunks[index].sectors;
        }

        *(memory + index * 4 + 3) = chunks[index].sectors;
        *((uint32_t *) memory + index + CHUNKS_IN_REGION) = chunks[index].timestamp;
    }
}


struct vect2d {
    int64_t x;
    int64_t y;
} typedef vect2d;

struct pos {
    vect2d region;
    vect2d chunk;
    vect2d chunk_in_region;
} typedef pos;

void replace_chunk(chunk *in_region, chunk *repl_region, pos pos) {
    int64_t index = pos.chunk_in_region.y * CHUNKS_PER_REGION + pos.chunk_in_region.x;
    in_region[index].address = repl_region[index].address;
    in_region[index].sectors = repl_region[index].sectors;
}

ssize_t region_file_size(chunk *chunks) {
    ssize_t size = 0;
    for (int index = 0; index < CHUNKS_IN_REGION; ++index) {
        size += chunks[index].sectors;  // in 4kiBs
    }
    size += 2; // beginning tables
    size *= FOUR_KIB;
    return size;
}

ssize_t get_filesize_or_die(int fd) {
    ssize_t act_position = lseek(fd, 0, SEEK_CUR);
    if (act_position < 0) {
        err(-1, "Cannot lseek the file");
    }

    ssize_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size < 0) {
        err(-1, "Cannot lseek the file end");
    }

    if (lseek(fd, act_position, SEEK_SET)) {
        err(-1, "Cannot lseek the actual position");
    }

    return file_size;
}

char* mmap_or_die(int fd, ssize_t size, int prot) {
    char* file_memory;
    file_memory = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if (file_memory == (void*) -1) {
        err(-1, "Cannot map file to memory");
    }

    return file_memory;
}

void tests() {
    unsigned char test1[] = {0xff, 0x1f, 0x11, 0x01};
    uint32_t test_value = load_3bytes_bigend(test1);

    unsigned char output1[] = {0x00, 0x00, 0x00, 0x00};
    store_3bytes_bigend(test_value, output1);
    if (test1[0] != output1[0] || test1[1] != output1[1] || test1[2] != output1[2]) {
        err(-2, "Problem with endiannes");
    }

    unsigned char output2[] = {0x00, 0x00, 0x00, 0x00};
    store_3bytes_bigend(test_value, output2 + 1);
    if (test1[0] != output2[1] || test1[1] != output2[2] || test1[2] != output2[3]) {
        err(-2, "Problem with endiannes 2");
    }
}

struct args {
    char *in_folder;
    char *repl_folder;
    int64_t x;
    int64_t y;

} typedef args;

void process_args(char** argv, args* args) {
    args->in_folder = argv[1];
    args->repl_folder = argv[2];
    args->x = strtoll(argv[3], NULL, 10);
    args->y = strtoll(argv[4], NULL, 10);
}


int64_t negdivl(int64_t dividend, int64_t divisor) {
    int64_t result;
    if (dividend >= 0)
        result = dividend / divisor;
    else {
        result = (dividend + 1) / divisor;
        result--;
    }

    return result;
}

void get_position(int64_t x, int64_t y, pos *position) {
    position->chunk.x = negdivl(x, BLOCKS_PER_CHUNK);
    position->chunk.y = negdivl(y, BLOCKS_PER_CHUNK);

    position->region.x = negdivl(position->chunk.x, CHUNKS_PER_REGION);
    position->region.y = negdivl(position->chunk.y, CHUNKS_PER_REGION);

    position->chunk_in_region.x = position->chunk.x % 32;
    position->chunk_in_region.y = position->chunk.y % 32;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Usage <old> <replacing> <x-coord> <y-coord>\n");
        return -1;
    }
    tests();
    args args = {0};
    process_args(argv, &args);

    pos pos = {0};

    get_position(args.x, args.y, &pos);

    int actdir_fd = open_actdir_or_die();
    int origdir_fd = open(args.in_folder, O_DIRECTORY, O_RDONLY);
    if (origdir_fd < 0) {
        err(-1, "Cannot open orig directory %s", args.in_folder);
    }

    int replacedir_fd = open(args.repl_folder, O_DIRECTORY, O_RDONLY);
    if (replacedir_fd < 0) {
        err(-1, "Cannot open replace directory %s", args.in_folder);
    }


    if (mkdirat(actdir_fd, "output", S_IRWXU | S_IXOTH | S_IXGRP | S_IRGRP | S_IROTH) < 0) {
        if (errno != EEXIST)
            err(-1, "Cannot create directory");
    }
    int outdir_fd = openat(actdir_fd, "output", O_DIRECTORY | O_RDONLY);
    if (outdir_fd < 0) {
        err(-1, "Cannot open directory");
    }

    char reg_name[512];
    snprintf((char *) &reg_name, sizeof (reg_name) - 1, "r.%ld.%ld.mca", pos.region.x, pos.region.y);

    int in_fd = open_or_die(origdir_fd, reg_name, O_RDONLY);
    int repl_fd = open_or_die(replacedir_fd, reg_name, O_RDONLY);
    int out_fd = open_or_die(outdir_fd, reg_name, O_RDWR | O_CREAT | O_TRUNC);

    ssize_t in_size = get_filesize_or_die(in_fd);
    unsigned char* file_cont = (unsigned  char*) mmap_or_die(in_fd, in_size, PROT_READ);

    ssize_t repl_size = get_filesize_or_die(repl_fd);
    unsigned char* file_repl = (unsigned  char*) mmap_or_die(repl_fd, repl_size, PROT_READ);


    chunk *in_region = malloc(sizeof (chunk) * 1024);
    if (in_region == NULL) {
        err(-1, "Cannot allocate in region");
    }
    chunk *repl_region = malloc(sizeof (chunk) * 1024);
    if (repl_region == NULL) {
        err(-1, "Cannot allocate repl region");
    }

    load_chunks(file_cont, in_region);
    load_chunks(file_repl, repl_region);

    replace_chunk(in_region, repl_region, pos);


    ssize_t counted_size = region_file_size(in_region);
    printf("counted region file size %zd\n", counted_size);

    if (ftruncate(out_fd, counted_size) < 0) {
        err(-1, "Cannot resize (trunct) file");
    }

    unsigned char* out_cont = (unsigned  char*) mmap_or_die(out_fd, counted_size, PROT_READ | PROT_WRITE);
    store_chunks(out_cont, in_region);

    free(in_region);
    free(repl_region);

    if (munmap(file_cont, in_size) < 0) {
        err(-1, "Cannot unmap file");
    }

    if (munmap(file_repl, repl_size) < 0) {
        err(-1, "Cannot unmap file");
    }

    if (munmap(out_cont, in_size) < 0) {
        err(-1, "Cannot unmap file");
    }


    printf("act dir %d, act file %d, size %ld\n", actdir_fd, in_fd, in_size);

    return 0;
}
