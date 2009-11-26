/*
 *  smtpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __smtpdefs_h
#define __smtpdefs_h

#define SMTP_HOST_NAME          "localhost"
#define DEFAULT_SMTP_PORT       25

#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH     4096
#endif
#define MAX_CONTENT_TYPE_LENGTH 25

/* Most newer SMTP servers report there capabilities when issung EHLO. */
struct smtp_server_capabilities
       {
          char auth_login;
          char auth_plain;
       };

/* Function prototypes. */
extern int  encode_base64(unsigned char *, int, unsigned char *),
            smtp_auth(unsigned char, char *, char *),
            smtp_connect(char *, int),
            smtp_helo(char *),
            smtp_ehlo(char *),
            smtp_noop(void),
            smtp_user(char *),
            smtp_rcpt(char *),
            smtp_open(void),
            smtp_write(char *, char *, int),
            smtp_write_iso8859(char *, char *, int),
            smtp_close(void),
            smtp_quit(void);
extern void get_content_type(char *, char *);

#endif /* __smtpdefs_h */
