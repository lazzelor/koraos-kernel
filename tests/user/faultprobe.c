// SPDX-License-Identifier: GPL-3.0-or-later
// EL0 fixture: faults a program takes and catches (fault_handler), machine
// code it writes and runs (code_sync), and the memory map (machine_info).
//   faultprobe          runs every case below, one "faultprobe: <case>" line
//                       each, and "faultprobe: all passed" (exit 0) or a FAIL
//                       line (exit 1)
//   faultprobe crash    a child that reads an unmapped address, uncaught
//   faultprobe disarmed a child that arms a handler, disarms it, and faults
//   faultprobe twice    a child whose handler faults in turn
//   faultprobe thread   a child whose second thread faults while main sleeps
#include "../../user/libk/koraos.h"

#include <setjmp.h>

// Past the end of the MMU's map (the low 4 GB) on every target, and not the
// Pi 4's PCIe window (24 GB): nothing is mapped there.
#define UNMAPPED 0x200000000ul

static int failures;

static void check(int ok, const char *what) {
    if (!ok) {
        printf("faultprobe: FAIL %s\n", what);
        failures++;
    }
}

// --- Catching ------------------------------------------------------------------

static jmp_buf env;
static volatile unsigned got_kind;
static volatile uintptr_t got_addr, got_pc;
static volatile uint64_t got_esr;

static void on_fault(unsigned kind, uintptr_t addr, uintptr_t pc, uint64_t esr) {
    got_kind = kind;
    got_addr = addr;
    got_pc = pc;
    got_esr = esr;
    longjmp(env, 1);
}

// Run what() with the handler armed: 1 if it faulted (got_* say how).
static int caught(void (*what)(void)) {
    got_kind = 0;
    got_addr = got_pc = 0;
    if (setjmp(env) == 0) {
        fault_handler(on_fault);
        what();
        fault_handler(NULL);
        return 0;
    }
    return 1;
}

static void read_unmapped(void) {
    (void)*(volatile uint64_t *)(UNMAPPED + 8);
}

static uintptr_t code_word;  // an address in the code region (machine_info)

static void write_code(void) {
    volatile uint32_t *p = (volatile uint32_t *)code_word;
    *p = *p;  // the same value: harmless if it did not fault
}

static void undefined(void) {
    asm volatile("udf #0");
}

static void breakpoint(void) {
    asm volatile("brk #1");
}

static void jump_unmapped(void) {
    ((void (*)(void))UNMAPPED)();
}

static void nothing(void) {
}

static void faults(void) {
    check(caught(read_unmapped) && got_kind == FAULT_DATA && got_addr == UNMAPPED + 8 &&
              got_pc != 0 && (got_esr >> 26) == 0x24,
          "a read of an unmapped address is caught, with its address");
    if (code_word != 0) {
        check(caught(write_code) && got_kind == (FAULT_DATA | FAULT_WRITE) &&
                  got_addr == code_word,
              "a write to code is caught as a write");
    }
    check(caught(undefined) && got_kind == FAULT_UNDEF && *(uint32_t *)got_pc == 0,
          "an undefined instruction, the pc at the UDF");
    check(caught(breakpoint) && got_kind == FAULT_BREAK && *(uint32_t *)got_pc == 0xD4200020,
          "a BRK, the pc at it");
    check(caught(jump_unmapped) && got_kind == FAULT_EXEC && got_addr == UNMAPPED &&
              got_pc == UNMAPPED,
          "a jump to where nothing is mapped");
    check(!caught(nothing), "no fault, no handler run");
    // Caught faults leave the program as it was: catch the same one again.
    check(caught(read_unmapped) && got_kind == FAULT_DATA, "and again");
    printf("faultprobe: data, code, undefined, break and jump faults caught\n");
}

// --- Machine code --------------------------------------------------------------

static void machine_code(void) {
    uint32_t *code = malloc(64);
    check(code != NULL, "a buffer for code");
    if (code == NULL) {
        return;
    }
    code[0] = 0x52800540;  // mov w0, #42
    code[1] = 0xD65F03C0;  // ret
    code_sync(code, 8);
    check(((int (*)(void))code)() == 42, "code written to RAM runs");
    code[0] = 0x0B010000;  // add w0, w0, w1: written over, so the old copy must go
    code_sync(code, 4);
    check(((int (*)(int, int))code)(40, 2) == 42, "rewritten code runs, with arguments");
    free(code);
    printf("faultprobe: machine code written to memory runs\n");
}

// --- The map -------------------------------------------------------------------

static const struct machine_region *region_of(const struct machine_info *m, uintptr_t addr) {
    for (unsigned i = 0; i < m->region_count; i++) {
        const struct machine_region *r = &m->regions[i];
        if (addr >= r->base && addr - r->base < r->size) {
            return r;
        }
    }
    return NULL;
}

static void memory_map(void) {
    static struct machine_info m;
    check(machine_info(&m) == 0, "machine_info");
    check(machine_info(NULL) == -1 && machine_info((void *)UNMAPPED) == -1, "bad pointers refused");
    check(m.board[0] != '\0' && m.region_count >= 3 && m.region_count <= MACHINE_REGIONS_MAX,
          "a board and its regions");
    for (unsigned i = 0; i < m.region_count; i++) {
        const struct machine_region *r = &m.regions[i];
        static const char *const kinds[] = {"?", "RAM", "code", "device"};
        printf("faultprobe:   %-6s 0x%09lx to 0x%09lx\n", kinds[r->kind <= 3 ? r->kind : 0],
               r->base, r->base + r->size);
        check(r->size != 0 && (i == 0 || r->base >= m.regions[i - 1].base + m.regions[i - 1].size),
              "regions in order, apart");
        if (r->kind == MEM_CODE && code_word == 0) {
            code_word = r->base + r->size / 2;
        }
    }
    int here = 0;
    const struct machine_region *stack = region_of(&m, (uintptr_t)&here);
    void *heap = malloc(16);
    const struct machine_region *mine = region_of(&m, (uintptr_t)heap);
    free(heap);
    check(stack != NULL && stack->kind == MEM_RAM && mine != NULL && mine->kind == MEM_RAM,
          "the stack and the heap are in RAM");
    check(region_of(&m, UNMAPPED) == NULL, "nothing mapped past 4 GB");
    if (m.periph_base != 0) {
        const struct machine_region *io = region_of(&m, m.periph_base);
        check(io != NULL && io->kind == MEM_DEVICE, "the Pi's peripherals are a device region");
    }
    printf("faultprobe: machine %s, %u regions\n", m.board, m.region_count);
}

// --- Faults that end a program -------------------------------------------------

static long thread_faults(void *arg) {
    (void)arg;
    msleep(20);
    read_unmapped();
    return 0;
}

static void bad_handler(unsigned kind, uintptr_t addr, uintptr_t pc, uint64_t esr) {
    (void)kind, (void)addr, (void)pc, (void)esr;
    read_unmapped();  // disarmed now: this one ends the program
}

static int child(const char *what) {
    if (!strcmp(what, "crash")) {
        read_unmapped();
    } else if (!strcmp(what, "disarmed")) {
        fault_handler(on_fault);
        fault_handler(NULL);
        read_unmapped();
    } else if (!strcmp(what, "twice")) {
        fault_handler(bad_handler);
        read_unmapped();
    } else if (!strcmp(what, "thread")) {
        if (thread_create(thread_faults, NULL, -1) < 0) {
            return 2;
        }
        msleep(5000);  // killed with the program long before
    } else {
        return 2;
    }
    return 1;
}

static void ended(void) {
    static const char *const children[] = {"crash", "disarmed", "twice", "thread"};
    for (unsigned i = 0; i < 4; i++) {
        int code = 0;
        int pid = spawn_flags("faultprobe", 2, (char *[]){"faultprobe", (char *)children[i]},
                              SPAWN_NOWAIT);
        check(pid > 0 && waitpid(pid, &code, 0) == pid && code == EXIT_FAULT, children[i]);
    }
    printf("faultprobe: an uncaught fault ends its program, not the machine\n");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        return child(argv[1]);
    }
    memory_map();
    faults();
    machine_code();
    ended();
    if (failures == 0) {
        printf("faultprobe: all passed\n");
    }
    return failures != 0;
}
