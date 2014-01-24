#include <stdio.h>
#include <sys/socket.h>
/* bug in sys/un.h */
#define UNIX_PATH_MAX 108
#include <sys/un.h>
#include <sys/types.h>
#include <malloc.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/select.h>

#include "ipc.h"

#define IPCMAGIC 0xbabecafe

int ipc_check_closed(int socket) {
	int r;
	struct pollfd test;
	test.fd=socket;
	test.events=POLLIN|POLLOUT|POLLERR|POLLHUP|POLLNVAL;
	r=poll(&test,1,0);	
	printf("revents=%08x\n",test.revents);
	return 0;
}

ipch ipc_setup_listen(char *name) {
	int i,s,r;
	ipch h;
	struct sockaddr_un *socknaam;
	h=calloc(1,sizeof(*h));
	for(i=0;i<MAX_CONNECTIONS;i++) {
		h->connection[i]=-1;
	}
	socknaam=malloc(sizeof(*socknaam));
	unlink(name);
	if(!socknaam) {
		free(h);
		return NULL;
	}
	s=socket(PF_UNIX,SOCK_STREAM,0);
	if(s<0) {
		free(h);
		return NULL;
	}
	h->listen_fd=s;
	socknaam->sun_family=AF_UNIX;
	strncpy(socknaam->sun_path,name,UNIX_PATH_MAX);
	r=bind(s,(struct sockaddr *)socknaam,sizeof(*socknaam));
	if(r) {
		close(s);
		free(h);
		return NULL;
	}
	r=listen(s,5);
	if(r) {
		close(s);
		free(h);
		return NULL;
	}
	return h;
}

int ipc_destroy(ipch h) {
	int i;
	if(h->listen_fd>=0) close(h->listen_fd);
	for(i=0;i<MAX_CONNECTIONS;i++) {
		if(h->connection[i]>=0) close(h->connection[i]);
	}
	free(h);
	return 0;
}

int is_ipc_connection(ipch h, int fd) {
	int i;
	for(i=0;i<MAX_CONNECTIONS;i++) {
		if(h->connection[i]==fd) return i;
	}
	return -1;
}

int ipc_set_fds(ipch h,fd_set *fd,int autoaccept) {
	int i,maxfd;
	if(autoaccept) {
	FD_SET(h->listen_fd,fd);
		maxfd=h->listen_fd;
	} else {
		maxfd=-1;
	}
	for(i=0;i<MAX_CONNECTIONS;i++) {
		if(h->connection[i]>maxfd) maxfd=h->connection[i];
		if(h->connection[i]>=0) FD_SET(h->connection[i],fd);
	}
	return maxfd;
}

int is_ipc_in_fds(ipch h,fd_set *fd,int autoaccept) {
	int i;
	if(autoaccept && FD_ISSET(h->listen_fd,fd)) {
		ipc_accept(h);
		FD_CLR(h->listen_fd,fd);
	}
	for(i=0;i<MAX_CONNECTIONS;i++) {
		if(h->connection[i]>=0 && FD_ISSET(h->connection[i],fd)) {
			FD_CLR(h->connection[i],fd);
			return h->connection[i];
		}
	}
	return -1;
}

int ipc_close(ipch h, int fd) {
	int i,r;
	r=i=is_ipc_connection(h,fd);
	if(i>=0) {
		r=close(h->connection[i]);
		h->connection[i]=-1;
		h->connections--;
	}
	return r;
}

int ipc_accept(ipch h) {
	int i;
	for(i=0;i<MAX_CONNECTIONS;i++) {
		if(h->connection[i]==-1) break;
	}
	if(i==MAX_CONNECTIONS) return -2;
	h->connection[i]=accept(h->listen_fd,NULL,NULL);
	h->brdfilter[i]=0;
	if(h->connection[i]>=0) h->connections++;
	return h->connection[i];
}

int ipc_connect(char *name) {
	int s,r;
	struct sockaddr_un *socknaam;
	socknaam=malloc(sizeof(*socknaam));
	if(!socknaam) return -2;
	s=socket(PF_UNIX,SOCK_STREAM,0);
	if(s<0) {
		return s;
	}
	socknaam->sun_family=AF_UNIX;
	strncpy(socknaam->sun_path,name,UNIX_PATH_MAX);
	r=connect(s,(struct sockaddr *)socknaam,sizeof(*socknaam));
	if(r) {
		close(s);
		return -2;
	}
	return s;
}

int ipc_sndmsg(int socket,void *msg, int msg_size) {
	int *b,l;
	if(msg_size<0) msg_size=strlen(msg)+1;
	l=msg_size+2*sizeof(int);
	b=alloca(l);
	if(!b) return -1;
	b[0]=IPCMAGIC;
	b[1]=msg_size;
	memcpy(&b[2],msg,msg_size);
	/* atomic write */
	//ipc_check_closed(socket);
	return write(socket,b,l);
}

int ipc_set_filter(ipch h, int fd, int filter) {
	int i,r;
	r=i=is_ipc_connection(h,fd);
	if(i>=0) {
		r=h->brdfilter[i];
		h->brdfilter[i]=filter;
	}
	return r;
}
int ipc_brdmsg(ipch h,void *msg, int msg_size,int brdflg) {
	int i,n;
	for(n=i=0;i<MAX_CONNECTIONS;i++) {
		if(h->connection[i]>=0 && (h->brdfilter[i]&brdflg)) {
			ipc_sndmsg(h->connection[i],msg, msg_size);
			n++;
		}
	}
	return n;
}

int ipc_rcvmsg(int socket, void *buf, int buf_size) {
	int n,header[2];
	n=read(socket,header,sizeof(header));
	if(n!=sizeof(header)) return -2;
	if(header[0]!=IPCMAGIC) return -3;
	if(header[1]>buf_size) return -4;
	n=read(socket,buf,header[1]);
	if(n!=header[1]) return -5;
	return header[1];	
}

int ipc_rcvmsga(int socket, void **buf, int *buf_size) {
	int n,header[2];
	void *buffer;
	if(!buf) return -1;
	n=read(socket,header,sizeof(header));
	if(n!=sizeof(header)) return -2;
	if(header[0]!=IPCMAGIC) return -3;
	/* Need to check a maximum amount */
	//if(header[1]>buf_size) return -4;
	buffer=malloc(header[1]);
	*buf=buffer;
	/* Bad practice to not test the buf_size with protocol spec */
	if(buf_size) *buf_size=header[1];
	if(!buffer) return -4;
	n=read(socket,buffer,header[1]);
	if(n!=header[1]) return -5;
	return header[1];	
}

