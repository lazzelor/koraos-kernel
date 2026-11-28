// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The machine and its memory map as a program sees it (machine_info,
// syscall 71; user/libk/koraos.h has the same layout). The layouts are
// pure, so the host tests check them (tests/host/machine_test.c).

#include "common.h"

#define MACHINE_REGIONS_MAX 6
#define MEM_RAM 1     // read and written
#define MEM_CODE 2    // read and run: writes fault
#define MEM_DEVICE 3  // the peripherals' registers

struct kmachine_region {
    uint64_t base;
    uint64_t size;
    uint32_t kind;
    uint32_t reserved;
};

struct kmachine_info {
    char board[32];
    uint32_t revision;  // the Pi's board revision code, 0 on virt
    uint32_t region_count;
    uint64_t ram_bytes;    // all the RAM, mapped or not
    uint64_t periph_base;  // the Pi's peripherals (PBASE), 0 on virt
    struct kmachine_region regions[MACHINE_REGIONS_MAX];
};

_Static_assert(sizeof(struct kmachine_info) == 200, "machine info ABI");

// A Pi's name from its revision code ("Raspberry Pi 3 Model B+"), or the
// generic "Raspberry Pi 3" or "Raspberry Pi 4" (rpi_version) for a code
// this table does not know.
const char *machine_pi_board(int rpi_version, uint32_t revision);

// A Pi's map (src/mm/mmu.c): code from 0 (page 0 and the 2 MB blocks of
// text) to code_end, RAM from there to the RAM's end or device_base,
// whichever comes first, and the peripherals from device_base (to the
// ARM's local ones' end, 0x40040000, on the Pi 3; to 4 GB on the Pi 4).
// ram_bytes 0 (not known) counts as RAM up to device_base.
void machine_pi_layout(struct kmachine_info *m, int rpi_version, uint32_t revision,
                       uint64_t ram_bytes, uint64_t code_end, uint64_t periph_base,
                       uint64_t device_base);

// virt's map: devices below the RAM, the RAM with the kernel's code (from
// code_start to code_end) in it, up to 4 GB (the end of the MMU's map).
void machine_virt_layout(struct kmachine_info *m, uint64_t ram_base, uint64_t ram_size,
                         uint64_t code_start, uint64_t code_end);
