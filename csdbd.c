#include "csync2.h"
#include "ipc.h"
#include "csdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>

static int csdbh=-1;


int cdb_simplesend(int handle, int message, char *data) {
	struct csdbmsg *msg;
	int s,ms;
	s=strlen(data)+1;
	ms=sizeof(msg[0])+s;
	msg=malloc(ms);
	if(!msg) {
		csync_debug(0,"out of memory");
		return -1;
	}	
	msg[0].message=message;
	msg[0].size=s;
	strcpy(msg[0].data,data);
	ipc_sndmsg(handle,msg,ms);
	csync_debug(3,"simplesend:%d, %d, %d, %d (%s)\n",sizeof(int),ms, msg[0].message,msg[0].size,msg[0].data);
	free(msg);
	return 0;
}

void cdb_sql(const char *err, const char *fmt, ...) {
	char *sql;
	va_list ap;
	va_start(ap, fmt);
	vasprintf(&sql, fmt, ap);
	va_end(ap);
	if(csdbh>=0) {
		struct csdbmsg_sql_status sr;
		int r;
//		cdb_simplesend(csdbh,CDB_PING,err);
//		r=ipc_rcvmsga(csdbh,&sr,sizeof(sr));
//		if(r!=sr||sr.message!=RDB_SQLSTATUS) {
//			csync_debug(0,"protocol error");
//		}
		cdb_simplesend(csdbh,CDB_SQLPLAIN,sql);
		r=ipc_rcvmsg(csdbh,&sr,sizeof(sr));
		if(r!=sizeof(sr) || sr.message!=RDB_SQLSTATUS) {
			csync_debug(0,"protocol error, received %d bytes",r);
		}
	} else {
		csync_debug(2, "SQL: %s\n", sql);
		csync_db_sql(err,"%s",sql);
	}
	free(sql);
}

int cdb_sqlselect(const char *err, const char *returnformat, struct textlist **ptl,const char *fmt, ...) {
	int count=0;
	int r=-1;
	int i;
	int cc=strlen(returnformat);
	char *sql;
	char *total;
	char *args[CSDBMAXCOLUMNS];
	int b=-1;
	int bv;
	int maxa;
	va_list ap;
	va_start(ap, fmt);
	vasprintf(&sql, fmt, ap);
	va_end(ap);
	asprintf(&total,"%c%s",cc+'0',sql);
	for(i=0;i<cc;i++) {
		if(returnformat[i]=='b') b=i;
	}
	for(i=0;i<CSDBMAXCOLUMNS;i++) {
		args[i]="";
	}
	if(csdbh>=0) {
		cdb_simplesend(csdbh,CDB_SQLSELECT,total);
		while(1) {
			void *msgb=NULL;
			struct csdbmsg *msg;
			struct csdbmsg_sql_row *row;
			int buffer_size,n;
			n=ipc_rcvmsga(csdbh,(void *)&msgb,&buffer_size);
			if(n<0) {
				csync_debug(0,"Connection closed unexptectedly");
				close(csdbh);
				csdbh=-1;
				if(msgb) free(msgb);
				break;
			}
			if(buffer_size<sizeof(struct csdbmsg)) {
				csync_debug(0,"Protocol error: out of sync?");
				close(csdbh);
				csdbh=-1;
				if(msgb) free(msgb);
				break;
			}
			msg=msgb;
			if(msg[0].message==RDB_SQLSTATUS) {
				if(msgb) free(msgb);
				r=0;
				break;	
			}
			if(msg[0].message!=RDB_SQLROW) {
				csync_debug(0,"Protocol error: out of sync?");
				close(csdbh);
				csdbh=-1;
				break;
			}
			row=msgb;
			maxa=-1;
			for(i=0;i<cc;i++) {
				if(returnformat[i]>='0' && returnformat[i]<='9') {
					n=(int)(returnformat[i]-'0');
					args[n]=&(row[0].data[row[0].coloff[i]]);
					if(maxa<n) maxa=n;
				}
			}
			bv=0;
			if(b>=0) bv=atoi(&(row[0].data[row[0].coloff[b]]));
			switch(maxa) {
				case 0: textlist_addorder(ptl, args[0], bv); break;
				case 1: textlist_addorder2(ptl, args[0], args[1], bv); break;
				case 2: textlist_addorder3(ptl, args[0], args[1], args[2], bv); break;
			}
			if(msgb) free(msgb);
		}
	} else {
		/* We have to perform it localy */
		int indexes[CSDBMAXCOLUMNS];
		maxa=-1;
		for(i=0;i<cc;i++) {
			int n;
			if(returnformat[i]>='0' && returnformat[i]<='9') {
				n=(int)(returnformat[i]-'0');
				indexes[n]=i;
				if(maxa<n) maxa=n;
			}
		}
		SQL_BEGIN(err,"%s",sql) {
			bv=0;
			if(b>=0) bv=atoi(SQL_V(b));
			switch(maxa) {
				case 0: textlist_addorder(ptl, SQL_V(indexes[0]), bv); break;
				case 1: textlist_addorder2(ptl, SQL_V(indexes[0]), SQL_V(indexes[1]), bv); break;
				case 2: textlist_addorder3(ptl, SQL_V(indexes[0]), SQL_V(indexes[1]), SQL_V(indexes[2]), bv); break;
			}
		} SQL_END
	}
	if(sql) free(sql);
	if(total) free(total);
	return count;
}


int csdb_connect() {
	if(!csync_csdbsocket) {
		csync_debug(0,"No socket defined");
		return -1;
	}
	csdbh=ipc_connect(csync_csdbsocket);
	if(csdbh<0) return csdbh;
	return 0;
}
int cdb_sqlstatus(int c, int rows, int result) {
	struct csdbmsg_sql_status sr;
	sr.message=RDB_SQLSTATUS;
	sr.size=sizeof(sr.rows)+sizeof(sr.result);
	sr.rows=rows;
	sr.result=result;
	ipc_sndmsg(c,&sr,sizeof(sr));
}
int cdbd_sql(ipch hserver,int c,struct csdbmsg *msg, int buffer_size) {
	csync_debug(2, "CDB_SQLPLAIN: %s;\n", msg[0].data);
	SQL(msg[0].data,"%s",msg[0].data);
	csync_debug(2, "CDB_SQLPLAIN(%d) finished\n",c, msg[0].data);
	cdb_sqlstatus(c,0,0);
	return 0;
}

int cdbd_sqlselect(ipch hserver,int c,struct csdbmsg *msg, int buffer_size) {
	int cc;
	cc=(int) (msg[0].data[0]-'0');
	csync_debug(2, "CDB_SQLSELECT(%d): %d, %s;\n",c,cc,&(msg[0].data[1]));
	SQL_BEGIN(msg[0].data,"%s",&(msg[0].data[1])) {
		struct csdbmsg_sql_row *row;
		int ps,i,off;
		ps=sizeof(*row);
		for(i=0;i<cc;i++) {
			ps+=strlen(SQL_V(i))+1;
		}
		row=malloc(ps);
		memset(row,0,ps);
		row[0].message=RDB_SQLROW;
		row[0].size=ps-(sizeof(struct csdbmsg));
		for(off=i=0;i<cc;i++) {
			row[0].coloff[i]=off;
			row[0].colsize[i]=strlen(SQL_V(i))+1;
			strcpy(&(row[0].data[off]),SQL_V(i));
			off+=row[0].colsize[i];
		}
		ipc_sndmsg(c,row,ps);
		free(row);
	} SQL_FIN {
		csync_debug(2, "CDB_SQLSELECT(%d) finished\n",c, msg[0].data);
		cdb_sqlstatus(c,SQL_COUNT,0);
	} SQL_END;
	return 0;
}

int cdbd_ping(ipch hserver,int c,struct csdbmsg *msg, int buffer_size) {
	msg[0].message=RDB_PONG;
	csync_debug(2,"CDB_PING:%s",msg[0].data);
	ipc_sndmsg(c,msg,buffer_size);
	return 0;
}
void handle_connection(ipch hserver, int c) {
	int n,r;
	struct csdbmsg *msg;
	int buffer_size;
	if((n=ipc_rcvmsga(c,(void *)&msg,&buffer_size))<0) {
		csync_debug(1,"closed connection");
		ipc_close(hserver,c);
		return;
	}
	if(buffer_size<sizeof(struct csdbmsg)) {
		csync_debug(0,"closed connection due to protocol error");
		ipc_close(hserver,c);
	}
	csync_debug(1,"command: %d(%d), data:%s(%d)",msg[0].message,buffer_size,msg[0].data,msg[0].size);
	r=1;
	switch(msg[0].message) {
		case CDB_PING:     r=cdbd_ping(hserver,c,msg,buffer_size); break;
		case CDB_SQLPLAIN: r=cdbd_sql(hserver,c,msg,buffer_size); break;
		case CDB_SQLSELECT: r=cdbd_sqlselect(hserver,c,msg,buffer_size); break;
		default: csync_debug(0,"unknown message %d",msg[0].message); break;
	}
	free(msg);
	if(r) {
		csync_debug(0,"closed connection %c due to protocol error %d",c,r);
		ipc_close(hserver,c);
	}
}

int csdb_daemon() {
	ipch hserver;
	int quit=0;
	fd_set rfds;
	FD_ZERO(&rfds);
	signal(SIGPIPE,SIG_IGN);
	if(!csync_csdbsocket) {
		csync_debug(0,"Trying to start csdbd: no socket defined");
		return -1;
	}
	hserver=ipc_setup_listen(csync_csdbsocket);
	while(!quit) {
		if(select(ipc_set_fds(hserver,&rfds,1)+1,&rfds,NULL,NULL,NULL)>0) {
			int c;
			while((c=is_ipc_in_fds(hserver,&rfds,1))>=0) handle_connection(hserver,c);
		}
	}
	ipc_destroy(hserver);
	return 0;
}

