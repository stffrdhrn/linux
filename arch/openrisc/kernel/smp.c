/*
 * Copyright (C) 2014 Stefan Kristiansson <stefan.kristiansson@saunalahti.fi>
 * Copyright (C) 2017 Stafford Horne <shorne@gmail.com>
 *
 * Based on arm64 and arc implementations
 * Copyright (C) 2013 ARM Ltd.
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/kexec.h>
#include <linux/sched.h>
#include <linux/sched/hotplug.h>
#include <linux/sched/mm.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <asm/cpuinfo.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/time.h>

typedef void (*smp_stop_action_t)(int cpu);

static void (*smp_cross_call)(const struct cpumask *, unsigned int);
static unsigned int ipi_irq;

unsigned long secondary_release = -1;
struct thread_info *secondary_thread_info;

enum ipi_msg_type {
	IPI_WAKEUP,
	IPI_RESCHEDULE,
	IPI_CALL_FUNC,
	IPI_CALL_FUNC_SINGLE,
};

static DEFINE_SPINLOCK(boot_lock);

static void boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	printk("boot_secondary: CPU%d ts->stack: %08lx, ti->ksp: %08lx",
		cpu, (unsigned long) idle->stack, task_thread_info(idle)->ksp);
	secondary_release = cpu;
	smp_cross_call(cpumask_of(cpu), IPI_WAKEUP);

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);
}

void __init smp_init_cpus(void)
{
	struct device_node *cpu;
	u32 cpu_id;

	for_each_of_cpu_node(cpu) {
		cpu_id = of_get_cpu_hwid(cpu, 0);
		if (cpu_id < NR_CPUS)
			set_cpu_possible(cpu_id, true);
	}
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned int cpu;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for_each_possible_cpu(cpu) {
		if (cpu < max_cpus)
			set_cpu_present(cpu, true);
	}
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

static DECLARE_COMPLETION(cpu_running);

int __cpu_up(unsigned int cpu, struct task_struct *idle)
{
	if (smp_cross_call == NULL) {
		pr_warn("CPU%u: failed to start, IPI controller missing",
			cpu);
		return -EIO;
	}

	secondary_thread_info = task_thread_info(idle);
	current_pgd[cpu] = init_mm.pgd;

	boot_secondary(cpu, idle);
	if (!wait_for_completion_timeout(&cpu_running,
					msecs_to_jiffies(1000))) {
		pr_crit("CPU%u: failed to start\n", cpu);
		return -EIO;
	}
	synchronise_count_master(cpu);

	return 0;
}

static void hotplug_wakeup(int cpu);

asmlinkage void secondary_start_kernel(void)
{
	struct mm_struct *mm = &init_mm;
	unsigned int cpu = smp_processor_id();
	/*
	 * All kernel threads share the same mm context; grab a
	 * reference and switch to it.
	 */
	mmgrab(mm);
	current->active_mm = mm;
	cpumask_set_cpu(cpu, mm_cpumask(mm));

	pr_info("CPU%u: Booted secondary processor\n", cpu);

	setup_cpuinfo();
	openrisc_clockevent_init();

	notify_cpu_starting(cpu);

	/*
	 * OK, now it's safe to let the boot CPU continue
	 */
	complete(&cpu_running);

	synchronise_count_slave(cpu);
	set_cpu_online(cpu, true);

	local_irq_enable();
	/*
	 * OK, it's off to the idle thread for us
	 */
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}

void handle_IPI(unsigned int ipi_msg)
{
	unsigned int cpu = smp_processor_id();

	switch (ipi_msg) {
	case IPI_WAKEUP:
		hotplug_wakeup(cpu);
		break;

	case IPI_RESCHEDULE:
		scheduler_ipi();
		break;

	case IPI_CALL_FUNC:
		generic_smp_call_function_interrupt();
		break;

	case IPI_CALL_FUNC_SINGLE:
		generic_smp_call_function_single_interrupt();
		break;

	default:
		WARN(1, "CPU%u: Unknown IPI message 0x%x\n", cpu, ipi_msg);
		break;
	}
}

void arch_smp_send_reschedule(int cpu)
{
	smp_cross_call(cpumask_of(cpu), IPI_RESCHEDULE);
}

static void stop_this_cpu(void *info)
{
	int cpu = smp_processor_id();

	local_irq_disable();

	/*
	 * This IPI callback may have an extra parameter during crash to save
	 * registers, but we could use this for other things.
	 */
	if (info != NULL) {
		smp_stop_action_t func = (smp_stop_action_t)info;
		func(cpu);
	}

	/* Remove this CPU */
	set_cpu_online(cpu, false);

	/* CPU Doze */
	if (mfspr(SPR_UPR) & SPR_UPR_PMP)
		mtspr(SPR_PMR, mfspr(SPR_PMR) | SPR_PMR_DME);
	/* If that didn't work, infinite loop */
	while (1)
		;
}

/*
 * The number of CPUs online, not counting this CPU (which may not be
 * fully online and so not counted in num_online_cpus()).
 */
static inline unsigned int num_other_online_cpus(void)
{
	unsigned int this_cpu_online = cpu_online(smp_processor_id());

	return num_online_cpus() - this_cpu_online;
}

void smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, NULL, 0);
}

void __init set_smp_cross_call(void (*fn)(const struct cpumask *, unsigned int),
			       unsigned int irq)
{
	smp_cross_call = fn;
	ipi_irq = irq;
}

void arch_send_call_function_single_ipi(int cpu)
{
	smp_cross_call(cpumask_of(cpu), IPI_CALL_FUNC_SINGLE);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	smp_cross_call(mask, IPI_CALL_FUNC);
}

#ifdef CONFIG_HOTPLUG_CPU
/*
 * __cpu_disable runs on the processor to be shutdown.
 */
int __cpu_disable(void)
{
	unsigned int cpu = smp_processor_id();

#ifdef CONFIG_GENERIC_ARCH_TOPOLOGY
	remove_cpu_topology(cpu);
#endif

	/*
	 * Take this CPU offline.  Once we clear this, we can't return,
	 * and we must not schedule until we're ready to give up the cpu.
	 */
	set_cpu_online(cpu, false);
	disable_percpu_irq(ipi_irq);

	/*
	 * OK - migrate IRQs away from this CPU
	 */
	irq_migrate_all_off_this_cpu();

	local_flush_tlb_all();

	return 0;
}

void arch_cpuhp_cleanup_dead_cpu(unsigned int cpu)
{
	pr_notice("CPU%u: shutdown\n", cpu);
}

void __noreturn arch_cpu_idle_dead(void)
{
	idle_task_exit();

	cpuhp_ap_report_dead();

	play_dead();

	/* We should never get here */
	BUG();
}

bool arch_cpu_is_hotpluggable(int cpu)
{
	return cpu > 0;
}

static bool is_cpu_in_dead_spin(unsigned long pc)
{
	unsigned long play_dead_start = (unsigned long) __pa(&play_dead);
	unsigned long play_dead_end = play_dead_start + play_dead_size;
	bool res = pc >= play_dead_start && pc < play_dead_end;

	printk("is_cpu_in_dead_spin: start: %08lx - pc: %08lx - end: %08lx -> %d",
	       play_dead_start, pc, play_dead_end, res);

	return res;
}

static void hotplug_wakeup(int cpu)
{
	struct pt_regs *irq_regs = get_irq_regs();
	printk("hotplug_wakeup: CPU%d", cpu);

	/* If the core is within play dead kick it out */
	if (is_cpu_in_dead_spin(irq_regs->pc)) {
		irq_regs->pc = __pa(&secondary_hotplug_release);
		irq_regs->sr = SPR_SR_SM;
		printk("hotplug_wakeup: release: %08lx, pc: %08lx, sp: %08lx",
		       (unsigned long) &secondary_hotplug_release, irq_regs->pc,
		       irq_regs->gpr[1]);
	}
}
#else
static void hotplug_wakeup(int cpu) { }
#endif /* CONFIG_HOTPLUG_CPU */

#ifdef CONFIG_KEXEC_CORE
static void crash_smp_save_regs(int cpu)
{
	crash_save_cpu(get_irq_regs(), cpu);
}

void crash_smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, crash_smp_save_regs, 0);
}
#endif

/* TLB flush operations - Performed on each CPU*/
static inline void ipi_flush_tlb_all(void *ignored)
{
	local_flush_tlb_all();
}

static inline void ipi_flush_tlb_mm(void *info)
{
	struct mm_struct *mm = (struct mm_struct *)info;

	local_flush_tlb_mm(mm);
}

static void smp_flush_tlb_mm(struct cpumask *cmask, struct mm_struct *mm)
{
	unsigned int cpuid;

	if (cpumask_empty(cmask))
		return;

	cpuid = get_cpu();

	if (cpumask_any_but(cmask, cpuid) >= nr_cpu_ids) {
		/* local cpu is the only cpu present in cpumask */
		local_flush_tlb_mm(mm);
	} else {
		on_each_cpu_mask(cmask, ipi_flush_tlb_mm, mm, 1);
	}
	put_cpu();
}

struct flush_tlb_data {
	unsigned long addr1;
	unsigned long addr2;
};

static inline void ipi_flush_tlb_page(void *info)
{
	struct flush_tlb_data *fd = (struct flush_tlb_data *)info;

	local_flush_tlb_page(NULL, fd->addr1);
}

static inline void ipi_flush_tlb_range(void *info)
{
	struct flush_tlb_data *fd = (struct flush_tlb_data *)info;

	local_flush_tlb_range(NULL, fd->addr1, fd->addr2);
}

static void smp_flush_tlb_range(const struct cpumask *cmask, unsigned long start,
				unsigned long end)
{
	unsigned int cpuid;

	if (cpumask_empty(cmask))
		return;

	cpuid = get_cpu();

	if (cpumask_any_but(cmask, cpuid) >= nr_cpu_ids) {
		/* local cpu is the only cpu present in cpumask */
		if ((end - start) <= PAGE_SIZE)
			local_flush_tlb_page(NULL, start);
		else
			local_flush_tlb_range(NULL, start, end);
	} else {
		struct flush_tlb_data fd;

		fd.addr1 = start;
		fd.addr2 = end;

		if ((end - start) <= PAGE_SIZE)
			on_each_cpu_mask(cmask, ipi_flush_tlb_page, &fd, 1);
		else
			on_each_cpu_mask(cmask, ipi_flush_tlb_range, &fd, 1);
	}
	put_cpu();
}

void flush_tlb_all(void)
{
	on_each_cpu(ipi_flush_tlb_all, NULL, 1);
}

void flush_tlb_mm(struct mm_struct *mm)
{
	smp_flush_tlb_mm(mm_cpumask(mm), mm);
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
	smp_flush_tlb_range(mm_cpumask(vma->vm_mm), uaddr, uaddr + PAGE_SIZE);
}

void flush_tlb_range(struct vm_area_struct *vma,
		     unsigned long start, unsigned long end)
{
	const struct cpumask *cmask = vma ? mm_cpumask(vma->vm_mm)
					  : cpu_online_mask;
	smp_flush_tlb_range(cmask, start, end);
}

/* Instruction cache invalidate - performed on each cpu */
static void ipi_icache_page_inv(void *arg)
{
	struct page *page = arg;

	local_icache_page_inv(page);
}

void smp_icache_page_inv(struct page *page)
{
	on_each_cpu(ipi_icache_page_inv, page, 1);
}
EXPORT_SYMBOL(smp_icache_page_inv);
