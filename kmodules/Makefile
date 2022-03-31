KERNEL_DIR ?= /home/leon/work/myHub/linux/kernel/
obj-m := module_test.o
modules:
		$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
clean: 
		$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
install:         
		cp *.ko $(KERNEL_DIR)/kmodules


