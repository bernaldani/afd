/*
 *  mon_ctrl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   mon_ctrl - controls and monitors other AFD's
 **
 ** SYNOPSIS
 **   mon_ctrl [--version][-w <work dir>][-f <font name>][-no_input]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.09.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), stderr                       */
#include <string.h>           /* strcpy(), strcmp()                      */
#include <ctype.h>            /* toupper()                               */
#include <unistd.h>           /* gethostname(), getcwd(), STDERR_FILENO  */
#include <stdlib.h>           /* getenv(), calloc()                      */
#include <math.h>             /* log10()                                 */
#include <sys/times.h>        /* times(), struct tms                     */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>         /* mmap()                                  */
#endif
#include <signal.h>           /* kill(), signal(), SIGINT                */
#include <pwd.h>              /* getpwuid()                              */
#include <fcntl.h>            /* O_RDWR                                  */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include "mon_ctrl.h"
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/CascadeBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "version.h"
#include "permission.h"

/* Global variables */
Display                 *display;
XtAppContext            app;
XtIntervalId            interval_id_afd;
GC                      letter_gc,
                        normal_letter_gc,
                        locked_letter_gc,
                        color_letter_gc,
                        default_bg_gc,
                        normal_bg_gc,
                        locked_bg_gc,
                        label_bg_gc,
                        red_color_letter_gc,
                        red_error_letter_gc,
                        tr_bar_gc,
                        color_gc,
                        black_line_gc,
                        white_line_gc,
                        led_gc;
Colormap                default_cmap;
XFontStruct             *font_struct;
XmFontList              fontlist = NULL;
Widget                  mw[5],       /* Main menu */
                        ow[7],       /* Monitor menu */
                        tw[2],       /* Test (ping, traceroute) */
                        vw[9],       /* Remote view menu */
                        cw[8],       /* Remote control menu */
                        sw[5],       /* Setup menu */
                        hw[3],       /* Help menu */
                        fw[13],      /* Select font */
                        rw[14],      /* Select rows */
                        hlw[NO_OF_HISTORY_LOGS],
                        lw[4],       /* AFD load */
                        lsw[3],      /* Select line style */
                        pw[5];       /* Popup menu */
Widget                  appshell,
                        label_window_w,
                        line_window_w;
Window                  label_window,
                        line_window;
float                   max_bar_length;
int                     bar_thickness_3,
                        current_font = -1,
                        current_his_log = -1,
                        current_row = -1,
                        current_style = -1,
                        his_log_set,
                        msa_fd = -1,
                        msa_id,
                        no_input,
                        line_length,
                        line_height = 0,
                        log_angle,
                        magic_value,
                        mon_log_fd = STDERR_FILENO,
                        no_selected,
                        no_selected_static,
                        no_of_active_process = 0,
                        no_of_columns,
                        no_of_rows,
                        no_of_rows_set,
                        no_of_afds,
                        no_of_jobs_selected,
                        redraw_time_line,
                        sys_log_fd = STDERR_FILENO,
                        window_width,
                        window_height,
                        x_center_log_status,
                        x_offset_log_status,
                        x_offset_log_history,
                        x_offset_led,
                        x_offset_bars,
                        x_offset_characters,
                        y_center_log,
                        y_offset_led;
#ifndef _NO_MMAP
off_t                   msa_size;
#endif
unsigned short          step_size;
unsigned long           color_pool[COLOR_POOL_SIZE];
unsigned int            glyph_height,
                        glyph_width,
                        text_offset;
char                    work_dir[MAX_PATH_LENGTH],
                        *p_work_dir,
                        mon_active_file[MAX_PATH_LENGTH],
                        line_style,
                        font_name[20],
                        blink_flag,
                        *ping_cmd = NULL,
                        *ptr_ping_cmd,
                        *traceroute_cmd = NULL,
                        *ptr_traceroute_cmd,
                        user[MAX_FILENAME_LENGTH],
                        username[MAX_USER_NAME_LENGTH];
clock_t                 clktck;
struct apps_list        *apps_list = NULL;
struct coord            coord[LOG_FIFO_SIZE];
struct mon_line         *connect_data;
struct mon_status_area  *msa;
struct mon_control_perm mcp;

/* Local function prototypes */
static void             mon_ctrl_exit(void),
                        create_pullright_font(Widget),
                        create_pullright_history(Widget),
                        create_pullright_load(Widget),
                        create_pullright_row(Widget),
                        create_pullright_style(Widget),
                        create_pullright_test(Widget),
                        eval_permissions(char *),
                        init_menu_bar(Widget, Widget *),
                        init_popup_menu(Widget),
                        init_mon_ctrl(int *, char **, char *),
                        sig_bus(int),
                        sig_exit(int),
                        sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char          window_title[100];
   static String fallback_res[] =
                 {
                    "*mwmDecorations : 42",
                    "*mwmFunctions : 12",
                    ".mon_ctrl*background : NavajoWhite2",
                    NULL
                 };
   Widget        mainform_w,
                 mainwindow,
                 menu_w;
   Arg           args[MAXARGS];
   Cardinal      argcount;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   init_mon_ctrl(&argc, argv, window_title);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   /* Create the top-level shell widget and initialise the toolkit */
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0, &argc, argv,
                              fallback_res, args, argcount);

   /* Get display pointer */
   if ((display = XtDisplay(appshell)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   mainwindow = XtVaCreateManagedWidget("Main_window",
                                        xmMainWindowWidgetClass, appshell,
                                        NULL);

   /* setup and determine window parameters */
   setup_mon_window(font_name);

   /* Get window size */
   (void)mon_window_size(&window_width, &window_height);

   /* Create managing widget for label and line widget */
   mainform_w = XmCreateForm(mainwindow, "mainform_w", NULL, 0);
   XtManageChild(mainform_w);

   if (no_input == False)
   {
      init_menu_bar(mainform_w, &menu_w);
   }

   /* Setup colors */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(appshell));

   /* Create the label_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) line_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[LABEL_BG]);
   argcount++;
   if (no_input == False)
   {
      XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,     menu_w);
      argcount++;
   }
   else
   {
      XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_FORM);
      argcount++;
   }
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   label_window_w = XmCreateDrawingArea(mainform_w, "label_window_w", args,
                                        argcount);
   XtManageChild(label_window_w);

   /* Get background color from the widget's resources */
   argcount = 0;
   XtSetArg(args[argcount], XmNbackground, &color_pool[LABEL_BG]);
   argcount++;
   XtGetValues(label_window_w, args, argcount);

   /* Create the line_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) window_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[DEFAULT_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget, label_window_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   line_window_w = XmCreateDrawingArea(mainform_w, "line_window_w", args,
                                       argcount);
   XtManageChild(line_window_w);

   /* Initialise the GC's */
   init_gcs();

   /* Get foreground color from the widget's resources */
   argcount = 0;
   XtSetArg(args[argcount], XmNforeground, &color_pool[FG]); argcount++;
   XtGetValues(line_window_w, args, argcount);

   /* Add call back to handle expose events for the label window */
   XtAddCallback(label_window_w, XmNexposeCallback,
                 (XtCallbackProc)mon_expose_handler_label, (XtPointer)0);

   /* Add call back to handle expose events for the line window */
   XtAddCallback(line_window_w, XmNexposeCallback,
                 (XtCallbackProc)mon_expose_handler_line, NULL);

   if (no_input == False)
   {
      XtAddEventHandler(line_window_w,
                        ButtonPressMask | Button1MotionMask,
                        False, (XtEventHandler)mon_input, NULL);

      /* Set toggle button for font|row */
      XtVaSetValues(fw[current_font], XmNset, True, NULL);
      XtVaSetValues(rw[current_row], XmNset, True, NULL);
      XtVaSetValues(lsw[current_style], XmNset, True, NULL);
      XtVaSetValues(hlw[current_his_log], XmNset, True, NULL);

      /* Setup popup menu */
      init_popup_menu(line_window_w);

      XtAddEventHandler(line_window_w,
                        EnterWindowMask | LeaveWindowMask,
                        False, (XtEventHandler)mon_focus, NULL);
   }

#ifdef _EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set signal handlers for mon_ctrl : %s",
                 strerror(errno));
   }

   /* Exit handler so we can close applications that the user started. */
   if (atexit(mon_ctrl_exit) != 0)
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set exit handler for mon_ctrl : %s\n\nWill not be able to close applications when terminating.",
                 strerror(errno));
   }

   /* Get window ID of three main windows */
   label_window = XtWindow(label_window_w);
   line_window = XtWindow(line_window_w);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_mon_ctrl() ++++++++++++++++++++++++++*/
static void
init_mon_ctrl(int *argc, char *argv[], char *window_title)
{
   int           i;
   unsigned int  new_bar_length;
   char          *buffer,
                 config_file[MAX_PATH_LENGTH],
                 *perm_buffer,
                 hostname[MAX_AFD_NAME_LENGTH],
                 mon_log_fifo[MAX_PATH_LENGTH],
                 sys_log_fifo[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   struct passwd *pwd;

   /* See if user wants some help. */
   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    "Usage: %s [-w <work_dir>] [-no_input] [-f <font name>]\n",
                    argv[0]);
      exit(SUCCESS);
   }
   /*
    * Determine the working directory. If it is not specified
    * in the command line try read it from the environment else
    * just take the default.
    */
   if (get_mon_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Disable all input? */
   if (get_arg(argc, argv, "-no_input", NULL, 0) == SUCCESS)
   {
      no_input = True;
   }
   else
   {
      no_input = False;
   }
   if (get_arg(argc, argv, "-f", font_name, 20) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }

   /* Now lets see if user may use this program */
   switch(get_permissions(&perm_buffer))
   {
      case NONE : /* User is not allowed to use this program */
         {
            char *user;

            if ((user = getenv("LOGNAME")) != NULL)
            {
               (void)fprintf(stderr,
                             "User %s is not permitted to use this program.\n",
                             user);
            }
            else
            {
               (void)fprintf(stderr, "%s\n",
                             PERMISSION_DENIED_STR);
            }
         }
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         eval_permissions(perm_buffer);
         free(perm_buffer);
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         mcp.mon_ctrl_list      = NULL;
         mcp.amg_ctrl           = YES; /* Start/Stop the AMG    */
         mcp.fd_ctrl            = YES; /* Start/Stop the FD     */
         mcp.rr_dc              = YES; /* Reread DIR_CONFIG     */
         mcp.rr_hc              = YES; /* Reread HOST_CONFIG    */
         mcp.startup_afd        = YES; /* Startup the AFD       */
         mcp.shutdown_afd       = YES; /* Shutdown the AFD      */
         mcp.info               = YES;
         mcp.info_list          = NULL;
         mcp.retry              = YES;
         mcp.retry_list         = NULL;
         mcp.disable            = YES;
         mcp.disable_list       = NULL;
         mcp.afd_ctrl           = YES; /* AFD Control Dialog    */
         mcp.afd_ctrl_list      = NULL;
         mcp.show_slog          = YES; /* View the system log   */
         mcp.show_slog_list     = NULL;
         mcp.show_rlog          = YES; /* View the receive log  */
         mcp.show_rlog_list     = NULL;
         mcp.show_tlog          = YES; /* View the transfer log */
         mcp.show_tlog_list     = NULL;
         mcp.show_ilog          = YES; /* View the input log    */
         mcp.show_ilog_list     = NULL;
         mcp.show_olog          = YES; /* View the output log   */
         mcp.show_olog_list     = NULL;
         mcp.show_elog          = YES; /* View the delete log   */
         mcp.show_elog_list     = NULL;
         mcp.afd_load           = YES;
         mcp.afd_load_list      = NULL;
         mcp.edit_hc            = YES; /* Edit Host Config      */
         mcp.edit_hc_list       = NULL;
         mcp.dir_ctrl           = YES;
         break;

      default : /* Something must be wrong! */
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   (void)strcpy(sys_log_fifo, p_work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcpy(mon_active_file, sys_log_fifo);
   (void)strcat(mon_active_file, MON_ACTIVE_FILE);
   (void)strcpy(mon_log_fifo, sys_log_fifo);
   (void)strcat(mon_log_fifo, MON_LOG_FIFO);
   (void)strcat(sys_log_fifo, MON_SYS_LOG_FIFO);

   /* Create and open sys_log + mon_log fifo. */
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((stat(mon_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(mon_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   mon_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((mon_log_fd = open(mon_log_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                mon_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Prepare title for mon_ctrl window */
#ifdef PRE_RELEASE
   (void)sprintf(window_title, "AFD_MON PRE %d.%d.%d-%d ",
                 MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)sprintf(window_title, "AFD_MON %d.%d.%d ", MAJOR, MINOR, BUG_FIX);
#endif
   if (get_afd_name(hostname) == INCORRECT)
   {
      if (gethostname(hostname, MAX_AFD_NAME_LENGTH) == 0)
      {
         hostname[0] = toupper((int)hostname[0]);
         (void)strcat(window_title, hostname);
      }
   }
   else
   {
      (void)strcat(window_title, hostname);
   }

   get_user(user);
   if ((pwd = getpwuid(getuid())) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "getpwuid() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)strcpy(username, pwd->pw_name);

   /*
    * Attach to the MSA and get the number of AFD's
    * and the msa_id of the MSA.
    */
   if (msa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      (void)fprintf(stderr, "Could not get clock ticks per second.\n");
      exit(INCORRECT);
   }

   /* Allocate memory for local 'MSA' */
   if ((connect_data = calloc(no_of_afds, sizeof(struct mon_line))) == NULL)
   {
      (void)fprintf(stderr, "calloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Read setup file of this user.
    */
   line_style = CHARACTERS_AND_BARS;
   no_of_rows_set = DEFAULT_NO_OF_ROWS;
   his_log_set = DEFAULT_NO_OF_HISTORY_LOGS;
   read_setup("mon_ctrl", NULL, &his_log_set);

   /* Determine the default bar length */
   max_bar_length  = 6 * BAR_LENGTH_MODIFIER;
   step_size = MAX_INTENSITY / max_bar_length;

   /* Initialise all display data for each AFD to monitor */
   for (i = 0; i < no_of_afds; i++)
   {
      (void)strcpy(connect_data[i].afd_alias, msa[i].afd_alias);
      (void)sprintf(connect_data[i].afd_display_str, "%-*s",
                    MAX_AFDNAME_LENGTH, connect_data[i].afd_alias);
      (void)memcpy(connect_data[i].sys_log_fifo, msa[i].sys_log_fifo,
                   LOG_FIFO_SIZE + 1);
      if (his_log_set > 0)
      {
         (void)memcpy(connect_data[i].log_history, msa[i].log_history,
                      (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
      }
      connect_data[i].sys_log_ec = msa[i].sys_log_ec;
      connect_data[i].amg = msa[i].amg;
      connect_data[i].fd = msa[i].fd;
      connect_data[i].archive_watch = msa[i].archive_watch;
      if ((connect_data[i].amg == OFF) ||
          (connect_data[i].fd == OFF) ||
          (connect_data[i].archive_watch == OFF))
      {
         connect_data[i].blink_flag = ON;
      }
      else
      {
         connect_data[i].blink_flag = OFF;
      }
      connect_data[i].blink = TR_BAR;
      connect_data[i].jobs_in_queue = msa[i].jobs_in_queue;
      connect_data[i].no_of_transfers = msa[i].no_of_transfers;
      connect_data[i].host_error_counter = msa[i].host_error_counter;
      connect_data[i].fc = msa[i].fc;
      connect_data[i].fs = msa[i].fs;
      connect_data[i].tr = msa[i].tr;
      connect_data[i].fr = msa[i].fr;
      connect_data[i].ec = msa[i].ec;
      connect_data[i].last_data_time = msa[i].last_data_time;
      connect_data[i].connect_status = msa[i].connect_status;
      CREATE_FC_STRING(connect_data[i].str_fc, connect_data[i].fc);
      CREATE_FS_STRING(connect_data[i].str_fs, connect_data[i].fs);
      CREATE_FS_STRING(connect_data[i].str_tr, connect_data[i].tr);
      CREATE_EC_STRING(connect_data[i].str_fr, connect_data[i].fr);
      CREATE_EC_STRING(connect_data[i].str_ec, connect_data[i].ec);
      CREATE_FC_STRING(connect_data[i].str_jq, connect_data[i].jobs_in_queue);
      CREATE_SFC_STRING(connect_data[i].str_at, connect_data[i].no_of_transfers);
      CREATE_EC_STRING(connect_data[i].str_hec, connect_data[i].host_error_counter);
      connect_data[i].average_tr = 0.0;
      connect_data[i].max_average_tr = 0.0;
      connect_data[i].no_of_hosts = msa[i].no_of_hosts;
      connect_data[i].max_connections = msa[i].max_connections;
      connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1] = max_bar_length / connect_data[i].max_connections;
      connect_data[i].scale[HOST_ERROR_BAR_NO - 1] = max_bar_length / connect_data[i].no_of_hosts;
      if (connect_data[i].no_of_transfers == 0)
      {
         new_bar_length = 0;
      }
      else if (connect_data[i].no_of_transfers >= connect_data[i].max_connections)
           {
              new_bar_length = max_bar_length;
           }
           else
           {
              new_bar_length = connect_data[i].no_of_transfers * connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1];
           }
      if (new_bar_length >= max_bar_length)
      {
         connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = max_bar_length;
         connect_data[i].blue_color_offset = MAX_INTENSITY;
         connect_data[i].green_color_offset = 0;
      }
      else
      {
         connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = new_bar_length;
         connect_data[i].blue_color_offset = new_bar_length * step_size;
         connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].blue_color_offset;
      }
      connect_data[i].bar_length[MON_TR_BAR_NO] = 0;
      if (connect_data[i].host_error_counter == 0)
      {
         connect_data[i].bar_length[HOST_ERROR_BAR_NO] = 0;
      }
      else if (connect_data[i].host_error_counter >= connect_data[i].no_of_hosts)
           {
              connect_data[i].bar_length[HOST_ERROR_BAR_NO] = max_bar_length;
           }
           else
           {
              connect_data[i].bar_length[HOST_ERROR_BAR_NO] = connect_data[i].host_error_counter *
                                                              connect_data[i].scale[HOST_ERROR_BAR_NO - 1];
           }
      connect_data[i].inverse = OFF;
      connect_data[i].expose_flag = NO;
   }

   log_angle = 360 / LOG_FIFO_SIZE;
   no_selected = no_selected_static = 0;
   redraw_time_line = STARTING_REDRAW_TIME;

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, MON_CONFIG_FILE);
   if ((access(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      int  str_length;
      char value[MAX_PATH_LENGTH];

      if (get_definition(buffer, PING_CMD_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         str_length = strlen(value);
         if (str_length > 0)
         {
            if ((ping_cmd = malloc(str_length + 4 + MAX_REAL_HOSTNAME_LENGTH)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);   
               exit(INCORRECT);
            }
            ping_cmd[0] = '"';
            (void)strcpy(&ping_cmd[1], value);
            ping_cmd[str_length + 1] = ' ';   
            ptr_ping_cmd = ping_cmd + str_length + 2;
         }
      }
      if (get_definition(buffer, TRACEROUTE_CMD_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         str_length = strlen(value);
         if (str_length > 0)
         {
            if ((traceroute_cmd = malloc(str_length + 4 + MAX_REAL_HOSTNAME_LENGTH)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            traceroute_cmd[0] = '"';
            (void)strcpy(&traceroute_cmd[1], value);
            traceroute_cmd[str_length + 1] = ' ';
            ptr_traceroute_cmd = traceroute_cmd + str_length + 2;
         }
      }
      free(buffer);
   }
   return;
}


/*+++++++++++++++++++++++++++ init_menu_bar() +++++++++++++++++++++++++++*/
static void
init_menu_bar(Widget mainform_w, Widget *menu_w)
{
   Arg      args[MAXARGS];
   Cardinal argcount;
   Widget   pull_down_w,
            pullright_font,
            pullright_history,
            pullright_load,
            pullright_row,
            pullright_test,
            pullright_line_style;

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,         XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNmarginHeight,    0);
   argcount++;
   XtSetArg(args[argcount], XmNmarginWidth,     0);
   argcount++;
   *menu_w = XmCreateSimpleMenuBar(mainform_w, "Menu Bar", args, argcount);

   /**********************************************************************/
   /*                          Monitor Menu                              */
   /**********************************************************************/
   pull_down_w = XmCreatePulldownMenu(*menu_w, "Monitor Pulldown", NULL, 0);
   XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[MON_W] = XtVaCreateManagedWidget("Monitor",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'M',
                              XmNsubMenuId,               pull_down_w,
                              NULL);

   if ((mcp.show_ms_log != NO_PERMISSION) ||
       (mcp.show_mm_log != NO_PERMISSION) ||
       (mcp.info != NO_PERMISSION) ||
       (mcp.retry != NO_PERMISSION) ||
       (mcp.disable != NO_PERMISSION) ||
       (traceroute_cmd != NULL) ||
       (ping_cmd != NULL))
   {
      if (mcp.show_ms_log != NO_PERMISSION)
      {
         ow[MON_SYS_LOG_W] = XtVaCreateManagedWidget("System Log",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(ow[MON_SYS_LOG_W], XmNactivateCallback, mon_popup_cb,
                       (XtPointer)MON_SYS_LOG_SEL);
      }
      if (mcp.show_mm_log != NO_PERMISSION)
      {
         ow[MON_LOG_W] = XtVaCreateManagedWidget("Monitor Log",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(ow[MON_LOG_W], XmNactivateCallback, mon_popup_cb,
                       (XtPointer)MON_LOG_SEL);
      }
      if (mcp.retry != NO_PERMISSION)
      {
         ow[MON_RETRY_W] = XtVaCreateManagedWidget("Retry",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              XmNmnemonic,             'R',
                              XmNaccelerator,          "Alt<Key>R",
                              NULL);
         XtAddCallback(ow[MON_RETRY_W], XmNactivateCallback, mon_popup_cb,
                       (XtPointer)MON_RETRY_SEL);
      }
      if ((traceroute_cmd != NULL) || (ping_cmd != NULL))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         pullright_test = XmCreateSimplePulldownMenu(pull_down_w,
                              "pullright_test", NULL, 0);
         ow[MON_TEST_W] = XtVaCreateManagedWidget("Test",
                              xmCascadeButtonWidgetClass, pull_down_w,
                              XmNfontList,                fontlist,
                              XmNsubMenuId,               pullright_test,
                              NULL);
         create_pullright_test(pullright_test);
      }
      if (mcp.info != NO_PERMISSION)
      {
         ow[MON_INFO_W] = XtVaCreateManagedWidget("Info",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(ow[MON_INFO_W], XmNactivateCallback, mon_popup_cb,
                       (XtPointer)MON_INFO_SEL);
      }
      XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              XmNseparatorType,       XmDOUBLE_LINE,
                              NULL);
      if (mcp.disable != NO_PERMISSION)
      {
         ow[MON_DISABLE_W] = XtVaCreateManagedWidget("Enable/Disable AFD",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(ow[MON_DISABLE_W], XmNactivateCallback, mon_popup_cb,
                       (XtPointer)MON_DISABLE_SEL);
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 XmNseparatorType,       XmDOUBLE_LINE,
                                 NULL);
      }
   }
   ow[MON_EXIT_W] = XtVaCreateManagedWidget("Exit",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              XmNmnemonic,             'x',
                              XmNaccelerator,          "Alt<Key>x",
                              NULL);
   XtAddCallback(ow[MON_EXIT_W], XmNactivateCallback, mon_popup_cb,
                 (XtPointer)EXIT_SEL);

   /**********************************************************************/
   /*                          RView Menu                                */
   /**********************************************************************/
   if ((mcp.afd_ctrl != NO_PERMISSION) ||
       (mcp.show_slog != NO_PERMISSION) ||
       (mcp.show_rlog != NO_PERMISSION) ||
       (mcp.show_tlog != NO_PERMISSION) ||
       (mcp.show_ilog != NO_PERMISSION) ||
       (mcp.show_olog != NO_PERMISSION) ||
       (mcp.show_elog != NO_PERMISSION) ||
       (mcp.show_queue != NO_PERMISSION) ||
       (mcp.afd_load != NO_PERMISSION))
   {
      pull_down_w = XmCreatePulldownMenu(*menu_w, "View Pulldown", NULL, 0);
      XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
      mw[LOG_W] = XtVaCreateManagedWidget("RView",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'R',
                              XmNsubMenuId,               pull_down_w,
                              NULL);
      if (mcp.afd_ctrl != NO_PERMISSION)
      {
         vw[MON_AFD_CTRL_W] = XtVaCreateManagedWidget("AFD Control",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              XmNmnemonic,             'A',
                              XmNaccelerator,          "Alt<Key>A",
                              NULL);
         XtAddCallback(vw[MON_AFD_CTRL_W], XmNactivateCallback,
                       start_remote_prog, (XtPointer)AFD_CTRL_SEL);
      }
      if ((mcp.show_slog != NO_PERMISSION) ||
          (mcp.show_rlog != NO_PERMISSION) ||
          (mcp.show_tlog != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         if (mcp.show_slog != NO_PERMISSION)
         {
            vw[MON_SYSTEM_W] = XtVaCreateManagedWidget("System Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'S',
                                 XmNaccelerator,          "Alt<Key>S",
                                 NULL);
            XtAddCallback(vw[MON_SYSTEM_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)S_LOG_SEL);
         }
         if (mcp.show_rlog != NO_PERMISSION)
         {
            vw[MON_RECEIVE_W] = XtVaCreateManagedWidget("Receive Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'R',
                                 XmNaccelerator,          "Alt<Key>R",
                                 NULL);
            XtAddCallback(vw[MON_RECEIVE_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)R_LOG_SEL);
         }
         if (mcp.show_tlog != NO_PERMISSION)
         {
            vw[MON_TRANS_W] = XtVaCreateManagedWidget("Transfer Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'T',
                                 XmNaccelerator,          "Alt<Key>T",
                                 NULL);
            XtAddCallback(vw[MON_TRANS_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)T_LOG_SEL);
         }
      }
      if ((mcp.show_ilog != NO_PERMISSION) ||
          (mcp.show_olog != NO_PERMISSION) ||
          (mcp.show_elog != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         if (mcp.show_ilog != NO_PERMISSION)
         {
            vw[MON_INPUT_W] = XtVaCreateManagedWidget("Input Log",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(vw[MON_INPUT_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)I_LOG_SEL);
         }
         if (mcp.show_olog != NO_PERMISSION)
         {
            vw[MON_OUTPUT_W] = XtVaCreateManagedWidget("Output Log",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(vw[MON_OUTPUT_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)O_LOG_SEL);
         }
         if (mcp.show_elog != NO_PERMISSION)
         {
            vw[MON_DELETE_W] = XtVaCreateManagedWidget("Delete Log",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(vw[MON_DELETE_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)E_LOG_SEL);
         }
      }
      if (mcp.show_queue != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         vw[MON_SHOW_QUEUE_W] = XtVaCreateManagedWidget("Queue",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(vw[MON_SHOW_QUEUE_W], XmNactivateCallback,
                       start_remote_prog, (XtPointer)SHOW_QUEUE_SEL);
      }
      if (mcp.afd_load != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         pullright_load = XmCreateSimplePulldownMenu(pull_down_w,
                                                     "pullright_load",
                                                     NULL, 0);
         vw[MON_VIEW_LOAD_W] = XtVaCreateManagedWidget("Load",
                              xmCascadeButtonWidgetClass, pull_down_w,
                              XmNfontList,                fontlist,
                              XmNsubMenuId,               pullright_load,
                              NULL);
         create_pullright_load(pullright_load);
      }
   }

   /**********************************************************************/
   /*                         RControl Menu                              */
   /**********************************************************************/
   if ((mcp.amg_ctrl != NO_PERMISSION) ||
       (mcp.fd_ctrl != NO_PERMISSION) ||
       (mcp.rr_dc != NO_PERMISSION) ||
       (mcp.rr_hc != NO_PERMISSION) ||
       (mcp.edit_hc != NO_PERMISSION) ||
       (mcp.dir_ctrl != NO_PERMISSION) ||
       (mcp.startup_afd != NO_PERMISSION) ||
       (mcp.shutdown_afd != NO_PERMISSION))
   {
      pull_down_w = XmCreatePulldownMenu(*menu_w, "Control Pulldown", NULL, 0);
      XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
      mw[CONTROL_W] = XtVaCreateManagedWidget("RControl",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'C',
                              XmNsubMenuId,               pull_down_w,
                              NULL);
      if (mcp.amg_ctrl != NO_PERMISSION)
      {
         cw[AMG_CTRL_W] = XtVaCreateManagedWidget("Start/Stop AMG",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(cw[AMG_CTRL_W], XmNactivateCallback,
                       start_remote_prog, (XtPointer)CONTROL_AMG_SEL);
      }
      if (mcp.fd_ctrl != NO_PERMISSION)
      {
         cw[FD_CTRL_W] = XtVaCreateManagedWidget("Start/Stop FD",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(cw[FD_CTRL_W], XmNactivateCallback,
                       start_remote_prog, (XtPointer)CONTROL_FD_SEL);
      }
      if ((mcp.rr_dc != NO_PERMISSION) || (mcp.rr_hc != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         if (mcp.rr_dc != NO_PERMISSION)
         {
            cw[RR_DC_W] = XtVaCreateManagedWidget("Reread DIR_CONFIG",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(cw[RR_DC_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)REREAD_DIR_CONFIG_SEL);
         }
         if (mcp.rr_hc != NO_PERMISSION)
         {
            cw[RR_HC_W] = XtVaCreateManagedWidget("Reread HOST_CONFIG",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(cw[RR_HC_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)REREAD_HOST_CONFIG_SEL);
         }
      }
      if (mcp.edit_hc != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         cw[EDIT_HC_W] = XtVaCreateManagedWidget("Edit HOST_CONFIG",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(cw[EDIT_HC_W], XmNactivateCallback,
                       start_remote_prog, (XtPointer)EDIT_HC_SEL);
      }
      if (mcp.dir_ctrl != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         cw[DIR_CTRL_W] = XtVaCreateManagedWidget("Directory Control",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(cw[DIR_CTRL_W], XmNactivateCallback,
                       start_remote_prog, (XtPointer)DIR_CTRL_SEL);
      }

      /* Startup/Shutdown of AFD */
      if ((mcp.startup_afd != NO_PERMISSION) ||
          (mcp.shutdown_afd != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
         if (mcp.startup_afd != NO_PERMISSION)
         {
            cw[STARTUP_AFD_W] = XtVaCreateManagedWidget("Startup AFD",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(cw[STARTUP_AFD_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)STARTUP_AFD_SEL);
         }
         if (mcp.shutdown_afd != NO_PERMISSION)
         {
            cw[SHUTDOWN_AFD_W] = XtVaCreateManagedWidget("Shutdown AFD",
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(cw[SHUTDOWN_AFD_W], XmNactivateCallback,
                          start_remote_prog, (XtPointer)SHUTDOWN_AFD_SEL);
         }
      }
   }

   /**********************************************************************/
   /*                           Setup Menu                               */
   /**********************************************************************/
   pull_down_w = XmCreatePulldownMenu(*menu_w, "Setup Pulldown", NULL, 0);
   XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   pullright_font = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_font", NULL, 0);
   pullright_row = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_row", NULL, 0);
   pullright_line_style = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_line_style", NULL, 0);
   pullright_history = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_history", NULL, 0);
   mw[CONFIG_W] = XtVaCreateManagedWidget("Setup",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'S',
                           XmNsubMenuId,               pull_down_w,
                           NULL);
   sw[FONT_W] = XtVaCreateManagedWidget("Font size",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_font,
                           NULL);
   create_pullright_font(pullright_font);
   sw[ROWS_W] = XtVaCreateManagedWidget("Number of rows",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_row,
                           NULL);
   create_pullright_row(pullright_row);
   sw[STYLE_W] = XtVaCreateManagedWidget("Line Style",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_line_style,
                           NULL);
   create_pullright_style(pullright_line_style);
   sw[HISTORY_W] = XtVaCreateManagedWidget("History Length",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_history,
                           NULL);
   create_pullright_history(pullright_history);
   XtVaCreateManagedWidget("Separator",
                           xmSeparatorWidgetClass, pull_down_w,
                           NULL);
   sw[SAVE_W] = XtVaCreateManagedWidget("Save Setup",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           XmNmnemonic,             'a',
                           XmNaccelerator,          "Alt<Key>a",
                           NULL);
   XtAddCallback(sw[SAVE_W], XmNactivateCallback, save_mon_setup_cb,
                 (XtPointer)0);

#ifdef _WITH_HELP_PULLDOWN
   /**********************************************************************/
   /*                            Help Menu                               */
   /**********************************************************************/
   pull_down_w = XmCreatePulldownMenu(*menu_w, "Help Pulldown", NULL, 0);
   XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[HELP_W] = XtVaCreateManagedWidget("Help",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'H',
                           XmNsubMenuId,               pull_down_w,
                           NULL);
   hw[ABOUT_W] = XtVaCreateManagedWidget("About AFD",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[HYPER_W] = XtVaCreateManagedWidget("Hyper Help",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[VERSION_W] = XtVaCreateManagedWidget("Version",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
#endif /* _WITH_HELP_PULLDOWN */

   XtManageChild(*menu_w);
   XtVaSetValues(*menu_w, XmNmenuHelpWidget, mw[HELP_W], NULL);

   return;
}


/*+++++++++++++++++++++++++ init_popup_menu() +++++++++++++++++++++++++++*/
static void
init_popup_menu(Widget line_window_w)
{
   XmString x_string;
   Widget   popupmenu;
   Arg      args[MAXARGS];
   Cardinal argcount;

   argcount = 0;
   XtSetArg(args[argcount], XmNtearOffModel, XmTEAR_OFF_ENABLED); argcount++;
   popupmenu = XmCreateSimplePopupMenu(line_window_w, "popup", args, argcount);

   if ((mcp.show_ms_log != NO_PERMISSION) ||
       (mcp.show_mm_log != NO_PERMISSION) ||
       (mcp.retry != NO_PERMISSION) ||
       (mcp.info != NO_PERMISSION) ||
       (mcp.disable != NO_PERMISSION))
   {
      if (mcp.show_ms_log != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("System Log");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[0] = XmCreatePushButton(popupmenu, "System", args, argcount);
         XtAddCallback(pw[0], XmNactivateCallback,
                       mon_popup_cb, (XtPointer)MON_SYS_LOG_SEL);
         XtManageChild(pw[0]);
         XmStringFree(x_string);
      }
      if (mcp.show_mm_log != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Monitor Log");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[1] = XmCreatePushButton(popupmenu, "Monitor", args, argcount);
         XtAddCallback(pw[1], XmNactivateCallback,
                       mon_popup_cb, (XtPointer)MON_LOG_SEL);
         XtManageChild(pw[1]);
         XmStringFree(x_string);
      }
      if (mcp.retry != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Retry");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Alt<Key>R"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'R'); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[2] = XmCreatePushButton(popupmenu, "Retry", args, argcount);
         XtAddCallback(pw[2], XmNactivateCallback,
                       mon_popup_cb, (XtPointer)MON_RETRY_SEL);
         XtManageChild(pw[2]);
         XmStringFree(x_string);
      }
      if (mcp.info != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Info");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>I"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'I'); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[3] = XmCreatePushButton(popupmenu, "Info", args, argcount);
         XtAddCallback(pw[3], XmNactivateCallback,
                       mon_popup_cb, (XtPointer)MON_INFO_SEL);
         XtManageChild(pw[3]);
         XmStringFree(x_string);
      }
      if (mcp.disable != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Disable");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>D"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'D'); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[4] = XmCreatePushButton(popupmenu, "Disable", args, argcount);
         XtAddCallback(pw[4], XmNactivateCallback,
                       mon_popup_cb, (XtPointer)MON_DISABLE_SEL);
         XtManageChild(pw[4]);
         XmStringFree(x_string);
      }
   }

   XtAddEventHandler(line_window_w,
                     ButtonPressMask | ButtonReleaseMask |
                     Button1MotionMask,
                     False, (XtEventHandler)popup_mon_menu_cb, popupmenu);

   return;
}


/*------------------------ create_pullright_test() ----------------------*/
static void
create_pullright_test(Widget pullright_test)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   if (ping_cmd != NULL)
   {
      /* Create pullright for "Ping" */
      argcount = 0;
      x_string = XmStringCreateLocalized(SHOW_PING_TEST);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      tw[PING_W] = XmCreatePushButton(pullright_test, "Ping",
                                      args, argcount);
      XtAddCallback(tw[PING_W], XmNactivateCallback, mon_popup_cb,
                    (XtPointer)PING_SEL);
      XtManageChild(tw[PING_W]);
      XmStringFree(x_string);
   }

   if (traceroute_cmd != NULL)
   {
      /* Create pullright for "Traceroute" */
      argcount = 0;
      x_string = XmStringCreateLocalized(SHOW_TRACEROUTE_TEST);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      tw[TRACEROUTE_W] = XmCreatePushButton(pullright_test, "Traceroute",
                                            args, argcount);
      XtAddCallback(tw[TRACEROUTE_W], XmNactivateCallback, mon_popup_cb,
                    (XtPointer)TRACEROUTE_SEL);
      XtManageChild(tw[TRACEROUTE_W]);
      XmStringFree(x_string);
   }

   return;
}


/*------------------------ create_pullright_load() ----------------------*/
static void
create_pullright_load(Widget pullright_line_load)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   /* Create pullright for "Files" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_FILE_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[FILE_LOAD_W] = XmCreatePushButton(pullright_line_load, "file",
				        args, argcount);
   XtAddCallback(lw[FILE_LOAD_W], XmNactivateCallback, start_remote_prog,
		 (XtPointer)VIEW_FILE_LOAD_SEL);
   XtManageChild(lw[FILE_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "KBytes" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_KBYTE_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[KBYTE_LOAD_W] = XmCreatePushButton(pullright_line_load, "kbytes",
				        args, argcount);
   XtAddCallback(lw[KBYTE_LOAD_W], XmNactivateCallback, start_remote_prog,
		 (XtPointer)VIEW_KBYTE_LOAD_SEL);
   XtManageChild(lw[KBYTE_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "Connections" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_CONNECTION_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[CONNECTION_LOAD_W] = XmCreatePushButton(pullright_line_load,
                                              "connection",
				              args, argcount);
   XtAddCallback(lw[CONNECTION_LOAD_W], XmNactivateCallback, start_remote_prog,
		 (XtPointer)VIEW_CONNECTION_LOAD_SEL);
   XtManageChild(lw[CONNECTION_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "Active-Transfers" */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_TRANSFER_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[TRANSFER_LOAD_W] = XmCreatePushButton(pullright_line_load,
                                              "active-transfers",
				              args, argcount);
   XtAddCallback(lw[TRANSFER_LOAD_W], XmNactivateCallback, start_remote_prog,
		 (XtPointer)VIEW_TRANSFER_LOAD_SEL);
   XtManageChild(lw[TRANSFER_LOAD_W]);
   XmStringFree(x_string);

   return;
}


/*------------------------ create_pullright_font() ----------------------*/
static void
create_pullright_font(Widget pullright_font)
{
   int             i;
   char            *font[NO_OF_FONTS] =
                   {
                      FONT_0, FONT_1, FONT_2, FONT_3, FONT_4, FONT_5, FONT_6,
                      FONT_7, FONT_8, FONT_9, FONT_10, FONT_11, FONT_12
                   };
   XmString        x_string;
   XmFontListEntry entry;
   XmFontList      tmp_fontlist;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XFontStruct     *p_font_struct;

   for (i = 0; i < NO_OF_FONTS; i++)
   {
      if ((current_font == -1) && (strcmp(font_name, font[i]) == 0))
      {
         current_font = i;
      }
      if ((p_font_struct = XLoadQueryFont(display, font[i])) != NULL)
      {
         if ((entry = XmFontListEntryLoad(display, font[i], XmFONT_IS_FONT, "TAG1")) == NULL)
         {
            (void)fprintf(stderr, "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         tmp_fontlist = XmFontListAppendEntry(NULL, entry);
         XmFontListEntryFree(&entry);

         argcount = 0;
         x_string = XmStringCreateLocalized(font[i]);
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
         XtSetArg(args[argcount], XmNfontList, tmp_fontlist); argcount++;
         fw[i] = XmCreateToggleButton(pullright_font, "font_x", args, argcount);
         XtAddCallback(fw[i], XmNvalueChangedCallback, change_mon_font_cb,
                       (XtPointer)i);
         XtManageChild(fw[i]);
         XmFontListFree(tmp_fontlist);
         XmStringFree(x_string);
         XFreeFont(display, p_font_struct);
      }
   }

   return;
}


/*------------------------ create_pullright_row() -----------------------*/
static void
create_pullright_row(Widget pullright_row)
{
   int      i;
   char     *row[NO_OF_ROWS] =
            {
               ROW_0, ROW_1, ROW_2, ROW_3, ROW_4, ROW_5, ROW_6,
               ROW_7, ROW_8, ROW_9, ROW_10, ROW_11, ROW_12, ROW_13
            };
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   for (i = 0; i < NO_OF_ROWS; i++)
   {
      if ((current_row == -1) && (no_of_rows_set == atoi(row[i])))
      {
         current_row = i;
      }
      argcount = 0;
      x_string = XmStringCreateLocalized(row[i]);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      rw[i] = XmCreateToggleButton(pullright_row, "row_x", args, argcount);
      XtAddCallback(rw[i], XmNvalueChangedCallback, change_mon_rows_cb,
                    (XtPointer)i);
      XtManageChild(rw[i]);
      XmStringFree(x_string);
   }

   return;
}


/*------------------------ create_pullright_style() ---------------------*/
static void
create_pullright_style(Widget pullright_line_style)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   /* Create pullright for "Line style" */
   argcount = 0;
   x_string = XmStringCreateLocalized("Bars only");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[STYLE_0_W] = XmCreateToggleButton(pullright_line_style, "style_0",
				   args, argcount);
   XtAddCallback(lsw[STYLE_0_W], XmNvalueChangedCallback, change_mon_style_cb,
		 (XtPointer)0);
   XtManageChild(lsw[STYLE_0_W]);
   current_style = line_style;
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Characters only");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[STYLE_1_W] = XmCreateToggleButton(pullright_line_style, "style_1",
				   args, argcount);
   XtAddCallback(lsw[STYLE_1_W], XmNvalueChangedCallback, change_mon_style_cb,
		 (XtPointer)1);
   XtManageChild(lsw[STYLE_1_W]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Characters and bars");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[STYLE_2_W] = XmCreateToggleButton(pullright_line_style, "style_2",
				   args, argcount);
   XtAddCallback(lsw[STYLE_2_W], XmNvalueChangedCallback, change_mon_style_cb,
		 (XtPointer)2);
   XtManageChild(lsw[STYLE_2_W]);
   XmStringFree(x_string);

   return;
}


/*---------------------- create_pullright_history() ---------------------*/
static void
create_pullright_history(Widget pullright_history)
{
   int      i;
   char     *his_log[NO_OF_HISTORY_LOGS] =
            {
               HIS_0, HIS_1, HIS_2, HIS_3, HIS_4
            };
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

   for (i = 0; i < NO_OF_HISTORY_LOGS; i++)
   {
      if ((current_his_log == -1) && (his_log_set == atoi(his_log[i])))
      {
         current_his_log = i;
      }
      argcount = 0;
      x_string = XmStringCreateLocalized(his_log[i]);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      hlw[i] = XmCreateToggleButton(pullright_history, "history_x", args,
                                    argcount);
      XtAddCallback(hlw[i], XmNvalueChangedCallback, change_mon_history_cb,
                    (XtPointer)i);
      XtManageChild(hlw[i]);
      XmStringFree(x_string);
   }

   return;
}


/*-------------------------- eval_permissions() -------------------------*/
/*                           ------------------                          */
/* Description: Checks the permissions on what the user may do.          */
/* Returns    : Fills the global structure acp with data.                */
/*-----------------------------------------------------------------------*/
static void
eval_permissions(char *perm_buffer)
{
   char *ptr;

   /*
    * If we find 'all' right at the beginning, no further evaluation
    * is needed, since the user has all permissions.
    */
   if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
       (perm_buffer[2] == 'l'))
   {
      mcp.mon_ctrl_list      = NULL;
      mcp.amg_ctrl           = YES;   /* Start/Stop the AMG    */
      mcp.fd_ctrl            = YES;   /* Start/Stop the FD     */
      mcp.rr_dc              = YES;   /* Reread DIR_CONFIG     */
      mcp.rr_hc              = YES;   /* Reread HOST_CONFIG    */
      mcp.startup_afd        = YES;   /* Startup AFD           */
      mcp.shutdown_afd       = YES;   /* Shutdown AFD          */
      mcp.info               = YES;
      mcp.info_list          = NULL;
      mcp.retry              = YES;
      mcp.retry_list         = NULL;
      mcp.disable            = YES;
      mcp.disable_list       = NULL;
      mcp.afd_ctrl           = YES;   /* AFD Control Dialog    */
      mcp.afd_ctrl_list      = NULL;
      mcp.show_slog          = YES;   /* View the system log   */
      mcp.show_slog_list     = NULL;
      mcp.show_rlog          = YES;   /* View the receive log  */
      mcp.show_rlog_list     = NULL;
      mcp.show_tlog          = YES;   /* View the transfer log */
      mcp.show_tlog_list     = NULL;
      mcp.show_ilog          = YES;   /* View the input log    */
      mcp.show_ilog_list     = NULL;
      mcp.show_olog          = YES;   /* View the output log   */
      mcp.show_olog_list     = NULL;
      mcp.show_elog          = YES;   /* View the delete log   */
      mcp.show_elog_list     = NULL;
      mcp.show_queue         = YES;   /* View the AFD queue    */
      mcp.edit_hc            = YES;   /* Edit Host Configuration */
      mcp.edit_hc_list       = NULL;
      mcp.dir_ctrl           = YES;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, MON_CTRL_PERM)) == NULL)
      {
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         free(perm_buffer);
         exit(INCORRECT);
      }
      else
      {
         /*
          * For future use. Allow to limit for host names
          * as well.
          */
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            store_host_names(mcp.mon_ctrl_list, ptr + 1);
         }
      }

      /* May the user start/stop the AMG? */
      if ((ptr = posi(perm_buffer, AMG_CTRL_PERM)) == NULL)
      {
         /* The user may NOT do any resending. */
         mcp.amg_ctrl = NO_PERMISSION;
      }
      else
      {
         mcp.amg_ctrl = NO_LIMIT;
      }

      /* May the user start/stop the FD? */
      if ((ptr = posi(perm_buffer, FD_CTRL_PERM)) == NULL)
      {
         /* The user may NOT do any resending. */
         mcp.fd_ctrl = NO_PERMISSION;
      }
      else
      {
         mcp.fd_ctrl = NO_LIMIT;
      }

      /* May the user reread the DIR_CONFIG? */
      if ((ptr = posi(perm_buffer, RR_DC_PERM)) == NULL)
      {
         /* The user may NOT do reread the DIR_CONFIG. */
         mcp.rr_dc = NO_PERMISSION;
      }
      else
      {
         mcp.rr_dc = NO_LIMIT;
      }

      /* May the user reread the HOST_CONFIG? */
      if ((ptr = posi(perm_buffer, RR_HC_PERM)) == NULL)
      {
         /* The user may NOT do reread the HOST_CONFIG. */
         mcp.rr_hc = NO_PERMISSION;
      }
      else
      {
         mcp.rr_hc = NO_LIMIT;
      }

      /* May the user use the dir_ctrl dialog? */
      if ((ptr = posi(perm_buffer, DIR_CTRL_PERM)) == NULL)
      {
         /* The user may NOT use the dir_ctrl dialog. */
         mcp.dir_ctrl = NO_PERMISSION;
      }
      else
      {
         mcp.dir_ctrl = NO_LIMIT;
      }

      /* May the user startup the AFD? */
      if ((ptr = posi(perm_buffer, STARTUP_PERM)) == NULL)
      {
         /* The user may NOT do a startup the AFD. */
         mcp.startup_afd = NO_PERMISSION;
      }
      else
      {
         mcp.startup_afd = NO_LIMIT;
      }

      /* May the user shutdown the AFD? */
      if ((ptr = posi(perm_buffer, SHUTDOWN_PERM)) == NULL)
      {
         /* The user may NOT do a shutdown the AFD. */
         mcp.shutdown_afd = NO_PERMISSION;
      }
      else
      {
         mcp.shutdown_afd = NO_LIMIT;
      }

      /* May the user view the information of a AFD? */
      if ((ptr = posi(perm_buffer, INFO_PERM)) == NULL)
      {
         /* The user may NOT view any information. */
         mcp.info = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.info = store_host_names(mcp.info_list, ptr + 1);
         }
         else
         {
            mcp.info = NO_LIMIT;
         }
      }

      /* May the user use the retry button for a particular AFD? */
      if ((ptr = posi(perm_buffer, RETRY_PERM)) == NULL)
      {
         /* The user may NOT use the retry button. */
         mcp.retry = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.retry = store_host_names(mcp.retry_list, ptr + 1);
         }
         else
         {
            mcp.retry = NO_LIMIT;
         }
      }

      /* May the user use the disable button for a particular AFD? */
      if ((ptr = posi(perm_buffer, DISABLE_AFD_PERM)) == NULL)
      {
         /* The user may NOT use the disable button. */
         mcp.disable = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.disable = store_host_names(mcp.disable_list, ptr + 1);
         }
         else
         {
            mcp.disable = NO_LIMIT;
         }
      }

      /* May the user call the AFD Control Dialog? */
      if ((ptr = posi(perm_buffer, RAFD_CTRL_PERM)) == NULL)
      {
         /* The user may NOT view the system log. */
         mcp.afd_ctrl = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.afd_ctrl = store_host_names(mcp.afd_ctrl_list, ptr + 1);
         }
         else
         {
            mcp.afd_ctrl = NO_LIMIT;
         }
      }

      /* May the user view the system log? */
      if ((ptr = posi(perm_buffer, SHOW_SLOG_PERM)) == NULL)
      {
         /* The user may NOT view the system log. */
         mcp.show_slog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.show_slog = store_host_names(mcp.show_slog_list, ptr + 1);
         }
         else
         {
            mcp.show_slog = NO_LIMIT;
         }
      }

      /* May the user view the receive log? */
      if ((ptr = posi(perm_buffer, SHOW_RLOG_PERM)) == NULL)
      {
         /* The user may NOT view the receive log. */
         mcp.show_rlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.show_rlog = store_host_names(mcp.show_rlog_list, ptr + 1);
         }
         else
         {
            mcp.show_rlog = NO_LIMIT;
         }
      }

      /* May the user view the transfer log? */
      if ((ptr = posi(perm_buffer, SHOW_TLOG_PERM)) == NULL)
      {
         /* The user may NOT view the transfer log. */
         mcp.show_tlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.show_tlog = store_host_names(mcp.show_tlog_list, ptr + 1);
         }
         else
         {
            mcp.show_tlog = NO_LIMIT;
         }
      }

      /* May the user view the input log? */
      if ((ptr = posi(perm_buffer, SHOW_ILOG_PERM)) == NULL)
      {
         /* The user may NOT view the input log. */
         mcp.show_ilog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.show_ilog = store_host_names(mcp.show_ilog_list, ptr + 1);
         }
         else
         {
            mcp.show_ilog = NO_LIMIT;
         }
      }

      /* May the user view the output log? */
      if ((ptr = posi(perm_buffer, SHOW_OLOG_PERM)) == NULL)
      {
         /* The user may NOT view the output log. */
         mcp.show_olog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.show_olog = store_host_names(mcp.show_olog_list, ptr + 1);
         }
         else
         {
            mcp.show_olog = NO_LIMIT;
         }
      }

      /* May the user view the delete log? */
      if ((ptr = posi(perm_buffer, SHOW_ELOG_PERM)) == NULL)
      {
         /* The user may NOT view the delete log. */
         mcp.show_elog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.show_elog = store_host_names(mcp.show_elog_list, ptr + 1);
         }
         else
         {
            mcp.show_elog = NO_LIMIT;
         }
      }

      /* May the user view the AFD queue? */
      if ((ptr = posi(perm_buffer, SHOW_QUEUE_PERM)) == NULL)
      {
         /* The user may NOT view the delete log. */
         mcp.show_queue = NO_PERMISSION;
      }
      else
      {
         mcp.show_queue = NO_LIMIT;
      }

      /* May the user edit the host configuration file? */
      if ((ptr = posi(perm_buffer, EDIT_HC_PERM)) == NULL)
      {
         /* The user may NOT edit the host configuration file. */
         mcp.edit_hc = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            mcp.edit_hc = store_host_names(mcp.edit_hc_list, ptr + 1);
         }
         else
         {
            mcp.edit_hc = NO_LIMIT;
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ mon_ctrl_exit() +++++++++++++++++++++++++++*/
static void
mon_ctrl_exit(void)
{
   int i;

   for (i = 0; i < no_of_active_process; i++)
   {
      if (kill(apps_list[i].pid, SIGINT) < 0)
      {
         (void)xrec(appshell, WARN_DIALOG,
                    "Failed to kill() process %s (%d) : %s",
                    apps_list[i].progname, apps_list[i].pid, strerror(errno));
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);

   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
                 __FILE__, __LINE__);

   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
