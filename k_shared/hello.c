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

static int mark_fd = 1;
static __thread char buff[BUFSIZ+1];

static void setup_ftrace_marker(void)
{
	struct stat st;
	char *files[]={
		"/sys/kernel/debug/tracing/trace_marker",
		"/debug/tracing/trace_marker",
		"/debugfs/tracing/trace_marker",
	};
	
	int ret;
	int i=0;
	for(i=0; i < (sizeof(files)/sizeof(char*));i++) {
		ret = stat(files[i], &st);
		if (ret >=0) {
			break;
		}
	}
	
	if (ret >= 0) {
		mark_fd = open(files[i], O_WRONLY);
		printf("cannot found the sys tracing.\n");
	}
	
	return 0;
}

static void ftrace_write(const char *fmt, ...)
{
	va_list ap;
	int n;
	
	if (mark_fd < 0)
		return ;
	
	va_start(ap, fmt);
	n = vsnprintf(buff, BUFSIZ, fmt, ap);
	va_end(ap);
	
	write(mark_fd, buff, n);
}
void sig_handler()
{
	printf("Oops! will exit...\n");
	if (mark_fd >= 0) {
		close(mark_fd);
	}
	_exit(0);
}
int main()
{ 
	printf("hello\n");
	int count = 0;
	
	signal(SIGINT,sig_handler);
	
	setup_ftrace_marker();
	ftrace_write("start program.\n");
	
	while(1) {
		usleep(300*1000);
		count++;
		ftrace_write("===test count=%d\n",count);
	}

	return 0;
}
