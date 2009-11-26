/*
 *  httpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   httpcmd - commands to send and retrieve files via HTTP
 **
 ** SYNOPSIS
 **   int  http_connect(char *hostname, int port, int sndbuf_size, int rcvbuf_size)
 **   int  http_ssl_auth(void)
 **   int  http_options(char *host, char *path)
 **   int  http_del(char *host, char *path, char *filename)
 **   int  http_get(char *host, char *path, char *filename, off_t *content_length, off_t offset)
 **   int  http_head(char *host, char *path, char *filename, off_t *content_length, time_t date)
 **   int  http_put(char *host, char *path, char *filename, off_t length)
 **   int  http_read(char *block, int blocksize)
 **   int  http_chunk_read(char **chunk, int *chunksize)
 **   int  http_version(void)
 **   int  http_write(char *block, char *buffer, int size)
 **   void http_quit(void)
 **
 ** DESCRIPTION
 **   httpcmd provides a set of commands to communicate with an HTTP
 **   server via BSD sockets.
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit HTTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the HTTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.04.2003 H.Kiehl Created
 **   25.12.2003 H.Kiehl Added traceing.
 **   28.04.2004 H.Kiehl Handle chunked reading.
 **   16.08.2006 H.Kiehl Added http_head() function.
 **   17.08.2006 H.Kiehl Added appending to http_get().
 */
DESCR__E_M3


#include <stdio.h>        /* fprintf(), fdopen(), fclose()               */
#include <stdarg.h>       /* va_start(), va_arg(), va_end()              */
#include <string.h>       /* memset(), memcpy(), strcpy()                */
#include <stdlib.h>       /* strtoul()                                   */
#include <ctype.h>        /* isdigit()                                   */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>  /* socket(), shutdown(), bind(), setsockopt()  */
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>  /* struct in_addr, sockaddr_in, htons()        */
#endif
#if defined (_WITH_TOS) && defined (IP_TOS)
# ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
# endif
# ifdef HAVE_NETINET_IP_H
#  include <netinet/ip.h> /* IPTOS_LOWDELAY, IPTOS_THROUGHPUT            */
# endif
#endif
#ifdef HAVE_ARPA_TELNET_H
# include <arpa/telnet.h> /* IAC, SYNCH, IP                              */
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>       /* struct hostent, gethostbyname()             */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>   /* inet_addr()                                 */
#endif
#ifdef WITH_SSL
# include <setjmp.h>      /* sigsetjmp(), siglongjmp()                   */
# include <signal.h>      /* signal()                                    */
# include <openssl/crypto.h>
# include <openssl/x509.h>  
# include <openssl/ssl.h>
#endif
#include <unistd.h>       /* select(), write(), read(), close(), alarm() */
#include <errno.h>
#include "fddefs.h"
#include "httpdefs.h"
#include "commondefs.h"

#ifdef WITH_SSL
SSL                              *ssl_con = NULL;
#endif

/* External global variables. */
extern int                       timeout_flag;
extern char                      msg_str[];
#ifdef LINUX
extern char                      *h_errlist[];  /* for gethostbyname()   */
extern int                       h_nerr;        /* for gethostbyname()   */
#endif
extern long                      transfer_timeout;

/* Local global variables. */
static int                       http_fd;
#ifdef WITH_SSL
static sigjmp_buf                env_alrm;
static SSL_CTX                   *ssl_ctx;
#endif
static struct timeval            timeout;
static struct http_message_reply hmr;

/* Local function prototypes. */
static int                       basic_authentication(void),
                                 check_connection(void),
                                 get_http_reply(int *),
                                 read_msg(int *);
#ifdef WITH_SSL
static void                      sig_handler(int);
#endif
#ifdef WITH_TRACE
static void                      store_http_options(int, int);
#endif


/*########################## http_connect() #############################*/
int
#ifdef WITH_SSL
http_connect(char *hostname, int port, char *user, char *passwd, int ssl, int sndbuf_size, int rcvbuf_size)
#else
http_connect(char *hostname, int port, char *user, char *passwd, int sndbuf_size, int rcvbuf_size)
#endif
{
#ifdef WITH_TRACE
   int                length;
#endif
   struct sockaddr_in sin;

   (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
   if ((sin.sin_addr.s_addr = inet_addr(hostname)) == -1)
   {
      register struct hostent *p_host = NULL;

      if ((p_host = gethostbyname(hostname)) == NULL)
      {
#if !defined (_HPUX) && !defined (_SCO)
         if (h_errno != 0)
         {
#ifdef LINUX
            if ((h_errno > 0) && (h_errno < h_nerr))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("Failed to gethostbyname() %s : %s"),
                         hostname, h_errlist[h_errno]);
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                         hostname, h_errno, strerror(errno));
            }
#else
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                      hostname, h_errno, strerror(errno));
#endif
         }
         else
         {
#endif /* !_HPUX && !_SCO */
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to gethostbyname() %s : %s"),
                      hostname, strerror(errno));
#if !defined (_HPUX) && !defined (_SCO)
         }
#endif
         return(INCORRECT);
      }

      /* Copy IP number to socket structure. */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((http_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                _("socket() error : %s"), strerror(errno));
      return(INCORRECT);
   }
   sin.sin_family = AF_INET;
   sin.sin_port = htons((u_short)port);

   if (sndbuf_size > 0)
   {
      if (setsockopt(http_fd, SOL_SOCKET, SO_SNDBUF,
                     (char *)&sndbuf_size, sizeof(sndbuf_size)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("setsockopt() error : %s"), strerror(errno));
      }
      hmr.sndbuf_size = sndbuf_size;
   }
   else
   {
      hmr.sndbuf_size = 0;
   }
   if (rcvbuf_size > 0)
   {
      if (setsockopt(http_fd, SOL_SOCKET, SO_RCVBUF,
                     (char *)&rcvbuf_size, sizeof(rcvbuf_size)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("setsockopt() error : %s"), strerror(errno));
      }
      hmr.rcvbuf_size = rcvbuf_size;
   }
   else
   {
      hmr.rcvbuf_size = 0;
   }
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   if (timeout_flag != OFF)
   {
      int reply = 1;

      if (setsockopt(http_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                     sizeof(reply)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
      }
# ifdef TCP_KEEPALIVE
      reply = timeout_flag;
      if (setsockopt(http_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                     sizeof(reply)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("setsockopt() TCP_KEEPALIVE error : %s"), strerror(errno));
      }
# endif
      timeout_flag = OFF;
   }
#endif

   if (connect(http_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                _("Failed to connect() to %s : %s"), hostname, strerror(errno));
      (void)close(http_fd);
      return(INCORRECT);
   }
#ifdef WITH_TRACE
   length = sprintf(msg_str, "Connected to %s", hostname);
   trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#endif

   (void)my_strncpy(hmr.hostname, hostname, MAX_REAL_HOSTNAME_LENGTH + 1);
   if (user != NULL)
   {
      (void)my_strncpy(hmr.user, user, MAX_USER_NAME_LENGTH + 1);
   }
   if (passwd != NULL)
   {
      (void)my_strncpy(hmr.passwd, passwd, MAX_USER_NAME_LENGTH + 1);
   }
   if ((user[0] != '\0') || (passwd[0] != '\0'))
   {
      if (basic_authentication() != SUCCESS)
      {
         return(INCORRECT);
      }
   }
   hmr.port = port;
   hmr.free = YES;
   hmr.http_version = 0;
#ifdef WITH_TRACE
   hmr.http_options = 0;
#endif
   hmr.bytes_buffered = 0;
   hmr.bytes_read = 0;

#ifdef WITH_SSL
   if ((ssl == YES) || (ssl == BOTH))
   {
      int  reply,
           tmp_errno;
      char *p_env,
           *p_env1;

      hmr.ssl = YES;
      SSLeay_add_ssl_algorithms();
      ssl_ctx=(SSL_CTX *)SSL_CTX_new(SSLv23_client_method());
      SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
      SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
      if ((p_env = getenv("SSL_CIPHER")) != NULL)
      {
         SSL_CTX_set_cipher_list(ssl_ctx, p_env);
      }
      else
      {
         SSL_CTX_set_cipher_list(ssl_ctx, NULL);
      }
      if (((p_env = getenv(X509_get_default_cert_file_env())) != NULL) &&
          ((p_env1 = getenv(X509_get_default_cert_dir_env())) != NULL))
      {
         SSL_CTX_load_verify_locations(ssl_ctx, p_env, p_env1);
      }
#ifdef WHEN_WE_KNOW
      if (((p_env = getenv("SSL_CRL_FILE")) != NULL) &&
          ((p_env1 = getenv("SSL_CRL_DIR")) != NULL))
      {
      }
      else
      {
      }
#endif
      SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

      ssl_con = (SSL *)SSL_new(ssl_ctx);
      SSL_set_connect_state(ssl_con);
      SSL_set_fd(ssl_con, http_fd);

      /*
       * NOTE: Because we have set SSL_MODE_AUTO_RETRY, a SSL_read() can
       *       block even when we use select(). The same thing might be true
       *       for SSL_write() but have so far not encountered this case.
       *       It might be cleaner not to set SSL_MODE_AUTO_RETRY and handle
       *       SSL_ERROR_WANT_READ error case.
       */
      if (signal(SIGALRM, sig_handler) == SIG_ERR)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("Failed to set signal handler : %s"), strerror(errno));
         return(INCORRECT);
      }


      if (sigsetjmp(env_alrm, 1) != 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("SSL_connect() timeout (%ld)"), transfer_timeout);
         timeout_flag = ON;
         return(INCORRECT);
      }
      (void)alarm(transfer_timeout);
      reply = SSL_connect(ssl_con);
      tmp_errno = errno;
      (void)alarm(0);
      if (reply <= 0)
      {
         char *ptr;

         ptr = ssl_error_msg("SSL_connect", ssl_con, reply, msg_str);
         reply = SSL_get_verify_result(ssl_con);
         if (reply == X509_V_ERR_CRL_SIGNATURE_FAILURE)
         {
            (void)strcpy(ptr,
                         _(" | Verify result: The signature of the certificate is invalid!"));
         }
         else if (reply == X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD)
              {
                  (void)strcpy(ptr,
                               _(" | Verify result: The CRL nextUpdate field contains an invalid time."));
              }
         else if (reply == X509_V_ERR_CRL_HAS_EXPIRED)
              {
                  (void)strcpy(ptr,
                               _(" | Verify result: The CRL has expired."));
              }
         else if (reply == X509_V_ERR_CERT_REVOKED)
              {
                 (void)strcpy(ptr,
                              _(" | Verify result: Certificate revoked."));
              }
         else if (reply > X509_V_OK)
              {
                 (void)sprintf(ptr, _(" | Verify result: %d"), reply);
              }
         reply = INCORRECT;
      }
      else
      {
         char             *ssl_version;
         int              length,
                          ssl_bits;
         const SSL_CIPHER *ssl_cipher;

         ssl_version = SSL_get_cipher_version(ssl_con);
         ssl_cipher = SSL_get_current_cipher(ssl_con);
         SSL_CIPHER_get_bits(ssl_cipher, &ssl_bits);
         length = strlen(msg_str);        
         (void)sprintf(&msg_str[length], "  <%s, cipher %s, %d bits>",
                       ssl_version, SSL_CIPHER_get_name(ssl_cipher),
                       ssl_bits);
         reply = SUCCESS;
      }
#ifdef WITH_SSL_READ_AHEAD
      /* This is not set because I could not detect any advantage using this. */
      SSL_set_read_ahead(ssl_con, 1);
#endif

      return(reply);
   }
   else
   {
      hmr.ssl = NO;
   }
#endif /* WITH_SSL */

   return(SUCCESS);
}


/*########################### http_version() ############################*/
int
http_version(void)
{
   return(hmr.http_version);
}


/*############################## http_get() #############################*/
int
http_get(char  *host,
         char  *path,
         char  *filename,
         off_t *content_length,
         off_t offset)
{
   int  reply;
   char range[13 + MAX_OFF_T_LENGTH + 1 + MAX_OFF_T_LENGTH + 3];

   hmr.bytes_read = 0;
   hmr.retries = 0;
   hmr.date = -1;
   if ((*content_length == 0) && (filename[0] != '\0'))
   {
      off_t end;

      if ((reply = http_head(host, path, filename, &end, NULL)) == SUCCESS)
      {
         *content_length = end;
         hmr.retries = 0;
      }
      else
      {
         return(reply);
      }
   }
   if ((offset) && (*content_length == offset))
   {
      return(NOTHING_TO_FETCH);
   }
retry_get_range:
   if ((offset == 0) || (offset < 0))
   {
      range[0] = '\0';
   }
   else
   {
      if (*content_length == 0)
      {
#if SIZEOF_OFF_T == 4
         (void)sprintf(range, "Range: bytes=%ld-\r\n", (pri_off_t)offset);
#else
         (void)sprintf(range, "Range: bytes=%lld-\r\n", (pri_off_t)offset);
#endif
      }
      else
      {
#if SIZEOF_OFF_T == 4
         (void)sprintf(range, "Range: bytes=%ld-%ld\r\n",
                       (pri_off_t)offset, (pri_off_t)*content_length);
#else
         (void)sprintf(range, "Range: bytes=%lld-%lld\r\n",
                       (pri_off_t)offset, (pri_off_t)*content_length);
#endif
      }
   }
retry_get:
   if ((reply = command(http_fd,
                        "GET %s%s%s HTTP/1.1\r\n%sUser-Agent: AFD/%s\r\n%sHost: %s\r\nAccept: *\r\n",
                        (*path != '/') ? "/" : "", path, filename, range,
                        PACKAGE_VERSION,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(&hmr.bytes_buffered)) == 200)
      {
         if (hmr.chunked == YES)
         {
            reply = CHUNKED;
         }
         else
         {
            reply = SUCCESS;
         }
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (check_connection() > INCORRECT)
                    {
                       goto retry_get;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_get", NULL,
                           _("Failed to create basic authentication."));
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_get", NULL,
                                _("Digest authentication not yet implemented."));
                   }
           }
      else if (reply == 416) /* Requested Range Not Satisfiable */
           {
              offset = 0;
              goto retry_get_range;
           }
      else if (reply == CONNECTION_REOPENED)
           {
              goto retry_get;
           }
   }

   return(reply);
}


/*############################## http_put() #############################*/
int
http_put(char *host, char *path, char *filename, off_t length, int first_file)
{
   hmr.retries = 0;
   hmr.date = -1;
   if (first_file == NO)
   {
      if (check_connection() == CONNECTION_REOPENED)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "http_put", NULL,
                   _("Reconnected."));
      }
   }
   return(command(http_fd,
#if SIZEOF_OFF_T == 4
                  "PUT %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\nContent-length: %ld\r\n%sHost: %s\r\nControl: overwrite=1\r\n",
#else
                  "PUT %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\nContent-length: %lld\r\n%sHost: %s\r\nControl: overwrite=1\r\n",
#endif
                  (*path != '/') ? "/" : "", path, filename,
                  PACKAGE_VERSION,
                  (pri_off_t)length,
                  (hmr.authorization == NULL) ? "" : hmr.authorization,
                  host));
}


/*######################### http_put_response() #########################*/
int
http_put_response(void)
{
   int reply;

   hmr.retries = 0;
   hmr.date = -1;
   hmr.content_length = 0;
retry_put_response:
   if (((reply = get_http_reply(NULL)) == 201) || (reply == 204) ||
       (reply == 200))
   {
      int total_read = 0,
          read_length;

      while (hmr.content_length > total_read)
      {
         if ((reply = read_msg(&read_length)) <= 0)
         {
            if (reply == 0)
            {
               trans_log(ERROR_SIGN,  __FILE__, __LINE__, "http_put_response", NULL,
                         _("Remote hang up. (%d %d)"),
                         hmr.content_length, total_read);
               timeout_flag = NEITHER;
            }
            else
            {
               trans_log(DEBUG_SIGN,  __FILE__, __LINE__, "http_put_response", NULL,
                         "(%d %d)", hmr.content_length, total_read);
            }
            return(INCORRECT);
         }
         else
         {
            total_read += read_length + 1; /* + 1 is the newline! */
         }
      }
      hmr.bytes_buffered = 0;
      hmr.bytes_read = 0;
      reply = SUCCESS;
   }
   else if (reply == 401)
        {
           if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
           {
              if (basic_authentication() == SUCCESS)
              {
                 if (check_connection() > INCORRECT)
                 {
                    goto retry_put_response;
                 }
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_put_response", NULL,
                        _("Failed to create basic authentication."));
           }
           else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                {
                   trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_put_response", NULL,
                             _("Digest authentication not yet implemented."));
                }
        }
   else if (reply == CONNECTION_REOPENED)
        {
           goto retry_put_response;
        }

   return(reply);
}


/*############################## http_del() #############################*/
int
http_del(char *host, char *path, char *filename)
{
   int reply;

   hmr.retries = 0;
   hmr.date = -1;
retry_del:
   if ((reply = command(http_fd,
                        "DELETE %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\n",
                        (*path != '/') ? "/" : "", path, filename,
                        PACKAGE_VERSION,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(NULL)) == 200)
      {
         reply = SUCCESS;
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (check_connection() > INCORRECT)
                    {
                       goto retry_del;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_del", NULL,
                           _("Failed to create basic authentication."));
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_del", NULL,
                                _("Digest authentication not yet implemented."));
                   }
           }
      else if (reply == CONNECTION_REOPENED)
           {
              goto retry_del;
           }
   }
   return(reply);
}


#ifdef WITH_TRACE
/*############################ http_options() ###########################*/
int
http_options(char *host, char *path)
{
   int reply;

   hmr.retries = 0;
   hmr.date = -1;
retry_options:
   if ((reply = command(http_fd,
                        "OPTIONS %s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\nAccept: *\r\n",
                        (*path == '\0') ? "*" : "", path,
                        PACKAGE_VERSION,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(NULL)) == 200)
      {
         reply = SUCCESS;
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (check_connection() > INCORRECT)
                    {
                       goto retry_options;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_options", NULL,
                           _("Failed to create basic authentication."));
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_options", NULL,
                                _("Digest authentication not yet implemented."));
                   }
           }
      else if (reply == CONNECTION_REOPENED)
           {
              goto retry_options;
           }
   }
   return(reply);
}
#endif


/*############################# http_head() #############################*/
int
http_head(char   *host,
          char   *path,
          char   *filename,
          off_t  *content_length,
          time_t *date)
{
   int reply;

   hmr.retries = 0;
   hmr.date = 0;
retry_head:
   if ((reply = command(http_fd,
                        "HEAD %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\nAccept: *\r\n",
                        (*path != '/') ? "/" : "", path, filename,
                        PACKAGE_VERSION,
                        (hmr.authorization == NULL) ? "" : hmr.authorization,
                        host)) == SUCCESS)
   {
      if ((reply = get_http_reply(NULL)) == 200)
      {
         reply = SUCCESS;
         *content_length = hmr.content_length;
         if (date != NULL)
         {
            *date = hmr.date;
         }
      }
      else if (reply == 401)
           {
              if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
              {
                 if (basic_authentication() == SUCCESS)
                 {
                    if (check_connection() > INCORRECT)
                    {
                       goto retry_head;
                    }
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_head", NULL,
                           _("Failed to create basic authentication."));
              }
              else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                   {
                      trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_head", NULL,
                                _("Digest authentication not yet implemented."));
                   }
           }
      else if (reply == CONNECTION_REOPENED)
           {
              goto retry_head;
           }
   }

   return(reply);
}


/*++++++++++++++++++++++++ basic_authentication() +++++++++++++++++++++++*/
static int
basic_authentication(void)
{
   size_t length;
   char   *dst_ptr,
          *src_ptr,
          base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
          userpasswd[MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH];

   /*
    * Let us first construct the authorization string from user name
    * and passwd:  <user>:<passwd>
    * And then encode this to using base-64 encoding.
    */
   length = sprintf(userpasswd, "%s:%s", hmr.user, hmr.passwd);
   if (hmr.authorization != NULL)
   {
      free(hmr.authorization);
   }
   if ((hmr.authorization = malloc(21 + length + (length / 3) + 2 + 1)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "basic_authentication", NULL,
                _("malloc() error : %s"), strerror(errno));
      return(INCORRECT);
   }
   dst_ptr = hmr.authorization;
   *dst_ptr = 'A'; *(dst_ptr + 1) = 'u'; *(dst_ptr + 2) = 't';
   *(dst_ptr + 3) = 'h'; *(dst_ptr + 4) = 'o'; *(dst_ptr + 5) = 'r';
   *(dst_ptr + 6) = 'i'; *(dst_ptr + 7) = 'z'; *(dst_ptr + 8) = 'a';
   *(dst_ptr + 9) = 't'; *(dst_ptr + 10) = 'i'; *(dst_ptr + 11) = 'o';
   *(dst_ptr + 12) = 'n'; *(dst_ptr + 13) = ':'; *(dst_ptr + 14) = ' ';
   *(dst_ptr + 15) = 'B'; *(dst_ptr + 16) = 'a'; *(dst_ptr + 17) = 's';
   *(dst_ptr + 18) = 'i'; *(dst_ptr + 19) = 'c'; *(dst_ptr + 20) = ' ';
   dst_ptr += 21;
   src_ptr = userpasswd;
   while (length > 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = base_64[((int)(*(src_ptr + 2))) & 0x3F];
      src_ptr += 3;
      length -= 3;
      dst_ptr += 4;
   }
   if (length == 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = '=';
      dst_ptr += 4;
   }
   else if (length == 1)
        {
           *dst_ptr = base_64[(int)(*src_ptr) >> 2];
           *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
           *(dst_ptr + 2) = '=';
           *(dst_ptr + 3) = '=';
           dst_ptr += 4;
        }
   *dst_ptr = '\r';
   *(dst_ptr + 1) = '\n';
   *(dst_ptr + 2) = '\0';

   return(SUCCESS);
}


/*############################# http_write() ############################*/
int
http_write(char *block, char *buffer, int size)
{
   char   *ptr = block;
   int    status;
   fd_set wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(http_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(http_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_write", NULL,
                     _("select() error : %s"), strerror(errno));
           return(INCORRECT);
        }
   else if (FD_ISSET(http_fd, &wset))
        {
           /*
            * When buffer is not NULL we want to sent the
            * data in ASCII-mode.
            */
           if (buffer != NULL)
           {
              register int i,
                           count = 0;

              for (i = 0; i < size; i++)
              {
                 if (*ptr == '\n')
                 {
                    buffer[count++] = '\r';
                    buffer[count++] = '\n';
                 }
                 else
                 {
                    buffer[count++] = *ptr;
                 }
                 ptr++;
              }
              size = count;
              ptr = buffer;
           }

#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((status = write(http_fd, ptr, size)) != size)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_write", NULL,
                           _("write() error (%d) : %s"),
                           status, strerror(errno));
                 return(errno);
              }
#ifdef WITH_SSL
           }
           else
           {
              if (ssl_write(ssl_con, ptr, size) != size)
              {
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, ptr, size, NULL);
#endif
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_write", NULL,
                     _("Unknown condition."));
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


/*############################# http_read() #############################*/
int
http_read(char *block, int blocksize)
{
   int bytes_read,
       status;

   if (hmr.bytes_buffered > 0)
   {
      if (hmr.bytes_buffered >= blocksize)
      {
         memcpy(block, msg_str, blocksize);
         if (hmr.bytes_buffered > blocksize)
         {
            hmr.bytes_buffered -= blocksize;
            (void)memmove(msg_str, msg_str + blocksize, hmr.bytes_buffered);
         }
         else
         {
            hmr.bytes_buffered = 0;
         }
         hmr.bytes_read = 0;
         bytes_read = blocksize;
      }
      else
      {
         memcpy(block, msg_str, hmr.bytes_buffered);
         bytes_read = hmr.bytes_buffered;
         hmr.bytes_buffered = 0;
         hmr.bytes_read = 0;
      }
      return(bytes_read);
   }

#ifdef WITH_SSL
   if ((ssl_con != NULL) && (SSL_pending(ssl_con)))
   {
      if ((bytes_read = SSL_read(ssl_con, block, blocksize)) == INCORRECT)
      {
         if ((status = SSL_get_error(ssl_con,
                                     bytes_read)) == SSL_ERROR_SYSCALL)
         {
            if (errno == ECONNRESET)
            {
               timeout_flag = CON_RESET;
            }
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                      _("SSL_read() error : %s"), strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                      _("SSL_read() error %d"), status);
         }
         return(INCORRECT);
      }
# ifdef WITH_TRACE
      trace_log(NULL, 0, BIN_R_TRACE, block, bytes_read, NULL);
# endif
   }
   else
#endif
   {
      fd_set rset;

      /* Initialise descriptor set. */
      FD_ZERO(&rset);
      FD_SET(http_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(http_fd, &rset))
           {
#ifdef WITH_SSL
              if (ssl_con == NULL)
              {
#endif
                 if ((bytes_read = read(http_fd, block, blocksize)) == -1)
                 {
                    if (errno == ECONNRESET)
                    {
                       timeout_flag = CON_RESET;
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                              _("read() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 int tmp_errno;

                 /*
                  * Remember we have set SSL_MODE_AUTO_RETRY. This
                  * means the SSL lib may do several read() calls. We
                  * just assured one read() with select(). So lets
                  * set an an alarm since we might block on subsequent
                  * calls to read(). It might be better when we reimplement
                  * this without SSL_MODE_AUTO_RETRY and handle
                  * SSL_ERROR_WANT_READ ourself.
                  */
                 if (sigsetjmp(env_alrm, 1) != 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                              _("SSL_read() timeout (%ld)"), transfer_timeout);
                    timeout_flag = ON;
                    return(INCORRECT);
                 }
                 (void)alarm(transfer_timeout);
                 bytes_read = SSL_read(ssl_con, block, blocksize);
                 tmp_errno = errno;
                 (void)alarm(0);

                 if (bytes_read == INCORRECT)
                 {
                    if ((status = SSL_get_error(ssl_con,
                                                bytes_read)) == SSL_ERROR_SYSCALL)
                    {
                       if (tmp_errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                                 _("SSL_read() error : %s"),
                                 strerror(tmp_errno));
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                                 _("SSL_read() error %d"), status);
                    }
                    return(INCORRECT);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, BIN_R_TRACE, block, bytes_read, NULL);
#endif
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }
   }

   return(bytes_read);
}


/*########################## http_chunk_read() ##########################*/
int
http_chunk_read(char **chunk, int *chunksize)
{
   int bytes_buffered,
       read_length;

   if ((hmr.bytes_buffered > 0) && (msg_str[4] == '\r') && (msg_str[5] == '\n'))
   {
      bytes_buffered = hmr.bytes_buffered;
      msg_str[4] = '\0';
      read_length = 5;
      hmr.bytes_buffered = 0;
      hmr.bytes_read = 0;
   }
   else
   {
      bytes_buffered = 0;
   }

   /* First, try read the chunk size. */
   if ((bytes_buffered) || ((bytes_buffered = read_msg(&read_length)) > 0))
   {
      int    bytes_read,
             status,
             tmp_chunksize;
      fd_set rset;

      tmp_chunksize = (int)strtol(msg_str, NULL, 16);
      if (errno == ERANGE)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", msg_str,
                   _("Failed to determine the chunk size with strtol() : %s"),
                   strerror(errno));
         return(INCORRECT);
      }
      else
      {
         if (tmp_chunksize == 0)
         {
            hmr.bytes_read = 0;
            return(HTTP_LAST_CHUNK);
         }
         tmp_chunksize += 2;
         if (tmp_chunksize > *chunksize)
         {
            if ((*chunk = realloc(*chunk, tmp_chunksize)) == NULL)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                         _("Failed to realloc() %d bytes : %s"),
                         tmp_chunksize, strerror(errno));
               return(INCORRECT);
            }
            *chunksize = tmp_chunksize;
         }
         bytes_buffered -= (read_length + 1);
         if (tmp_chunksize > bytes_buffered)
         {
            (void)memcpy(*chunk, msg_str + read_length + 1, bytes_buffered);
            hmr.bytes_read = 0;
         }
         else
         {
            (void)memcpy(*chunk, msg_str + read_length + 1, tmp_chunksize);
            hmr.bytes_read = tmp_chunksize;
            return(tmp_chunksize);
         }
      }

      FD_ZERO(&rset);
      do
      {
#ifdef WITH_SSL
         if ((ssl_con != NULL) && (SSL_pending(ssl_con)))
         {
            if ((bytes_read = SSL_read(ssl_con, (*chunk + bytes_buffered),
                                       tmp_chunksize - bytes_buffered)) == INCORRECT)
            {
               if ((status = SSL_get_error(ssl_con,
                                           bytes_read)) == SSL_ERROR_SYSCALL)
               {
                  if (errno == ECONNRESET)
                  {
                     timeout_flag = CON_RESET;
                  }
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                            _("SSL_read() error : %s"), strerror(errno));
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                            _("SSL_read() error %d"), status);
               }
               return(INCORRECT);
            }
            if (bytes_read == 0)
            {
               /* Premature end, remote side has closed connection. */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                         _("Remote side closed connection (expected: %d read: %d)"),
                         tmp_chunksize, bytes_buffered);
               return(INCORRECT);
            }
# ifdef WITH_TRACE
            trace_log(NULL, 0, BIN_R_TRACE, (*chunk + bytes_buffered),
                      bytes_read, NULL);
# endif
            bytes_buffered += bytes_read;
         }
         else
         {
#endif
            FD_SET(http_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;
   
            /* Wait for message x seconds and then continue. */
            status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

            if (status == 0)
            {
               /* Timeout has arrived. */
               timeout_flag = ON;
               return(INCORRECT);
            }
            else if (FD_ISSET(http_fd, &rset))
                 {
#ifdef WITH_SSL
                    if (ssl_con == NULL)
                    {
#endif
                       if ((bytes_read = read(http_fd, (*chunk + bytes_buffered),
                                              tmp_chunksize - bytes_buffered)) == -1)
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                    _("read() error : %s"), strerror(errno));
                          return(INCORRECT);
                       }
#ifdef WITH_SSL
                    }
                    else
                    {
                       int tmp_errno;

                       /*
                        * Remember we have set SSL_MODE_AUTO_RETRY. This
                        * means the SSL lib may do several read() calls. We
                        * just assured one read() with select(). So lets
                        * set an an alarm since we might block on subsequent
                        * calls to read(). It might be better when we
                        * reimplement this without SSL_MODE_AUTO_RETRY
                        * and handle SSL_ERROR_WANT_READ ourself.
                        */
                       if (sigsetjmp(env_alrm, 1) != 0)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                    _("SSL_read() timeout (%ld)"),
                                    transfer_timeout);
                          timeout_flag = ON;
                          return(INCORRECT);
                       }
                       (void)alarm(transfer_timeout);
                       bytes_read = SSL_read(ssl_con, (*chunk + bytes_buffered),
                                             tmp_chunksize - bytes_buffered);
                       tmp_errno = errno;
                       (void)alarm(0);

                       if (bytes_read == INCORRECT)
                       {
                          if ((status = SSL_get_error(ssl_con,
                                                      bytes_read)) == SSL_ERROR_SYSCALL)
                          {
                             if (tmp_errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                       _("SSL_read() error : %s"),
                                       strerror(tmp_errno));
                          }
                          else
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                       _("SSL_read() error %d"), status);
                          }
                          return(INCORRECT);
                       }
                    }
#endif
                    if (bytes_read == 0)
                    {
                       /* Premature end, remote side has closed connection. */
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                 _("Remote side closed connection (expected: %d read: %d)"),
                                 tmp_chunksize, bytes_buffered);
                       return(INCORRECT);
                    }
#ifdef WITH_TRACE
                    trace_log(NULL, 0, BIN_R_TRACE, (*chunk + bytes_buffered),
                              bytes_read, NULL);
#endif
                    bytes_buffered += bytes_read;
                 }
            else if (status < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                              _("select() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                              _("Unknown condition."));
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
         }
#endif
      } while (bytes_buffered < tmp_chunksize);

      if (bytes_buffered == tmp_chunksize)
      {
         bytes_buffered -= 2;
      }
   }
   else if (bytes_buffered == 0)
        {
           timeout_flag = NEITHER;
           trans_log(ERROR_SIGN,  __FILE__, __LINE__, "http_chunk_read", NULL,
                     _("Remote hang up."));
           bytes_buffered = INCORRECT;
        }

   return(bytes_buffered);
}


/*############################ http_quit() ##############################*/
void
http_quit(void)
{
   if ((hmr.authorization != NULL) && (hmr.free != NO))
   {
      free(hmr.authorization);
      hmr.authorization = NULL;
   }
   if (http_fd != -1)
   {
      if ((timeout_flag != ON) && (timeout_flag != CON_RESET))
      {
         if (shutdown(http_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "http_quit", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
      }
#ifdef WITH_SSL
      if (ssl_con != NULL)
      {
         SSL_free(ssl_con);
         ssl_con = NULL;
      }
#endif
      if (close(http_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "http_quit", NULL,
                   _("close() error : %s"), strerror(errno));
      }
      http_fd = -1;
   }
   return;
}


/*+++++++++++++++++++++++++ check_connection() ++++++++++++++++++++++++++*/
static int
check_connection(void)
{
   int connection_closed;

   if (hmr.close == YES)
   {
      connection_closed = YES;
      hmr.free = NO;
      http_quit();
      hmr.free = YES;
   }
   else
   {
      /*
       * TODO: We still need the code to check if connection is still up!
       */
      connection_closed = NO;
   }
   if (connection_closed == YES)
   {
      int status;

      if ((status = http_connect(hmr.hostname, hmr.port,
#ifdef WITH_SSL
                                 hmr.user, hmr.passwd, hmr.ssl, hmr.sndbuf_size, hmr.rcvbuf_size)) != SUCCESS)
#else
                                 hmr.user, hmr.passwd, hmr.sndbuf_size, hmr.rcvbuf_size)) != SUCCESS)
#endif
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "check_connection", msg_str,
                   _("HTTP connection to %s at port %d failed (%d)."),
                   hmr.hostname, hmr.port, status);
         return(INCORRECT);
      }
      else
      {
         return(CONNECTION_REOPENED);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ get_http_reply() ++++++++++++++++++++++++++*/
static int
get_http_reply(int *ret_bytes_buffered)
{
   int bytes_buffered,
       read_length,
       status_code = INCORRECT;

   /* First read start line. */
   hmr.bytes_buffered = 0;
   if (ret_bytes_buffered != NULL)
   {
      *ret_bytes_buffered = 0;
   }
   if ((bytes_buffered = read_msg(&read_length)) > 0)
   {
      hmr.close = NO;
      hmr.chunked = NO;
      if ((read_length > 12) &&
          (((msg_str[0] == 'H') || (msg_str[0] == 'h')) &&
           ((msg_str[1] == 'T') || (msg_str[1] == 't')) &&
           ((msg_str[2] == 'T') || (msg_str[2] == 't')) &&
           ((msg_str[3] == 'P') || (msg_str[3] == 'p')) &&
           (msg_str[4] == '/') &&
           (isdigit((int)(msg_str[5]))) && (msg_str[6] == '.') &&
           (isdigit((int)(msg_str[7]))) && (msg_str[8] == ' ') &&
           (isdigit((int)(msg_str[9]))) && (isdigit((int)(msg_str[10]))) &&
           (isdigit((int)(msg_str[11])))))
      {
         hmr.http_version = ((msg_str[5] - '0') * 10) + (msg_str[7] - '0');
         status_code = ((msg_str[9] - '0') * 100) +
                       ((msg_str[10] - '0') * 10) +
                       (msg_str[11] - '0');
         if (read_length <= MAX_HTTP_HEADER_BUFFER)
         {
            (void)memcpy(hmr.msg_header, msg_str, read_length);
            hmr.header_length = read_length;
         }
         else
         {
            (void)memcpy(hmr.msg_header, msg_str, MAX_HTTP_HEADER_BUFFER - 1);
            hmr.msg_header[MAX_HTTP_HEADER_BUFFER - 1] = '\0';
            hmr.header_length = MAX_HTTP_HEADER_BUFFER;
         }

         /*
          * Now lets read the headers and lets store that what we
          * think might be usefull later.
          */
         for (;;)
         {
            if ((bytes_buffered = read_msg(&read_length)) <= 0)
            {
               if (bytes_buffered == 0)
               {
                  trans_log(ERROR_SIGN,  __FILE__, __LINE__, "get_http_reply", NULL,
                            _("Remote hang up."));
                  timeout_flag = NEITHER;
               }
               return(INCORRECT);
            }

            /* Check if we have reached header end. */
            if  ((read_length == 1) && (msg_str[0] == '\0'))
            {
               if (status_code >= 300)
               {
                  (void)strcpy(msg_str, hmr.msg_header);
               }
               break;
            }

            /* Content-Length: */
            if ((read_length > 15) && (msg_str[14] == ':') &&
                ((msg_str[8] == 'L') || (msg_str[8] == 'l')) &&
                ((msg_str[9] == 'E') || (msg_str[9] == 'e')) &&
                ((msg_str[10] == 'N') || (msg_str[10] == 'n')) &&
                ((msg_str[11] == 'G') || (msg_str[11] == 'g')) &&
                ((msg_str[12] == 'T') || (msg_str[12] == 't')) &&
                ((msg_str[13] == 'H') || (msg_str[13] == 'h')) &&
                ((msg_str[0] == 'C') || (msg_str[0] == 'c')) &&
                ((msg_str[1] == 'O') || (msg_str[1] == 'o')) &&
                ((msg_str[2] == 'N') || (msg_str[2] == 'n')) &&
                ((msg_str[3] == 'T') || (msg_str[3] == 't')) &&
                ((msg_str[4] == 'E') || (msg_str[4] == 'e')) &&
                ((msg_str[5] == 'N') || (msg_str[5] == 'n')) &&
                ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                (msg_str[7] == '-'))
            {
               int i = 15;

               while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                      (i < read_length))
               {
                  i++;
               }
               if (i < read_length)
               {
                  int k = i;

                  while ((isdigit((int)(msg_str[k]))) && (k < read_length))
                  {
                     k++;
                  }
                  if (k > 0)
                  {
                     if (msg_str[k] != '\0')
                     {
                        msg_str[k] = '\0';
                     }
                     errno = 0;
                     hmr.content_length = (off_t)str2offt(&msg_str[i], NULL, 10);
                     if (errno != 0)
                     {
                        hmr.content_length = 0;
                     }
                  }
               }
            }
                 /* Connection: */
            else if ((read_length > 11) && (msg_str[10] == ':') &&
                     ((msg_str[0] == 'C') || (msg_str[0] == 'c')) &&
                     ((msg_str[1] == 'O') || (msg_str[1] == 'o')) &&
                     ((msg_str[2] == 'N') || (msg_str[2] == 'n')) &&
                     ((msg_str[3] == 'N') || (msg_str[3] == 'n')) &&
                     ((msg_str[4] == 'E') || (msg_str[4] == 'e')) &&
                     ((msg_str[5] == 'C') || (msg_str[5] == 'c')) &&
                     ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                     ((msg_str[7] == 'I') || (msg_str[7] == 'i')) &&
                     ((msg_str[8] == 'O') || (msg_str[8] == 'o')) &&
                     ((msg_str[9] == 'N') || (msg_str[9] == 'n')))
                 {
                    int i = 11;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if (((i + 4) < read_length) &&
                        ((msg_str[i] == 'C') || (msg_str[i] == 'c')) &&
                        ((msg_str[i + 1] == 'L') || (msg_str[i + 1] == 'l')) &&
                        ((msg_str[i + 2] == 'O') || (msg_str[i + 2] == 'o')) &&
                        ((msg_str[i + 3] == 'S') || (msg_str[i + 3] == 's')) &&
                        ((msg_str[i + 4] == 'E') || (msg_str[i + 4] == 'e')))
                    {
                       hmr.close = YES;
                    }
                 }
            else if ((read_length > 17) && (msg_str[16] == ':') &&
                     (msg_str[3] == '-') &&
                     ((msg_str[0] == 'W') || (msg_str[0] == 'w')) &&
                     ((msg_str[1] == 'W') || (msg_str[1] == 'w')) &&
                     ((msg_str[2] == 'W') || (msg_str[2] == 'w')) &&
                     ((msg_str[4] == 'A') || (msg_str[4] == 'a')) &&
                     ((msg_str[5] == 'U') || (msg_str[5] == 'u')) &&
                     ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                     ((msg_str[7] == 'H') || (msg_str[7] == 'h')) &&
                     ((msg_str[8] == 'E') || (msg_str[8] == 'e')) &&
                     ((msg_str[9] == 'N') || (msg_str[9] == 'n')) &&
                     ((msg_str[10] == 'T') || (msg_str[10] == 't')) &&
                     ((msg_str[11] == 'I') || (msg_str[11] == 'i')) &&
                     ((msg_str[12] == 'C') || (msg_str[12] == 'c')) &&
                     ((msg_str[13] == 'A') || (msg_str[13] == 'a')) &&
                     ((msg_str[14] == 'T') || (msg_str[14] == 't')) &&
                     ((msg_str[15] == 'E') || (msg_str[15] == 'e')))
                 {
                    int i = 17;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if ((i + 4) < read_length)
                    {
                       if (((msg_str[i] == 'B') || (msg_str[i] == 'b')) &&
                           ((msg_str[i + 1] == 'A') || (msg_str[i + 1] == 'a')) &&
                           ((msg_str[i + 2] == 'S') || (msg_str[i + 2] == 's')) &&
                           ((msg_str[i + 3] == 'I') || (msg_str[i + 3] == 'i')) &&
                           ((msg_str[i + 4] == 'C') || (msg_str[i + 4] == 'c')))
                       {
                          hmr.www_authenticate = WWW_AUTHENTICATE_BASIC;
                       }
                       else if (((msg_str[i] == 'D') || (msg_str[i] == 'd')) &&
                                ((msg_str[i + 1] == 'I') || (msg_str[i + 1] == 'i')) &&
                                ((msg_str[i + 2] == 'G') || (msg_str[i + 2] == 'g')) &&
                                ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
                                ((msg_str[i + 4] == 'S') || (msg_str[i + 4] == 's')) &&
                                ((msg_str[i + 5] == 'T') || (msg_str[i + 5] == 't')))
                       {
                          hmr.www_authenticate = WWW_AUTHENTICATE_DIGEST;
                       }
                    }
                 }
                 /* Transfer-encoding: */
            else if ((read_length > 18) && (msg_str[17] == ':') &&
                     (msg_str[8] == '-') &&
                     ((msg_str[0] == 'T') || (msg_str[0] == 't')) &&
                     ((msg_str[1] == 'R') || (msg_str[1] == 'r')) &&
                     ((msg_str[2] == 'A') || (msg_str[2] == 'a')) &&
                     ((msg_str[3] == 'N') || (msg_str[3] == 'n')) &&
                     ((msg_str[4] == 'S') || (msg_str[4] == 's')) &&
                     ((msg_str[5] == 'F') || (msg_str[5] == 'f')) &&
                     ((msg_str[6] == 'E') || (msg_str[6] == 'e')) &&
                     ((msg_str[7] == 'R') || (msg_str[7] == 'r')) &&
                     ((msg_str[9] == 'E') || (msg_str[9] == 'e')) &&
                     ((msg_str[10] == 'N') || (msg_str[10] == 'n')) &&
                     ((msg_str[11] == 'C') || (msg_str[11] == 'c')) &&
                     ((msg_str[12] == 'O') || (msg_str[12] == 'o')) &&
                     ((msg_str[13] == 'D') || (msg_str[13] == 'd')) &&
                     ((msg_str[14] == 'I') || (msg_str[14] == 'i')) &&
                     ((msg_str[15] == 'N') || (msg_str[15] == 'n')) &&
                     ((msg_str[16] == 'G') || (msg_str[16] == 'g')))
                 {
                    int i = 18;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if ((i + 7) < read_length)
                    {
                       if (((msg_str[i] == 'C') || (msg_str[i] == 'c')) &&
                           ((msg_str[i + 1] == 'H') || (msg_str[i + 1] == 'h')) &&
                           ((msg_str[i + 2] == 'U') || (msg_str[i + 2] == 'u')) &&
                           ((msg_str[i + 3] == 'N') || (msg_str[i + 3] == 'n')) &&
                           ((msg_str[i + 4] == 'K') || (msg_str[i + 4] == 'k')) &&
                           ((msg_str[i + 5] == 'E') || (msg_str[i + 5] == 'e')) &&
                           ((msg_str[i + 6] == 'D') || (msg_str[i + 6] == 'd')))
                       {
                          hmr.chunked = YES;
                       }
                    }
                 }
                 /* Last-modified: */
            else if ((hmr.date != -1) &&
                     (read_length > 14) && (msg_str[13] == ':') &&
                     (msg_str[4] == '-') &&
                     ((msg_str[0] == 'L') || (msg_str[0] == 'l')) &&
                     ((msg_str[1] == 'A') || (msg_str[1] == 'a')) &&
                     ((msg_str[2] == 'S') || (msg_str[2] == 's')) &&
                     ((msg_str[3] == 'T') || (msg_str[3] == 't')) &&
                     ((msg_str[5] == 'M') || (msg_str[5] == 'm')) &&
                     ((msg_str[6] == 'O') || (msg_str[6] == 'o')) &&
                     ((msg_str[7] == 'D') || (msg_str[7] == 'd')) &&
                     ((msg_str[8] == 'I') || (msg_str[8] == 'i')) &&
                     ((msg_str[9] == 'F') || (msg_str[9] == 'f')) &&
                     ((msg_str[10] == 'I') || (msg_str[10] == 'i')) &&
                     ((msg_str[11] == 'E') || (msg_str[11] == 'e')) &&
                     ((msg_str[12] == 'D') || (msg_str[12] == 'd')))
                 {
                    int i = 14;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    if (i < read_length)
                    {
                       hmr.date = datestr2unixtime(&msg_str[i]);
                    }
                 }
#ifdef WITH_TRACE
                 /* Allow: */
            else if ((read_length > 6) && (msg_str[5] == ':') &&
                     ((msg_str[0] == 'A') || (msg_str[0] == 'a')) &&
                     ((msg_str[1] == 'L') || (msg_str[1] == 'l')) &&
                     ((msg_str[2] == 'L') || (msg_str[2] == 'l')) &&
                     ((msg_str[3] == 'O') || (msg_str[3] == 'o')) &&
                     ((msg_str[4] == 'W') || (msg_str[4] == 'w')))
                 {
                    int i = 6;

                    while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                           (i < read_length))
                    {
                       i++;
                    }
                    store_http_options(i, read_length);
                 }
#endif
         }
         if ((ret_bytes_buffered != NULL) && (bytes_buffered > read_length))
         {
            *ret_bytes_buffered = bytes_buffered - read_length - 1;
            if (msg_str[0] != '\0')
            {
               (void)memmove(msg_str, msg_str + read_length + 1,
                             *ret_bytes_buffered);
            }
         }
      }
   }
   else if (bytes_buffered == 0)
        {
           if (hmr.retries == 0)
           {
              hmr.close = YES;
              if ((status_code = check_connection()) == CONNECTION_REOPENED)
              {
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, "get_http_reply", NULL,
                           _("Reconnected."));
                 hmr.retries = 1;
              }
           }
           else
           {
              timeout_flag = NEITHER;
              trans_log(ERROR_SIGN,  __FILE__, __LINE__, "get_http_reply", NULL,
                        _("Remote hang up."));
              status_code = INCORRECT;
           }
        }

#ifdef DEBUG
   if (status_code == INCORRECT)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, "get_http_reply", msg_str,
                _("Returning INCORRECT (bytes_buffered = %d)"), bytes_buffered);
   }
#endif
   return(status_code);
}


/*----------------------------- read_msg() ------------------------------*/
/*                             ------------                              */
/* Reads blockwise from http_fd until it has read one complete line.     */
/* From this line it removes the \r\n and inserts a \0 and returns the   */
/* the number of bytes it buffered. The number of bytes in the line are  */
/* returned by read_length.                                              */
/*-----------------------------------------------------------------------*/
static int
read_msg(int *read_length)
{
   static int  bytes_buffered;
   static char *read_ptr = NULL;
   int         status;
   fd_set      rset;

   *read_length = 0;
   if (hmr.bytes_read == 0)
   {
      bytes_buffered = 0;
   }
   else
   {
      (void)memmove(msg_str, read_ptr + 1, hmr.bytes_read);
      bytes_buffered = hmr.bytes_read;
      read_ptr = msg_str;
   }

   FD_ZERO(&rset);
   for (;;)
   {
      if (hmr.bytes_read <= 0)
      {
#ifdef WITH_SSL
         if ((ssl_con != NULL) && (SSL_pending(ssl_con)))
         {
            if ((hmr.bytes_read = SSL_read(ssl_con,
                                           &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
            {
               if (hmr.bytes_read == 0)
               {
                  return(0);
               }
               else
               {
                  if ((status = SSL_get_error(ssl_con,
                                              hmr.bytes_read)) == SSL_ERROR_SYSCALL)
                  {
                     if (errno == ECONNRESET)
                     {
                        timeout_flag = CON_RESET;
                     }
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                               _("SSL_read() error (after reading %d bytes) : %s"),
                               bytes_buffered, strerror(errno));
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                               _("SSL_read() error (after reading %d bytes) (%d)"),
                               bytes_buffered, status);
                  }
                  hmr.bytes_read = 0;
                  return(INCORRECT);
               }
            }
# ifdef WITH_TRACE
            trace_log(NULL, 0, BIN_CMD_R_TRACE,
                      &msg_str[bytes_buffered], hmr.bytes_read, NULL);
# endif
            read_ptr = &msg_str[bytes_buffered];
            bytes_buffered += hmr.bytes_read;
         }
         else
         {
#endif
            /* Initialise descriptor set. */
            FD_SET(http_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;

            /* Wait for message x seconds and then continue. */
            status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

            if (status == 0)
            {
               /* Timeout has arrived. */
               timeout_flag = ON;
               hmr.bytes_read = 0;
               return(INCORRECT);
            }
            else if (FD_ISSET(http_fd, &rset))
                 {
#ifdef WITH_SSL
                    if (ssl_con == NULL)
                    {
#endif
                       if ((hmr.bytes_read = read(http_fd, &msg_str[bytes_buffered],
                                                  (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                       {
                          if (hmr.bytes_read == 0)
                          {
                             return(0);
                          }
                          else
                          {
                             if (errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                       _("read() error (after reading %d bytes) : %s"),
                                       bytes_buffered, strerror(errno));
                             hmr.bytes_read = 0;
                             return(INCORRECT);
                          }
                       }
#ifdef WITH_SSL
                    }
                    else
                    {
                       if ((hmr.bytes_read = SSL_read(ssl_con,
                                                      &msg_str[bytes_buffered],
                                                      (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                       {
                          if (hmr.bytes_read == 0)
                          {
                             return(0);
                          }
                          else
                          {
                             if ((status = SSL_get_error(ssl_con,
                                                         hmr.bytes_read)) == SSL_ERROR_SYSCALL)
                             {
                                if (errno == ECONNRESET)
                                {
                                   timeout_flag = CON_RESET;
                                }
                                trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                          _("SSL_read() error (after reading %d bytes) : %s"),
                                          bytes_buffered, strerror(errno));
                             }
                             else
                             {
                                trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                          _("SSL_read() error (after reading %d bytes) (%d)"),
                                          bytes_buffered, status);
                             }
                             hmr.bytes_read = 0;
                             return(INCORRECT);
                          }
                       }
                    }
#endif
#ifdef WITH_TRACE
                    trace_log(NULL, 0, BIN_CMD_R_TRACE,
                              &msg_str[bytes_buffered], hmr.bytes_read, NULL);
#endif
                    read_ptr = &msg_str[bytes_buffered];
                    bytes_buffered += hmr.bytes_read;
                 }
            else if (status < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                              _("select() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                              _("Unknown condition."));
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
         }
#endif
      }

      /* Evaluate what we have read. */
      do
      {
         if (*read_ptr == '\n')
         {
            if (*(read_ptr - 1) == '\r')
            {
               *(read_ptr - 1) = '\0';
               hmr.bytes_read--;
            }
            else
            {
               *read_ptr = '\0';
            }
            *read_length = read_ptr - msg_str;
            return(bytes_buffered);
         }
         read_ptr++;
         hmr.bytes_read--;
      } while (hmr.bytes_read > 0);
   } /* for (;;) */
}


#ifdef WITH_TRACE
/*------------------------- store_http_options() ------------------------*/
static void
store_http_options(int i, int read_length)
{
   while (msg_str[i] != '\0')
   {
      if (((i + 4) < read_length) &&
          ((msg_str[i] == 'H') || (msg_str[i] == 'h')) &&
          ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
          ((msg_str[i + 2] == 'A') || (msg_str[i + 2] == 'a')) &&
          ((msg_str[i + 3] == 'D') || (msg_str[i + 3] == 'd')) &&
          ((msg_str[i + 4] == ',') || (msg_str[i + 4] == '\0')))
      {
         hmr.http_options |= HTTP_OPTION_HEAD;
         i += 4;
      }
      else if (((i + 3) < read_length) &&
               ((msg_str[i] == 'G') || (msg_str[i] == 'g')) &&
               ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
               ((msg_str[i + 2] == 'T') || (msg_str[i + 2] == 't')) &&
               ((msg_str[i + 3] == ',') || (msg_str[i + 3] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_GET;
              i += 3;
           }
      else if (((i + 3) < read_length) &&
               ((msg_str[i] == 'P') || (msg_str[i] == 'p')) &&
               ((msg_str[i + 1] == 'U') || (msg_str[i + 1] == 'u')) &&
               ((msg_str[i + 2] == 'T') || (msg_str[i + 2] == 't')) &&
               ((msg_str[i + 3] == ',') || (msg_str[i + 3] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_PUT;
              i += 3;
           }
      else if (((i + 4) < read_length) &&
               ((msg_str[i] == 'M') || (msg_str[i] == 'm')) &&
               ((msg_str[i + 1] == 'O') || (msg_str[i + 1] == 'o')) &&
               ((msg_str[i + 2] == 'V') || (msg_str[i + 2] == 'v')) &&
               ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
               ((msg_str[i + 4] == ',') || (msg_str[i + 4] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_MOVE;
              i += 4;
           }
      else if (((i + 6) < read_length) &&
               ((msg_str[i] == 'D') || (msg_str[i] == 'd')) &&
               ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
               ((msg_str[i + 2] == 'L') || (msg_str[i + 2] == 'l')) &&
               ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
               ((msg_str[i + 4] == 'T') || (msg_str[i + 4] == 't')) &&
               ((msg_str[i + 5] == 'E') || (msg_str[i + 5] == 'e')) &&
               ((msg_str[i + 6] == ',') || (msg_str[i + 6] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_DELETE;
              i += 6;
           }
           else /* Ignore any other options. */
           {
              while ((msg_str[i] != ',') && (msg_str[i] != '\0') &&
                     (i < read_length))
              {
                 i++;
              }
           }

      if (msg_str[i] == ',')
      {
         i++;
         while (((msg_str[i] == ' ') || (msg_str[i] == '\t')) &&
                (i < read_length))
         {
            i++;
         }
      }
   }

   return;
}
#endif


#ifdef WITH_SSL
/*+++++++++++++++++++++++++++++ sig_handler() +++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}
#endif
