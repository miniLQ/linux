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

#define MSG_LEN          256

#define KTOP_INTERVAL (1*MSEC_PER_SEC)

#define KTOP_DIR_NAME             "ktop"
#define KTOP_INTERVAL_PROC_NAME   "interval"
#define KTOP_THRESHOLD_PROC_NAME  "threshold"
#define KTOP_PID_PROC_NAME        "pid_focus"


typedef struct _cpuload_cfg {
       int interval;
       int threshold;
       int pid_focus;
       char reserve[20];
} cpuload_cfg;

cpuload_cfg  gktop_config = {
	.interval = MSEC_PER_SEC * 3,
	.threshold = 10,
	.pid_focus = 0,
};

static u8 cur_idx=0;
struct list_head ktop_list[KTOP_I];
u64 ktop_time[KTOP_I];
#define KTOP_MAX 1000
//TODO reset this ktop_n
u32 ktop_n = KTOP_MAX;
#ifdef KTOP_DEBUG_PRINT
u32 ktop_stat__add;
#endif

DEFINE_SPINLOCK(ktop_lock);

struct sock *nlsk_cpuload = NULL;
extern struct net init_net;

char report_buf[1024];

static int ktop_report(void);

struct device *cpuload_dev = NULL;
char * s_index[2];

static ssize_t _send_cpuload( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
    s_index[0] = buf;
    s_index[1] = NULL;
    kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, s_index);

    return 0;
}
static int send_usrmsg_cpuload(char *pbuf, uint16_t len)
{
	return 	_send_cpuload(cpuload_dev,NULL, pbuf, len);
}

void __always_inline ktop_add(struct task_struct *p)
{
	unsigned long flags;
	if (!p->sched_info.ktop.sum_exec[cur_idx]) {
		if (spin_trylock_irqsave(&ktop_lock, flags)) {
			/* TODO this could change to single ktop_n */
			if (ktop_n < KTOP_MAX) {
				ktop_n++;
				p->sched_info.ktop.sum_exec[cur_idx]  = max(1U, (u32)(p->se.sum_exec_runtime >> 20));
				get_task_struct(p);
				list_add(&p->sched_info.ktop.list_entry[cur_idx], &ktop_list[cur_idx]);
			}
			spin_unlock_irqrestore(&ktop_lock, flags);
		}
	}
#ifdef KTOP_DEBUG_PRINT
	ktop_stat__add++;
#endif
}
static void clean_ktop_list(void)
{
	struct list_head *k, *l, *m;
	struct task_struct *p;
	cur_idx = 0;
	ktop_n = 0;

	list_for_each_safe(l, m, &ktop_list[cur_idx]) {
		k = l - cur_idx;
		p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
		p->sched_info.ktop.sum_exec[cur_idx] = 0;
		//printk("---pid=%d,cur_idx=%d\n",p->pid,cur_idx);
		list_del(l);
		put_task_struct(p);
	}
}
static void ktop_timer_func(struct timer_list *timer)
{
	unsigned long flags;

#ifdef KTOP_DEBUG_PRINT
	ktop_pr_dbg("ktop_n=%d", ktop_n);
	ktop_pr_dbg("ktop_stat__add=%d", ktop_stat__add);
	ktop_stat__add = 0;
#endif

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
	struct task_struct *p, *r, *q;
	struct task_struct *r2;
	struct list_head report_list;
	struct list_head *k, *l, *n, *o;
	struct list_head *k2, *n2, *o2;
	bool report;
	int h, i, j = 0, start_idx;
	u32 run_tasks = 0;
	u64 now;
	u64 delta;
	unsigned long flags;
	u32 sum_exec_=0;
	INIT_LIST_HEAD(&report_list);

	spin_lock_irqsave(&ktop_lock, flags);

	start_idx = cur_idx + 1;
	if (start_idx == KTOP_I)
		start_idx = 0;

	now = ktime_get_boot_fast_ns();
	delta = now - ktop_time[start_idx];

	for (h = 0, i = start_idx; h < KTOP_I; h++) {
		list_for_each_safe(l, o, &ktop_list[i]) {
			int a;
			u32 sum;
			bool added = false;
			report = false;
			k = l - i;

			p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
			a = i;
			while (a != start_idx) {
				a++;
				if (a == KTOP_I)
					a = 0;
				if (p->sched_info.ktop.sum_exec[i])
					added = true;
			}
			if (added)
				break;
			run_tasks++;
			sum = (u32) (p->se.sum_exec_runtime >> 20);
			p->sched_info.ktop.sum_exec[KTOP_REPORT - 1] =
			    sum > p->sched_info.ktop.sum_exec[i] ?
			    (sum - p->sched_info.ktop.sum_exec[i]) : 0;
			ktop_pr_dbg("%s() line:%d start: p=%d comm=%s sum=%u\n",
						__func__, __LINE__, p->pid, p->comm,
						p->sched_info.ktop.sum_exec[KTOP_REPORT - 1]);
			list_for_each(n, &report_list) {
				k = n - (KTOP_REPORT - 1);
				r = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
				//printk("run_tasks:%d,p--->time:%d,r--->time:%d\n", run_tasks,p->sched_info.ktop.sum_exec[KTOP_REPORT - 1],r->sched_info.ktop.sum_exec[KTOP_REPORT - 1] );
				if (p->sched_info.ktop.sum_exec[KTOP_REPORT - 1] > r->sched_info.ktop.sum_exec[KTOP_REPORT - 1]) {
					report = true;
					q = r;
				} else {
					break;
				}
			}
			if (report || j < KTOP_RP_NUM) {
				get_task_struct(p);
				if (!report) {
					list_add(&p->sched_info.ktop.list_entry[KTOP_REPORT - 1], &report_list);
				} else {
					list_add(&p->sched_info.ktop.list_entry[KTOP_REPORT - 1], &q->sched_info.ktop.list_entry[KTOP_REPORT  - 1]);
				}
				j++;
				if (j > KTOP_RP_NUM) {
					k = report_list.next;
					k = k - (KTOP_REPORT - 1);
					p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
					list_del(report_list.next);
					put_task_struct(p);
				}
			}
		}
		if (i == 0)
			i = KTOP_I - 1;
		else
			i--;
	}

	spin_unlock_irqrestore(&ktop_lock, flags);

#ifdef KTOP_DEBUG_PRINT
	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT - 1);
		r = container_of(k, struct task_struct,	sched_info.ktop.list_entry[0]);
		ktop_pr_dbg("%s() line:%d final: p=%d comm=%-16s sum=%u\n",
			    __func__, __LINE__, r->pid, r->comm,
			    r->sched_info.ktop.sum_exec[KTOP_REPORT - 1]);
	}
#endif

	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT - 1);
		r = container_of(k, struct task_struct,	sched_info.ktop.list_entry[0]);

		list_for_each_safe(n2,o2,n) {
			if (n2 == &report_list) {
				break;
			}
			k2 = n2 - (KTOP_REPORT - 1);
			r2 = container_of(k2, struct task_struct,	sched_info.ktop.list_entry[0]);
			if (r->tgid == r2->tgid) {
				r->sched_info.ktop.sum_exec[KTOP_REPORT - 1] += r2->sched_info.ktop.sum_exec[KTOP_REPORT - 1];
				list_del(n2);
				put_task_struct(r2);
			}
		}
	}
	
#ifdef KTOP_DEBUG_PRINT
	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT - 1);
		r = container_of(k, struct task_struct,	sched_info.ktop.list_entry[0]);
		ktop_pr_dbg("%s() line:%d final: p=%d comm=%-16s sum=%u\n",
			    __func__, __LINE__, r->pid, r->comm,
			    r->sched_info.ktop.sum_exec[KTOP_REPORT - 1]);
	}
#endif

	if (!list_empty(&report_list)) {
		char comm[TASK_COMM_LEN];

		list_for_each_prev_safe(l, o, &report_list) {
			k = l - (KTOP_REPORT - 1);
			p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
			if (p->group_leader != p) {
				rcu_read_lock();
				q = find_task_by_vpid(p->tgid);
				if (q)
					get_task_comm(comm, q);
				rcu_read_unlock();
			}
			sum_exec_ = (p->sched_info.ktop.sum_exec[KTOP_REPORT - 1]  *100) / (u32) ((delta*nr_cpu_ids) >> 20);
			if(sum_exec_ > ((p->tgid != gktop_config.pid_focus)?gktop_config.threshold:gktop_config.threshold<<2)) {
				sprintf(report_buf,"CPULOAD=%-9d %-16s %-11u",
									p->tgid, 
									(p->group_leader != p) ? (q ? comm : "EXITED") : p->comm,
									sum_exec_);

				send_usrmsg_cpuload(report_buf, strlen(report_buf));
			}
			list_del(l);
			put_task_struct(p);
		}
	}

	return 0;
}

static int ktop_interval_show(struct seq_file *m, void *v)
{
	printk("%d\n", gktop_config.interval);

	return 0;
}

static int ktop_threshold_show(struct seq_file *m, void *v)
{
	printk("%d\n", gktop_config.threshold);

	return 0;
}

static int ktop_pid_show(struct seq_file *m, void *v)
{
	printk("%d\n", gktop_config.pid_focus);

	return 0;
}

static ssize_t ktop_interval_write(struct file *file, const char __user * user_buf, size_t nbytes, loff_t * ppos)
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

	if (val != gktop_config.interval) {
		spin_lock_irqsave(&ktop_lock, flags);

		gktop_config.interval = val;
		mod_timer(&ktop_timer, jiffies + msecs_to_jiffies(gktop_config.interval));

		spin_unlock_irqrestore(&ktop_lock, flags);
	}

	return nbytes;
}

static ssize_t ktop_threshold_write(struct file *file, const char __user * user_buf, size_t nbytes, loff_t * ppos)
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

static ssize_t ktop_pid_write(struct file *file, const char __user * user_buf, size_t nbytes, loff_t * ppos)
{
	char buf[32];
	size_t buf_size;
	long val;
	unsigned long flags;
	struct list_head *k, *l, *m;
	struct task_struct *p;

	if (!nbytes)
		return -EINVAL;
	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size - 1] = '\0';
	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

	if (val != gktop_config.pid_focus) {
		spin_lock_irqsave(&ktop_lock, flags);

		list_for_each_safe(l, m, &ktop_list[cur_idx]) {
			k = l - cur_idx;
			p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
			if (p->pid == gktop_config.pid_focus) {
				p->sched_info.ktop.sum_exec[cur_idx] = 0;
				//printk("---del pid=%d,cur_idx=%d\n",p->pid,cur_idx);
				list_del(l);
				put_task_struct(p);
			}
		}

		gktop_config.pid_focus = val;

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

struct proc_dir_entry *ktop_proc_dir = NULL;

static struct class cpuload_event_class = {
        .name =         "cpuload_event",
        .owner =        THIS_MODULE,
};

static int __init proc_ktop_init(void)
{
	int i;
	int ret = -1;

	struct proc_dir_entry *file_interval = NULL;
	struct proc_dir_entry *file_threshold = NULL;
	struct proc_dir_entry *file_pid = NULL;

	printk("------proc ktop init.\n");

	for (i = 0; i < KTOP_I; i++) {
		INIT_LIST_HEAD(&ktop_list[i]);
	}

	ktop_proc_dir = proc_mkdir(KTOP_DIR_NAME, NULL);
	if (ktop_proc_dir == NULL) {
		printk("%s proc create %s failed\n", __func__, KTOP_DIR_NAME);
		return -EINVAL;
	}

	file_interval = proc_create(KTOP_INTERVAL_PROC_NAME, 0666, ktop_proc_dir, &ktop_interval_proc);
	if (!file_interval) {
		printk("%s proc_create failed!\n", __func__);
		goto out0;
	}

	file_threshold = proc_create(KTOP_THRESHOLD_PROC_NAME, 0666, ktop_proc_dir, &ktop_threshold_proc);
	if (!file_threshold) {
		printk("%s proc_create failed!\n", __func__);
		goto out1;
	}

	file_pid = proc_create(KTOP_PID_PROC_NAME, 0666, ktop_proc_dir, &ktop_pid_proc);
	if (!file_pid) {
		printk("%s proc_create failed!\n", __func__);
		goto out2;
	}

    ret = class_register(&cpuload_event_class);
    if( ret < 0 ){
        printk(KERN_ERR "cpuload_event: class_register fail\n");
        return ret;
    }

    cpuload_dev = device_create(&cpuload_event_class, NULL, MKDEV(0, 0), NULL, "cpuload_event");
    if( cpuload_dev == NULL){
        printk(KERN_ERR "cpuload_event:device_create fail\r\n");
		goto out3;
    }

	ktop_timer.expires = jiffies + msecs_to_jiffies(gktop_config.interval);
	add_timer(&ktop_timer);

	return 0;

out3:
	class_unregister(&cpuload_event_class);
out2:
	remove_proc_entry(KTOP_THRESHOLD_PROC_NAME, ktop_proc_dir);
out1:
	remove_proc_entry(KTOP_INTERVAL_PROC_NAME, ktop_proc_dir);
out0:
	remove_proc_entry(KTOP_DIR_NAME, NULL);

	return -ENOMEM;
}

fs_initcall(proc_ktop_init);
#if 0
static void test_netlink_exit(void)
{
	if (nlsk_cpuload) {
		netlink_kernel_release(nlsk_cpuload);	/* release .. */
		nlsk_cpuload = NULL;
	}
	printk("test_netlink_exit!\n");

	remove_proc_entry(KTOP_INTERVAL_PROC_NAME, ktop_proc_dir);
	remove_proc_entry(KTOP_THRESHOLD_PROC_NAME, ktop_proc_dir);
	remove_proc_entry(KTOP_PID_PROC_NAME, ktop_proc_dir);

	remove_proc_entry(KTOP_DIR_NAME, NULL);
}

#endif
MODULE_AUTHOR("leon.yu <leon.yu@nio.com>");
MODULE_DESCRIPTION("cpuload_event");
MODULE_LICENSE("GPL v2");
