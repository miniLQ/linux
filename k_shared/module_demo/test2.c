#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/mm_test"
#define MEMORY_SIZE (1024 * 1024) // 1MB

int main() {
    int fd;
    char read_buffer[100], write_buffer[100];
    off_t offset = 0;

    // 打开设备文件
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("failed to open the device.");
        return EXIT_FAILURE;
    }

    // 设置偏移量并写入数据
    lseek(fd, 200, SEEK_SET);
    sprintf(write_buffer, "xxxxxxxxxHello, mm_test!aaaaaaaaaa");
    if (write(fd, write_buffer, sizeof(write_buffer)) != sizeof(write_buffer)) {
        perror("write operation failed.");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("written successfully.\n");

    // 随机访问内存并读取数据
    lseek(fd, 210, SEEK_SET);
    if (read(fd, read_buffer, sizeof(read_buffer)) != sizeof(read_buffer)) {
        perror("read failed.");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("read data: %s\n", read_buffer);

    // 关闭设备文件
    close(fd);

    return EXIT_SUCCESS;
}

