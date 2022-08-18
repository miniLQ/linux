#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init proc_test_init(void)
{
        printk("---proc_test_init\n");
        return 0;
}    
 
static void __exit proc_test_exit(void)
{
        printk("---proc_test_exit\n");
}
 
 
module_init(proc_test_init);
module_exit(proc_test_exit);
MODULE_LICENSE("GPL");


