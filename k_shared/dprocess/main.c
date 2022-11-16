#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("---input error.\n");
		exit(1);
	}

    int fd = open(argv[1],O_RDWR);
    if( -1 == fd ) {
        perror("Error:"),exit(-1);
	}

	write(fd, argv[2], strlen(argv[2]));

	close(fd);

	return 0;
}
