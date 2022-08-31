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

#define NETLINK_CPULOAD  30 
#define MSG_LEN          256
#define USER_PORT        100

#define KTOP_DEBUG_PRINT
//#define KTOP_MANUAL

#define KTOP_INTERVAL (1*MSEC_PER_SEC)

static int ktop_interval_s= MSEC_PER_SEC*1;

typedef struct _cpuload_cfg
{
	int interval;
	int pid_focus;
	char reserve[8];
} cpuload_cfg;

static u8 cur_idx;
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
int send_usrmsg_cpuload(char *pbuf, uint16_t len)
{
    struct sk_buff *nl_skb;
    struct nlmsghdr *nlh;

    int ret;

    /* 创建sk_buff 空间 */
    nl_skb = nlmsg_new(len, GFP_ATOMIC);
    if(!nl_skb)
    {
        printk("netlink alloc failure\n");
        return -1;
    }

    /* 设置netlink消息头部 */
    nlh = nlmsg_put(nl_skb, 0, 0, NETLINK_CPULOAD, len, 0);
    if(nlh == NULL)
    {
        printk("nlmsg_put failaure \n");
        nlmsg_free(nl_skb);
        return -1;
    }

    /* 拷贝数据发送 */
    memcpy(nlmsg_data(nlh), pbuf, len);
    ret = netlink_unicast(nlsk_cpuload, nl_skb, USER_PORT, MSG_DONTWAIT);

    //nlmsg_free(nl_skb);

    return ret;
}

void __always_inline ktop_add(struct task_struct *p)
{
	unsigned long flags;
	if (!p->sched_info.ktop.sum_exec[cur_idx]) {
		if(spin_trylock_irqsave(&ktop_lock, flags)) {
			/* TODO this could change to single ktop_n */
			if (ktop_n < KTOP_MAX) {
				ktop_n++;
				p->sched_info.ktop.sum_exec[cur_idx] =
					max(1U, (u32)(p->se.sum_exec_runtime >> 20));
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

static void ktop_timer_func(struct timer_list * timer)
{
	struct task_struct *p;
	struct list_head *k, *l, *m;

#ifdef KTOP_DEBUG_PRINT
	ktop_pr_dbg("ktop_n=%d", ktop_n);
	ktop_pr_dbg("ktop_stat__add=%d", ktop_stat__add);
	ktop_stat__add = 0;
#endif
	if(spin_trylock(&ktop_lock)) {
		cur_idx++;
		if (cur_idx >= KTOP_I)
			cur_idx = 0;
		ktop_n = 0;
		list_for_each_safe(l, m, &ktop_list[cur_idx]) {
			k = l - cur_idx;
			p = container_of(k, struct task_struct, sched_info.ktop.list_entry[0]);
			p->sched_info.ktop.sum_exec[cur_idx] = 0;
			list_del(l);
			put_task_struct(p);
		}
		ktop_time[cur_idx] = ktime_get_boot_fast_ns();
		spin_unlock(&ktop_lock);
		mod_timer(timer, jiffies + msecs_to_jiffies(MSEC_PER_SEC*ktop_interval_s));
	} else
		mod_timer(timer, jiffies + msecs_to_jiffies(MSEC_PER_SEC*ktop_interval_s*2));


	ktop_report();
}
DEFINE_TIMER(ktop_timer, ktop_timer_func);

static int count=0;
static int ktop_show(struct seq_file *m, void *v)
{
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

	seq_printf(m, "count=%d,cur_idx=%d\n",count++,cur_idx);

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

	return 0;
}
static int ktop_report(void)
{
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

	//printk("count=%d,cur_idx=%d\n",count++,cur_idx);

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
		//seq_printf(m, "duration:%d total_tasks:%u\n", (u32)(delta >> 20), run_tasks);
		//seq_printf(m, "%-9s %-16s %-11s %-9s %-16s\n", "TID", "COMM", "SUM", "PID", "PROCESS-COMM");
		sprintf(report_buf, "duration:%d total_tasks:%u\n", (u32)(delta >> 20), run_tasks);
		send_usrmsg_cpuload(report_buf,strlen(report_buf));

		sprintf(report_buf,  "%-9s %-16s %-11s %-9s %-16s\n", "TID", "COMM", "SUM", "PID", "PROCESS-COMM");
		send_usrmsg_cpuload(report_buf,strlen(report_buf));

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
		/*	seq_printf(m, "%-9d %-16s %-11u %-9d %-16s\n",
				   p->pid, p->comm, p->sched_info.ktop.sum_exec[KTOP_REPORT-1],
				   p->tgid, (p->group_leader != p) ? (q ? comm : "EXITED") : p->comm);
		*/
		u32 sum_exec_ =  (p->sched_info.ktop.sum_exec[KTOP_REPORT-1]*100)/(u32)(delta>>20);
		if (sum_exec_ > 30) {
			sprintf(report_buf, "%-9d %-16s %-11u %-9d %-16s\n",
				   p->pid, p->comm, sum_exec_,
				   p->tgid, (p->group_leader != p) ? (q ? comm : "EXITED") : p->comm);
	
			send_usrmsg_cpuload(report_buf,strlen(report_buf));
		}
			list_del(l);
			put_task_struct(p);
		}
	}

	return 0;
}


static ssize_t ktop_write(struct file *file, const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	long val;

	if (!nbytes)
		return -EINVAL;
	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size - 1] = '\0';
	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

	 printk("---write buf:%s",buf);


#ifdef KTOP_MANUAL
	if (val == 1) {
		ktop_timer.expires = jiffies + msecs_to_jiffies(KTOP_INTERVAL);
		add_timer(&ktop_timer);
	} else if (val == 2) {
		del_timer(&ktop_timer);
	}
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


static void netlink_rcv_msg_cpuload(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = NULL;
    char *umsg = NULL;
    //char *kmsg = "hello users!!!";
	 cpuload_cfg *puser_data=NULL;
    if(skb->len >= nlmsg_total_size(0))
    {
        nlh = nlmsg_hdr(skb);
        umsg = NLMSG_DATA(nlh);
        if(umsg)
        {
            printk("---kernel recv from user: %s\n", umsg);
			puser_data = (cpuload_cfg *)umsg;
			printk("set interval=%d,pid_front=%d\n",puser_data->interval,puser_data->pid_focus);

			ktop_interval_s = puser_data->interval;

			mod_timer(&ktop_timer, jiffies + msecs_to_jiffies(MSEC_PER_SEC*ktop_interval_s));

            //send_usrmsg_cpuload(kmsg, strlen(kmsg));
			//sprintf(kmsg1,"send count:%d",count++);
            //send_usrmsg(kmsg1, strlen(kmsg1));
        }
    }
}



struct netlink_kernel_cfg cfg_cpuload = { 
        .input  = netlink_rcv_msg_cpuload, /* set recv callback */
};  

static int __init proc_ktop_init(void)
{
	int i;
    printk("0------test_netlink_init\n");
	for(i = 0 ; i < KTOP_I ; i++) {
		INIT_LIST_HEAD(&ktop_list[i]);
	}

#ifndef KTOP_MANUAL
	ktop_timer.expires = jiffies + msecs_to_jiffies(MSEC_PER_SEC*ktop_interval_s);
	add_timer(&ktop_timer);
#endif

	proc_create("ktop", 0666, NULL, &ktop_proc);
#if 1
    /* create netlink socket */
    nlsk_cpuload = (struct sock *)netlink_kernel_create(&init_net, NETLINK_CPULOAD, &cfg_cpuload);
    if(nlsk_cpuload == NULL)
    {   
        printk("netlink_kernel_create error !\n");
        return -1; 
    }   
    printk("------test_netlink_init\n");

#endif
return 0;
}

fs_initcall(proc_ktop_init);
#if 0
void test_netlink_exit(void)
{
    if (nlsk_cpuload){
        netlink_kernel_release(nlsk_cpuload); /* release ..*/
        nlsk_cpuload = NULL;
    }   
    printk("test_netlink_exit!\n");
}

#endif
