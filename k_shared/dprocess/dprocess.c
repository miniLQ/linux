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

static int w_count=0;
static int dprocess_open(struct inode *inode, struct file *filp)
{
	w_count++;
	if (w_count >= 3) {
		w_count = 0;
		__set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}

	printk("---open dprocess.\n");

	return 0;
}

static int dprocess_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t dprocess_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	printk("---write.\n");
	
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
