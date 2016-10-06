#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern const char * const sys_siglist[];
void my_handler(int sig,siginfo_t *siginfo,void *context) {
	printf("%s from %ld",sys_siglist[sig],(long)siginfo->si_pid);
	exit(0);
}

int main(int argc,char* argv[]) {
	unsigned int t = 10;
	struct sigaction sa;
	sa.sa_sigaction = &my_handler;
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss,SIGUSR1);
	sigaddset(&ss,SIGUSR2);
	sa.sa_mask = ss;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1,&sa,NULL);
	sigaction(SIGUSR2,&sa,NULL);
	while (t) {
		t = sleep(t);
	}
	printf("No signals were caught");
	return 0;
}
