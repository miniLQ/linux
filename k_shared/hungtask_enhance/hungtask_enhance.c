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


#include "hungtask_enhance.h"

static int hungtask_enhance_open(struct inode *inode, struct file *filp)
{
	printk("---open hungtask_enhance.\n");

	return 0;
}

static int hungtask_enhance_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t hungtask_enhance_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	printk("---write.\n");
	
	return 0;
}

static ssize_t hungtask_enhance_read(struct file *file, char __user *ubuf, size_t count, loff_t *offs)
{
	printk("---read.\n");

	return 0;
}

long hungtask_enhance_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}
static loff_t hungtask_enhance_llseek(struct file *file, loff_t offset, int whence)
{
	return 0;
}

#ifdef FASYNC 
static int hungtask_enhance_fasync (int fd, struct file *filp, int on)
{
	printk("driver: fasync\n");
	return 0;
}
#endif

static int hungtask_enhance_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0 ;
}
static struct file_operations hungtask_enhance_fops = {
	.owner			= THIS_MODULE,
	.open			= hungtask_enhance_open,
	.release		= hungtask_enhance_close,
	.read 			= hungtask_enhance_read,
	.write 			= hungtask_enhance_write,
	.llseek  		= hungtask_enhance_llseek,
#ifdef FASYNC
	.fasync			= hungtask_enhance_fasync,
#endif
	.unlocked_ioctl	= hungtask_enhance_ioctl,
	.mmap		= hungtask_enhance_mmap,
};

static struct miscdevice hungtask_enhance_miscdev = {
	.minor  = MISC_DYNAMIC_MINOR,
	.name   = "hungtask_enhance",
	.fops   = &hungtask_enhance_fops,
};

static int __init hungtask_enhance_init(void)
{
    printk("---hungtask_enhance_init\n");
	if (misc_register(&hungtask_enhance_miscdev) < 0) {
		printk("misc device register failed!\n");
		return -EFAULT;
	}

	return 0;
}
 
static void __exit hungtask_enhance_exit(void)
{
    printk("---hungtask_enhance_exit\n");
	misc_deregister(&hungtask_enhance_miscdev);
}
 
 
module_init(hungtask_enhance_init);
module_exit(hungtask_enhance_exit);
MODULE_LICENSE("GPL");
