环境:
处理器:arm64
内核：linux-stable
ubuntu版本：20.04

1.安装编译工具链
@sudo apt-get install gcc-aarch64-linux-gnu
查看版本号：
leon@leon:~/work/myHub/linux$ aarch64-linux-gnu-gcc -v
Using built-in specs.
COLLECT_GCC=aarch64-linux-gnu-gcc
COLLECT_LTO_WRAPPER=/usr/lib/gcc-cross/aarch64-linux-gnu/9/lto-wrapper
Target: aarch64-linux-gnu
Configured with: ../src/configure -v --with-pkgversion='Ubuntu 9.3.0-10ubuntu1' --with-bugurl=file:///usr/share/doc/gcc-9/README.Bugs --enable-languages=c,ada,c++,go,d,fortran,objc,obj-c++,gm2 --prefix=/usr --with-gcc-major-version-only --program-suffix=-9 --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --libdir=/usr/lib --enable-nls --with-sysroot=/ --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --with-default-libstdcxx-abi=new --enable-gnu-unique-object --disable-libquadmath --disable-libquadmath-support --enable-plugin --enable-default-pie --with-system-zlib --without-target-system-zlib --enable-libpth-m2 --enable-multiarch --enable-fix-cortex-a53-843419 --disable-werror --enable-checking=release --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=aarch64-linux-gnu --program-prefix=aarch64-linux-gnu- --includedir=/usr/aarch64-linux-gnu/include
Thread model: posix
gcc version 9.3.0 (Ubuntu 9.3.0-10ubuntu1) 

可知安装完成；

2.安装qemu
$ sudo apt-get install qemu-system-arm
查看版本号：
leon@leon:~/work/myHub/linux$ qemu-system-aarch64 --version
QEMU emulator version 4.2.1 (Debian 1:4.2-3ubuntu6.16)
Copyright (c) 2003-2019 Fabrice Bellard and the QEMU Project developers
leon@leon:~/work/myHub/linux$ 

3.制作根文件系统rootfs
制作一个最小根文件系统，支持运行动态编译的应用程序，具体步骤如下：
(1)下载busybox源码
http://busybox.net/downloads/
下载最新即可，
tar jxvf busybox-1.33.1.tar.bz2 

(2)配置
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make menuconfig
Settings --->
 [*] Build static binary (no shared libs)   
 
(3)编译
make
make install

PS：编译过程报错的话，把相应项去掉即可；
编译完成，在busybox生成_install目录

(4)完善系统目录结构
1.$ ls
bin  linuxrc  sbin  usr

我们主要需要更新etc、dev和lib目录

cd etc/
add profile file

#!/bin/sh
export HOSTNAME=myQEMU
export USER=root
export HOME=/home
export PS1="[$USER@$HOSTNAME \W]\# "
PATH=/bin:/sbin:/usr/bin:/usr/sbin
LD_LIBRARY_PATH=/lib:/usr/lib:$LD_LIBRARY_PATH
export PATH LD_LIBRARY_PATH

add inittab:
::sysinit:/etc/init.d/rcS
::respawn:-/bin/sh
::askfirst:-/bin/sh
::ctrlaltdel:/bin/umount -a -r

add fstab:
#device  mount-point    type     options   dump   fsck order
proc /proc proc defaults 0 0
tmpfs /tmp tmpfs defaults 0 0
sysfs /sys sysfs defaults 0 0
tmpfs /dev tmpfs defaults 0 0
debugfs /sys/kernel/debug debugfs defaults 0 0
kmod_mount /mnt 9p trans=virtio 0 0

指定挂载的文件系统；

mkdir init.d
cd init.d
add rcS文件：
mkdir -p /sys
mkdir -p /tmp
mkdir -p /proc
mkdir -p /mnt
/bin/mount -a
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
echo /sbin/mdev > /proc/sys/kernel/hotplug
mdev -s

cd dev
sudo mknod console c 5 1
sudo mknod null c 1 3

拷贝lib库，支持动态编译的应用程序运行；
cd lib
cp /usr/aarch64-linux-gnu/lib/*.so* -a .

aarch64-linux-gnu-strip *

内核编译：
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

sudo cp ../busybox-1.33.1/_install/  . -a
cp arch/arm/configs/vexpress_defconfig  .config

make menuconfig
xxxjpg
CONFIG_UEVENT_HELPER=y
CONFIG_UEVENT_HELPER_PATH="/sbin/hotplug"
CONFIG_INITRAMFS_SOURCE="_install_arm64"

make all -j8

创建共享目录
在内核源码目录，创建共享目录：
mkdir kmodules

qemu-system-aarch64 -machine virt -cpu cortex-a57 -machine type=virt -m 1024 -smp 4 -kernel arch/arm64/boot/Image --append "rdinit=/linuxrc root=/dev/vda rw console=ttyAMA0 loglevel=8"  -nographic --fsdev local,id=kmod_dev,path=$PWD/kmodules,security_model=none -device virtio-9p-device,fsdev=kmod_dev,mount_tag=kmod_mount


使用模拟磁盘
上述initramfs的方式，将根文件系统打包到内核源码，运行时都是在内存中，可以操作，但系统重启就会丢失，下面用模拟磁盘方式挂载根文件系统
制作磁盘文件
dd if=/dev/zero of=rootfs_ext4.img bs=1M count=1024
mkfs.ext4 rootfs_ext4.img
mkdir -p tmpfs
sudo mount  -t ext4 rootfs_ext4.img  tmpfs/ -o loop
sudo cp -af _install_arm64/* tmpfs/
sudo umount  tmpfs
chmod 777 rootfs_ext4.img

rootfs_ext4.img就是即将用来挂载的磁盘；

qemu-system-aarch64 -machine virt -cpu cortex-a57 -machine type=virt -m 1024 -smp 4 -kernel arch/arm64/boot/Image --append "noinitrd root=/dev/vda rw console=ttyAMA0 loglevel=8"  -nographic -drive if=none,file=rootfs_ext4.img,id=hd0 -device virtio-blk-device,drive=hd0 --fsdev local,id=kmod_dev,path=$PWD/kmodules,security_model=none -device virtio-9p-device,fsdev=kmod_dev,mount_tag=kmod_mount

[root@myQEMU ]# df -mh
Filesystem                Size      Used Available Use% Mounted on
/dev/root               975.9M     12.8M    895.9M   1% /
tmpfs                   496.1M         0    496.1M   0% /tmp
tmpfs                   496.1M         0    496.1M   0% /dev
kmod_mount              981.8G     28.8G    903.1G   3% /mnt


共享文件：
前面已经支持了主机和qemu上的系统共享目录，这个目录就是kmodules目录：
通过mount可以查看被挂载到了qemu上的系统的/mnt目录下
[root@myQEMU ]# mount
/dev/root on / type ext4 (rw,relatime)
proc on /proc type proc (rw,relatime)
tmpfs on /tmp type tmpfs (rw,relatime)
sysfs on /sys type sysfs (rw,relatime)
tmpfs on /dev type tmpfs (rw,relatime)
kmod_mount on /mnt type 9p (rw,sync,dirsync,relatime,access=client,trans=virtio)
devpts on /dev/pts type devpts (rw,relatime,mode=600,ptmxmode=000)

在ubuntu的kmodules目录，创建一个文件hello.c
#include <stdio.h>

int main(int argc, char **argv)
{
        printf("Hello World,Qemu!\n");

        return 0;
}
在qemu查看，
[root@myQEMU mnt]# cat hello.c 
#include <stdio.h>

int main(int argc, char **argv)
{
        printf("Hello World,Qemu!\n");

        return 0;
}

在ubuntu编译，aarch64-linux-gnu-gcc  hello.c
在qemu运行，
[root@myQEMU mnt]# ./a.out 
Hello World,Qemu!
[root@myQEMU mnt]# 
可见共享目录，可以执行含动态库的应用程序；

内核模块测试：
Makefile文件
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

KERNEL_DIR ?=  /home/leon/work/myHub/linux/
obj-m := module_test.o

modules:
         $(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
         $(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
         cp *.ko $(KERNEL_DIR)/kmodules
		 
驱动程序module_test.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
   static int __init module_test_init(void)
{
        printk("module_test_init\n");
        return 0;
}    
  
  static void __exit module_test_exit(void)
{
        printk("module_test_exit\n");
}
  
  
module_init(module_test_init);
module_exit(module_test_exit);
MODULE_LICENSE("GPL");

在ubuntu端交叉编译
leon@leon:~/work/myHub/linux/kmodules/module_test$ make modules 
make -C /home/leon/work/myHub/linux/ M=/home/leon/work/myHub/linux/kmodules/module_test modules
make[1]: Entering directory '/home/leon/work/myHub/linux'
  CC [M]  /home/leon/work/myHub/linux/kmodules/module_test/module_test.o
  MODPOST /home/leon/work/myHub/linux/kmodules/module_test/Module.symvers
  LD [M]  /home/leon/work/myHub/linux/kmodules/module_test/module_test.ko
make[1]: Leaving directory '/home/leon/work/myHub/linux'
leon@leon:~/work/myHub/linux/kmodules/module_test$ 


在qemu加载驱动
[root@myQEMU module_test]# insmod module_test.ko 
module_test: loading out-of-tree module taints kernel.
---module_test init...
[root@myQEMU module_test]# rmmod module_test.ko 
---module_test exit...
[root@myQEMU module_test]# 
