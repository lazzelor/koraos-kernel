// SPDX-License-Identifier: GPL-3.0-or-later
//
// The machine and its memory map, for machine_info (syscall 71): the
// board's name, and the ranges src/mm/mmu.c maps that hold something.

#include "sys/machine.h"

static void set_board(struct kmachine_info *m, const char *name) {
    unsigned i = 0;
    for (; name[i] != '\0' && i + 1 < sizeof(m->board); i++) {
        m->board[i] = name[i];
    }
    for (; i < sizeof(m->board); i++) {
        m->board[i] = '\0';
    }
}

static void add(struct kmachine_info *m, uint64_t base, uint64_t end, uint32_t kind) {
    if (end > base && m->region_count < MACHINE_REGIONS_MAX) {
        m->regions[m->region_count++] = (struct kmachine_region){base, end - base, kind, 0};
    }
}

static void clear(struct kmachine_info *m) {
    char *p = (char *)m;
    for (unsigned i = 0; i < sizeof(*m); i++) {
        p[i] = 0;
    }
}

const char *machine_pi_board(int rpi_version, uint32_t revision) {
    // A new-style code (bit 23) has the board's type in bits 4 to 11.
    if (revision & (1u << 23)) {
        switch ((revision >> 4) & 0xff) {
        case 0x08: return "Raspberry Pi 3 Model B";
        case 0x0a: return "Compute Module 3";
        case 0x0d: return "Raspberry Pi 3 Model B+";
        case 0x0e: return "Raspberry Pi 3 Model A+";
        case 0x10: return "Compute Module 3+";
        case 0x11: return "Raspberry Pi 4 Model B";
        case 0x13: return "Raspberry Pi 400";
        case 0x14: return "Compute Module 4";
        case 0x18: return "Compute Module 4S";
        default: break;
        }
    }
    return rpi_version == 4 ? "Raspberry Pi 4" : "Raspberry Pi 3";
}

void machine_pi_layout(struct kmachine_info *m, int rpi_version, uint32_t revision,
                       uint64_t ram_bytes, uint64_t code_end, uint64_t periph_base,
                       uint64_t device_base) {
    clear(m);
    set_board(m, machine_pi_board(rpi_version, revision));
    m->revision = revision;
    m->ram_bytes = ram_bytes;
    m->periph_base = periph_base;
    uint64_t ram_end = ram_bytes != 0 && ram_bytes < device_base ? ram_bytes : device_base;
    add(m, 0, code_end, MEM_CODE);
    add(m, code_end, ram_end, MEM_RAM);
    // The Pi 3's peripherals (16 MB) are followed by the ARM's local ones
    // (the cores' timers and mailboxes); the Pi 4's fill the top 64 MB of
    // the low 4 GB, the GIC included.
    add(m, device_base, rpi_version == 4 ? (uint64_t)1 << 32 : 0x40040000, MEM_DEVICE);
}

void machine_virt_layout(struct kmachine_info *m, uint64_t ram_base, uint64_t ram_size,
                         uint64_t code_start, uint64_t code_end) {
    clear(m);
    set_board(m, "QEMU virt");
    m->ram_bytes = ram_size;
    // The MMU maps the low 4 GB only.
    uint64_t map_end = (uint64_t)1 << 32;
    uint64_t ram_end = ram_base + ram_size < map_end ? ram_base + ram_size : map_end;
    add(m, 0, ram_base, MEM_DEVICE);
    add(m, ram_base, code_start, MEM_RAM);
    add(m, code_start, code_end, MEM_CODE);
    add(m, code_end, ram_end, MEM_RAM);
}
