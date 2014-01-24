#include "csync2.h"
#include "csdb.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int csdbCheckDirtyFilename_(const char *filename) {
	int c;
	struct textlist *tl=NULL;
	c=cdb_sqlselect("check if dirty exists","0",&tl,"SELECT 1 FROM dirty WHERE filename = '%s' LIMIT 1",filename);
	textlist_free(tl);
	return c;
}

void csdbDeleteDirtyFilename_Peername_(const char *filename, const char * peername) {
	cdb_sql("Remove dirty-file entry.",
		"DELETE FROM dirty WHERE filename = '%s'"
		"%s%s%s", filename,peername?" AND peername ='":"",peername?peername:"",peername?"'":"");
}
void csdbDeleteFileFilename_(const char *filename) {
	cdb_sql("Remove old file from file db",
	    "DELETE FROM file WHERE filename = '%s'", filename);
}
void csdbAddFileFilename_Check_(const char *filename, const char *checktxt) {
	cdb_sql(
		"Add new file",
		"INSERT INTO file (filename, checktxt) VALUES "
		"('%s', '%s')", filename, checktxt);
}
void csdbAddDirtyFilename_Flags_Myname_Peername_(const char *filename, int flags, const char *myname, const char *peername) {
	if(flags==DIRTY_FORCE) {
			cdb_sql("Marking File forced Dirty",
				"REPLACE INTO dirty (filename, force, myname, peername) "
				"VALUES ('%s', 1, '%s', '%s')",
				filename, myname, peername) ;
	} else {
			cdb_sql("Marking File Dirty",
				"INSERT INTO dirty (filename, force, myname, peername) "
				"VALUES ('%s', 0, '%s', '%s')",
				filename, myname, peername) ;
	}
}
struct textlist *csdbGetPeers(void) {
	struct textlist *tl = 0;
	cdb_sqlselect("Get hosts from dirty table","0",&tl,"SELECT peername FROM dirty GROUP BY peername ORDER BY random()");
	return tl;
}
struct textlist *csdbGetDirtyFilesPeername_Maxamount_(const char *peername, int limit) {
	struct textlist *tl = 0;
	char *limit_work="";
	if(limit) {
		asprintf(&limit_work," LIMIT %d",limit);
	}
	if(peername) {
		cdb_sqlselect("Dirty Files for peer","01b",&tl,
			"SELECT filename, myname, force FROM dirty WHERE peername = '%s' "
			"ORDER by filename ASC%s",peername,limit_work);
	} else {
		cdb_sqlselect("All Dirty Files","012b",&tl,
			"SELECT filename, myname, peername, force FROM dirty "
			"ORDER by filename ASC%s",limit_work);
	}
	if(limit) free(limit_work);
	return tl;
}	
struct textlist *csdbGetDirty(void) {
	struct textlist *tl = 0;
	cdb_sqlselect("DUMP dirty DB", "012b",&tl,"SELECT filename, myname, peername, force FROM dirty");
	return tl;
}
void csdbUpdateDirtyFilename_Recursive_Force_(char *name,int recursive, int force) {
	char *sql;
	if(recursive) {
		if(name) {
			asprintf(&sql,
				"UPDATE dirty SET force = %d WHERE "
				"filename = > '%s/' AND "
				"filename < '%s0'", force, name, name);
		} else {
			asprintf(&sql,"UPDATE dirty SET force = %d",force);
		}
		cdb_sql("Mark subdirs as to be forced","%s",sql);
		free(sql);
	}
	if(name) {
		asprintf(&sql,"UPDATE dirty SET force = %d WHERE filename = '%s'", force,name);
		cdb_sql("Mark entry as to be forced","%s",sql);
	}
}
struct textlist *csdbGetFilesNamesonly(void) {
	struct textlist *tl = 0;
	cdb_sqlselect("Query file DB","0",&tl,"SELECT filename FROM file");
	return tl;
}
struct textlist *csdbGetFilesRecursiveFilename_(const char *filename) {
	struct textlist *tl = 0;
	char *where="";
	if(filename) {
		asprintf(&where,
			" WHERE filename= '%s' "
			"UNION ALL SELECT checktxt, filename FROM file WHERE "
			"filename > '%s/' "
			"AND filename < '%s0'",filename,filename,filename);
	}
	cdb_sqlselect("Query file DB", "01", &tl,"SELECT checktxt, filename FROM file%s ORDER BY filename",where);
			csync_debug_ping(2);
	if(filename) free(where);
			csync_debug_ping(2);
	return tl;
}
struct textlist *csdbGetFilesFilename_(const char *filename) {
	struct textlist *tl = 0;
	char *where="";
	if(filename) {
		asprintf(&where," WHERE filename= '%s'",filename);
	}
	cdb_sqlselect("Query file DB", "01", &tl,"SELECT checktxt, filename FROM file%s ORDER BY filename",where);
			csync_debug_ping(2);
	if(filename) free(where);
			csync_debug_ping(2);
	return tl;
}

struct textlist *csdbGetHintsMaxamount_(int limit) {
	struct textlist *tl = NULL;
	char *limit_work="";
	if(limit) {
		asprintf(&limit_work," LIMIT %d",limit);
	}
	cdb_sqlselect("Dump all hints", "0b", &tl, "SELECT filename, recursive FROM hint%s",limit_work);
	if(limit) free(limit_work);
	return tl;
}

void csdbDeleteHintFilename_Flags_(const char *filename, int flags) {
	cdb_sql("Remove processed hint.", "DELETE FROM hint WHERE filename = '%s' " "and recursive = %d", filename, flags);
}
void csdbAddHintFilename_Flags_(const char *filename, int flags) {
	cdb_sql("Adding Hint", "INSERT INTO hint (filename, recursive) " "VALUES ('%s', %d)", filename, flags);
}

void csdbAddActionFilename_Command_Logfile_(const char *filename, const char *command, const char *logfile) {
	cdb_sql(
		"Add new action",
		"INSERT INTO action (filename, command, logfile) "
		"VALUES ('%s', '%s', '%s')",
		filename, command, logfile);
}
struct textlist *csdbGetActionCommand_Logfile_(const char *command, const char *logfile) {
	struct textlist *tl = NULL;
	cdb_sqlselect(
		"Get action", "0", &tl,
		"SELECT filename FROM action WHERE command = '%s' AND logfile = '%s'",
		command,logfile);
	return tl;
}
void csdbDeleteActionFilename_Command_Logfile_(const char *filename, const char *command, const char *logfile) {
	cdb_sql(
		"Add new action",
		"DELETE FROM action WHERE "
		"command = '%s' AND "
		"logfile = '%s' AND "
		"filename = '%s'",
		command,logfile,filename);
}
struct textlist *csdbGetActionCommands(void) {
	struct textlist *tl = NULL;
	cdb_sqlselect(
		"Get actions", "01", &tl,
		"SELECT command, logfile FROM action GROUP BY command, logfile");
	return tl;
}
struct textlist *csdbGetCertPeername_(const char *peername) {
	struct textlist *tl = NULL;
	cdb_sqlselect(
		"Get certs", "0", &tl,
		"SELECT certdata FROM x509_cert WHERE peername = '%s'",peername);
	return tl;
}
void csdbAddCertPeername_Cert_(const char *peername, const char *certdata) {
	cdb_sql(
		"Add new file",
		"INSERT INTO x509_cert (peername, certdata) VALUES "
		"('%s', '%s')", peername, certdata);
}
