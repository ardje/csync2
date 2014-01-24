#ifndef CSDB_H
#define CSDB_H 1
/* csdb internal stuff */
enum csdb_messages {
	CDB_PING,
	CDB_SQLPLAIN,
	CDB_SQLSELECT,
	CDB_SQLPREPARE,
	CDB_SQLNEXT,
	CDB_SQLCLOSE,

	RDB_PONG,
	RDB_SQLSTATUS,
	RDB_SQLROW
};

struct csdbmsg {
	int message;
	int size;
	char data[];
};
struct csdbmsg_sql_status {
	int message;
	int size; //(sizeof rows,result)
	int rows;
	int result;
};
#define CSDBMAXCOLUMNS 5
struct csdbmsg_sql_row {
	int message;
	int size; //(sizeof rows,result)
	int columns;
	int colsize[CSDBMAXCOLUMNS];
	int coloff[CSDBMAXCOLUMNS];
	char data[];
};
#define CDBSQLPLAIN(e, s, ...) cdb_sql(e, s, ##__VA_ARGS__)
extern void cdb_sql(const char *err, const char *fmt, ...);
extern int cdb_sqlselect(const char *err, const char *returnformat, struct textlist **ptl,const char *fmt, ...);

#endif /* CSDB_H */
