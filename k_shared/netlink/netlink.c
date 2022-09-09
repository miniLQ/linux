#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <net/sock.h>
#include <linux/netlink.h>

#define NETLINK_CPULOAD 30 
#define MSG_LEN          256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason");
MODULE_DESCRIPTION("netlink");

struct sock *nlsk = NULL;
extern struct net init_net;

__u32 user_pid=0;

int send_usrmsg(char *pbuf, uint16_t len)
{
    struct sk_buff *nl_skb;
    struct nlmsghdr *nlh;

    int ret;

    /* 创建sk_buff 空间 */
    nl_skb = nlmsg_new(len, GFP_ATOMIC);
    if(!nl_skb)
    {
        printk("netlink alloc failure\n");
        return -1;
    }

    /* 设置netlink消息头部 */
    nlh = nlmsg_put(nl_skb, 0, 0, NETLINK_CPULOAD, len, 0);
    if(nlh == NULL)
    {
        printk("nlmsg_put failaure \n");
        nlmsg_free(nl_skb);
        return -1;
    }

    /* 拷贝数据发送 */
    memcpy(nlmsg_data(nlh), pbuf, len);
   // ret = netlink_unicast(nlsk, nl_skb, user_pid,  MSG_DONTWAIT);
	netlink_broadcast(nlsk, nl_skb, 0, 1,  MSG_DONTWAIT);
//	ret = netlink_unicast(nlsk, nl_skb, 0,  MSG_DONTWAIT);
//    nlmsg_free(nl_skb);

    return ret;
}
static int count=0;
static void netlink_rcv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = NULL;
    char *umsg = NULL;
    char *kmsg = "hello users!!!";
    char kmsg1[256]; 
    if(skb->len >= nlmsg_total_size(0))
    {
        nlh = nlmsg_hdr(skb);
        umsg = NLMSG_DATA(nlh);
        if(umsg)
        {
            printk("---kernel recv from user: %s,and userid=%d\n\n", umsg,nlh->nlmsg_pid);
			user_pid = nlh->nlmsg_pid;
            //send_usrmsg(kmsg, strlen(kmsg));
			sprintf(kmsg1,"send count:%d",count++);
            send_usrmsg(kmsg1, strlen(kmsg1));
//			sprintf(kmsg1,"send count:%d",count++);
 //           send_usrmsg(kmsg1, strlen(kmsg1));
//			sprintf(kmsg1,"send count:%d",count++);
 //           send_usrmsg(kmsg1, strlen(kmsg1));

        }
    }
}

struct netlink_kernel_cfg cfg = { 
        .input  = netlink_rcv_msg, /* set recv callback */
};  

static void ktop_timer_func(struct timer_list *timer)
{
	unsigned long flags;
	char kmsg1[256];

	sprintf(kmsg1,"ktop timer count:%d",count++);
	send_usrmsg(kmsg1, strlen(kmsg1));
	mod_timer(timer, jiffies + msecs_to_jiffies(3000));
}

DEFINE_TIMER(ktop_timer, ktop_timer_func);
int test_netlink_init(void)
{
    /* create netlink socket */
    nlsk = (struct sock *)netlink_kernel_create(&init_net, NETLINK_CPULOAD, &cfg);
    if(nlsk == NULL)
    {   
        printk("netlink_kernel_create error !\n");
        return -1; 
    }   
    printk("test_netlink_init\n");
    
#ifndef KTOP_MANUAL
	ktop_timer.expires = jiffies + msecs_to_jiffies(10000);
	add_timer(&ktop_timer);
#endif
    return 0;
}

void test_netlink_exit(void)
{
    if (nlsk){
        netlink_kernel_release(nlsk); /* release ..*/
        nlsk = NULL;
    }   
    printk("test_netlink_exit!\n");
}

module_init(test_netlink_init);
module_exit(test_netlink_exit);
