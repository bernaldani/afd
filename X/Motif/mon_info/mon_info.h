/*
 *  mon_info.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __mon_info_h
#define __mon_info_h

#include "x_common_defs.h"

#define MAXARGS                  20
#define MAX_INFO_STRING_LENGTH   40
#define NO_OF_MSA_ROWS           4
#define MSA_INFO_TEXT_WIDTH_L    15
#define MSA_INFO_TEXT_WIDTH_R    18
#define MON_INFO_LENGTH          20

#define UPDATE_INTERVAL          1000
#define FILE_UPDATE_INTERVAL     4

#define INFO_IDENTIFIER          "INFO-"

struct prev_values
       {
          char   real_hostname[MAX_REAL_HOSTNAME_LENGTH];
          char   r_work_dir[MAX_PATH_LENGTH];
          int    port;
          int    poll_interval;
          int    max_connections;
          int    no_of_hosts;
          time_t last_data_time;
       };

/* Function prototypes */
extern void close_button(Widget, XtPointer, XtPointer),
            update_info(Widget);

#endif /* __mon_info_h */