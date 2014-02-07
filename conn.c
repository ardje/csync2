/*
 *  csync2 - cluster synchronization tool, 2nd generation
 *  LINBIT Information Technologies GmbH <http://www.linbit.com>
 *  Copyright (C) 2004, 2005, 2006  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "csync2.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#ifdef HAVE_LIBGNUTLS_OPENSSL
#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>
#endif

int conn_fd_in  = -1;
int conn_fd_out = -1;
int conn_clisok = 0;

#ifdef HAVE_LIBGNUTLS_OPENSSL
int csync_conn_usessl = 0;

static gnutls_session_t conn_tls_session;
static gnutls_certificate_credentials_t conn_x509_cred;
#endif

int conn_open(const char *peername)
{
        struct sockaddr_in sin;
        struct hostent *hp;
	int on = 1;

        hp = gethostbyname(peername);
        if ( ! hp ) {
                csync_debug(1, "Can't resolve peername.\n");
                return -1;
        }

        conn_fd_in = socket(hp->h_addrtype, SOCK_STREAM, 0);
        if (conn_fd_in < 0) {
                csync_debug(1, "Can't create socket.\n");
                return -1;
        }

        sin.sin_family = hp->h_addrtype;
        bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
        sin.sin_port = htons(csync_port);

        if (connect(conn_fd_in, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
                csync_debug(1, "Can't connect to remote host.\n");
		close(conn_fd_in); conn_fd_in = -1;
                return -1;
        }

	if (setsockopt(conn_fd_in, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on) ) < 0 ) {
                csync_debug(1, "Can't set TCP_NODELAY option on TCP socket.\n");
		close(conn_fd_in); conn_fd_in = -1;
                return -1;
	}

	conn_fd_out = conn_fd_in;
	conn_clisok = 1;
#ifdef HAVE_LIBGNUTLS_OPENSSL
	csync_conn_usessl = 0;
#endif

	return 0;
}

int conn_set(int infd, int outfd)
{
	int on = 1;

	conn_fd_in  = infd;
	conn_fd_out = outfd;
	conn_clisok = 1;
#ifdef HAVE_LIBGNUTLS_OPENSSL
	csync_conn_usessl = 0;
#endif

	// when running in server mode, this has been done already
	// in csync2.c with more restrictive error handling..
	if ( setsockopt(conn_fd_out, IPPROTO_TCP, TCP_NODELAY, &on, (socklen_t) sizeof(on)) < 0 )
                csync_debug(1, "Can't set TCP_NODELAY option on TCP socket.\n");

	return 0;
}


#ifdef HAVE_LIBGNUTLS_OPENSSL

char *ssl_keyfile = ETCDIR "/csync2_ssl_key.pem";
char *ssl_certfile = ETCDIR "/csync2_ssl_cert.pem";
static void ssl_log(int level, const char* msg)
{ csync_debug(level, "%s", msg); }

int conn_activate_ssl(int server_role)
{
	static int sslinit = 0;
	gnutls_alert_description_t alrt;
	//char *ssl_keyfile;
	//char *ssl_certfile;
	int err;

	if (csync_conn_usessl)
		return 0;

	gnutls_global_init();
	gnutls_global_set_log_function(ssl_log);
	gnutls_global_set_log_level(10);

	gnutls_certificate_allocate_credentials(&conn_x509_cred);

	err = gnutls_certificate_set_x509_key_file(conn_x509_cred, ssl_certfile, ssl_keyfile, GNUTLS_X509_FMT_PEM);
	if(err != GNUTLS_E_SUCCESS) {
		gnutls_certificate_free_credentials(conn_x509_cred);
		gnutls_global_deinit();

		csync_fatal(
			"SSL: failed to use key file %s and/or certificate file %s: %s (%s)\n",
			ssl_keyfile,
			ssl_certfile,
			gnutls_strerror(err),
			gnutls_strerror_name(err)
		);
	}

	if(server_role) {
		gnutls_certificate_free_cas(conn_x509_cred);
 

		if(gnutls_certificate_set_x509_trust_file(conn_x509_cred, ssl_certfile, GNUTLS_X509_FMT_PEM) < 1) {
			gnutls_certificate_free_credentials(conn_x509_cred);
			gnutls_global_deinit();
 

			csync_fatal(
				"SSL: failed to use certificate file %s as CA.\n",
				ssl_certfile
			);
		}
	} else
		gnutls_certificate_free_ca_names(conn_x509_cred);
 
	gnutls_init(&conn_tls_session, (server_role ? GNUTLS_SERVER : GNUTLS_CLIENT));
	gnutls_priority_set_direct(conn_tls_session, "PERFORMANCE", NULL);
	gnutls_credentials_set(conn_tls_session, GNUTLS_CRD_CERTIFICATE, conn_x509_cred);
 


	if(server_role) {
		gnutls_certificate_send_x509_rdn_sequence(conn_tls_session, 0);
		gnutls_certificate_server_set_request(conn_tls_session, GNUTLS_CERT_REQUIRE);
	}
 

	gnutls_transport_set_ptr2(
		conn_tls_session,
		(gnutls_transport_ptr_t)(long)conn_fd_in,
		(gnutls_transport_ptr_t)(long)conn_fd_out
	);

	err = gnutls_handshake(conn_tls_session);
	switch(err) {
	case GNUTLS_E_SUCCESS:
		break;

	case GNUTLS_E_WARNING_ALERT_RECEIVED:
		alrt = gnutls_alert_get(conn_tls_session);
		fprintf(
			csync_debug_out,
			"SSL: warning alert received from peer: %d (%s).\n",
			alrt, gnutls_alert_get_name(alrt)
		);
		break;

	case GNUTLS_E_FATAL_ALERT_RECEIVED:
		alrt = gnutls_alert_get(conn_tls_session);
		fprintf(
			csync_debug_out,
			"SSL: fatal alert received from peer: %d (%s).\n",
			alrt, gnutls_alert_get_name(alrt)
		);

	default:
		gnutls_bye(conn_tls_session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(conn_tls_session);
		gnutls_certificate_free_credentials(conn_x509_cred);
		gnutls_global_deinit();

		csync_fatal(
			"SSL: handshake failed: %s (%s)\n",
			gnutls_strerror(err),
			gnutls_strerror_name(err)
		);
	}
	csync_conn_usessl = 1;

	return 0;
}

int conn_check_peer_cert(const char *peername, int callfatal)
{
	int i, cert_is_ok = -1;
	const gnutls_datum_t *peercerts;
	unsigned npeercerts;

	if (!csync_conn_usessl)
		return 1;

	peercerts = gnutls_certificate_get_peers(conn_tls_session, &npeercerts);
	if(peercerts == NULL || npeercerts == 0) {
		if (callfatal)
			csync_fatal("Peer did not provide an SSL X509 cetrificate.\n");
		csync_debug(1, "Peer did not provide an SSL X509 cetrificate.\n");
		return 0;
	}

	{
		char certdata[peercerts[0].size*2 + 1];
		struct textlist *tl;
		for (i=0; i<peercerts[0].size; i++)
			sprintf(certdata+i*2, "%02X", peercerts[0].data[i]);
		certdata[peercerts[0].size*2] = 0;
		tl=csdbGetCertPeername_(url_encode(peername));
		if (tl)
			if(!strcmp(tl->value, certdata))
				// Found and equal
				cert_is_ok = 1;
			else
				// Found but not equal
				cert_is_ok = 0;
		if(tl) textlist_free(tl);

		// Not found, add cert
		if (cert_is_ok < 0) {
			csync_debug(1, "Adding peer x509 certificate to db: %s\n", certdata);
			csdbAddCertPeername_Cert_(url_encode(peername), url_encode(certdata));
			return 1;
		}

		csync_debug(2, "Peer x509 certificate is: %s\n", certdata);

		if (!cert_is_ok) {
			if (callfatal)
				csync_fatal("Peer did provide a wrong SSL X509 cetrificate.\n");
			csync_debug(1, "Peer did provide a wrong SSL X509 cetrificate.\n");
			return 0;
		}
	}

	return 1;
}

#else

int conn_check_peer_cert(const char *peername, int callfatal)
{
	return 1;
}

#endif /* HAVE_LIBGNUTLS_OPENSSL */

int conn_close()
{
	if ( !conn_clisok ) return -1;

#ifdef HAVE_LIBGNUTLS_OPENSSL
	if ( csync_conn_usessl ) {
		gnutls_bye(conn_tls_session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(conn_tls_session);
		gnutls_certificate_free_credentials(conn_x509_cred);
		gnutls_global_deinit();
	}
#endif

	if ( conn_fd_in != conn_fd_out) close(conn_fd_in);
	close(conn_fd_out);

	conn_fd_in  = -1;
	conn_fd_out = -1;
	conn_clisok =  0;

	return 0;
}

static inline int READ(void *buf, size_t count)
{
#ifdef HAVE_LIBGNUTLS_OPENSSL
	if (csync_conn_usessl)
		return gnutls_record_recv(conn_tls_session, buf, count);
	else
#endif
		return read(conn_fd_in, buf, count);
}

static inline int WRITE(const void *buf, size_t count)
{
	static int n, total;

#ifdef HAVE_LIBGNUTLS_OPENSSL
	if (csync_conn_usessl)
		return gnutls_record_send(conn_tls_session, buf, count);
	else
#endif
	{
		total = 0;

		while (count > total) {
			n = write(conn_fd_out, ((char *) buf) + total, count - total);

			if (n >= 0)
				total += n;
			else {
				if (errno == EINTR)
					continue;
				else
					return -1;
			}
		}

		return total;
	}
}

int conn_raw_read(void *buf, size_t count)
{
	static char buffer[512];
	static int buf_start=0, buf_end=0;

	if ( buf_start == buf_end ) {
		if (count > 128)
			return READ(buf, count);
		else {
			buf_start = 0;
			buf_end = READ(buffer, 512);
			if (buf_end < 0) { buf_end=0; return -1; }
		}
	}

	if ( buf_start < buf_end ) {
		size_t real_count = buf_end - buf_start;
		if ( real_count > count ) real_count = count;

		memcpy(buf, buffer+buf_start, real_count);
		buf_start += real_count;

		return real_count;
	}

	return 0;
}

void conn_debug(const char *name, const unsigned char*buf, size_t count)
{
	int i;

	if ( csync_debug_level < 3 ) return;

	fprintf(csync_debug_out, "%s> ", name);
	for (i=0; i<count; i++) {
		switch (buf[i]) {
			case '\n':
				fprintf(csync_debug_out, "\\n");
				break;
			case '\r':
				fprintf(csync_debug_out, "\\r");
				break;
			default:
				if (buf[i] < 32 || buf[i] >= 127)
					fprintf(csync_debug_out, "\\%03o", buf[i]);
				else
					fprintf(csync_debug_out, "%c", buf[i]);
				break;
		}
	}
	fprintf(csync_debug_out, "\n");
}

int conn_read(void *buf, size_t count)
{
	int pos, rc;

	for (pos=0; pos < count; pos+=rc) {
		rc = conn_raw_read(buf+pos, count-pos);
		if (rc <= 0) return pos;
	}

	conn_debug("Peer", buf, pos);
	return pos;
}

int conn_write(const void *buf, size_t count)
{
	conn_debug("Local", buf, count);
	return WRITE(buf, count);
}

void conn_printf(const char *fmt, ...)
{
	char dummy, *buffer;
	va_list ap;
	int size;

	va_start(ap, fmt);
	size = vsnprintf(&dummy, 1, fmt, ap);
	buffer = alloca(size+1);
	va_end(ap);

	va_start(ap, fmt);
	vsnprintf(buffer, size+1, fmt, ap);
	va_end(ap);

	conn_write(buffer, size);
}

int conn_gets(char *s, int size)
{
	int i=0;

	while (i<size-1) {
		int rc = conn_raw_read(s+i, 1);
		if (rc != 1) break;
		if (s[i++] == '\n') break;
	}
	s[i] = 0;

	conn_debug("Peer", s, i);
	if (i > 1 && s[i-1] != '\n')
		csync_fatal("Received line too long for buffer size (%u), giving up.\n", size);
	return i;
}

