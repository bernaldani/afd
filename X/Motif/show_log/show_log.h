/*
 *  show_log.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __show_log_h
#define __show_log_h

#include "x_common_defs.h"

/* What information should be displayed */
#define SHOW_INFO               1
#define SHOW_CONFIG             2
#define SHOW_WARN               4
#define SHOW_ERROR              8
#define SHOW_FATAL              16
#define SHOW_DEBUG              32
#define SHOW_ALL                63

#define MISS                    0
#define HIT                     1

#define SYSTEM_STR              "System"
#define TRANSFER_STR            "Transfer"
#define TRANS_DB_STR            "Debug"
#define MONITOR_STR             "Monitor"
#define MON_SYSTEM_STR          "Monsystem"

#define SYSTEM_LOG_TYPE         1
#define TRANSFER_LOG_TYPE       2
#define TRANS_DB_LOG_TYPE       3
#define MONITOR_LOG_TYPE        4
#define MON_SYSTEM_LOG_TYPE     5

#define MAX_LINE_COUNTER_DIGITS 6
#define LOG_START_TIMEOUT       100
#define LOG_TIMEOUT             1000
#define FALLING_SAND_SPEED      50

/* Function prototypes */
extern int  log_filter(char *, char *);
extern void check_log(Widget),
            toggled(Widget, XtPointer, XtPointer),
            toggled_jobs(Widget, XtPointer, XtPointer),
            close_button(Widget, XtPointer, XtPointer),
            update_button(Widget, XtPointer, XtPointer),
#ifdef _WITH_SCROLLBAR
            slider_moved(Widget, XtPointer, XtPointer),
#endif
            auto_scroll_switch(Widget, XtPointer, XtPointer),
            init_text(void);

#endif /* __show_log_h */