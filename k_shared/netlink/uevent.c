
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>



#define MSG_LEN          256

#define MAX_PLOAD        125

static int fd = -1;

/* Returns 0 on failure, 1 on success */
int main()
{
    struct sockaddr_nl addr;
    int sz = 64*1024;
    int s;
	char buffer[1024];

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

	
    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(s < 0)
        return 0;

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));
	
	if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return 0;
    }
	printf("===test  at %d\n",__LINE__);

    fd = s;
    while (1) {
        struct pollfd fds;
        int nr;

        fds.fd = fd;
        fds.events = POLLIN;
        fds.revents = 0;
        nr = poll(&fds, 1, -1);

        if(nr > 0 && (fds.revents & POLLIN)) {
			int count = recv(fd, &buffer, 4096, 0);
			printf("===test  at %d,recv:%d\n",__LINE__,count);
			char *pbuf = buffer;
            if (count > 0) {
            	pbuf[count] = 0;
				while(*pbuf) {
					printf("%s\n",pbuf);
					while(*pbuf++);
				}
            }
        }
    }

    // won't get here
    return 0;
}


