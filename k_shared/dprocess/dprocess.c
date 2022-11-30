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
#include <linux/miscdevice.h>


#include "dprocess.h"
static struct task_struct *pblock_thread = NULL; 
static int w_count=0;
static char *pbuf;
static int dprocess_open(struct inode *inode, struct file *filp)
{
	w_count++;
	if (w_count >= 3) {
		w_count = 0;
	//	__set_current_state(TASK_UNINTERRUPTIBLE);
		//__set_current_state(__TASK_STOPPED);
	//	schedule();
	}

	printk("---open dprocess.\n");
	pbuf = vmalloc(1024*100);
	if (pbuf) {
		memset(pbuf,0xaa,1024*100);
	}

	return 0;
}

static int dprocess_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t dprocess_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[128];
	memset(tmp, 0, sizeof(tmp));

	if ((count < 2) || (count > sizeof(tmp) - 1)) {
		pr_err("hung_task: string too long or too short.\n");
		return -EINVAL;
	}
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;

	if (tmp[count - 1] == '\n') {
		tmp[count - 1] = '\0';
	}

	pr_err("d state:tmp=%s, count %d\n", tmp, (int)count);

	struct task_struct *cur = get_current();
	printk("---cur pid=%d,comm=%s\n", cur->pid, cur->comm);

	if (strncmp(tmp, "test_d_block", strlen("test_d_block")) == 0) {
		if (pblock_thread == NULL) {
			pblock_thread = get_current();
			printk("---set d state pid:%d,comm:%s\n", pblock_thread->pid,pblock_thread->comm);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule();
			printk("---set d state , after schedule\n");
			pblock_thread = NULL;
		}
	} else if (strncmp(tmp, "test_d_unblock", strlen("test_d_unblock")) == 0) {
		if (pblock_thread != NULL) {
			printk("call wake_up_process pid:%d\n", pblock_thread->pid);
			wake_up_process(pblock_thread);
		}
	} else
		pr_err("hung_task: only accept on or off !\n");

	return 0;
}

static ssize_t dprocess_read(struct file *file, char __user *ubuf, size_t count, loff_t *offs)
{
	printk("---read.\n");

	return 0;
}

long dprocess_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}
static loff_t dprocess_llseek(struct file *file, loff_t offset, int whence)
{
	return 0;
}

#ifdef FASYNC 
static int dprocess_fasync (int fd, struct file *filp, int on)
{
	printk("driver: fasync\n");
	return 0;
}
#endif

static int dprocess_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0 ;
}
static struct file_operations dprocess_fops = {
	.owner			= THIS_MODULE,
	.open			= dprocess_open,
	.release		= dprocess_close,
	.read 			= dprocess_read,
	.write 			= dprocess_write,
	.llseek  		= dprocess_llseek,
#ifdef FASYNC
	.fasync			= dprocess_fasync,
#endif
	.unlocked_ioctl	= dprocess_ioctl,
	.mmap		= dprocess_mmap,
};

static struct miscdevice dprocess_miscdev = {
	.minor  = MISC_DYNAMIC_MINOR,
	.name   = "dprocess",
	.fops   = &dprocess_fops,
	.mode = 0666,//S_IFREG|S_IRWXUGO,//S_IALLUGO, 
};

static int __init dprocess_init(void)
{
    printk("---dprocess_init\n");
	if (misc_register(&dprocess_miscdev) < 0) {
		printk("misc device register failed!\n");
		return -EFAULT;
	}

	return 0;
}
 
static void __exit dprocess_exit(void)
{
    printk("---dprocess_exit\n");
	misc_deregister(&dprocess_miscdev);
}
 
 
module_init(dprocess_init);
module_exit(dprocess_exit);
MODULE_LICENSE("GPL");
