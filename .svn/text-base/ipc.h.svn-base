#ifndef _IPC_H_
#define _IPC_H_
#define MAX_CONNECTIONS 16
typedef struct {
	int listen_fd;
	int connections;
	int connection[MAX_CONNECTIONS];
	int brdfilter[MAX_CONNECTIONS];
} *ipch;
#define ipc_accept_fd(handle) (handle->listen_fd)
extern int ipc_set_fds(ipch ipc, fd_set *fds,int autoaccept);
extern int ipc_set_filter(ipch h, int fd, int filter);
extern int is_ipc_in_fds(ipch ipc, fd_set *fds,int autoaccept);
extern int ipc_accept(ipch h);
extern int is_ipc_connection(ipch h, int fd);
extern int ipc_sndmsg(int socket,void *msg, int msg_size);
extern int ipc_brdmsg(ipch h,void *msg, int msg_size,int brdflg);
extern int ipc_rcvmsg(int socket, void *buf, int buf_size);
extern int ipc_connect(char *name);
extern int ipc_close(ipch h,int fd);
extern ipch ipc_setup_listen(char *name);
extern int ipc_destroy(ipch h);
/* receive msg, alloc buffer on reception */
extern int ipc_rcvmsga(int socket, void **buf, int *buf_size);
#endif
