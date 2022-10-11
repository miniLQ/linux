// SPDX-License-Identifier: GPL-2.0-only
/*
 * Detect Hung Task
 *
 * kernel/hung_task.c - kernel thread for detecting tasks stuck in D state
 *
 */

#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/lockdep.h>
#include <linux/export.h>
#include <linux/panic_notifier.h>
#include <linux/sysctl.h>
#include <linux/suspend.h>
#include <linux/utsname.h>
#include <linux/sched/signal.h>
#include <linux/sched/debug.h>
#include <linux/sched/sysctl.h>

#include <trace/events/sched.h>

/*
 * The number of tasks checked:
 */
int __read_mostly sysctl_hung_task_check_count = PID_MAX_LIMIT;

/*
 * Limit number of tasks checked in a batch.
 *
 * This value controls the preemptibility of khungtaskd since preemption
 * is disabled during the critical section. It also controls the size of
 * the RCU grace period. So it needs to be upper-bound.
 */
#define HUNG_TASK_LOCK_BREAK (HZ / 10)

/*
 * Zero means infinite timeout - no checking done:
 */
///超时周期，默认120s
unsigned long __read_mostly sysctl_hung_task_timeout_secs = CONFIG_DEFAULT_HUNG_TASK_TIMEOUT;

/*
 * Zero (default value) means use sysctl_hung_task_timeout_secs:
 */
unsigned long __read_mostly sysctl_hung_task_check_interval_secs;

int __read_mostly sysctl_hung_task_warnings = 10;

static int __read_mostly did_panic;
static bool hung_task_show_lock;
static bool hung_task_call_panic;
static bool hung_task_show_all_bt;

static struct task_struct *watchdog_task;

#ifdef CONFIG_SMP
/*
 * Should we dump all CPUs backtraces in a hung task event?
 * Defaults to 0, can be changed via sysctl.
 */
unsigned int __read_mostly sysctl_hung_task_all_cpu_backtrace;
#endif /* CONFIG_SMP */

/*
 * Should we panic (and reboot, if panic_timeout= is set) when a
 * hung task is detected:
 */
unsigned int __read_mostly sysctl_hung_task_panic =
				CONFIG_BOOTPARAM_HUNG_TASK_PANIC_VALUE;

static int
hung_task_panic(struct notifier_block *this, unsigned long event, void *ptr)
{
	did_panic = 1;

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = hung_task_panic,
};

static void check_hung_task(struct task_struct *t, unsigned long timeout)
{
	///进程总切换次数=主动切换次数+被动切换次数
	unsigned long switch_count = t->nvcsw + t->nivcsw;

	/*
	 * Ensure the task is not frozen.
	 * Also, skip vfork and any other user process that freezer should skip.
	 */
	///frozen的进程，vfork创建的父进程, 都直接跳过
	if (unlikely(t->flags & (PF_FROZEN | PF_FREEZER_SKIP)))
	    return;

	/*
	 * When a freshly created task is scheduled once, changes its state to
	 * TASK_UNINTERRUPTIBLE without having ever been switched out once, it
	 * musn't be checked.
	 */
	///新创建，还没调度过的进程，跳过
	if (unlikely(!switch_count))
		return;

	///被切换过
	if (switch_count != t->last_switch_count) {
		t->last_switch_count = switch_count;
		t->last_switch_time = jiffies;
		return;
	}
	///timeout没超时
	if (time_is_after_jiffies(t->last_switch_time + timeout * HZ))
		return;

	trace_sched_process_hang(t);

	if (sysctl_hung_task_panic) {
		///调整控制台打印级别，保证panic时能看到show_lock等有用信息
		console_verbose();
		hung_task_show_lock = true;
		hung_task_call_panic = true;
	}

	/*
	 * Ok, the task did not get scheduled for more than 2 minutes,
	 * complain:
	 */
	///打印sysctl_hung_task_warnings个hung_task,sysctl_hung_task_warnings==-1时，打印所有hung_task
	if (sysctl_hung_task_warnings) {
		if (sysctl_hung_task_warnings > 0)
			sysctl_hung_task_warnings--;
		pr_err("INFO: task %s:%d blocked for more than %ld seconds.\n",
		       t->comm, t->pid, (jiffies - t->last_switch_time) / HZ);
		pr_err("      %s %s %.*s\n",
			print_tainted(), init_utsname()->release,
			(int)strcspn(init_utsname()->version, " "),
			init_utsname()->version);
		pr_err("\"echo 0 > /proc/sys/kernel/hung_task_timeout_secs\""
			" disables this message.\n");
		sched_show_task(t);
		hung_task_show_lock = true;

		if (sysctl_hung_task_all_cpu_backtrace)
			hung_task_show_all_bt = true;
	}

	touch_nmi_watchdog();
}

/*
 * To avoid extending the RCU grace period for an unbounded amount of time,
 * periodically exit the critical section and enter a new one.
 *
 * For preemptible RCU it is sufficient to call rcu_read_unlock in order
 * to exit the grace period. For classic RCU, a reschedule is required.
 */
static bool rcu_lock_break(struct task_struct *g, struct task_struct *t)
{
	bool can_cont;

	get_task_struct(g);
	get_task_struct(t);
	rcu_read_unlock();
	cond_resched();
	rcu_read_lock();
	///pid_alive判断进程是否还活着
	can_cont = pid_alive(g) && pid_alive(t);
	put_task_struct(t);
	put_task_struct(g);

	return can_cont;
}

/*
 * Check whether a TASK_UNINTERRUPTIBLE does not get woken up for
 * a really long time (120 seconds). If that happens, print out
 * a warning.
 */
/*遍历系统所有进程,检测是否有D状态进程超过120s，若有做相应处理*/
static void check_hung_uninterruptible_tasks(unsigned long timeout)
{
	///每次遍历进程的最大个数，防止锁占用时间太长
	///可以配置，默认系统最大进程数
	int max_count = sysctl_hung_task_check_count;
	unsigned long last_break = jiffies;
	struct task_struct *g, *t;

	/*
	 * If the system crashed already then all bets are off,
	 * do not report extra hung tasks:
	 */
	///进程已经crash或panic，不报告hungtask
	if (test_taint(TAINT_DIE) || did_panic)
		return;

	hung_task_show_lock = false;
	rcu_read_lock();

	///双重循环，遍历所有进程的所有线程
	for_each_process_thread(g, t) {
		if (!max_count--)
			goto unlock;
		///避免rcu占用时间过长，超过HUNG_TASK_LOCK_BREAK主动让出CPU
		if (time_after(jiffies, last_break + HUNG_TASK_LOCK_BREAK)) {
			///如果线程已经不alive,退出循环,用continue更合适?
			if (!rcu_lock_break(g, t))
				goto unlock;
			last_break = jiffies;
		}
		/* use "==" to skip the TASK_KILLABLE tasks waiting on NFS */
		if (READ_ONCE(t->__state) == TASK_UNINTERRUPTIBLE)
			check_hung_task(t, timeout);
	}
 unlock:
	rcu_read_unlock();
	if (hung_task_show_lock)
		debug_show_all_locks();

	///打印所有堆栈信息
	if (hung_task_show_all_bt) {
		hung_task_show_all_bt = false;
		trigger_all_cpu_backtrace();
	}

	///panic
	if (hung_task_call_panic)
		panic("hung_task: blocked tasks");
}

static long hung_timeout_jiffies(unsigned long last_checked,
				 unsigned long timeout)
{
	/* timeout of 0 will disable the watchdog */
	return timeout ? last_checked - jiffies + timeout * HZ :
		MAX_SCHEDULE_TIMEOUT;
}

/*
 * Process updating of timeout sysctl
 */
///当 /proc/sys/kernel/hung_task_timeout_secs写入新值时, 唤醒khungtaskd
int proc_dohung_task_timeout_secs(struct ctl_table *table, int write,
				  void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_doulongvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		goto out;

	pr_err("---ret=%d,write=%d\n",ret,write);
	///唤醒watchdog_task
	wake_up_process(watchdog_task);

 out:
	return ret;
}

static atomic_t reset_hung_task = ATOMIC_INIT(0);

void reset_hung_task_detector(void)
{
	atomic_set(&reset_hung_task, 1);
}
EXPORT_SYMBOL_GPL(reset_hung_task_detector);

static bool hung_detector_suspended;

///获取待机挂起状态
static int hungtask_pm_notify(struct notifier_block *self,
			      unsigned long action, void *hcpu)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
		hung_detector_suspended = true;
		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		hung_detector_suspended = false;
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

/*
 * kthread which checks for tasks stuck in D state
 */
static int watchdog(void *dummy)
{
	unsigned long hung_last_checked = jiffies;

	///设置nice=0,避免占用过多CPU资源
	set_user_nice(current, 0);

	for ( ; ; ) {
		unsigned long timeout = sysctl_hung_task_timeout_secs;
		unsigned long interval = sysctl_hung_task_check_interval_secs;
		long t;

		if (interval == 0)
			interval = timeout;
		interval = min_t(unsigned long, interval, timeout);
		t = hung_timeout_jiffies(hung_last_checked, interval);
		///超时,执行检测hungtask
		///系统待机时，不做hungtask
		if (t <= 0) {
			///atomic_xchg返回reset_hung_task旧值
			///比如从虚拟机中恢复时(commit:8b414521bc5375注释)，检测的hung_task可能不准,过滤掉?
			if (!atomic_xchg(&reset_hung_task, 0) &&
			    !hung_detector_suspended)
				///判断超时用timerout，检测周期是interval
				check_hung_uninterruptible_tasks(timeout);
			hung_last_checked = jiffies;
			continue;
		}
		///t jiffies后唤醒本线程
		schedule_timeout_interruptible(t);
	}

	return 0;
}

/*
 * func: 创建一个内核线程，用来检测系统中是否有进程，维持D状态超过120s(默认)
 */
static int __init hung_task_init(void)
{
	///注册panic通知链，在panic时执行相应操作,这里设置did_panic=1;
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);

	///系统待机时，不执行hung_task
	/* Disable hung task detector on suspend */
	pm_notifier(hungtask_pm_notify, 0);

	watchdog_task = kthread_run(watchdog, NULL, "khungtaskd");

	return 0;
}
subsys_initcall(hung_task_init);
