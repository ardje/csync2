#include <stdio.h>
#include <sys/socket.h>
/* bug in sys/un.h */
#define UNIX_PATH_MAX 108
#include <sys/un.h>
#include <sys/types.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include "ipc.h"

int quit=0;
#define CMP(x,y) (!strncmp(x,y,sizeof(y)-1))
void handle_connection(ipch hserver, int c) {
	int n;
	char buffer[256];
	char buffer2[256];
	if((n=ipc_rcvmsg(c,buffer,sizeof(buffer)))<0) {
		ipc_close(hserver,c);
		return;
	}
	printf("Handling \"%s\" from %d\n",buffer,c);
	if(CMP(buffer,"DOWN")) quit=1;
	if(CMP(buffer,"EVENTS")) {
		ipc_set_filter(hserver,c,1);
	}
	sprintf(buffer2,"OK: %s",buffer);
	ipc_sndmsg(c,buffer2,-1);
}

int main(int argc, char **argv) {
	int n;
	char buffer[256];
	ipch hserver;
	fd_set rfds;
	setvbuf(stdout,NULL,_IONBF,0);
	FD_ZERO(&rfds);
	signal(SIGPIPE,SIG_IGN);
	hserver=ipc_setup_listen(argv[1]);
	while(!quit) {
		FD_SET(0,&rfds);
		if(select(ipc_set_fds(hserver,&rfds,1)+1,&rfds,NULL,NULL,NULL)>0) {
			int c;
			if(FD_ISSET(0,&rfds)) {
				FD_CLR(0,&rfds);
				n=read(0,buffer,256);
				buffer[n-1]=0;
				printf("Broadcasting %s\n", buffer);
				ipc_brdmsg(hserver,buffer,-1,1);
			}
			while((c=is_ipc_in_fds(hserver,&rfds,1))>=0) handle_connection(hserver,c);
		}
	}
	ipc_destroy(hserver);
#define PUTS(x) write(0,x,strlen(x));
	return 0;
}
