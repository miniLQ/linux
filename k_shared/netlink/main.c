#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#define NETLINK_CPULOAD 30 
#define MSG_LEN          256
#define USER_PORT        100

#define MAX_PLOAD        125

typedef struct _cpuload_cfg
{
	int interval;
	int threshold;
	int pid_focus;
	char reserve[20];
}cpuload_cfg;

typedef struct _user_msg_info
{
    struct nlmsghdr hdr;
    char  msg[MSG_LEN];
} user_msg_info;

int main(int argc, char **argv)
{
    int skfd;
    int ret;
    user_msg_info u_info;
    socklen_t len;
    struct nlmsghdr *nlh = NULL;
    struct sockaddr_nl saddr, daddr;
    char *umsg = "user hello netlink!!";
	cpuload_cfg cfg_info;
	if (argc < 4) {
		cfg_info.interval = 20;
		cfg_info.pid_focus= 20;
		cfg_info.threshold = 30;
	} else {
		cfg_info.interval = atoi(argv[1]);
		cfg_info.pid_focus= atoi(argv[2]); 
		cfg_info.threshold = atoi(argv[3]);
	}

	

    /* 创建NETLINK socket */
    skfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_CPULOAD);
    if(skfd == -1)
    {
        perror("create socket error\n");
        return -1;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.nl_family = AF_NETLINK; //AF_NETLINK
    saddr.nl_pid = USER_PORT;  //端口号(port ID) 
    saddr.nl_groups = 0;
    if(bind(skfd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0)
    {
        perror("bind() error\n");
        close(skfd);
        return -1;
    }

    memset(&daddr, 0, sizeof(daddr));
    daddr.nl_family = AF_NETLINK;
    daddr.nl_pid = 0; // to kernel 
    daddr.nl_groups = 0;

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PLOAD));
    memset(nlh, 0, sizeof(struct nlmsghdr));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PLOAD);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = saddr.nl_pid; //self port

    //memcpy(NLMSG_DATA(nlh), umsg, strlen(umsg));
    memcpy(NLMSG_DATA(nlh), (char *)&cfg_info, sizeof(cfg_info));
    ret = sendto(skfd, nlh, nlh->nlmsg_len, 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_nl));
    if(!ret)
    {
        perror("sendto error\n");
        close(skfd);
        exit(-1);
    }
    printf("===send kernel:%s\n", umsg);

    memset(&u_info, 0, sizeof(u_info));
    len = sizeof(struct sockaddr_nl);
	while(1) {
		ret = recvfrom(skfd, &u_info, sizeof(user_msg_info), 0, (struct sockaddr *)&daddr, &len);
		if(!ret)
		{
			perror("recv form kernel error\n");
			close(skfd);
			exit(-1);
		}

    	//printf("===from kernel:%s\n", u_info.msg);
    	printf("%s\n", u_info.msg);
	}
    close(skfd);

    free((void *)nlh);
    return 0;
}
