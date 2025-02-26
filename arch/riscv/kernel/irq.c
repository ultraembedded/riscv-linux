// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 * Copyright (C) 2018 Christoph Hellwig
 */

#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/seq_file.h>
#include <asm/smp.h>

#ifdef CONFIG_XILINX_INTC
extern unsigned int xintc_get_irq(void);
#endif

/*
 * Possible interrupt causes:
 */
#define INTERRUPT_CAUSE_SOFTWARE    1
#define INTERRUPT_CAUSE_TIMER       5
#define INTERRUPT_CAUSE_EXTERNAL    9

/*
 * The high order bit of the trap cause register is always set for
 * interrupts, which allows us to differentiate them from exceptions
 * quickly.  The INTERRUPT_CAUSE_* macros don't contain that bit, so we
 * need to mask it off.
 */
#define INTERRUPT_CAUSE_FLAG	(1UL << (__riscv_xlen - 1))

int arch_show_interrupts(struct seq_file *p, int prec)
{
	show_ipi_stats(p, prec);
	return 0;
}

asmlinkage void __irq_entry do_IRQ(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();
	switch (regs->scause & ~INTERRUPT_CAUSE_FLAG) {
#ifdef CONFIG_RISCV_TIMER
	case INTERRUPT_CAUSE_TIMER:
		riscv_timer_interrupt();
		break;
#endif
#ifdef CONFIG_SMP
	case INTERRUPT_CAUSE_SOFTWARE:
		/*
		 * We only use software interrupts to pass IPIs, so if a non-SMP
		 * system gets one, then we don't know what to do.
		 */
		riscv_software_interrupt();
		break;
#endif
	case INTERRUPT_CAUSE_EXTERNAL:
#ifdef CONFIG_XILINX_INTC
		{
			unsigned int irq;
			irq = xintc_get_irq();
		next_irq:
			BUG_ON(!irq);
			generic_handle_irq(irq);

			irq = xintc_get_irq();
			if (irq != -1U) {
				pr_debug("next irq: %d\n", irq);
				goto next_irq;
			}
		}
		csr_clear(sip, 1 << INTERRUPT_CAUSE_EXTERNAL);
#else
		handle_arch_irq(regs);
#endif
		break;
	default:
		panic("unexpected interrupt cause");
	}
	irq_exit();

	set_irq_regs(old_regs);
}

void __init init_IRQ(void)
{
	irqchip_init();
}

