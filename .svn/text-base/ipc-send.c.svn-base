#include <stdio.h>
#include <sys/socket.h>
#include <malloc.h>
#include <sys/select.h>
#include <unistd.h>
#include "ipc.h"

int main(int argc, char **argv) {
	int s,quit,r;
	char buffer[256];
	fd_set rfds;
	setvbuf(stdout,NULL,_IONBF,0);
	s=ipc_connect(argv[1]);
	FD_ZERO(&rfds);
	for(quit=0;!quit;) {
		FD_SET(s,&rfds);
		FD_SET(0,&rfds);
		bzero(buffer,256);
		select(s+1,&rfds,NULL,NULL,NULL);
		if(FD_ISSET(0,&rfds)) {
			buffer[255]=0;
			r=read(0,buffer,255);
			if(r>0) {
				buffer[r-1]=0;
				ipc_sndmsg(s,buffer,-1);
			} else {
				quit=1;
			}
		}
		if(FD_ISSET(s,&rfds)) {
			buffer[255]=0;
			r=ipc_rcvmsg(s,buffer,255);
			if(r>=0) {
				printf("GOT: %s\n",buffer);
			} else {
				quit=1;
			}
		}
	}
	close(s);
	return 0;
}
