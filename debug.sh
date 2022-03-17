qemu-system-aarch64 -machine virt -cpu cortex-a57 -machine type=virt -m 1024 -smp 1 -kernel arch/arm64/boot/Image --append "rdinit=/linuxrc root=/dev/vda rw console=ttyAMA0 loglevel=8"  -nographic --fsdev local,id=kmod_dev,path=$PWD/k_shared,security_model=none -device virtio-9p-device,fsdev=kmod_dev,mount_tag=kmod_mount -S -s

