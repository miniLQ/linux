#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
   static int __init module_test_init(void)
{
	        printk("---module_test_init\n");
		        return 0;
}    

  static void __exit module_test_exit(void)
{
	        printk("---module_test_exit\n");
}


module_init(module_test_init);
module_exit(module_test_exit);
MODULE_LICENSE("GPL");

