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


#include "kdemo.h"

static int kdemo_open(struct inode *inode, struct file *filp)
{
	printk("---open kdemo.\n");

	return 0;
}

static int kdemo_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t kdemo_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	printk("---write.\n");
	
	return 0;
}

static ssize_t kdemo_read(struct file *file, char __user *ubuf, size_t count, loff_t *offs)
{
	printk("---read.\n");

	return 0;
}

long kdemo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}
static loff_t kdemo_llseek(struct file *file, loff_t offset, int whence)
{
	return 0;
}

#ifdef FASYNC 
static int kdemo_fasync (int fd, struct file *filp, int on)
{
	printk("driver: fasync\n");
	return 0;
}
#endif

static int kdemo_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0 ;
}
static struct file_operations kdemo_fops = {
	.owner			= THIS_MODULE,
	.open			= kdemo_open,
	.release		= kdemo_close,
	.read 			= kdemo_read,
	.write 			= kdemo_write,
	.llseek  		= kdemo_llseek,
#ifdef FASYNC
	.fasync			= kdemo_fasync,
#endif
	.unlocked_ioctl	= kdemo_ioctl,
	.mmap		= kdemo_mmap,
};

static struct miscdevice kdemo_miscdev = {
	.minor  = MISC_DYNAMIC_MINOR,
	.name   = "kdemo",
	.fops   = &kdemo_fops,
};

static int __init kdemo_init(void)
{
    printk("---kdemo_init\n");
	if (misc_register(&kdemo_miscdev) < 0) {
		printk("misc device register failed!\n");
		return -EFAULT;
	}

	return 0;
}
 
static void __exit kdemo_exit(void)
{
    printk("---kdemo_exit\n");
	misc_deregister(&kdemo_miscdev);
}
 
 
module_init(kdemo_init);
module_exit(kdemo_exit);
MODULE_LICENSE("GPL");
