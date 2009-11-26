/*
 *  mouse_handler.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mouse_handler - handles all mouse- AND key events of the mon_ctrl
 **                   dialog
 **
 ** SYNOPSIS
 **   void mon_focus(Widget w, XtPointer client_data, XEvent *event)
 **   void mon_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void popup_mon_menu_cb(Widget w, XtPointer client_data, XEvent *event)
 **   void save_mon_setup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void mon_popup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void mon_control_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_font_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_row_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_style_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_mon_history_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.09.1998 H.Kiehl Created
 **   10.09.2000 H.Kiehl Addition of history logs.
 **   10.07.2003 H.Kiehl Added support for ssh.
 **   21.10.2003 H.Kiehl Log everything done by mon_ctrl dialog.
 **   27.06.2004 H.Kiehl Added error history.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fprintf(), stderr                      */
#include <string.h>            /* strcpy(), strlen()                     */
#include <stdlib.h>            /* atoi(), exit()                         */
#include <sys/types.h>
#include <sys/wait.h>          /* waitpid()                              */
#include <fcntl.h>
#include <unistd.h>            /* fork()                                 */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include "mshow_log.h"
#include "mon_ctrl.h"
#include "permission.h"

/* External global variables. */
extern Display                 *display;
extern Widget                  fw[],
                               rw[],
                               hlw[],
                               tw[],
                               lsw[];
extern Window                  button_window,
                               label_window,
                               line_window;
extern Pixmap                  button_pixmap,
                               label_pixmap,
                               line_pixmap;
extern XFontStruct             *font_struct;
extern GC                      letter_gc,
                               normal_letter_gc,
                               locked_letter_gc,
                               color_letter_gc,
                               red_color_letter_gc,
                               red_error_letter_gc,
                               default_bg_gc,
                               normal_bg_gc,
                               locked_bg_gc,
                               label_bg_gc,
                               tr_bar_gc,
                               color_gc,
                               black_line_gc,
                               white_line_gc,
                               led_gc;
extern int                     depth,
                               no_of_active_process,
                               no_of_afds,
                               no_of_jobs_selected,
                               line_length,
                               line_height,
                               x_offset_proc,
                               x_offset_ec,
                               x_offset_eh,
                               no_selected,
                               no_selected_static,
                               no_of_rows,
                               no_of_rows_set,
                               his_log_set,
                               window_width;
extern unsigned int            glyph_width;
extern XT_PTR_TYPE             current_font,
                               current_his_log,
                               current_row,
                               current_style;
extern unsigned long           color_pool[];
extern float                   max_bar_length;
extern char                    *p_work_dir,
                               profile[],
                               *ping_cmd,
                               *ptr_ping_cmd,
                               *traceroute_cmd,
                               *ptr_traceroute_cmd,
                               line_style,
                               fake_user[],
                               font_name[],
                               user[],
                               username[];
extern struct mon_line         *connect_data;
extern struct mon_status_area  *msa;
extern struct mon_control_perm mcp;
extern struct apps_list        *apps_list;

/* Local global variables. */
static int                     in_window = NO;


/*############################ mon_focus() ##############################*/
void
mon_focus(Widget      w,
          XtPointer   client_data,
          XEvent      *event)
{
   if (event->xany.type == EnterNotify)
   {
      in_window = YES;
   }
   if (event->xany.type == LeaveNotify)
   {
      in_window = NO;
   }

   return;
}


/*############################ mon_input() ##############################*/
void
mon_input(Widget      w,
          XtPointer   client_data,
          XEvent      *event)
{
   int        select_no;
   static int last_motion_pos = -1;

   /* Handle any motion event. */
   if ((event->xany.type == MotionNotify) && (in_window == YES))
   {
      select_no = (event->xbutton.y / line_height) +
                  ((event->xbutton.x / line_length) * no_of_rows);

      if ((select_no < no_of_afds) && (last_motion_pos != select_no))
      {
         if (event->xkey.state & ControlMask)
         {
            if (connect_data[select_no].inverse == STATIC)
            {
               connect_data[select_no].inverse = OFF;
               no_selected_static--;
            }
            else
            {
               connect_data[select_no].inverse = STATIC;
               no_selected_static++;
            }

            draw_line_status(select_no, select_no);
            XFlush(display);
         }
         else if (event->xkey.state & ShiftMask)
              {
                 if (connect_data[select_no].inverse == ON)
                 {
                    connect_data[select_no].inverse = OFF;
                    no_selected--;
                 }
                 else if (connect_data[select_no].inverse == STATIC)
                      {
                         connect_data[select_no].inverse = OFF;
                         no_selected_static--;
                      }
                      else
                      {
                         connect_data[select_no].inverse = ON;
                         no_selected++;
                      }

                 draw_line_status(select_no, 1);
                 XFlush(display);
              }
      }
      last_motion_pos = select_no;

      return;
   }

   /* Handle any button press event. */
   if (event->xbutton.button == 1)
   {
      select_no = (event->xbutton.y / line_height) +
                  ((event->xbutton.x / line_length) * no_of_rows);

      /* Make sure that this field does contain a channel. */
      if (select_no < no_of_afds)
      {
         if (((event->xkey.state & Mod1Mask) ||
             (event->xkey.state & Mod4Mask)) &&
             (event->xany.type == ButtonPress))
         {
            int    gotcha = NO,
                   i;
            Window window_id;

            for (i = 0; i < no_of_active_process; i++)
            {
               if ((apps_list[i].position == select_no) &&
                   (strcmp(apps_list[i].progname, MON_INFO) == 0))
               {
                  if ((window_id = get_window_id(apps_list[i].pid,
                                                 MON_CTRL)) != 0L)
                  {
                     gotcha = YES;
                  }
                  break;
               }
            }
            if (gotcha == NO)
            {
               char *args[8],
                    progname[MAX_PATH_LENGTH];

               args[0] = progname;
               args[1] = WORK_DIR_ID;
               args[2] = p_work_dir;
               args[3] = "-f";
               args[4] = font_name;
               args[5] = "-a";
               args[6] = msa[select_no].afd_alias;
               args[7] = NULL;
               (void)strcpy(progname, MON_INFO);

               make_xprocess(progname, progname, args, select_no);
            }
            else
            {
               XRaiseWindow(display, window_id);
               XSetInputFocus(display, window_id, RevertToParent,
                              CurrentTime);
            }
         }
         else if (event->xany.type == ButtonPress)
              {
                 if (event->xkey.state & ControlMask)
                 {
                    if (connect_data[select_no].inverse == STATIC)
                    {
                       connect_data[select_no].inverse = OFF;
                       no_selected_static--;
                    }
                    else
                    {
                       connect_data[select_no].inverse = STATIC;
                       no_selected_static++;
                    }

                    draw_line_status(select_no, 1);
                    XFlush(display);
                 }
                 else if (event->xkey.state & ShiftMask)
                      {
                         if (connect_data[select_no].inverse == ON)
                         {
                            connect_data[select_no].inverse = OFF;
                            no_selected--;
                         }
                         else if (connect_data[select_no].inverse == STATIC)
                              {
                                 connect_data[select_no].inverse = OFF;
                                 no_selected_static--;
                              }
                              else
                              {
                                 connect_data[select_no].inverse = ON;
                                 no_selected++;
                              }

                         draw_line_status(select_no, 1);
                         XFlush(display);
                      }
                 else if (line_style != BARS_ONLY)
                      {
                         int x_offset,
                             y_offset;

                          x_offset = event->xbutton.x -
                                     ((event->xbutton.x / line_length) *
                                     line_length);
                          y_offset = event->xbutton.y -
                                     ((event->xbutton.y / line_height) *
                                     line_height);

#ifdef _DEBUG
                         (void)fprintf(stderr,
                                       "x_offset=%d y_offset=%d EC:%d-%d EH:%d-%d Y:%d-%d\n",
                                       x_offset, y_offset, x_offset_ec,
                                       (x_offset_ec + (2 * glyph_width)),
                                       x_offset_eh,
                                       (x_offset_eh + (2 * glyph_width)),
                                       SPACE_ABOVE_LINE,
                                       (line_height - SPACE_BELOW_LINE));
#endif
                         if ((((x_offset > x_offset_ec) &&
                               (x_offset < (x_offset_ec + (2 * glyph_width))) &&
                               (msa[select_no].ec > 0)) ||
                              ((x_offset > x_offset_eh) &&
                               (x_offset < (x_offset_eh + (2 * glyph_width))) &&
                               (msa[select_no].host_error_counter > 0))) &&
                             ((y_offset > SPACE_ABOVE_LINE) &&
                              (y_offset < (line_height - SPACE_BELOW_LINE))))
                         {
                            popup_error_history(event->xbutton.x_root,
                                                event->xbutton.y_root,
                                                select_no);
                         }
                         else
                         {
                            destroy_error_history();
                         }
                      }
                      else
                      {
                         destroy_error_history();
                      }

                 last_motion_pos = select_no;
              }
#ifdef _DEBUG
         (void)fprintf(stderr, "input(): no_selected = %d    select_no = %d\n",
                       no_selected, select_no);
         (void)fprintf(stderr, "input(): xbutton.x     = %d\n",
                       event->xbutton.x);
         (void)fprintf(stderr, "input(): xbutton.y     = %d\n",
                       event->xbutton.y);
#endif
      }
   }

   return;
}


/*########################## popup_mon_menu_cb() ########################*/
void
popup_mon_menu_cb(Widget      w,
                  XtPointer   client_data,
                  XEvent      *event)
{
   Widget popup = (Widget)client_data;

   if ((event->xany.type != ButtonPress) ||
       (event->xbutton.button != 3) ||
       (event->xkey.state & ControlMask))
   {
      return;
   }

   /* Position the menu where the event occurred. */
   XmMenuPosition(popup, (XButtonPressedEvent *) (event));
   XtManageChild(popup);

   return;
}


/*######################## save_mon_setup_cb() ##########################*/
void
save_mon_setup_cb(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   write_setup(-1, -1, his_log_set, NULL, 0, 0);

   return;
}


/*########################### mon_popup_cb() ############################*/
void
mon_popup_cb(Widget    w,
             XtPointer client_data,
             XtPointer call_data)
{
   int         i,
#ifdef _DEBUG
               j,
#endif
               k;
   XT_PTR_TYPE sel_typ = (XT_PTR_TYPE)client_data;
   char        host_err_no[1025],
               progname[MAX_PROCNAME_LENGTH + 1],
               **hosts = NULL,
               **args = NULL,
               log_typ[30],
               display_error,
       	       err_msg[1025 + 100];
   size_t      new_size = (no_of_afds + 8) * sizeof(char *);

   if ((no_selected == 0) && (no_selected_static == 0) &&
       ((sel_typ == MON_RETRY_SEL) || (sel_typ == MON_SWITCH_SEL) ||
        (sel_typ == MON_INFO_SEL) || (sel_typ == PING_SEL) ||
        (sel_typ == TRACEROUTE_SEL) || (sel_typ == VIEW_FILE_LOAD_SEL) ||
        (sel_typ == VIEW_KBYTE_LOAD_SEL) ||
        (sel_typ == VIEW_CONNECTION_LOAD_SEL) ||
        (sel_typ == VIEW_TRANSFER_LOAD_SEL)))
   {
      (void)xrec(INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   RT_ARRAY(hosts, no_of_afds, (MAX_AFDNAME_LENGTH + 1), char);
   if ((args = malloc(new_size)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   switch (sel_typ)
   {
      case MON_RETRY_SEL:
      case MON_SWITCH_SEL:
      case MON_DISABLE_SEL:
         break;

      case PING_SEL : /* Ping test. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = ping_cmd;
         args[6] = NULL;
         (void)strcpy(progname, SHOW_CMD);
         break;

      case TRACEROUTE_SEL : /* Traceroute test. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = traceroute_cmd;
         args[6] = NULL;
         (void)strcpy(progname, SHOW_CMD);
         break;

      case MON_INFO_SEL : /* Information. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-a";
         args[7] = NULL;
         (void)strcpy(progname, MON_INFO);
         break;

      case MON_SYS_LOG_SEL : /* System Log. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         args[7] = NULL;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, MON_SYSTEM_STR);
	 make_xprocess(progname, progname, args, -1);
	 return;

      case MON_LOG_SEL : /* Monitor Log. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-l";
         args[6] = log_typ;
         (void)strcpy(progname, SHOW_LOG);
         break;

      case VIEW_FILE_LOAD_SEL : /* File Load. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_FILE_LOAD);
	 make_xprocess(progname, progname, args, -1);
	 return;

      case VIEW_KBYTE_LOAD_SEL : /* KByte Load. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_KBYTE_LOAD);
	 make_xprocess(progname, progname, args, -1);
	 return;

      case VIEW_CONNECTION_LOAD_SEL : /* Connection Load. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_CONNECTION_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

      case VIEW_TRANSFER_LOAD_SEL : /* Active Transfers Load. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_TRANSFER_LOAD);
         make_xprocess(progname, progname, args, -1);
         return;

      case EXIT_SEL  : /* Exit. */
         XFreeFont(display, font_struct);
         font_struct = NULL;
         XFreeGC(display, letter_gc);
         XFreeGC(display, normal_letter_gc);
         XFreeGC(display, locked_letter_gc);
         XFreeGC(display, color_letter_gc);
         XFreeGC(display, default_bg_gc);
         XFreeGC(display, normal_bg_gc);
         XFreeGC(display, locked_bg_gc);
         XFreeGC(display, label_bg_gc);
         XFreeGC(display, tr_bar_gc);
         XFreeGC(display, color_gc);
         XFreeGC(display, black_line_gc);
         XFreeGC(display, white_line_gc);
         XFreeGC(display, led_gc);

         /* Free all the memory from the permission stuff. */
         if (mcp.mon_ctrl_list != NULL)
         {
            FREE_RT_ARRAY(mcp.mon_ctrl_list);
         }
         if (mcp.info_list != NULL)
         {
            FREE_RT_ARRAY(mcp.info_list);
         }
         if (mcp.retry_list != NULL)
         {
            FREE_RT_ARRAY(mcp.retry_list);
         }
         if (mcp.switch_list != NULL)
         {
            FREE_RT_ARRAY(mcp.switch_list);
         }
         if (mcp.disable_list != NULL)
         {
            FREE_RT_ARRAY(mcp.disable_list);
         }
         if (mcp.show_slog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_slog_list);
         }
         if (mcp.show_elog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_elog_list);
         }
         if (mcp.show_rlog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_rlog_list);
         }
         if (mcp.show_tlog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_tlog_list);
         }
         if (mcp.show_ilog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_ilog_list);
         }
         if (mcp.show_olog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_olog_list);
         }
         if (mcp.show_dlog_list != NULL)
         {
            FREE_RT_ARRAY(mcp.show_dlog_list);
         }
         if (mcp.afd_load_list != NULL)
         {
            FREE_RT_ARRAY(mcp.afd_load_list);
         }
         if (mcp.edit_hc_list != NULL)
         {
            FREE_RT_ARRAY(mcp.edit_hc_list);
         }
         free(connect_data);
         free(args);
         FREE_RT_ARRAY(hosts);
         exit(SUCCESS);

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible item selection (%d).", sel_typ);
#else
         (void)xrec(WARN_DIALOG, "Impossible item selection (%ld).", sel_typ);
#endif
         free(args);
         FREE_RT_ARRAY(hosts)
         return;
    }
#ifdef _DEBUG
    (void)fprintf(stderr, "Selected %d AFD's (", no_selected);
    for (i = j = 0; i < no_of_afds; i++)
    {
       if (connect_data[i].inverse > OFF)
       {
	  if (j++ < (no_selected - 1))
	  {
             (void)fprintf(stderr, "%d, ", i);
          }
	  else
	  {
             j = i;
          }
       }
    }
    if (no_selected > 0)
    {
       (void)fprintf(stderr, "%d)\n", j);
    }
    else
    {
       (void)fprintf(stderr, "none)\n");
    }
#endif

   /* Set each host. */
   k = display_error = 0;
   for (i = 0; i < no_of_afds; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         switch (sel_typ)
         {
            case MON_RETRY_SEL : /* Retry to connect to remote AFD only if */
                                 /* we are currently disconnected from it. */
               if (check_host_permissions(msa[i].afd_alias,
                                          mcp.retry_list,
                                          mcp.retry) == SUCCESS)
               {
                  if ((msa[i].connect_status == DISCONNECTED) ||
                      (msa[i].connect_status == ERROR_ID))
                  {
                     int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     int  readfd;
#endif
                     char retry_fifo[MAX_PATH_LENGTH];

                     (void)sprintf(retry_fifo, "%s%s%s%d",
                                   p_work_dir, FIFO_DIR,
                                   RETRY_MON_FIFO, i);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(retry_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(retry_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to open() %s : %s (%s %d)",
                                   retry_fifo, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     else
                     {
                        if (write(fd, &i, sizeof(int)) != sizeof(int))
                        {
                           (void)xrec(ERROR_DIALOG,
                                      "Failed to write() to %s : %s (%s %d)",
                                      retry_fifo, strerror(errno),
                                      __FILE__, __LINE__);
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Failed to close() FIFO %s : %s",
                                      retry_fifo, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Failed to close() FIFO %s : %s",
                                      retry_fifo, strerror(errno));
                        }
                     }
                  }
               }
               else
               {
                  (void)xrec(INFO_DIALOG,
                             "You do not have the permission to retry connection to %s",
                             msa[i].afd_alias);
               }
               break;

            case MON_SWITCH_SEL: /* Switch to another AFD. */
               if (check_host_permissions(msa[i].afd_alias,
                                          mcp.switch_list,
                                          mcp.switch_afd) == SUCCESS)
               {
                  if (msa[i].afd_switching != NO_SWITCHING)
                  {
                     if (msa[i].afd_toggle == (HOST_ONE - 1))
                     {
                        msa[i].afd_toggle = (HOST_TWO - 1);
                     }
                     else
                     {
                        msa[i].afd_toggle = (HOST_ONE - 1);
                     }
                     mconfig_log(SYS_LOG, CONFIG_SIGN,
                                 "SWITCHING %s", msa[i].afd_alias);
                  }
               }
               else
               {
                  (void)xrec(INFO_DIALOG,
                             "You do not have the permission to switch %s",
                             msa[i].afd_alias);
               }
               break;

            case MON_DISABLE_SEL : /* Enable/Disable AFD. */
               if (check_host_permissions(msa[i].afd_alias,
                                          mcp.disable_list,
                                          mcp.disable) == SUCCESS)
               {
                  if (msa[i].connect_status == DISABLED)
                  {
                     int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     int  readfd;
#endif
                     char mon_cmd_fifo[MAX_PATH_LENGTH];

                     (void)sprintf(mon_cmd_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, MON_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(mon_cmd_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to open() %s : %s (%s %d)",
                                   mon_cmd_fifo, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     else
                     {
                        char cmd[1 + SIZEOF_INT];

                        cmd[0] = ENABLE_MON;
                        (void)memcpy(&cmd[1], &i, SIZEOF_INT);
                        if (write(fd, cmd, (1 + SIZEOF_INT)) != (1 + SIZEOF_INT))
                        {
                           (void)xrec(ERROR_DIALOG,
                                      "Failed to write() to %s : %s (%s %d)",
                                      mon_cmd_fifo, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        else
                        {
                           mconfig_log(SYS_LOG, CONFIG_SIGN,
                                       "ENABLED monitoring for AFD %s",
                                       msa[i].afd_alias);
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Failed to close() FIFO %s : %s",
                                      mon_cmd_fifo, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Failed to close() FIFO %s : %s",
                                      mon_cmd_fifo, strerror(errno));
                        }
                     }
                  }
                  else
                  {
                     if (xrec(QUESTION_DIALOG,
                              "Are you shure that you want to disable %s\nThis AFD will then not be monitored.",
                              msa[i].afd_alias) == YES)
                     {
                        int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        int  readfd;
#endif
                        char mon_cmd_fifo[MAX_PATH_LENGTH];

                        (void)sprintf(mon_cmd_fifo, "%s%s%s",
                                      p_work_dir, FIFO_DIR, MON_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (open_fifo_rw(mon_cmd_fifo, &readfd, &fd) == -1)
#else
                        if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
                        {
                           (void)xrec(ERROR_DIALOG,
                                      "Failed to open() %s : %s (%s %d)",
                                      mon_cmd_fifo, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        else
                        {
                           char cmd[1 + SIZEOF_INT];

                           cmd[0] = DISABLE_MON;
                           (void)memcpy(&cmd[1], &i, SIZEOF_INT);
                           if (write(fd, cmd, (1 + SIZEOF_INT)) != (1 + SIZEOF_INT))
                           {
                              (void)xrec(ERROR_DIALOG,
                                         "Failed to write() to %s : %s (%s %d)",
                                         mon_cmd_fifo, strerror(errno),
                                         __FILE__, __LINE__);
                           }
                           else
                           {
                              mconfig_log(SYS_LOG, CONFIG_SIGN,
                                          "DISABLED monitoring for AFD %s",
                                          msa[i].afd_alias);
                           }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           if (close(readfd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO %s : %s",
                                         mon_cmd_fifo, strerror(errno));
                           }
#endif
                           if (close(fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO %s : %s",
                                         mon_cmd_fifo, strerror(errno));
                           }
                        }
                     }
                  }
               }
               else
               {
                  (void)xrec(INFO_DIALOG,
                             "You do not have the permission to disable %s",
                             msa[i].afd_alias);
               }
               break;

            case MON_LOG_SEL : /* Monitor Log. */
               (void)strcpy(hosts[k], msa[i].afd_alias);
               args[k + 7] = hosts[k];
               k++;
               break;

            case PING_SEL  :   /* Show ping test. */
               (void)sprintf(ptr_ping_cmd, "%s %s\"",
                             msa[i].hostname[(int)msa[i].afd_toggle],
                             msa[i].afd_alias);
               make_xprocess(progname, progname, args, i);
               break;

            case TRACEROUTE_SEL  :   /* Show traceroute test. */
               (void)sprintf(ptr_traceroute_cmd, "%s %s\"",
                             msa[i].hostname[(int)msa[i].afd_toggle],
                             msa[i].afd_alias);
               make_xprocess(progname, progname, args, i);
               break;

            case MON_INFO_SEL : /* Show information for this AFD. */
               {
                  int    gotcha = NO,
                         ii;
                  Window window_id;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == i) &&
                         (strcmp(apps_list[ii].progname, MON_INFO) == 0))
                     {
                        if ((window_id = get_window_id(apps_list[ii].pid,
                                                       MON_CTRL)) != 0L)
                        {
                           gotcha = YES;
                        }
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[6] = msa[i].afd_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     XRaiseWindow(display, window_id);
                     XSetInputFocus(display, window_id, RevertToParent,
                                    CurrentTime);
                  }
               }
               break;

            default :
               (void)xrec(WARN_DIALOG,
                          "Impossible selection! NOOO this can't be true! (%s %d)",
                          __FILE__, __LINE__);
               free(args);
               FREE_RT_ARRAY(hosts)
               return;
         }
      }
   } /* for (i = 0; i < no_of_afds; i++) */

   if (sel_typ == MON_LOG_SEL)
   {
      (void)strcpy(log_typ, MONITOR_STR);
      args[k + 7] = NULL;
      make_xprocess(progname, progname, args, -1);
   }

   /* Memory for arg list stuff no longer needed. */
   free(args);
   FREE_RT_ARRAY(hosts)

   if (display_error > 0)
   {
      if (display_error > 1)
      {
         (void)sprintf(err_msg, "Operation for hosts %s not done.",
                       host_err_no);
      }
      else
      {
         (void)sprintf(err_msg, "Operation for host %s not done.",
                       host_err_no);
      }
   }

   for (i = 0; i < no_of_afds; i++)
   {
      if (connect_data[i].inverse == ON)
      {
         connect_data[i].inverse = OFF;
         draw_line_status(i, -1);
      }
   }

   /* Make sure that all changes are shown. */
   XFlush(display);

   no_selected = 0;

   return;
}

#define _WITH_MINUS_N_OPTION_
/*######################## start_remote_prog() ##########################*/
void
start_remote_prog(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   int         arg_count,
               display_offset,
               i;
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;
   char        **args,
               progname[MAX_PATH_LENGTH];

   if ((no_selected == 0) && (no_selected_static == 0))
   {
      (void)xrec(INFO_DIALOG,
                 "You must first select an AFD!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   if ((args = malloc(15 * sizeof(char *))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      return;
   }

   for (i = 0; i < no_of_afds; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         /*
          * Arglist is build up as follows:
#ifdef _WITH_MINUS_N_OPTION_
          *  rsh -n  -l <username> host rafdd_cmd <display> <AFD workdir> cmd+args
          *  [0] [1] [2]   [3]     [4]     [5]      [6]          [7]
#else
          *  rsh -l <username> host rafdd_cmd <display> <AFD workdir> cmd+args
          *  [0] [1]   [2]     [3]     [4]      [5]          [6]
#endif
          *  ssh -X  -l <username> host rafdd_cmd_ssh <AFD workdir> cmd+args
          *  [0] [1] [2]   [3]     [4]       [5]          [6]
          *
          *  ssh -X  -C  -l <username> host rafdd_cmd_ssh <AFD workdir> cmd+args
          *  [0] [1] [2] [3]   [4]     [5]       [6]          [7]
          */
         args[0] = msa[i].rcmd;
         if (args[0][0] == 's')
         {
            if (msa[i].options & MINUS_Y_FLAG)
            {
               args[1] = "-Y";
            }
            else
            {
               args[1] = "-X";
            }
            if (msa[i].options & COMPRESS_FLAG)
            {
               args[2] = "-C";
               arg_count = 3;
            }
            else
            {
               arg_count = 2;
            }
            args[arg_count] = "-l";
            args[arg_count + 3] = progname;
            display_offset = 0;
         }
         else
         {
            char local_display[90];

            (void)strcpy(local_display, XDisplayName(NULL));
            if (local_display[0] == ':')
            {
               char hostname[90];
      
               if (gethostname(hostname, 80) == 0)
               {
                  (void)strcat(hostname, local_display);
                  (void)strcpy(local_display, hostname);
               }
            }
#ifdef _WITH_MINUS_N_OPTION_
            args[1] = "-n";
            arg_count = 2;
#else
            arg_count = 1;
#endif
            args[arg_count] = "-l";
            args[arg_count + 3] = progname;
            args[arg_count + 4] = local_display;
            display_offset = 1;
         }

         switch (item_no)
         {
            case AFD_CTRL_SEL : /* Remote afd_ctrl. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = AFD_CTRL;
                  args[arg_count + display_offset + 6] = "-f";
                  args[arg_count + display_offset + 7] = font_name;
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 8] = "-u";
                     args[arg_count + display_offset + 9] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 8] = "-p";
                     args[arg_count + display_offset + offset + 9] = profile;
                     args[arg_count + display_offset + offset + 10] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 8] = NULL;
                  }
               }
               break;

            case S_LOG_SEL : /* Remote System Log. */
               args[arg_count + display_offset + 5] = SHOW_LOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               args[arg_count + display_offset + 8] = "-l";
               args[arg_count + display_offset + 9] = SYSTEM_STR;
               args[arg_count + display_offset + 10] = NULL;
               break;

            case E_LOG_SEL : /* Remote Event Log. */
               args[arg_count + display_offset + 5] = SHOW_ELOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               if (fake_user[0] == '\0')
               {
                  args[arg_count + display_offset + 8] = NULL;
               }
               else
               {
                  args[arg_count + display_offset + 8] = "-u";
                  args[arg_count + display_offset + 9] = fake_user;
                  args[arg_count + display_offset + 10] = NULL;
               }
               break;

            case R_LOG_SEL : /* Remote Receive Log. */
               args[arg_count + display_offset + 5] = SHOW_LOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               args[arg_count + display_offset + 8] = "-l";
               args[arg_count + display_offset + 9] = RECEIVE_STR;
               args[arg_count + display_offset + 10] = NULL;
               break;

            case T_LOG_SEL : /* Remote Transfer Log. */
               args[arg_count + display_offset + 5] = SHOW_LOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               args[arg_count + display_offset + 8] = "-l";
               args[arg_count + display_offset + 9] = TRANSFER_STR;
               args[arg_count + display_offset + 10] = NULL;
               break;

            case I_LOG_SEL : /* Remote Input Log. */
               args[arg_count + display_offset + 5] = SHOW_ILOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               if (fake_user[0] == '\0')
               {
                  args[arg_count + display_offset + 8] = NULL;
               }
               else
               {
                  args[arg_count + display_offset + 8] = "-u";
                  args[arg_count + display_offset + 9] = fake_user;
                  args[arg_count + display_offset + 10] = NULL;
               }
               break;

            case O_LOG_SEL : /* Remote Output Log. */
               args[arg_count + display_offset + 5] = SHOW_OLOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               if (fake_user[0] == '\0')
               {
                  args[arg_count + display_offset + 8] = NULL;
               }
               else
               {
                  args[arg_count + display_offset + 8] = "-u";
                  args[arg_count + display_offset + 9] = fake_user;
                  args[arg_count + display_offset + 10] = NULL;
               }
               break;

            case D_LOG_SEL : /* Remote Delete Log. */
               args[arg_count + display_offset + 5] = SHOW_DLOG;
               args[arg_count + display_offset + 6] = "-f";
               args[arg_count + display_offset + 7] = font_name;
               if (fake_user[0] == '\0')
               {
                  args[arg_count + display_offset + 8] = NULL;
               }
               else
               {
                  args[arg_count + display_offset + 8] = "-u";
                  args[arg_count + display_offset + 9] = fake_user;
                  args[arg_count + display_offset + 10] = NULL;
               }
               break;

            case SHOW_QUEUE_SEL : /* View AFD Queue. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = SHOW_QUEUE;
                  args[arg_count + display_offset + 6] = "-f";
                  args[arg_count + display_offset + 7] = font_name;
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 8] = "-u";
                     args[arg_count + display_offset + 9] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 8] = "-p";
                     args[arg_count + display_offset + offset + 9] = profile;
                     args[arg_count + display_offset + offset + 10] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 8] = NULL;
                  }
               }
               break;

            case VIEW_FILE_LOAD_SEL :
               args[arg_count + display_offset + 5] = AFD_LOAD;
               args[arg_count + display_offset + 6] = SHOW_FILE_LOAD;
               args[arg_count + display_offset + 7] = "-f";
               args[arg_count + display_offset + 8] = font_name;
               args[arg_count + display_offset + 9] = NULL;
               break;

            case VIEW_KBYTE_LOAD_SEL :
               args[arg_count + display_offset + 5] = AFD_LOAD;
               args[arg_count + display_offset + 6] = SHOW_KBYTE_LOAD;
               args[arg_count + display_offset + 7] = "-f";
               args[arg_count + display_offset + 8] = font_name;
               args[arg_count + display_offset + 9] = NULL;
               break;

            case VIEW_CONNECTION_LOAD_SEL :
               args[arg_count + display_offset + 5] = AFD_LOAD;
               args[arg_count + display_offset + 6] = SHOW_CONNECTION_LOAD;
               args[arg_count + display_offset + 7] = "-f";
               args[arg_count + display_offset + 8] = font_name;
               args[arg_count + display_offset + 9] = NULL;
               break;

            case VIEW_TRANSFER_LOAD_SEL :
               args[arg_count + display_offset + 5] = AFD_LOAD;
               args[arg_count + display_offset + 6] = SHOW_TRANSFER_LOAD;
               args[arg_count + display_offset + 7] = "-f";
               args[arg_count + display_offset + 8] = font_name;
               args[arg_count + display_offset + 9] = NULL;
               break;

            case CONTROL_AMG_SEL : /* Start/Stop AMG. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = AFD_CMD;
                  args[arg_count + display_offset + 6] = "-Y";
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 7] = "-u";
                     args[arg_count + display_offset + 8] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 7] = "-p";
                     args[arg_count + display_offset + offset + 8] = profile;
                     args[arg_count + display_offset + offset + 9] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 7] = NULL;
                  }
               }
               break;

            case CONTROL_FD_SEL : /* Start/Stop FD. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = AFD_CMD;
                  args[arg_count + display_offset + 6] = "-Z";
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 7] = "-u";
                     args[arg_count + display_offset + 8] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 7] = "-p";
                     args[arg_count + display_offset + offset + 8] = profile;
                     args[arg_count + display_offset + offset + 9] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 7] = NULL;
                  }
               }
               break;

            case REREAD_DIR_CONFIG_SEL : /* Reread DIR_CONFIG file. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = "udc";
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 6] = "-u";
                     args[arg_count + display_offset + 7] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 6] = "-p";
                     args[arg_count + display_offset + offset + 7] = profile;
                     args[arg_count + display_offset + offset + 8] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 6] = NULL;
                  }
               }
               break;

            case REREAD_HOST_CONFIG_SEL : /* Reread HOST_CONFIG file. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = "uhc";
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 6] = "-u";
                     args[arg_count + display_offset + 7] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 6] = "-p";
                     args[arg_count + display_offset + offset + 7] = profile;
                     args[arg_count + display_offset + offset + 8] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 6] = NULL;
                  }
               }
               break;

            case EDIT_HC_SEL : /* Edit HOST_CONFIG file. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = EDIT_HC;
                  args[arg_count + display_offset + 6] = "-f";
                  args[arg_count + display_offset + 7] = font_name;
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 8] = "-u";
                     args[arg_count + display_offset + 9] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 8] = "-p";
                     args[arg_count + display_offset + offset + 9] = profile;
                     args[arg_count + display_offset + offset + 10] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 8] = NULL;
                  }
               }
               break;

            case DIR_CTRL_SEL : /* Dir_control dialog. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = DIR_CTRL;
                  args[arg_count + display_offset + 6] = "-f";
                  args[arg_count + display_offset + 7] = font_name;
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 8] = "-u";
                     args[arg_count + display_offset + 9] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 8] = "-p";
                     args[arg_count + display_offset + offset + 9] = profile;
                     args[arg_count + display_offset + offset + 10] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 8] = NULL;
                  }
               }
               break;

            case STARTUP_AFD_SEL : /* Startup AFD. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = "afd";
                  args[arg_count + display_offset + 6] = "-a";
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 7] = "-u";
                     args[arg_count + display_offset + 8] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 7] = "-p";
                     args[arg_count + display_offset + offset + 8] = profile;
                     args[arg_count + display_offset + offset + 9] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 7] = NULL;
                  }
               }
	       break;

            case SHUTDOWN_AFD_SEL : /* Shutdown AFD. */
               {
                  int offset;

                  args[arg_count + display_offset + 5] = "afd";
                  args[arg_count + display_offset + 6] = "-S";
                  if (fake_user[0] != '\0')
                  {
                     args[arg_count + display_offset + 7] = "-u";
                     args[arg_count + display_offset + 8] = fake_user;
                     offset = 2;
                  }
                  else
                  {
                     offset = 0;
                  }
                  if (profile[0] != '\0')
                  {
                     args[arg_count + display_offset + offset + 7] = "-p";
                     args[arg_count + display_offset + offset + 8] = profile;
                     args[arg_count + display_offset + offset + 9] = NULL;
                  }
                  else
                  {
                     args[arg_count + display_offset + offset + 7] = NULL;
                  }
               }
	       break;

            default :
               (void)xrec(INFO_DIALOG,
#if SIZEOF_LONG == 4
                          "This function [%d] has not yet been implemented.",
#else
                          "This function [%ld] has not yet been implemented.",
#endif
                          item_no);
               free(args);
               return;
         }

         if (msa[i].r_work_dir[0] == '\0')
         {
            (void)xrec(WARN_DIALOG,
                       "Did not yet receive remote working directory from %s.\nTry again latter.",
                       msa[i].afd_alias);
         }
         else
         {
            int    gotcha = NO;
#ifdef WHEN_WE_KNOW
            Window window_id;
#endif

            if ((item_no == AFD_CTRL_SEL) || (item_no == DIR_CTRL_SEL))
            {
               int    j;

               for (j = 0; j < no_of_active_process; j++)
               {
                  if ((apps_list[j].position == i) &&
                      (((item_no == AFD_CTRL_SEL) &&
                        (strcmp(apps_list[j].progname, AFD_CTRL) == 0)) ||
                       ((item_no == DIR_CTRL_SEL) &&
                        (strcmp(apps_list[j].progname, DIR_CTRL) == 0))))
                  {
#ifdef WHEN_WE_KNOW
                     if ((window_id = get_window_id(apps_list[j].pid,
                                                    MON_CTRL)) != 0L)
                     {
                        gotcha = YES;
                     }
#else
                     gotcha = YES;
#endif
                     break;
                  }
               }
            }
            if (gotcha == NO)
            {
               int j;

               args[arg_count + 1] = username;
               for (j = 0; j < MAX_CONVERT_USERNAME; j++)
               {
                  if (msa[i].convert_username[j][0][0] != '\0')
                  {
                     if (strcmp(msa[i].convert_username[j][0], username) == 0)
                     {
                        args[arg_count + 1] = msa[i].convert_username[j][1];
                        break;
                     }
                  }
               }
               args[arg_count + 2] = msa[i].hostname[(int)msa[i].afd_toggle];
               args[arg_count + display_offset + 4] = msa[i].r_work_dir;
               if (args[0][0] == 's')
               {
                  if (msa[i].options & DONT_USE_FULL_PATH_FLAG)
                  {
                     (void)strcpy(progname, "rafdd_cmd_ssh");
                  }
                  else
                  {
                     (void)sprintf(progname, "%s/bin/rafdd_cmd_ssh",
                                   msa[i].r_work_dir);
                  }
               }
               else
               {
                  if (msa[i].options & DONT_USE_FULL_PATH_FLAG)
                  {
                     (void)strcpy(progname, "rafdd_cmd");
                  }
                  else
                  {
                     (void)sprintf(progname, "%s/bin/rafdd_cmd",
                                   msa[i].r_work_dir);
                  }
               }
               make_xprocess(args[0], args[arg_count + display_offset + 5], args, i);
#ifdef TEST_OUTPUT
               {
                  int j;

                  for (j = 0; args[j] != NULL; j++)
                  {
                     printf("%s ", args[j]);
                  }
                  printf("\n");
               }
#endif
               switch (item_no)
               {
                  case AFD_CTRL_SEL : /* Remote afd_ctrl. */
                     mconfig_log(MON_LOG, DEBUG_SIGN, "%-*s: %s started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 AFD_CTRL);
                     break;

                  case DIR_CTRL_SEL : /* Remote dir_ctrl. */
                     mconfig_log(MON_LOG, DEBUG_SIGN, "%-*s: %s started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 DIR_CTRL);
                     break;

                  case S_LOG_SEL : /* Remote System Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: System Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case E_LOG_SEL : /* Remote Event Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: Event Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case R_LOG_SEL : /* Remote Receive Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: Receive Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case T_LOG_SEL : /* Remote Transfer Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: Transfer Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case I_LOG_SEL : /* Remote Input Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: Input Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case O_LOG_SEL : /* Remote Output Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: Output Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case D_LOG_SEL : /* Remote Delete Log. */
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: Delete Log started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case SHOW_QUEUE_SEL : /* View AFD Queue. */
                     mconfig_log(MON_LOG, DEBUG_SIGN, "%-*s: %s started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 SHOW_QUEUE);
                     break;

                  case VIEW_FILE_LOAD_SEL :
                     mconfig_log(MON_LOG, DEBUG_SIGN, "%-*s: %s Files started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 AFD_LOAD);
                     break;

                  case VIEW_KBYTE_LOAD_SEL :
                     mconfig_log(MON_LOG, DEBUG_SIGN, "%-*s: %s KBytes started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 AFD_LOAD);
                     break;

                  case VIEW_CONNECTION_LOAD_SEL :
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: %s Connections started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 AFD_LOAD);
                     break;

                  case VIEW_TRANSFER_LOAD_SEL :
                     mconfig_log(MON_LOG, DEBUG_SIGN,
                                 "%-*s: %s Active-Transfers started",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias,
                                 AFD_LOAD);
                     break;

                  case CONTROL_AMG_SEL : /* Start/Stop AMG. */
                     mconfig_log(MON_LOG, CONFIG_SIGN,
                                 "%-*s: Start/Stop AMG initiated",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case CONTROL_FD_SEL : /* Start/Stop FD. */
                     mconfig_log(MON_LOG, CONFIG_SIGN,
                                 "%-*s: Start/Stop FD initiated",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case REREAD_DIR_CONFIG_SEL : /* Reread DIR_CONFIG file. */
                     mconfig_log(MON_LOG, CONFIG_SIGN,
                                 "%-*s: Reread DIR_CONFIG initiated",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case REREAD_HOST_CONFIG_SEL : /* Reread HOST_CONFIG file. */
                     mconfig_log(MON_LOG, CONFIG_SIGN,
                                 "%-*s: Reread HOST_CONFIG initiated",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case EDIT_HC_SEL : /* Edit HOST_CONFIG file. */
                     mconfig_log(MON_LOG, CONFIG_SIGN, "%-*s: %s called",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias, EDIT_HC);
                     break;

                  case STARTUP_AFD_SEL : /* Startup AFD. */
                     mconfig_log(MON_LOG, CONFIG_SIGN,
                                 "%-*s: AFD startup initiated",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  case SHUTDOWN_AFD_SEL : /* Shutdown AFD. */
                     mconfig_log(MON_LOG, CONFIG_SIGN,
                                 "%-*s: AFD shutdown initiated",
                                 MAX_AFDNAME_LENGTH, msa[i].afd_alias);
                     break;

                  default : /* For all others, no need to log anything. */
                     break;
               }
               if (connect_data[i].inverse == ON)
               {
                  connect_data[i].inverse = OFF;
                  draw_line_status(i, -1);
                  no_selected--;
               }
            }
            else
            {
#ifdef WHEN_WE_KNOW
               XRaiseWindow(display, window_id);
               XSetInputFocus(display, window_id, RevertToParent, CurrentTime);
#else
               (void)xrec(INFO_DIALOG,
                          "%s dialog for %s is already open on your display.",
                          (item_no == AFD_CTRL_SEL) ? AFD_CTRL : DIR_CTRL,
                          msa[i].afd_alias);
#endif
            }
         }
      }
   }
   free(args);

   return;
}


/*######################## change_mon_font_cb() #########################*/
void
change_mon_font_cb(Widget    w,
                   XtPointer client_data,
                   XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;
   XGCValues   gc_values;

   if (current_font != item_no)
   {
      XtVaSetValues(fw[current_font], XmNset, False, NULL);
      current_font = item_no;
   }

   switch (item_no)
   {
      case 0   :
         (void)strcpy(font_name, FONT_0);
         break;

      case 1   :
         (void)strcpy(font_name, FONT_1);
         break;

      case 2   :
         (void)strcpy(font_name, FONT_2);
         break;

      case 3   :
         (void)strcpy(font_name, FONT_3);
         break;

      case 4   :
         (void)strcpy(font_name, FONT_4);
         break;

      case 5   :
         (void)strcpy(font_name, FONT_5);
         break;

      case 6   :
         (void)strcpy(font_name, FONT_6);
         break;

      case 7   :
         (void)strcpy(font_name, FONT_7);
         break;

      case 8   :
         (void)strcpy(font_name, FONT_8);
         break;

      case 9   :
         (void)strcpy(font_name, FONT_9);
         break;

      case 10  :
         (void)strcpy(font_name, FONT_10);
         break;

      case 11  :
         (void)strcpy(font_name, FONT_11);
         break;

      case 12  :
         (void)strcpy(font_name, FONT_12);
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible font selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible font selection (%ld).", item_no);
#endif
         return;
    }

#ifdef _DEBUG
   (void)fprintf(stderr, "You have chosen: %s\n", font_name);
#endif

   /* Calculate the new values for global variables. */
   setup_mon_window(font_name);

   /* Load the font into the old GC. */
   gc_values.font = font_struct->fid;
   XChangeGC(display, letter_gc, GCFont, &gc_values);
   XChangeGC(display, normal_letter_gc, GCFont, &gc_values);
   XChangeGC(display, locked_letter_gc, GCFont, &gc_values);
   XChangeGC(display, color_letter_gc, GCFont, &gc_values);
   XChangeGC(display, red_color_letter_gc, GCFont, &gc_values);
   XChangeGC(display, red_error_letter_gc, GCFont, &gc_values);
   XFlush(display);

   /* Resize and redraw window if necessary. */
   if (resize_mon_window() == YES)
   {
      calc_mon_but_coord(window_width);
      redraw_all();
      XFlush(display);
   }

   return;
}


/*######################## change_mon_rows_cb() #########################*/
void
change_mon_rows_cb(Widget    w,
                   XtPointer client_data,
                   XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (current_row != item_no)
   {
      XtVaSetValues(rw[current_row], XmNset, False, NULL);
      current_row = item_no;
   }

   switch (item_no)
   {
      case 0   :
         no_of_rows_set = atoi(ROW_0);
         break;

      case 1   :
         no_of_rows_set = atoi(ROW_1);
         break;

      case 2   :
         no_of_rows_set = atoi(ROW_2);
         break;

      case 3   :
         no_of_rows_set = atoi(ROW_3);
         break;

      case 4   :
         no_of_rows_set = atoi(ROW_4);
         break;

      case 5   :
         no_of_rows_set = atoi(ROW_5);
         break;

      case 6   :
         no_of_rows_set = atoi(ROW_6);
         break;

      case 7   :
         no_of_rows_set = atoi(ROW_7);
         break;

      case 8   :
         no_of_rows_set = atoi(ROW_8);
         break;

      case 9   :
         no_of_rows_set = atoi(ROW_9);
         break;

      case 10  :
         no_of_rows_set = atoi(ROW_10);
         break;

      case 11  :
         no_of_rows_set = atoi(ROW_11);
         break;

      case 12  :
         no_of_rows_set = atoi(ROW_12);
         break;

      case 13  :
         no_of_rows_set = atoi(ROW_13);
         break;

      case 14  :
         no_of_rows_set = atoi(ROW_14);
         break;

      case 15  :
         no_of_rows_set = atoi(ROW_15);
         break;

      case 16  :
         no_of_rows_set = atoi(ROW_16);
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible row selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible row selection (%ld).", item_no);
#endif
         return;
   }

   if (no_of_rows_set == 0)
   {
      no_of_rows_set = 2;
   }

#ifdef _DEBUG
   (void)fprintf(stderr, "%s: You have chosen: %d rows/column\n",
                 __FILE__, no_of_rows_set);
#endif

   if (resize_mon_window() == YES)
   {
      calc_mon_but_coord(window_width);
      redraw_all();
      XFlush(display);
   }

  return;
}


/*######################## change_mon_style_cb() ########################*/
void
change_mon_style_cb(Widget    w,
                    XtPointer client_data,
                    XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (current_style != item_no)
   {
      XtVaSetValues(lsw[current_style], XmNset, False, NULL);
      current_style = item_no;
   }

   switch (item_no)
   {
      case 0   :
         line_style = BARS_ONLY;
         break;

      case 1   :
         line_style = CHARACTERS_ONLY;
         break;

      case 2   :
         line_style = CHARACTERS_AND_BARS;
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible row selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible row selection (%ld).", item_no);
#endif
         return;
   }

#ifdef _DEBUG
   switch (line_style)
   {
      case BARS_ONLY             :
         (void)fprintf(stderr, "Changing line style to bars only.\n");
         break;

      case CHARACTERS_ONLY       :
         (void)fprintf(stderr, "Changing line style to characters only.\n");
         break;

      case CHARACTERS_AND_BARS   :
         (void)fprintf(stderr, "Changing line style to bars and characters.\n");
         break;

      default                    : /* IMPOSSIBLE !!! */
         break;
   }
#endif

   setup_mon_window(font_name);

   if (resize_mon_window() == YES)
   {
      calc_mon_but_coord(window_width);
      redraw_all();
      XFlush(display);
   }

   return;
}


/*###################### change_mon_history_cb() ########################*/
void
change_mon_history_cb(Widget    w,
                      XtPointer client_data,
                      XtPointer call_data)
{
   int         i;
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (current_his_log != item_no)
   {
      XtVaSetValues(hlw[current_his_log], XmNset, False, NULL);
      current_his_log = item_no;
   }

   switch (item_no)
   {
      case 0 :
         his_log_set = atoi(HIS_0);
         break;

      case 1 :
         his_log_set = atoi(HIS_1);
         break;

      case 2 :
         his_log_set = atoi(HIS_2);
         break;

      case 3 :
         his_log_set = atoi(HIS_3);
         break;

      case 4 :
         his_log_set = atoi(HIS_4);
         break;

      case 5 :
         his_log_set = atoi(HIS_5);
         break;

      case 6 :
         his_log_set = atoi(HIS_6);
         break;

      case 7 :
         his_log_set = atoi(HIS_7);
         break;

      case 8 :
         his_log_set = atoi(HIS_8);
         break;

      default:
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible history selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible history selection (%ld).", item_no);
#endif
         return;
   }

#ifdef _DEBUG
   (void)fprintf(stderr, "%s: You have chosen: %d history logs\n",
                 __FILE__, his_log_set);
#endif

   setup_mon_window(font_name);

   if (resize_mon_window() == YES)
   {
      calc_mon_but_coord(window_width);
      XClearWindow(display, line_window);
      XFreePixmap(display, label_pixmap);
      label_pixmap = XCreatePixmap(display, label_window, window_width,
                                   line_height, depth);
      XFreePixmap(display, line_pixmap);
      line_pixmap = XCreatePixmap(display, line_window, window_width,
                                  (line_height * no_of_rows), depth);
      XFillRectangle(display, line_pixmap, default_bg_gc, 0, 0,
                     window_width, (line_height * no_of_rows));
      XFreePixmap(display, button_pixmap);
      button_pixmap = XCreatePixmap(display, button_window, window_width,
                                    line_height, depth);

      /* Redraw label line at top. */
      draw_label_line();

      /* Redraw all status lines. */
      for (i = 0; i < no_of_afds; i++)
      {
         if (his_log_set > 0)
         {
            (void)memcpy(connect_data[i].log_history, msa[i].log_history,
                         (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
         }
         draw_line_status(i, 1);
      }

      /* Redraw buttons at bottom. */
      draw_mon_button_line();

      XFlush(display);
   }

   return;
}
