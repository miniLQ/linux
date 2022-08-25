#include <stdio.h>
#include <stdlib.h>

static int count=0;


struct list_head{
	struct list_head *prev;
	struct list_head *next;
};

struct data{
	int val;
	struct list_head link;
};
struct list_head ptest;


void __list_add(struct list_head *entry,
                struct list_head *prev, struct list_head *next)
{
    next->prev = entry;
    entry->next = next;
    entry->prev = prev;
    prev->next = entry;
}
void list_add(struct list_head *entry, struct list_head *head)
{
    __list_add(entry, head, head->next);
}

void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

#ifndef container_of
#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - (char *) &((type *)0)->member)
#endif


static inline void
INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list->prev = list;
}


void link_test(struct list_head * link)
{
	struct list_head *l,*o, *k;
	struct data *p,*q;
	int i=0;

	printf("list head size=x%x\n",sizeof(struct list_head));
	for (i=0; i < 10; i++)
	{
		struct data *ptmp = (struct data*)malloc(sizeof(struct data));
		ptmp->val = i+1;

		list_add(&ptmp->link,link);
	}

	for (l = link->next,o=l->next; l != link; l = o,o = o->next)
	{
		printf("---addr l=0x%x\n",l);
		printf("---addr o=0x%x\n",o);
		printf("---addr k=0x%x\n",k);

		k = l -1;

		p = container_of(l, struct data, link);
		printf("---l val=%d\n",p->val);
		q = container_of(k, struct data, link);
		printf("---k val=%d\n\n",q->val);

	}
}
void link_free(struct list_head *link)
{
	struct list_head *l,*o;
	struct data *p;

		//list_del(l);
		//kfree(p);

}
static int ktop_show()
{
	INIT_LIST_HEAD(&ptest);
	link_test(&ptest);
	link_free(&ptest);

	return 0;
}


int main()
{
	ktop_show();
	return 0;
}

