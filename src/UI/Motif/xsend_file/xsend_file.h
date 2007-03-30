/*
 *  xsend_file.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005, 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __xsend_file_h
#define __xsend_file_h

#include "x_common_defs.h"

/* Definitions for sending files. */
#define SET_ASCII               'A'
#define SET_BIN                 'I'
#define SET_DOS                 'D'
#define SET_LOCK_DOT            4
#define SET_LOCK_OFF            5
#define SET_LOCK_DOT_VMS        6
#define SET_LOCK_PREFIX         7
#define SET_ACTIVE              8
#define SET_PASSIVE             9
#define HOSTNAME_NO_ENTER       20
#define HOSTNAME_ENTER          21
#define USER_NO_ENTER           22
#define USER_ENTER              23
#define PASSWORD_NO_ENTER       24
#define PASSWORD_ENTER          25
#define TARGET_DIR_NO_ENTER     26
#define TARGET_DIR_ENTER        27
#define PORT_NO_ENTER           28
#define PORT_ENTER              29
#define TIMEOUT_NO_ENTER        30
#define TIMEOUT_ENTER           31
#define PREFIX_NO_ENTER         32
#define PREFIX_ENTER            33
#define PROXY_NO_ENTER          34
#define PROXY_ENTER             35

#define MAX_TIMEOUT_DIGITS      4       /* Maximum number of digits for  */
                                        /* the timeout field.            */
#define MAX_PORT_DIGITS         5       /* Maximum number of digits for  */
                                        /* the port field.               */

#define SEND_BUTTON             1
#define STOP_BUTTON             2

/* Structure holding all data of for sending files. */
struct send_data
       {
          char        hostname[MAX_FILENAME_LENGTH];
          char        proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          char        smtp_server[MAX_FILENAME_LENGTH];
          char        user[MAX_USER_NAME_LENGTH + 1];
          char        target_dir[MAX_PATH_LENGTH];
          char        prefix[MAX_FILENAME_LENGTH];
          char        subject[MAX_PATH_LENGTH];
          char        create_target_dir;
          char        mode_flag;      /* FTP passive or active mode. */
          char        attach_file_flag;
          XT_PTR_TYPE lock;           /* DOT, DOT_VMS, OFF, etc.     */
          XT_PTR_TYPE transfer_mode;
          XT_PTR_TYPE protocol;
          int         port;
          int         debug;
          time_t      timeout;
          char        *password;
       };

/* Function prototypes */
extern void attach_file_toggle(Widget, XtPointer, XtPointer),
            close_button(Widget, XtPointer, XtPointer),
            create_target_toggle(Widget, XtPointer, XtPointer),
            create_url_file(void),
            debug_toggle(Widget, XtPointer, XtPointer),
            enter_passwd(Widget, XtPointer, XtPointer),
            active_passive_radio(Widget, XtPointer, XtPointer),
            lock_radio(Widget, XtPointer, XtPointer),
            mode_radio(Widget, XtPointer, XtPointer),
            extended_toggle(Widget, XtPointer, XtPointer),
            protocol_toggled(Widget, XtPointer, XtPointer),
            send_button(Widget, XtPointer, XtPointer),
            send_file(void),
            send_save_input(Widget, XtPointer, XtPointer);

#endif /* __xsend_file_h */
