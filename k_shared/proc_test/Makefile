KERNEL_DIR ?= ~/work/my_linux
obj-m := proc_test.o
CFLAG := -Wall -Wno-error 
modules:
	$(MAKE) $(CFLAG) -C $(KERNEL_DIR) M=$(PWD) modules 
clean: 
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
install:         
	cp *.ko $(KERNEL_DIR)/kmodules


