#include "arch/exception.h"
#include "arch/fpsimd.h"
#include "arch/sysregs.h"
#include "arch/trapframe.h"
#include "common.h"
#include "lib/panic.h"
#include "lib/printf.h"
#include "proc/fault.h"
#include "proc/task.h"
#include "sys/syscall.h"

extern char vectors[];

void exception_init(void) {
    asm volatile("msr vbar_el1, %0" ::"r"(vectors));
    asm volatile("isb");
}

static const char *vector_name(uint64_t type) {
    switch (type) {
    case 0: return "EL1t sync";
    case 1: return "EL1t IRQ";
    case 2: return "EL1t FIQ";
    case 3: return "EL1t SError";
    case 5: return "EL1h IRQ";
    case 6: return "EL1h FIQ";
    case 7: return "EL1h SError";
    case 9: return "EL0 IRQ";
    case 10: return "EL0 FIQ";
    case 11: return "EL0 SError";
    case 12: return "EL0(32) sync";
    case 13: return "EL0(32) IRQ";
    case 14: return "EL0(32) FIQ";
    case 15: return "EL0(32) SError";
    default: return "unknown";
    }
}

static void halt(void) {
    while (1) {
        asm volatile("wfi");
    }
}

void handle_invalid_entry(uint64_t type, uint64_t esr, uint64_t far,
                          struct trapframe *tf) {
    panic_begin();
    printf("\n*** Unexpected exception: %s ***\n", vector_name(type));
    printf("  ESR=0x%lx EC=0x%lx FAR=0x%lx ELR=0x%lx\n", esr, ESR_EC(esr), far,
           tf->elr);
    halt();
}

void handle_sync_el1(uint64_t esr, uint64_t far, struct trapframe *tf) {
    panic_begin();
    printf("\n*** Synchronous exception in EL1 (kernel fault) ***\n");
    if (ESR_EC(esr) == ESR_EC_FP_ASIMD) {
        printf("  kernel code used FP/SIMD; the kernel must be integer-only\n");
    }
    printf("  ESR=0x%lx EC=0x%lx FAR=0x%lx ELR=0x%lx\n", esr, ESR_EC(esr), far,
           tf->elr);
    halt();
}

static void user_fault(uint64_t esr, uint64_t far, struct trapframe *tf);

// Synchronous exception from EL0: a syscall, the first FP/SIMD use since the
// task was switched in, or a user fault.
void handle_sync_el0(uint64_t esr, uint64_t far, struct trapframe *tf) {
    task_die_if_killed();  // its process exited or was killed meanwhile
    uint64_t ec = ESR_EC(esr);
    if (ec == ESR_EC_SVC64) {
        syscall_handle(tf);
        return;
    }
    if (ec == ESR_EC_FP_ASIMD) {
        fpsimd_trap();  // the faulting instruction is retried on return
        return;
    }
    user_fault(esr, far, tf);
}

// A fault in a program: its fault handler runs in place of the instruction
// (fault_handler, syscall 70), or the program ends with EXIT_FAULT. The
// rest of the system goes on: with no memory protection, what the program
// wrote before the fault is the only harm it can have done.
static void user_fault(uint64_t esr, uint64_t far, struct trapframe *tf) {
    task_t *t = task_current();
    unsigned kind = fault_kind(esr);
    uint64_t addr = fault_has_addr(esr) ? far : 0;
    if (t->fault_handler != 0) {
        // fn(kind, addr, pc, esr) on the thread's stack, aligned in case the
        // fault was a misaligned SP. It must not return: x30 is 0. One fault
        // only: a fault in the handler ends the program.
        tf->regs[0] = kind;
        tf->regs[1] = addr;
        tf->regs[2] = tf->elr;
        tf->regs[3] = esr;
        tf->regs[30] = 0;
        tf->sp &= ~(uint64_t)15;
        tf->elr = t->fault_handler;
        t->fault_handler = 0;
        return;
    }
    // "[fault]", not a panic's "***": the machine goes on.
    printf("[fault] %s (pid %d, thread %d): %s", t->proc != NULL ? t->proc->name : t->name,
           t->proc != NULL ? t->proc->pid : 0, t->pid, fault_name(kind));
    if (fault_has_addr(esr)) {
        const char *how = ESR_EC(esr) != ESR_EC_DABT_LOWER ? "at"
                          : kind & FAULT_WRITE ? "writing" : "reading";
        printf(" %s 0x%lx", how, addr);
    }
    printf(", pc 0x%lx, ESR 0x%lx: program ended\n", tf->elr, esr);
    task_exit(EXIT_FAULT);
}
