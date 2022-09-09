
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>



#define NETLINK_CPULOAD 30 
#define MSG_LEN          256

#define MAX_PLOAD        125

typedef struct _user_msg_info
{
    struct nlmsghdr hdr;
    char  msg[MSG_LEN];
} user_msg_info;
static int fd = -1;

/* Returns 0 on failure, 1 on success */
int main()
{
    struct sockaddr_nl addr;
    int sz = 64*1024;
    int s;
	char buffer[1024];

    user_msg_info u_info;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    s = socket(PF_NETLINK, SOCK_DGRAM, 30);
    if(s < 0)
        return 0;

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));
	
	if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return 0;
    }
	printf("---test  at %d\n",__LINE__);

    fd = s;
    while (1) {
        struct pollfd fds;
        int nr;
		int buffer_length=1024;

	printf("---test  at %d\n",__LINE__);
        fds.fd = fd;
        fds.events = POLLIN;
        fds.revents = 0;
        nr = poll(&fds, 1, -1);

        if(nr > 0 && (fds.revents & POLLIN)) {
			int count = recv(fd, &u_info, sizeof(user_msg_info), 0);
//            int count = recv(fd, buffer, buffer_length, 0);
            if (count > 0) {
	//			printf("rev:%s\n",buffer);
    		printf("%s\n", u_info.msg);
            //    return count;
            }
        }
    }

    // won't get here
    return 0;
}


