#ifndef __ASM_ARCH_MSM_CACHE_ERP_H
#define __ASM_ARCH_MSM_CACHE_ERP_H

struct ipq_reg {
	char *name;
	void __iomem *addr; /* ioremap-ed address */
};

struct ipq_error_status_reg {
	struct ipq_reg *reg;
	int num;
};
#endif /* __ASM_ARCH_MSM_CACHE_ERP_H */
