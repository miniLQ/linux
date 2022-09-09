#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>

#include <linux/proc_fs.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/cpumask.h>
#include "../../../kernel/sched/sched.h"
#include "ktop.h"

#define KTOP_INTERVAL (MSEC_PER_SEC)
//static u8 cur_idx;
struct list_head ktop_list[KTOP_I];
u64 ktop_time[KTOP_I];
#define KTOP_MAX 1000
//TODO reset this ktop_n
u32 ktop_n = KTOP_MAX;
#ifdef KTOP_DEBUG_PRINT
u32 ktop_stat__add;
#endif

DEFINE_SPINLOCK(ktop_lock);

static int count=0;


struct data{
	int val;
	int count;
	struct list_head node; 
};
struct list_head ptest;

void link_test(struct list_head * link)
{
	struct list_head *l,*o, *k;
	struct list_head *l2,*o2, *k2;
	struct list_head *l3,*o3, *k3;
	struct data *p,*q;
	struct data *p2,*q2;
	struct data *p3,*q3;
	int i=0;

	for (i=0; i < 10; i++)
	{
		struct data *ptmp = (struct data*)kmalloc(sizeof(struct data), GFP_KERNEL);
		ptmp->val = (i+1)%3;
		ptmp->count = 1;

		list_add(&ptmp->node,link);
	}
		printk("sizeof(list_head)=%d\n",sizeof(struct list_head));
	list_for_each(l, link) {
		printk("l=0x%x,link=0x%x\n",l,link);
		p = container_of(l, struct data, node);
	//	printk("---l0=0x%x p->val=%d,p->count=%d\n",l, p->val,p->count);
		list_for_each_safe(l2,o2,l) {
			if (l2 == link) {
				break;
			}
			p2= container_of(l2, struct data, node);
	//		printk("---l2=0x%x,p2->val=%d,node=0x%x\n",l2,p2->val,p2);
			if (p->val == p2->val) {
				p->count += p2->count;
				list_del(l2);
			}
		}
#if 0
		printk("==============================0===============================\n");
		list_for_each_safe(l3, o3, link) {
			p3 = container_of(l3, struct data, node);
			printk("---------after filter,l3=0x%x p3->val=%d,p3->count=%d, p3=0x%x\n",l3, p3->val,p3->count,p3);
		}
		printk("==============================1===============================\n");
#endif
//		printk("---l1 p->val=%d,p->count=%d\n",p->val,p->count);
	}
#if 1
	list_for_each_safe(l, o, link) {
		p = container_of(l, struct data, node);
		printk("---------at last after filter, p->val=%d,p->count=%d\n",p->val,p->count);
	}
#endif
}
void link_free(struct list_head *link)
{
#if 0
	struct list_head *l,*o;
	struct data *p;

	list_for_each_safe(l, o, link) {
		p = container_of(l, struct data, link);
		list_del(l);
		kfree(p);
	}

#endif	
}
static int ktop_show(struct seq_file *m, void *v)
{
		seq_printf(m, "count=%d\n",count++);
		link_test(&ptest);
//		link_free(&ptest);
#if 0
	struct task_struct *p, *r, *q;
	struct list_head report_list;
	struct list_head *k, *l, *n, *o;
	bool report;
	int h, i, j = 0, start_idx;
	u32 run_tasks = 0;
	u64 now;
	u64 delta;
	unsigned long flags;
	INIT_LIST_HEAD(&report_list);

	spin_lock_irqsave(&ktop_lock, flags);

	start_idx = cur_idx + 1;
	if (start_idx == KTOP_I)
		start_idx =  0;

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
			run_tasks ++;
			sum = (u32)(p->se.sum_exec_runtime >> 20);
			p->sched_info.ktop.sum_exec[KTOP_REPORT-1] =
				sum > p->sched_info.ktop.sum_exec[i] ?
				(sum - p->sched_info.ktop.sum_exec[i]) : 0;
			ktop_pr_dbg("%s() line:%d start: p=%d comm=%s sum=%u\n", __func__, __LINE__,
				    p->pid, p->comm, p->sched_info.ktop.sum_exec[KTOP_REPORT-1]);
			list_for_each(n, &report_list) {
				k = n - (KTOP_REPORT-1);
				r = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
				if (p->sched_info.ktop.sum_exec[KTOP_REPORT-1] >
				    r->sched_info.ktop.sum_exec[KTOP_REPORT-1]) {
					report = true;
					q = r;
				}
				else
					break;
			}
			if (report || j < KTOP_RP_NUM) {
				get_task_struct(p);
				if(!report)
					list_add(&p->sched_info.ktop.list_entry[KTOP_REPORT-1], &report_list);
				else
					list_add(&p->sched_info.ktop.list_entry[KTOP_REPORT-1],
						 &q->sched_info.ktop.list_entry[KTOP_REPORT-1]);
				j++;
				if (j > KTOP_RP_NUM) {
					k = report_list.next;
					k = k - (KTOP_REPORT-1);
					p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
					list_del(report_list.next);
					put_task_struct(p);
				}
			}
		}
		if (i == 0)
			i =  KTOP_I -1;
		else
			i--;
	}

	spin_unlock_irqrestore(&ktop_lock, flags);

#ifdef KTOP_DEBUG_PRINT
	list_for_each(n, &report_list) {
		k = n - (KTOP_REPORT-1);
		r = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
		ktop_pr_dbg("%s() line:%d final: p=%d comm=%-16s sum=%u\n",
			    __func__, __LINE__, r->pid, r->comm,
			    r->sched_info.ktop.sum_exec[KTOP_REPORT-1]);
	}
#endif

	if(!list_empty(&report_list)) {
		char comm[TASK_COMM_LEN];
		seq_printf(m, "duration:%d total_tasks:%u\n", (u32)(delta >> 20), run_tasks);
		seq_printf(m, "%-9s %-16s %-11s %-9s %-16s\n", "TID", "COMM", "SUM", "PID", "PROCESS-COMM");
		list_for_each_prev_safe(l, o, &report_list) {
			k = l - (KTOP_REPORT-1);
			p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
			if (p->group_leader != p) {
				rcu_read_lock();
				q = find_task_by_vpid(p->tgid);
				if (q)
					get_task_comm(comm, q);
				rcu_read_unlock();
			}
			seq_printf(m, "%-9d %-16s %-11u %-9d %-16s\n",
				   p->pid, p->comm, p->sched_info.ktop.sum_exec[KTOP_REPORT-1],
				   p->tgid, (p->group_leader != p) ? (q ? comm : "EXITED") : p->comm);
			list_del(l);
			put_task_struct(p);
		}
	}

#endif
	return 0;
}
static int write_count=0;
static ssize_t ktop_write(struct file *file, const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	long val;
	memset(buf,0x0,sizeof(buf)/sizeof(char));
	printk("---write times:%d\n",write_count++);
	if (!nbytes)
		return -EINVAL;
	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size - 1] = '\0';
	printk("---write buf:%s",buf);

	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

#if 0
#ifdef KTOP_MANUAL
	if (val == 1) {
		ktop_timer.expires = jiffies + msecs_to_jiffies(KTOP_INTERVAL);
		add_timer(&ktop_timer);
	} else if (val == 2) {
		del_timer(&ktop_timer);
	}
#endif
#endif
	return nbytes;
}

static int ktop_open(struct inode *inode, struct file *file)
{
	return single_open(file, ktop_show, NULL);
}

static const struct proc_ops ktop_proc = {
	.proc_open           = ktop_open,
	.proc_read           = seq_read,
	.proc_lseek         = seq_lseek,
	.proc_write          = ktop_write,
	.proc_release        = single_release,
};

static int __init proc_test_init(void)
{
        printk("---proc_test_init\n");
	INIT_LIST_HEAD(&ptest);

#if 0
	int i;
	for(i = 0 ; i < KTOP_I ; i++) {
		INIT_LIST_HEAD(&ktop_list[i]);
	}
#endif
#ifndef KTOP_MANUAL

	//ktop_timer.expires = jiffies + msecs_to_jiffies(KTOP_INTERVAL);
	//add_timer(&ktop_timer);
#endif

	proc_create("ktop", 0666, NULL, &ktop_proc);

	return 0;
}
 
static void __exit proc_test_exit(void)
{
        printk("---proc_test_exit\n");

	remove_proc_entry("ktop", NULL);
}
 
 
module_init(proc_test_init);
module_exit(proc_test_exit);
MODULE_LICENSE("GPL");


