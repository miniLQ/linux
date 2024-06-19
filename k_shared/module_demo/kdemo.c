#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "mm_test"
#define MEM_SIZE (1024 * 1024) // 1MB
#define TEST_KMALLOC  1

struct page *g_page;
static void *mm_test_buffer;


static loff_t mm_test_llseek(struct file *file, loff_t offset, int whence) {
    loff_t newpos;

    switch (whence) {
        case SEEK_SET:
            newpos = offset;
            break;
        case SEEK_CUR:
            newpos = file->f_pos + offset;
            break;
        case SEEK_END:
            newpos = MEM_SIZE + offset;
            break;
        default:
            return -EINVAL;
    }

    if (newpos < 0 || newpos > MEM_SIZE)
        return -EINVAL;

    file->f_pos = newpos;
    return newpos;
}

static ssize_t mm_test_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    if (*ppos >= MEM_SIZE)
        return 0;

    if (*ppos + count > MEM_SIZE)
        count = MEM_SIZE - *ppos;

    if (copy_to_user(buf, mm_test_buffer + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static ssize_t mm_test_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    if (*ppos >= MEM_SIZE)
        return -ENOSPC;

    if (*ppos + count > MEM_SIZE)
        count = MEM_SIZE - *ppos;

    if (copy_from_user(mm_test_buffer + *ppos, buf, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static const struct file_operations mm_test_fops = {
    .owner = THIS_MODULE,
    .llseek = mm_test_llseek,
    .read = mm_test_read,
    .write = mm_test_write,
};

static struct miscdevice mm_test_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &mm_test_fops,
};

static int __init mm_test_init(void) {
    int ret;
	unsigned long phys_addr;

#ifdef TEST_KMALLOC 
//也可以直接用kmalloc分配1M,方便调试
     mm_test_buffer = kmalloc(MEM_SIZE, GFP_KERNEL);

#else
 	g_page =  alloc_pages(GFP_KERNEL, get_order(MEM_SIZE));
	if (!g_page) {
        printk(KERN_ERR "Failed to allocate memory\n");
        return -ENOMEM;
    }

	phys_addr = page_to_phys(page);
	// 映射为虚拟地址
    mm_test_buffer = (unsigned long *)ioremap(phys_addr, MEM_SIZE);

#endif
	if (!mm_test_buffer)
        return -ENOMEM;

    memset(mm_test_buffer, 0, MEM_SIZE);

    ret = misc_register(&mm_test_misc_device);
    if (ret) {
        kfree(mm_test_buffer);
        return ret;
    }

    printk(KERN_INFO "mm_test device registered.\n");
    return 0;
}

static void __exit mm_test_exit(void) {
    misc_deregister(&mm_test_misc_device);
#ifdef TEST_KMALLOC 
    kfree(mm_test_buffer);
#else
	//__free(g_page, MEM_SIZE>>12);
#endif
    printk(KERN_INFO "mm_test device unregistered.\n");
}

module_init(mm_test_init);
module_exit(mm_test_exit);
MODULE_LICENSE("GPL");
