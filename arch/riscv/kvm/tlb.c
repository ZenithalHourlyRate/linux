// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Ventana Micro Systems Inc.
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/kvm_host.h>
#include <asm/csr.h>
#include <asm/hwcap.h>
#include <asm/insn-def.h>

#define has_svinval()	\
	static_branch_unlikely(&riscv_isa_ext_keys[RISCV_ISA_EXT_KEY_SVINVAL])

void kvm_riscv_local_hfence_gvma_vmid_gpa(unsigned long vmid,
					  gpa_t gpa, gpa_t gpsz,
					  unsigned long order)
{
	gpa_t pos;

	if (PTRS_PER_PTE < (gpsz >> order)) {
		kvm_riscv_local_hfence_gvma_vmid_all(vmid);
		return;
	}

	if (has_svinval()) {
		asm volatile (SFENCE_W_INVAL() ::: "memory");
		for (pos = gpa; pos < (gpa + gpsz); pos += BIT(order))
			asm volatile (HINVAL_GVMA(%0, %1)
			: : "r" (pos >> 2), "r" (vmid) : "memory");
		asm volatile (SFENCE_INVAL_IR() ::: "memory");
	} else {
		for (pos = gpa; pos < (gpa + gpsz); pos += BIT(order))
			asm volatile (HFENCE_GVMA(%0, %1)
			: : "r" (pos >> 2), "r" (vmid) : "memory");
	}
}

void kvm_riscv_local_hfence_gvma_vmid_all(unsigned long vmid)
{
	asm volatile(HFENCE_GVMA(zero, %0) : : "r" (vmid) : "memory");
}

void kvm_riscv_local_hfence_gvma_gpa(gpa_t gpa, gpa_t gpsz,
				     unsigned long order)
{
	gpa_t pos;

	if (PTRS_PER_PTE < (gpsz >> order)) {
		kvm_riscv_local_hfence_gvma_all();
		return;
	}

	if (has_svinval()) {
		asm volatile (SFENCE_W_INVAL() ::: "memory");
		for (pos = gpa; pos < (gpa + gpsz); pos += BIT(order))
			asm volatile(HINVAL_GVMA(%0, zero)
			: : "r" (pos >> 2) : "memory");
		asm volatile (SFENCE_INVAL_IR() ::: "memory");
	} else {
		for (pos = gpa; pos < (gpa + gpsz); pos += BIT(order))
			asm volatile(HFENCE_GVMA(%0, zero)
			: : "r" (pos >> 2) : "memory");
	}
}

void kvm_riscv_local_hfence_gvma_all(void)
{
	asm volatile(HFENCE_GVMA(zero, zero) : : : "memory");
}

void kvm_riscv_local_hfence_vvma_asid_gva(unsigned long vmid,
					  unsigned long asid,
					  unsigned long gva,
					  unsigned long gvsz,
					  unsigned long order)
{
	unsigned long pos, hgatp;

	if (PTRS_PER_PTE < (gvsz >> order)) {
		kvm_riscv_local_hfence_vvma_asid_all(vmid, asid);
		return;
	}

	hgatp = csr_swap(CSR_HGATP, vmid << HGATP_VMID_SHIFT);

	if (has_svinval()) {
		asm volatile (SFENCE_W_INVAL() ::: "memory");
		for (pos = gva; pos < (gva + gvsz); pos += BIT(order))
			asm volatile(HINVAL_VVMA(%0, %1)
			: : "r" (pos), "r" (asid) : "memory");
		asm volatile (SFENCE_INVAL_IR() ::: "memory");
	} else {
		for (pos = gva; pos < (gva + gvsz); pos += BIT(order))
			asm volatile(HFENCE_VVMA(%0, %1)
			: : "r" (pos), "r" (asid) : "memory");
	}

	csr_write(CSR_HGATP, hgatp);
}

void kvm_riscv_local_hfence_vvma_asid_all(unsigned long vmid,
					  unsigned long asid)
{
	unsigned long hgatp;

	hgatp = csr_swap(CSR_HGATP, vmid << HGATP_VMID_SHIFT);

	asm volatile(HFENCE_VVMA(zero, %0) : : "r" (asid) : "memory");

	csr_write(CSR_HGATP, hgatp);
}

void kvm_riscv_local_hfence_vvma_gva(unsigned long vmid,
				     unsigned long gva, unsigned long gvsz,
				     unsigned long order)
{
	unsigned long pos, hgatp;

	if (PTRS_PER_PTE < (gvsz >> order)) {
		kvm_riscv_local_hfence_vvma_all(vmid);
		return;
	}

	hgatp = csr_swap(CSR_HGATP, vmid << HGATP_VMID_SHIFT);

	if (has_svinval()) {
		asm volatile (SFENCE_W_INVAL() ::: "memory");
		for (pos = gva; pos < (gva + gvsz); pos += BIT(order))
			asm volatile(HINVAL_VVMA(%0, zero)
			: : "r" (pos) : "memory");
		asm volatile (SFENCE_INVAL_IR() ::: "memory");
	} else {
		for (pos = gva; pos < (gva + gvsz); pos += BIT(order))
			asm volatile(HFENCE_VVMA(%0, zero)
			: : "r" (pos) : "memory");
	}

	csr_write(CSR_HGATP, hgatp);
}

void kvm_riscv_local_hfence_vvma_all(unsigned long vmid)
{
	unsigned long hgatp;

	hgatp = csr_swap(CSR_HGATP, vmid << HGATP_VMID_SHIFT);

	asm volatile(HFENCE_VVMA(zero, zero) : : : "memory");

	csr_write(CSR_HGATP, hgatp);
}

void kvm_riscv_local_tlb_sanitize(struct kvm_vcpu *vcpu)
{
	unsigned long vmid;

	if (!kvm_riscv_gstage_vmid_bits() ||
	    vcpu->arch.last_exit_cpu == vcpu->cpu)
		return;

	/*
	 * On RISC-V platforms with hardware VMID support, we share same
	 * VMID for all VCPUs of a particular Guest/VM. This means we might
	 * have stale G-stage TLB entries on the current Host CPU due to
	 * some other VCPU of the same Guest which ran previously on the
	 * current Host CPU.
	 *
	 * To cleanup stale TLB entries, we simply flush all G-stage TLB
	 * entries by VMID whenever underlying Host CPU changes for a VCPU.
	 */

	vmid = READ_ONCE(vcpu->kvm->arch.vmid.vmid);
	kvm_riscv_local_hfence_gvma_vmid_all(vmid);
}
