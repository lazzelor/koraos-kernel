// SPDX-License-Identifier: GPL-3.0-or-later
// Host tests for the machine's name and memory map (src/sys/machine.c), as
// machine_info (syscall 71) reports them.
#include "sys/machine.h"
#include "test.h"

#define MB ((uint64_t)1 << 20)
#define GB ((uint64_t)1 << 30)

static int region_is(const struct kmachine_info *m, unsigned i, uint64_t base, uint64_t end,
                     uint32_t kind) {
    return i < m->region_count && m->regions[i].base == base &&
           m->regions[i].size == end - base && m->regions[i].kind == kind;
}

static void boards(void) {
    CHECK(strcmp(machine_pi_board(3, 0xa02082), "Raspberry Pi 3 Model B") == 0, "Pi 3B");
    CHECK(strcmp(machine_pi_board(3, 0xa020d3), "Raspberry Pi 3 Model B+") == 0, "Pi 3B+");
    CHECK(strcmp(machine_pi_board(4, 0xc03114), "Raspberry Pi 4 Model B") == 0, "Pi 4B 4 GB");
    CHECK(strcmp(machine_pi_board(4, 0xc03130), "Raspberry Pi 400") == 0, "Pi 400");
    CHECK(strcmp(machine_pi_board(4, 0xb03199), "Raspberry Pi 4") == 0, "an unknown type");
    CHECK(strcmp(machine_pi_board(3, 0), "Raspberry Pi 3") == 0, "no revision");
}

static void pi3(void) {
    struct kmachine_info m;
    machine_pi_layout(&m, 3, 0xa02082, GB, 4 * MB, 0x3F000000, 0x3F000000);
    CHECK(strcmp(m.board, "Raspberry Pi 3 Model B") == 0, "board");
    CHECK(m.revision == 0xa02082 && m.ram_bytes == GB && m.periph_base == 0x3F000000, "facts");
    CHECK(m.region_count == 3, "three regions: %u", m.region_count);
    CHECK(region_is(&m, 0, 0, 4 * MB, MEM_CODE), "code from 0");
    CHECK(region_is(&m, 1, 4 * MB, 0x3F000000, MEM_RAM), "RAM up to the peripherals");
    CHECK(region_is(&m, 2, 0x3F000000, 0x40040000, MEM_DEVICE), "peripherals and local ones");
}

static void pi4(void) {
    struct kmachine_info m;
    // 8 GB: what lies past the peripherals and 4 GB is not mapped.
    machine_pi_layout(&m, 4, 0xd03114, 8 * GB, 6 * MB, 0xFE000000, 0xFC000000);
    CHECK(region_is(&m, 1, 6 * MB, 0xFC000000, MEM_RAM), "8 GB: RAM up to the devices");
    CHECK(region_is(&m, 2, 0xFC000000, 4 * GB, MEM_DEVICE), "devices to 4 GB");
    CHECK(m.ram_bytes == 8 * GB, "all the RAM counted");
    // 1 GB: nothing between its end and the devices.
    machine_pi_layout(&m, 4, 0xa03111, GB, 6 * MB, 0xFE000000, 0xFC000000);
    CHECK(region_is(&m, 1, 6 * MB, GB, MEM_RAM), "1 GB: RAM to its end");
    // Unknown RAM: as far as the map goes.
    machine_pi_layout(&m, 4, 0, 0, 6 * MB, 0xFE000000, 0xFC000000);
    CHECK(region_is(&m, 1, 6 * MB, 0xFC000000, MEM_RAM), "RAM unknown: up to the devices");
}

static void virt(void) {
    struct kmachine_info m;
    machine_virt_layout(&m, GB, GB, GB, GB + 4 * MB);
    CHECK(strcmp(m.board, "QEMU virt") == 0 && m.revision == 0 && m.periph_base == 0, "facts");
    CHECK(m.region_count == 3, "no RAM below the code: %u regions", m.region_count);
    CHECK(region_is(&m, 0, 0, GB, MEM_DEVICE), "devices below the RAM");
    CHECK(region_is(&m, 1, GB, GB + 4 * MB, MEM_CODE), "code");
    CHECK(region_is(&m, 2, GB + 4 * MB, 2 * GB, MEM_RAM), "RAM after it");
    machine_virt_layout(&m, GB, 8 * GB, GB + 2 * MB, GB + 6 * MB);
    CHECK(m.region_count == 4 && region_is(&m, 1, GB, GB + 2 * MB, MEM_RAM), "RAM below the code");
    CHECK(region_is(&m, 3, GB + 6 * MB, 4 * GB, MEM_RAM), "the map ends at 4 GB");
}

TEST_MAIN(boards, pi3, pi4, virt)
