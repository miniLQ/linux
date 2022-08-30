#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

 #include <signal.h>
 #include <pthread.h>
 void thread1()
 {
	 int i=0;
	 while(1) {
			 i++;
		 }
 }

int main()
{ 
	printf("hello\n");
	pthread_t id,id1;
	int ret = 0;
	ret = pthread_create(&id,NULL,(void *)thread1,NULL);
	if (ret) {
		printf("create pthread error.\n");
		exit(1);
	}
	ret = pthread_create(&id1,NULL,(void *)thread1,NULL);
	if (ret) {
		printf("create pthread error.\n");
		exit(1);
	}

	pthread_join(id,NULL);
	pthread_join(id1,NULL);

	return 0;
}
