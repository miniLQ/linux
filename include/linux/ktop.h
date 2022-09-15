#ifndef _LINUX_KTOP_H
#define _LINUX_KTOP_H

/* debug switch */
//#define KTOP_DEBUG_PRINT
//#define KTOP_MANUAL

#define KTOP_I 1
#define KTOP_REPORT 3

struct ktop_info {
	u32			sum_exec[KTOP_REPORT];
	struct list_head	list_entry[KTOP_REPORT];
};
extern void ktop_add(struct task_struct *p);
#define KTOP_RP_NUM 20

extern struct timer_list ktop_timer;


#ifdef KTOP_DEBUG_PRINT
#define ktop_pr_dbg(fmt, ...) do {pr_err("ktop: " fmt, ##__VA_ARGS__); } while (0)
#else /* KTOP_DEBUG_PRINT */
#define ktop_pr_dbg(fmt, ...) do {} while (0)
#endif /* KTOP_DEBUG_PRINT */

#endif /* _LINUX_KTOP_H */



