#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{ 
	int count = 0;
	
	int val = atoi(argv[1]);
	printf("hello,input:%d\n",val);
	while(1) {
		if (val > 0) {
			usleep(val*1000);
		}
		count++;
	}

	return 0;
}
