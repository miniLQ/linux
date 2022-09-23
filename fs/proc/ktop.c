#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/cpumask.h>
#include "../../../kernel/sched/sched.h"
#include <linux/ktop.h>

#include <linux/kernel.h>

#include <linux/types.h>
#include <net/sock.h>
#include <linux/netlink.h>

#define MSG_LEN  (512)

#define KTOP_INTERVAL (1*MSEC_PER_SEC)

#define KTOP_DIR_NAME             "ktop"
#define KTOP_INTERVAL_PROC_NAME   "interval"
#define KTOP_THRESHOLD_PROC_NAME  "threshold"
#define KTOP_PID_PROC_NAME        "pid_foreground"

#define KTOP_MAX (1000)

struct cpuload_cfg {
	int interval;
	int threshold;
	int pid_foreground;
};

struct cpuload_cfg  gktop_config = {
	.interval = MSEC_PER_SEC * 3,
	.threshold = 10,
	.pid_foreground = 0,
};
struct ktop_info {
	int flag;
	int pid;
	struct task_struct *task;
	u32 sum_exec[KTOP_REPORT];
	struct list_head	list_entry[KTOP_REPORT];
};
struct ktop_info ktop_list[KTOP_MAX];

static u8 cur_idx;
u64 ktop_time[KTOP_I];
//TODO reset this ktop_n
u32 ktop_n = KTOP_MAX;
#ifdef KTOP_DEBUG_PRINT
u32 ktop_stat__add;
#endif

DEFINE_SPINLOCK(ktop_lock);

char report_buf[MSG_LEN];

static int ktop_report(void);

struct device *cpuload_dev;
char *s_index[2];

static ssize_t _send_cpuload(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	s_index[0] = buf;
	s_index[1] = NULL;
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, s_index);

	return 0;
}

static int send_usrmsg_cpuload(char *pbuf, uint16_t len)
{
	return _send_cpuload(cpuload_dev, NULL, pbuf, len);
}

__always_inline void ktop_add(struct task_struct *p)
{
	unsigned long flags = 0;
	int k = 0;

	for (k = 0; k < KTOP_MAX; k++) {
		if (ktop_list[k].flag == 0)	{
			if (spin_trylock_irqsave(&ktop_lock, flags)) {
				if (ktop_n < KTOP_MAX) {
					ktop_n++;
					ktop_list[k].sum_exec[cur_idx] = max(1U, (u32)(p->se.sum_exec_runtime >> 20));
					get_task_struct(p);
					ktop_list[k].pid = p->pid;
					ktop_list[k].task = p;
					ktop_list[k].flag = 1;
				}
				spin_unlock_irqrestore(&ktop_lock, flags);
			}
			break;
		}

		if (ktop_list[k].pid == p->pid)	{
			break;
		}
	}
#ifdef KTOP_DEBUG_PRINT
	ktop_stat__add++;
#endif
}
static void clean_ktop_list(void)
{
	int k = 0;

	//printk("---clear ktop_list.\n");

	for (k = 0; k < KTOP_MAX; k++) {
		if (ktop_list[k].flag) {
			ktop_list[k].flag = 0;
			ktop_list[k].sum_exec[cur_idx] = 0;
//printk("---clean pid = %d\n", ktop_list[k].pid);
			if (ktop_list[k].task)
				put_task_struct(ktop_list[k].task);

			ktop_list[k].task = NULL;
			ktop_list[k].pid = 0;
		}
	}
	ktop_n = 0;
}
static void ktop_timer_func(struct timer_list *timer)
{
	unsigned long flags;
	int k = 0;

//printk("---test at file_%s line_%d",__FILE__, __LINE__);
#ifdef KTOP_DEBUG_PRINT
	ktop_pr_dbg("ktop_n=%d", ktop_n);
	ktop_pr_dbg("ktop_stat__add=%d", ktop_stat__add);
	ktop_stat__add = 0;
#endif

	for (k = 0; k < KTOP_MAX; k++) {
		if (ktop_list[k].flag) {
//printk("--report index=%d, pid = %d,sum_exex=%d\n",k, ktop_list[k].pid, ktop_list[k].sum_exec[0]);
		}
	}
	ktop_report();

	spin_lock_irqsave(&ktop_lock, flags);

	clean_ktop_list();

	ktop_time[cur_idx] = ktime_get_boot_fast_ns();
	mod_timer(timer, jiffies + msecs_to_jiffies(gktop_config.interval));

	spin_unlock_irqrestore(&ktop_lock, flags);
}

DEFINE_TIMER(ktop_timer, ktop_timer_func);

static int ktop_report(void)
{
	struct task_struct *p, *q;
	struct ktop_info *pktop, *qk, *rk, *rk2;
	struct list_head report_list;
	struct list_head *k, *l, *n, *o;
	struct list_head *k2, *n2, *o2;
	bool report;
	int i, j = 0, start_idx;
	u32 run_tasks = 0;
	u64 now;
	u64 delta;
	unsigned long flags;
	u32 sum_exec_ = 0, sum = 0;

	INIT_LIST_HEAD(&report_list);

	spin_lock_irqsave(&ktop_lock, flags);

	start_idx = 0;

	now = ktime_get_boot_fast_ns();
	delta = now - ktop_time[start_idx];

	for (i = 0; i < KTOP_MAX; i++) {
		if (ktop_list[i].flag) {
			report = false;
			pktop = &ktop_list[i];
			p = pktop->task;
			run_tasks++;
			sum = (u32) (p->se.sum_exec_runtime >> 20);
			pktop->sum_exec[KTOP_REPORT - 1] = sum > pktop->sum_exec[0] ? (sum - pktop->sum_exec[0]) : 0;

			ktop_pr_dbg("%s() line:%d start: p=%d comm=%s sum=%u\n",
						__func__, __LINE__, pktop->task->pid, pktop->task->comm, pktop->sum_exec[KTOP_REPORT - 1]);

			list_for_each(n, &report_list) {
				k = n - (KTOP_REPORT - 1);
				rk = container_of(k, struct ktop_info, list_entry[0]);
			//	printk("run_tasks:%d,p--->time:%d,r--->time:%d\n", run_tasks,pktop->sum_exec[KTOP_REPORT - 1],rk->sum_exec[KTOP_REPORT - 1] );
				if (pktop->sum_exec[KTOP_REPORT - 1] > rk->sum_exec[KTOP_REPORT - 1]) {
					report = true;
					qk = rk;
				} else {
					break;
				}
			}

			if (report || j < KTOP_RP_NUM) {
				if (!report) {
					list_add(&pktop->list_entry[KTOP_REPORT - 1], &report_list);
				} else {
					list_add(&pktop->list_entry[KTOP_REPORT - 1], &qk->list_entry[KTOP_REPORT - 1]);
				}
				j++;
				if (j > KTOP_RP_NUM) {
					list_del(report_list.next);
				}
			}
		}
	}

	spin_unlock_irqrestore(&ktop_lock, flags);

#ifdef KTOP_DEBUG_PRINT
	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT - 1);
		rk = container_of(k, struct ktop_info, list_entry[0]);
		ktop_pr_dbg("%s() line:%d final: p=%d comm=%-16s sum=%u\n",
			    __func__, __LINE__, rk->pid, rk->task->comm,
			    rk->sum_exec[KTOP_REPORT - 1]);
	}
#endif
	//按进程统计
	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT - 1);
		rk = container_of(k, struct ktop_info, list_entry[0]);

		list_for_each_safe(n2, o2, n) {
			if (n2 == &report_list) {
				break;
			}
			k2 = n2 - (KTOP_REPORT - 1);
			rk2 = container_of(k2, struct ktop_info, list_entry[0]);
			if (rk->task->tgid == rk2->task->tgid) {
				rk->sum_exec[KTOP_REPORT - 1] += rk2->sum_exec[KTOP_REPORT - 1];
				list_del(n2);
			}
		}
	}
#ifdef KTOP_DEBUG_PRINT
	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT - 1);
		rk = container_of(k, struct ktop_info, list_entry[0]);
		ktop_pr_dbg("%s() line:%d final: p=%d comm=%-16s sum=%u\n",
			    __func__, __LINE__, rk->pid, rk->task->comm,
			    rk->sum_exec[KTOP_REPORT - 1]);
	}
#endif
	//上报
	if (!list_empty(&report_list)) {
		char comm[TASK_COMM_LEN];

		list_for_each_prev_safe(l, o, &report_list) {
			k = l - (KTOP_REPORT - 1);
			pktop = container_of(k, struct ktop_info, list_entry[0]);
			p = pktop->task;
			if (p->group_leader != p) {
				rcu_read_lock();
				q = find_task_by_vpid(p->tgid);
				if (q)
					get_task_comm(comm, q);
				rcu_read_unlock();
			}
			sum_exec_ = (pktop->sum_exec[KTOP_REPORT - 1]  * 100) / (u32) ((delta*nr_cpu_ids) >> 20);
			if (sum_exec_ >
					((p->tgid != gktop_config.pid_foreground)
					 ? gktop_config.threshold
					 : gktop_config.threshold<<2)) {
				snprintf(report_buf, MSG_LEN,
						"CPULOAD=%d|%s|%1u",
						p->tgid, (p->group_leader != p)
						? (q ? comm : "EXITED") : p->comm, sum_exec_);

				send_usrmsg_cpuload(report_buf, strlen(report_buf));
			}
			list_del(l);
		}
	}

	return 0;
}

static int ktop_interval_show(struct seq_file *m, void *v)
{
	pr_warn("%d\n", gktop_config.interval);

	return 0;
}

static int ktop_threshold_show(struct seq_file *m, void *v)
{
	pr_warn("%d\n", gktop_config.threshold);

	return 0;
}

static int ktop_pid_show(struct seq_file *m, void *v)
{
	pr_warn("%d\n", gktop_config.pid_foreground);

	return 0;
}

static ssize_t ktop_interval_write(struct file *file,
		const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	long val;
	unsigned long flags;

	if (!nbytes)
		return -EINVAL;

	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size - 1] = '\0';
	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

	if (val < 100)
		val = 100;
	if (val != gktop_config.interval) {
		spin_lock_irqsave(&ktop_lock, flags);

		gktop_config.interval = val;
		mod_timer(&ktop_timer, jiffies
				+ msecs_to_jiffies(gktop_config.interval));

		spin_unlock_irqrestore(&ktop_lock, flags);
	}

	return nbytes;
}

static ssize_t ktop_threshold_write(struct file *file,
		const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	long val;
	unsigned long flags;

	if (!nbytes)
		return -EINVAL;
	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size - 1] = '\0';
	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

	//printk("---write threshold val:%ld\n", val);

	spin_lock_irqsave(&ktop_lock, flags);
	gktop_config.threshold = val;
	spin_unlock_irqrestore(&ktop_lock, flags);

	return nbytes;
}

static ssize_t ktop_pid_write(struct file *file,
		const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	long val;
	unsigned long flags;
	int k = 0;

	if (!nbytes)
		return -EINVAL;
	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size - 1] = '\0';
	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

	if (val != gktop_config.pid_foreground) {
		spin_lock_irqsave(&ktop_lock, flags);
		for (k = 0; k < KTOP_MAX; k++) {
			if ((ktop_list[k].pid == gktop_config.pid_foreground) && ktop_list[k].flag) {
				ktop_list[k].sum_exec[cur_idx] = 0;
				ktop_list[k].flag = 0;
				put_task_struct(ktop_list[k].task);
				ktop_list[k].task = NULL;
				break;
			}
		}

		gktop_config.pid_foreground = val;

		spin_unlock_irqrestore(&ktop_lock, flags);
	}

	return nbytes;
}

static int ktop_interval_open(struct inode *inode, struct file *file)
{
	return single_open(file, ktop_interval_show, NULL);
}

static int ktop_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, ktop_threshold_show, NULL);
}

static int ktop_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, ktop_pid_show, NULL);
}

static const struct proc_ops ktop_interval_proc = {
	.proc_open = ktop_interval_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = ktop_interval_write,
	.proc_release = single_release,
};

static const struct proc_ops ktop_threshold_proc = {
	.proc_open = ktop_threshold_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = ktop_threshold_write,
	.proc_release = single_release,
};

static const struct proc_ops ktop_pid_proc = {
	.proc_open = ktop_pid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = ktop_pid_write,
	.proc_release = single_release,
};

struct proc_dir_entry *ktop_proc_dir;

static struct class cpuload_event_class = {
	.name = "cpuload_event",
	.owner = THIS_MODULE,
};

static int __init proc_ktop_init(void)
{
	int ret = -1;
	int k = 0;

	struct proc_dir_entry *file_interval = NULL;
	struct proc_dir_entry *file_threshold = NULL;
	struct proc_dir_entry *file_pid = NULL;

	ktop_proc_dir = proc_mkdir(KTOP_DIR_NAME, NULL);
	if (ktop_proc_dir == NULL) {
		pr_err("proc create %s failed\n", KTOP_DIR_NAME);
		return -EINVAL;
	}

	file_interval = proc_create(KTOP_INTERVAL_PROC_NAME,
			0666, ktop_proc_dir, &ktop_interval_proc);
	if (!file_interval) {
		pr_err("proc create %s failed\n", KTOP_INTERVAL_PROC_NAME);

		goto out0;
	}

	file_threshold = proc_create(KTOP_THRESHOLD_PROC_NAME,
				0666, ktop_proc_dir, &ktop_threshold_proc);
	if (!file_threshold) {
		pr_err("proc create %s failed\n", KTOP_THRESHOLD_PROC_NAME);

		goto out1;
	}

	file_pid = proc_create(KTOP_PID_PROC_NAME, 0666,
						ktop_proc_dir, &ktop_pid_proc);
	if (!file_pid) {
		pr_err("proc create %s failed\n", KTOP_PID_PROC_NAME);

		goto out2;
	}

	ret = class_register(&cpuload_event_class);
	if (ret < 0) {
		pr_err("%s cpuload_event: class_register fail.\n", __func__);

		goto out3;
	}

	cpuload_dev = device_create(&cpuload_event_class,
					NULL, MKDEV(0, 0),
					NULL, "cpuload_event");
	if (cpuload_dev == NULL) {
		pr_err("%s cpuload_event: device_create fail.\n", __func__);

		goto out4;
	}

	cur_idx = 0;
	for (k = 0; k < KTOP_MAX; k++) {
		ktop_list[k].flag = 0;
		ktop_list[k].pid = 0;
		ktop_list[k].sum_exec[cur_idx] = 0;
	}

	ktop_timer.expires = jiffies + msecs_to_jiffies(gktop_config.interval);
	add_timer(&ktop_timer);

	return 0;

out4:
	class_unregister(&cpuload_event_class);
out3:
	remove_proc_entry(KTOP_PID_PROC_NAME, ktop_proc_dir);
out2:
	remove_proc_entry(KTOP_THRESHOLD_PROC_NAME, ktop_proc_dir);
out1:
	remove_proc_entry(KTOP_INTERVAL_PROC_NAME, ktop_proc_dir);
out0:
	remove_proc_entry(KTOP_DIR_NAME, NULL);

	return -ENOMEM;
}

fs_initcall(proc_ktop_init);

MODULE_AUTHOR("leon.yu <leon.yu@nio.com>");
MODULE_DESCRIPTION("cpuload_event");
MODULE_LICENSE("GPL v2");
