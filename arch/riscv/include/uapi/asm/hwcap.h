/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copied from arch/arm64/include/asm/hwcap.h
 *
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2017 SiFive
 */
#ifndef _UAPI_ASM_RISCV_HWCAP_H
#define _UAPI_ASM_RISCV_HWCAP_H

/*
 * Linux saves the floating-point registers according to the ISA Linux is
 * executing on, as opposed to the ISA the user program is compiled for.  This
 * is necessary for a handful of esoteric use cases: for example, userspace
 * threading libraries must be able to examine the actual machine state in
 * order to fully reconstruct the state of a thread.
 */
#define COMPAT_HWCAP_ISA_I	(1 << ('I' - 'A'))
#define COMPAT_HWCAP_ISA_M	(1 << ('M' - 'A'))
#define COMPAT_HWCAP_ISA_A	(1 << ('A' - 'A'))
#define COMPAT_HWCAP_ISA_F	(1 << ('F' - 'A'))
#define COMPAT_HWCAP_ISA_D	(1 << ('D' - 'A'))
#define COMPAT_HWCAP_ISA_C	(1 << ('C' - 'A'))

/*
 * HWCAP2 flags - for elf_hwcap2 (in kernel) and AT_HWCAP2
 *
 * As only 32 bits of elf_hwcap (in kernel) could be used
 * and RISC-V has reserved 26 bits of it, other caps like
 * bitmanip and crypto can not be placed in AT_HWCAP
 */
#define COMPAT_HWCAP2_ISA_ZBA   (1 <<  0)
#define COMPAT_HWCAP2_ISA_ZBB   (1 <<  1)
#define COMPAT_HWCAP2_ISA_ZBC   (1 <<  2)
#define COMPAT_HWCAP2_ISA_ZBS   (1 <<  3)
#define COMPAT_HWCAP2_ISA_ZBKB  (1 <<  4)
#define COMPAT_HWCAP2_ISA_ZBKC  (1 <<  5)
#define COMPAT_HWCAP2_ISA_ZBKX  (1 <<  6)
#define COMPAT_HWCAP2_ISA_ZKND  (1 <<  7)
#define COMPAT_HWCAP2_ISA_ZKNE  (1 <<  8)
#define COMPAT_HWCAP2_ISA_ZKNH  (1 <<  9)
#define COMPAT_HWCAP2_ISA_ZKSED (1 << 10)
#define COMPAT_HWCAP2_ISA_ZKSH  (1 << 11)
#define COMPAT_HWCAP2_ISA_ZKR   (1 << 12)
#define COMPAT_HWCAP2_ISA_ZKT   (1 << 13)

#endif /* _UAPI_ASM_RISCV_HWCAP_H */
