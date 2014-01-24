#include "csync2.h"
#include <syslog.h>
#include <stdarg.h>

int cslogopen(char *mode) {
	openlog(mode,LOG_PID,LOG_DAEMON);	
	return 0;
}

void cslog(int severity, const char *format) {
	int prio;
	va_list ap;
	va_start(ap,format);
	switch(severity) {
		case CS_DEBUG: prio=LOG_DEBUG; break;
		case CS_INFO:  prio=LOG_INFO; break;
		case CS_WARN:  prio=LOG_WARN; break;
		case CS_ERROR: prio=LOG_ERROR; break;
	}
	vsyslog(severity,format,ap);
	va_end(ap);
	return;
}
