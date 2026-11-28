// SPDX-License-Identifier: GPL-3.0-or-later
// Host tests for the kinds of fault a program takes (src/proc/fault.c), from
// syndromes as an AArch64 core reports them.
#include "proc/fault.h"
#include "test.h"

#define ESR(ec, iss) (((uint64_t)(ec) << 26) | (1u << 25) | (iss))

static void kinds(void) {
    // A read of an unmapped address: a level-1 translation fault (DFSC 5).
    CHECK(fault_kind(ESR(0x24, 0x05)) == FAULT_DATA, "a read");
    CHECK(fault_has_addr(ESR(0x24, 0x05)), "with its address");
    // A write to a read-only block: a permission fault (DFSC 0xd), WnR set.
    CHECK(fault_kind(ESR(0x24, 0x4d)) == (FAULT_DATA | FAULT_WRITE), "a write");
    CHECK(FAULT_KIND(fault_kind(ESR(0x24, 0x4d))) == FAULT_DATA, "FAULT_KIND drops the write");
    // FnV: FAR not valid (an external abort may say so).
    CHECK(!fault_has_addr(ESR(0x24, 0x410)), "FnV: no address");
    CHECK(fault_kind(ESR(0x24, 0x21)) == FAULT_ALIGN, "a misaligned load");
    CHECK(fault_kind(ESR(0x24, 0x61)) == (FAULT_ALIGN | FAULT_WRITE), "a misaligned store");
    CHECK(fault_kind(ESR(0x20, 0x0f)) == FAULT_EXEC && fault_has_addr(ESR(0x20, 0x0f)),
          "a jump to where no code may run, with its address");
    CHECK(fault_kind(ESR(0x22, 0)) == FAULT_ALIGN && fault_has_addr(ESR(0x22, 0)),
          "a misaligned PC, the PC as address");
    CHECK(fault_kind(ESR(0x26, 0)) == FAULT_ALIGN && !fault_has_addr(ESR(0x26, 0)),
          "a misaligned SP, no address");
    CHECK(fault_kind(ESR(0x00, 0)) == FAULT_UNDEF, "UDF");
    CHECK(fault_kind(ESR(0x18, 0)) == FAULT_UNDEF, "a system register EL0 may not use");
    CHECK(fault_kind(ESR(0x3c, 7)) == FAULT_BREAK && !fault_has_addr(ESR(0x3c, 7)), "BRK #7");
    CHECK(fault_kind(ESR(0x0e, 0)) == FAULT_OTHER, "an illegal execution state");
}

static void names(void) {
    CHECK(strcmp(fault_name(FAULT_DATA | FAULT_WRITE), "data abort") == 0, "data abort");
    CHECK(strcmp(fault_name(FAULT_UNDEF), "undefined instruction") == 0, "undefined");
    CHECK(strcmp(fault_name(FAULT_BREAK), "breakpoint") == 0, "breakpoint");
    CHECK(strcmp(fault_name(99), "fault") == 0, "anything else");
}

TEST_MAIN(kinds, names)
