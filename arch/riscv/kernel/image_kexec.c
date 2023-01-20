// SPDX-License-Identifier: GPL-2.0
/*
 * Kexec image loader

 * Adapted from arch/arm64/kernel/kexec_image.c
 * Copyright (C) 2018 Linaro Limited
 * Author: AKASHI Takahiro <takahiro.akashi@linaro.org>
 */
#define pr_fmt(fmt)	"kexec_file(Image): " fmt

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kexec.h>
#include <linux/pe.h>
#include <linux/string.h>
#include <linux/verification.h>
#include <asm/byteorder.h>
#include <asm/image.h>


static int image_probe(const char *kernel_buf, unsigned long kernel_len)
{
	const struct riscv_image_header *h =
		(const struct riscv_image_header *)(kernel_buf);

	if (!h || (kernel_len < sizeof(*h)))
		return -EINVAL;

	if (memcmp(&h->magic2, RISCV_IMAGE_MAGIC2, sizeof(h->magic2)))
		return -EINVAL;

	return 0;
}

static void *image_load(struct kimage *image,
				char *kernel, unsigned long kernel_len,
				char *initrd, unsigned long initrd_len,
				char *cmdline, unsigned long cmdline_len)
{
	struct riscv_image_header *h;
	u64 flags;
	bool be_image, be_kernel;
	struct kexec_buf kbuf;
	unsigned long text_offset, kernel_segment_number;
	unsigned long kernel_start;
	struct kexec_segment *kernel_segment;
	int ret;

	h = (struct riscv_image_header *)kernel;
	if (!h->image_size)
		return ERR_PTR(-EINVAL);

	/* Check cpu features */
	flags = le64_to_cpu(h->flags);
	be_image = __HEAD_FLAG(BE);
	be_kernel = IS_ENABLED(CONFIG_CPU_BIG_ENDIAN);
	if (be_image != be_kernel)
		return ERR_PTR(-EINVAL);

	/* Load the kernel */
	kbuf.image = image;
	kbuf.buf_min = 0;
	kbuf.buf_max = ULONG_MAX;
	kbuf.top_down = false;

	kbuf.buffer = kernel;
	kbuf.bufsz = kernel_len;
	kbuf.mem = KEXEC_BUF_MEM_UNKNOWN;
	kbuf.memsz = le64_to_cpu(h->image_size);
	text_offset = le64_to_cpu(h->text_offset);
	kbuf.buf_align = PAGE_SIZE;

	/* Adjust kernel segment with TEXT_OFFSET */
	kbuf.memsz += text_offset;

	kernel_segment_number = image->nr_segments;

	/*
	 * The location of the kernel segment may make it impossible to satisfy
	 * the other segment requirements, so we try repeatedly to find a
	 * location that will work.
	 */
	while ((ret = kexec_add_buffer(&kbuf)) == 0) {
		/* Try to load additional data */
		kernel_segment = &image->segment[kernel_segment_number];
		ret = load_other_segments(image, kernel_segment->mem,
					  kernel_segment->memsz, initrd,
					  initrd_len, cmdline);
		if (!ret)
			break;

		/*
		 * We couldn't find space for the other segments; erase the
		 * kernel segment and try the next available hole.
		 */
		image->nr_segments -= 1;
		kbuf.buf_min = kernel_segment->mem + kernel_segment->memsz;
		kbuf.mem = KEXEC_BUF_MEM_UNKNOWN;
	}

	if (ret) {
		pr_err("Could not find any suitable kernel location!");
		return ERR_PTR(ret);
	}

	kernel_segment = &image->segment[kernel_segment_number];
	kernel_segment->mem += text_offset;
	kernel_segment->memsz -= text_offset;
	kernel_start = kernel_segment->mem;
	image->start = kernel_start;


	pr_debug("Loaded kernel at 0x%lx bufsz=0x%lx memsz=0x%lx\n",
				kernel_segment->mem, kbuf.bufsz,
				kernel_segment->memsz);

#ifdef CONFIG_ARCH_HAS_KEXEC_PURGATORY
	/* Add purgatory to the image */
	kbuf.top_down = true;
	kbuf.mem = KEXEC_BUF_MEM_UNKNOWN;
	ret = kexec_load_purgatory(image, &kbuf);
	if (ret) {
		pr_err("Error loading purgatory ret=%d\n", ret);
		return ERR_PTR(ret);
	}
	ret = kexec_purgatory_get_set_symbol(image, "riscv_kernel_entry",
					     &kernel_start,
					     sizeof(kernel_start), 0);
	if (ret)
		pr_err("Error update purgatory ret=%d\n", ret);
#endif /* CONFIG_ARCH_HAS_KEXEC_PURGATORY */

	return ret ? ERR_PTR(ret) : NULL;
}

#ifdef CONFIG_KEXEC_IMAGE_VERIFY_SIG
static int image_verify_sig(const char *kernel, unsigned long kernel_len)
{
	return verify_pefile_signature(kernel, kernel_len, NULL,
				       VERIFYING_KEXEC_PE_SIGNATURE);
}
#endif

const struct kexec_file_ops image_kexec_ops = {
	.probe = image_probe,
	.load = image_load,
#ifdef CONFIG_KEXEC_IMAGE_VERIFY_SIG
	.verify_sig = image_verify_sig,
#endif
};
