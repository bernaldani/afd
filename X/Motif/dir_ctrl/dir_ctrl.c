/*
 *  dir_ctrl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dir_ctrl - controls and monitors the directories from the DIR_CONFIG
 **
 ** SYNOPSIS
 **   dir_ctrl [--version][-w <work dir>][-no_input][-f <numeric font>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.2000 H.Kiehl Created
 **   05.05.2002 H.Kiehl Show the number files currently in the directory.
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

#include "dir_ctrl.h"
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
#include "version.h"
#include "permission.h"

/* Global variables */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_dir;
GC                         letter_gc,
                           normal_letter_gc,
                           locked_letter_gc,
                           color_letter_gc,
                           default_bg_gc,
                           normal_bg_gc,
                           locked_bg_gc,
                           label_bg_gc,
                           red_color_letter_gc,
                           fr_bar_gc,
                           tr_bar_gc,
                           color_gc,
                           black_line_gc,
                           white_line_gc;
Colormap                   default_cmap;
XFontStruct                *font_struct;
XmFontList                 fontlist = NULL;
Widget                     mw[4],       /* Main menu */
                           dw[3],       /* Directory menu */
                           vw[8],       /* View menu */
                           sw[4],       /* Setup menu */
                           hw[3],       /* Help menu */
                           fw[13],      /* Select font */
                           rw[14],      /* Select rows */
                           lw[4],       /* AFD load */
                           lsw[3];      /* Select line style */
Widget                     appshell,
                           label_window_w,
                           line_window_w,
                           transviewshell = NULL;
Window                     label_window,
                           line_window;
float                      max_bar_length;
int                        bar_thickness_2,
                           current_font = -1,
                           current_row = -1,
                           current_style = -1,
                           fra_fd = -1,
                           fra_id,
                           no_input,
                           no_of_active_process = 0,
                           line_length,
                           line_height = 0,
                           magic_value,
                           no_selected,
                           no_selected_static,
                           no_of_columns,
                           no_of_rows,
                           no_of_rows_set,
                           no_of_dirs,
                           no_of_jobs_selected,
                           no_of_short_lines, /* Not yet used. */
                           redraw_time_line,
                           sys_log_fd = STDERR_FILENO,
                           window_width,
                           window_height,
                           x_offset_bars,
                           x_offset_characters,
                           x_offset_dir_full,
                           x_offset_type;
#ifndef _NO_MMAP
off_t                      fra_size;
#endif
unsigned long              color_pool[COLOR_POOL_SIZE];
unsigned int               glyph_height,
                           glyph_width,
                           text_offset;
char                       work_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           afd_active_file[MAX_PATH_LENGTH],
                           line_style,
                           font_name[20],
                           blink_flag,
                           user[MAX_FULL_USER_ID_LENGTH],
                           username[MAX_USER_NAME_LENGTH];
clock_t                    clktck;
struct tms                 tmsdummy;
struct apps_list           *apps_list = NULL;
struct dir_line            *connect_data;
struct fileretrieve_status *fra;
struct dir_control_perm    dcp;

/* Local function prototypes */
static void                dir_ctrl_exit(void),
                           create_pullright_font(Widget),
                           create_pullright_load(Widget),
                           create_pullright_row(Widget),
                           create_pullright_style(Widget),
                           eval_permissions(char *),
                           init_menu_bar(Widget, Widget *),
                           init_popup_menu(Widget),
                           init_dir_ctrl(int *, char **, char *),
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
                    ".dir_ctrl*background : NavajoWhite2",
                    NULL
                 };
   Widget        mainform_w,
                 mainwindow,
                 menu_w;
   Arg           args[MAXARGS];
   Cardinal      argcount;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   init_dir_ctrl(&argc, argv, window_title);

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
   setup_dir_window(font_name);

   /* Get window size */
   (void)dir_window_size(&window_width, &window_height);

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
                 (XtCallbackProc)dir_expose_handler_label, (XtPointer)0);

   /* Add call back to handle expose events for the line window */
   XtAddCallback(line_window_w, XmNexposeCallback,
                 (XtCallbackProc)dir_expose_handler_line, NULL);

   if (no_input == False)
   {
      XtAddEventHandler(line_window_w,
                        ButtonPressMask | Button1MotionMask,
                        False, (XtEventHandler)dir_input, NULL);

      /* Set toggle button for font|row */
      XtVaSetValues(fw[current_font], XmNset, True, NULL);
      XtVaSetValues(rw[current_row], XmNset, True, NULL);
      XtVaSetValues(lsw[current_style], XmNset, True, NULL);

      /* Setup popup menu */
      init_popup_menu(line_window_w);

      XtAddEventHandler(line_window_w,
                        EnterWindowMask | LeaveWindowMask,
                        False, (XtEventHandler)dir_focus, NULL);
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
                 "Failed to set signal handlers for dir_ctrl : %s",
                 strerror(errno));
   }

   /* Exit handler so we can close applications that the user started. */
   if (atexit(dir_ctrl_exit) != 0)
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set exit handler for dir_ctrl : %s\n\nWill not be able to close applications when terminating.",
                 strerror(errno));
   }

   /* Get window ID of three main windows. */
   label_window = XtWindow(label_window_w);
   line_window = XtWindow(line_window_w);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_dir_ctrl() ++++++++++++++++++++++++++*/
static void
init_dir_ctrl(int *argc, char *argv[], char *window_title)
{
   int           i;
   char          *perm_buffer,
                 hostname[MAX_AFD_NAME_LENGTH],
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
   if (get_afd_path(argc, argv, work_dir) < 0)
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
         dcp.dir_ctrl_list      = NULL;
         dcp.info               = YES;
         dcp.info_list          = NULL;
         dcp.disable            = YES;
         dcp.disable_list       = NULL;
         dcp.show_slog          = YES; /* View the system log    */
         dcp.show_slog_list     = NULL;
         dcp.show_rlog          = YES; /* View the receive log   */
         dcp.show_rlog_list     = NULL;
         dcp.show_tlog          = YES; /* View the transfer log  */
         dcp.show_tlog_list     = NULL;
         dcp.show_ilog          = YES; /* View the input log     */
         dcp.show_ilog_list     = NULL;
         dcp.show_olog          = YES; /* View the output log    */
         dcp.show_olog_list     = NULL;
         dcp.show_elog          = YES; /* View the delete log    */
         dcp.show_elog_list     = NULL;
         dcp.show_queue         = YES; /* View show_queue dialog */
         dcp.show_queue_list    = NULL;
         break;

      default : /* Something must be wrong! */
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   (void)strcpy(sys_log_fifo, p_work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcpy(afd_active_file, sys_log_fifo);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);

   /* Create and open sys_log fifo. */
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

   /* Prepare title for dir_ctrl window */
#ifdef PRE_RELEASE
   (void)sprintf(window_title, "DIR_CTRL %d.%d.%d-pre%d ",
                 MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)sprintf(window_title, "DIR_CTRL %d.%d.%d ", MAJOR, MINOR, BUG_FIX);
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
      (void)rec(sys_log_fd, FATAL_SIGN, "getpwuid() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)strcpy(username, pwd->pw_name);

   /*
    * Attach to the MSA and get the number of AFD's
    * and the msa_id of the MSA.
    */
   if (fra_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FRA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      (void)fprintf(stderr, "Could not get clock ticks per second.\n");
      exit(INCORRECT);
   }

   /* Allocate memory for local 'FRA' */
   if ((connect_data = calloc(no_of_dirs, sizeof(struct dir_line))) == NULL)
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
   read_setup(DIR_CTRL, NULL, NULL, NULL, 0, 0);

   /* Determine the default bar length */
   max_bar_length  = 6 * BAR_LENGTH_MODIFIER;

   /* Initialise all display data for each directory to monitor */
   for (i = 0; i < no_of_dirs; i++)
   {
      (void)strcpy(connect_data[i].dir_alias, fra[i].dir_alias);
      (void)sprintf(connect_data[i].dir_display_str, "%-*s",
                    MAX_DIR_ALIAS_LENGTH, connect_data[i].dir_alias);
      connect_data[i].dir_status = fra[i].dir_status;
      connect_data[i].bytes_received = fra[i].bytes_received;
      connect_data[i].files_received = fra[i].files_received;
      connect_data[i].dir_flag = fra[i].dir_flag;
      connect_data[i].files_in_dir = fra[i].files_in_dir;
      connect_data[i].files_queued = fra[i].files_queued;
      connect_data[i].bytes_in_dir = fra[i].bytes_in_dir;
      connect_data[i].bytes_in_queue = fra[i].bytes_in_queue;
      connect_data[i].max_process = fra[i].max_process;
      connect_data[i].no_of_process = fra[i].no_of_process;
      CREATE_FC_STRING(connect_data[i].str_files_in_dir,
                       connect_data[i].files_in_dir);
      CREATE_FS_STRING(connect_data[i].str_bytes_in_dir,
                       connect_data[i].bytes_in_dir);
      CREATE_FC_STRING(connect_data[i].str_files_queued,
                       connect_data[i].files_queued);
      CREATE_FS_STRING(connect_data[i].str_bytes_queued,
                       connect_data[i].bytes_in_queue);
      CREATE_EC_STRING(connect_data[i].str_np, connect_data[i].no_of_process);
      connect_data[i].last_retrieval = fra[i].last_retrieval;
      connect_data[i].bytes_per_sec = 0;
      connect_data[i].prev_bytes_per_sec = 0;
      connect_data[i].str_tr[0] = connect_data[i].str_tr[1] = ' ';
      connect_data[i].str_tr[2] = '0'; connect_data[i].str_tr[3] = 'B';
      connect_data[i].str_tr[4] = '\0';
      connect_data[i].average_tr = 0.0;
      connect_data[i].files_per_sec = 0;
      connect_data[i].prev_files_per_sec = 0;
      connect_data[i].max_average_tr = 0.0;
      connect_data[i].str_fr[0] = ' ';
      connect_data[i].str_fr[1] = '0';
      connect_data[i].str_fr[2] = '.';
      connect_data[i].str_fr[3] = '0';
      connect_data[i].str_fr[4] = '\0';
      connect_data[i].average_fr = 0.0;
      connect_data[i].max_average_fr = 0.0;
      connect_data[i].bar_length[BYTE_RATE_BAR_NO] = 0;
      connect_data[i].bar_length[FILE_RATE_BAR_NO] = 0;
      connect_data[i].start_time = times(&tmsdummy);
      connect_data[i].inverse = OFF;
      connect_data[i].expose_flag = NO;
   }

   no_selected = no_selected_static = 0;
   redraw_time_line = STARTING_DIR_REDRAW_TIME;

   return;
}


/*+++++++++++++++++++++++++++ init_menu_bar() +++++++++++++++++++++++++++*/
static void
init_menu_bar(Widget mainform_w, Widget *menu_w)
{
   Arg      args[MAXARGS];
   Cardinal argcount;
   Widget   dir_pull_down_w,
            view_pull_down_w,
            setup_pull_down_w,
#ifdef _WITH_HELP_PULLDOWN
            help_pull_down_w,
#endif
            pullright_font,
            pullright_load,
            pullright_row,
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
   /*                         Directory Menu                             */
   /**********************************************************************/
   dir_pull_down_w = XmCreatePulldownMenu(*menu_w,
                                          "Directory Pulldown", NULL, 0);
   XtVaSetValues(dir_pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[DIR_W] = XtVaCreateManagedWidget("Dir",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'D',
                              XmNsubMenuId,               dir_pull_down_w,
                              NULL);

   if ((dcp.disable != NO_PERMISSION) || (dcp.afd_load != NO_PERMISSION))
   {
      if (dcp.disable != NO_PERMISSION)
      {
         dw[DIR_DISABLE_W] = XtVaCreateManagedWidget("Enable/Disable",
                              xmPushButtonWidgetClass, dir_pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(dw[DIR_DISABLE_W], XmNactivateCallback, dir_popup_cb,
                       (XtPointer)DIR_DISABLE_SEL);
      }
      if (dcp.afd_load != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, dir_pull_down_w,
                              NULL);
         pullright_load = XmCreateSimplePulldownMenu(dir_pull_down_w,
                                                     "pullright_load",
                                                     NULL, 0);
         dw[DIR_VIEW_LOAD_W] = XtVaCreateManagedWidget("Load",
                              xmCascadeButtonWidgetClass, dir_pull_down_w,
                              XmNfontList,                fontlist,
                              XmNsubMenuId,               pullright_load,
                              NULL);
         create_pullright_load(pullright_load);
      }
      XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, dir_pull_down_w,
                              XmNseparatorType,       XmDOUBLE_LINE,
                              NULL);
   }
   dw[DIR_EXIT_W] = XtVaCreateManagedWidget("Exit",
                              xmPushButtonWidgetClass, dir_pull_down_w,
                              XmNfontList,             fontlist,
                              XmNmnemonic,             'x',
                              XmNaccelerator,          "Alt<Key>x",
                              NULL);
   XtAddCallback(dw[DIR_EXIT_W], XmNactivateCallback, dir_popup_cb,
                 (XtPointer)EXIT_SEL);

   /**********************************************************************/
   /*                           View Menu                                */
   /**********************************************************************/
   if ((dcp.show_slog != NO_PERMISSION) ||
       (dcp.show_rlog != NO_PERMISSION) ||
       (dcp.show_tlog != NO_PERMISSION) ||
       (dcp.show_ilog != NO_PERMISSION) ||
       (dcp.show_olog != NO_PERMISSION) ||
       (dcp.show_elog != NO_PERMISSION) ||
       (dcp.show_queue != NO_PERMISSION) ||
       (dcp.info != NO_PERMISSION))
   {
      view_pull_down_w = XmCreatePulldownMenu(*menu_w,
                                              "View Pulldown", NULL, 0);
      XtVaSetValues(view_pull_down_w,
                    XmNtearOffModel, XmTEAR_OFF_ENABLED,
                    NULL);
      mw[LOG_W] = XtVaCreateManagedWidget("View",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
                              XmNmnemonic,                'R',
                              XmNsubMenuId,               view_pull_down_w,
                              NULL);
      if ((dcp.show_slog != NO_PERMISSION) ||
          (dcp.show_rlog != NO_PERMISSION) ||
          (dcp.show_tlog != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, view_pull_down_w,
                              NULL);
         if (dcp.show_slog != NO_PERMISSION)
         {
            vw[DIR_SYSTEM_W] = XtVaCreateManagedWidget("System Log",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'S',
                                 XmNaccelerator,          "Alt<Key>S",
                                 NULL);
            XtAddCallback(vw[DIR_SYSTEM_W], XmNactivateCallback, dir_popup_cb,
                          (XtPointer)S_LOG_SEL);
         }
         if (dcp.show_rlog != NO_PERMISSION)
         {
            vw[DIR_RECEIVE_W] = XtVaCreateManagedWidget("Receive Log",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'R',
                                 XmNaccelerator,          "Alt<Key>R",
                                 NULL);
            XtAddCallback(vw[DIR_RECEIVE_W], XmNactivateCallback, dir_popup_cb,
                          (XtPointer)R_LOG_SEL);
         }
         if (dcp.show_tlog != NO_PERMISSION)
         {
            vw[DIR_TRANS_W] = XtVaCreateManagedWidget("Transfer Log",
                                 xmPushButtonWidgetClass, view_pull_down_w,
                                 XmNfontList,             fontlist,
                                 XmNmnemonic,             'T',
                                 XmNaccelerator,          "Alt<Key>T",
                                 NULL);
            XtAddCallback(vw[DIR_TRANS_W], XmNactivateCallback, dir_popup_cb,
                          (XtPointer)T_LOG_SEL);
         }
      }
      if ((dcp.show_ilog != NO_PERMISSION) ||
          (dcp.show_olog != NO_PERMISSION) ||
          (dcp.show_elog != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, view_pull_down_w,
                              NULL);
         if (dcp.show_ilog != NO_PERMISSION)
         {
            vw[DIR_INPUT_W] = XtVaCreateManagedWidget("Input Log",
                              xmPushButtonWidgetClass, view_pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(vw[DIR_INPUT_W], XmNactivateCallback, dir_popup_cb,
                          (XtPointer)I_LOG_SEL);
         }
         if (dcp.show_olog != NO_PERMISSION)
         {
            vw[DIR_OUTPUT_W] = XtVaCreateManagedWidget("Output Log",
                              xmPushButtonWidgetClass, view_pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(vw[DIR_OUTPUT_W], XmNactivateCallback, dir_popup_cb,
                          (XtPointer)O_LOG_SEL);
         }
         if (dcp.show_elog != NO_PERMISSION)
         {
            vw[DIR_DELETE_W] = XtVaCreateManagedWidget("Delete Log",
                              xmPushButtonWidgetClass, view_pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
            XtAddCallback(vw[DIR_DELETE_W], XmNactivateCallback, dir_popup_cb,
                          (XtPointer)E_LOG_SEL);
         }
      }
      if (dcp.show_queue != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, view_pull_down_w,
                              NULL);
         vw[DIR_SHOW_QUEUE_W] = XtVaCreateManagedWidget("Queue",
                           xmPushButtonWidgetClass, view_pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
         XtAddCallback(vw[DIR_SHOW_QUEUE_W], XmNactivateCallback, dir_popup_cb,
                       (XtPointer)SHOW_QUEUE_SEL);
      }
      if (dcp.info != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, view_pull_down_w,
                              NULL);
         vw[DIR_INFO_W] = XtVaCreateManagedWidget("Info",
                              xmPushButtonWidgetClass, view_pull_down_w,
                              XmNfontList,             fontlist,
                              NULL);
         XtAddCallback(vw[DIR_INFO_W], XmNactivateCallback, dir_popup_cb,
                       (XtPointer)DIR_INFO_SEL);
      }
   }

   /**********************************************************************/
   /*                           Setup Menu                               */
   /**********************************************************************/
   setup_pull_down_w = XmCreatePulldownMenu(*menu_w, "Setup Pulldown", NULL, 0);
   XtVaSetValues(setup_pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   pullright_font = XmCreateSimplePulldownMenu(setup_pull_down_w,
                                               "pullright_font", NULL, 0);
   pullright_row = XmCreateSimplePulldownMenu(setup_pull_down_w,
                                              "pullright_row", NULL, 0);
   pullright_line_style = XmCreateSimplePulldownMenu(setup_pull_down_w,
                                              "pullright_line_style", NULL, 0);
   mw[CONFIG_W] = XtVaCreateManagedWidget("Setup",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'S',
                           XmNsubMenuId,               setup_pull_down_w,
                           NULL);
   sw[FONT_W] = XtVaCreateManagedWidget("Font size",
                           xmCascadeButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_font,
                           NULL);
   create_pullright_font(pullright_font);
   sw[ROWS_W] = XtVaCreateManagedWidget("Number of rows",
                           xmCascadeButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_row,
                           NULL);
   create_pullright_row(pullright_row);
   sw[STYLE_W] = XtVaCreateManagedWidget("Line Style",
                           xmCascadeButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_line_style,
                           NULL);
   create_pullright_style(pullright_line_style);
   XtVaCreateManagedWidget("Separator",
                           xmSeparatorWidgetClass, setup_pull_down_w,
                           NULL);
   sw[SAVE_W] = XtVaCreateManagedWidget("Save Setup",
                           xmPushButtonWidgetClass, setup_pull_down_w,
                           XmNfontList,             fontlist,
                           XmNmnemonic,             'a',
                           XmNaccelerator,          "Alt<Key>a",
                           NULL);
   XtAddCallback(sw[SAVE_W], XmNactivateCallback, save_dir_setup_cb, (XtPointer)0);

#ifdef _WITH_HELP_PULLDOWN
   /**********************************************************************/
   /*                            Help Menu                               */
   /**********************************************************************/
   help_pull_down_w = XmCreatePulldownMenu(*menu_w, "Help Pulldown", NULL, 0);
   XtVaSetValues(help_pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[HELP_W] = XtVaCreateManagedWidget("Help",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
                           XmNmnemonic,                'H',
                           XmNsubMenuId,               help_pull_down_w,
                           NULL);
   hw[ABOUT_W] = XtVaCreateManagedWidget("About AFD",
                           xmPushButtonWidgetClass, help_pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[HYPER_W] = XtVaCreateManagedWidget("Hyper Help",
                           xmPushButtonWidgetClass, help_pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[VERSION_W] = XtVaCreateManagedWidget("Version",
                           xmPushButtonWidgetClass, help_pull_down_w,
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
   Widget   popupmenu,
            pushbutton;
   Arg      args[MAXARGS];
   Cardinal argcount;

   argcount = 0;
   XtSetArg(args[argcount], XmNtearOffModel, XmTEAR_OFF_ENABLED); argcount++;
   popupmenu = XmCreateSimplePopupMenu(line_window_w, "popup", args, argcount);

   if ((dcp.show_rlog != NO_PERMISSION) ||
       (dcp.disable != NO_PERMISSION) ||
       (dcp.info != NO_PERMISSION))
   {
      if (dcp.show_rlog != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Receive Log");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Receive", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       dir_popup_cb, (XtPointer)R_LOG_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (dcp.disable != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Retry");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Disable", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       dir_popup_cb, (XtPointer)DIR_DISABLE_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
      if (dcp.info != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Info");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>I"); argcount++;
         XtSetArg(args[argcount], XmNmnemonic, 'I'); argcount++;
         pushbutton = XmCreatePushButton(popupmenu, "Info", args, argcount);
         XtAddCallback(pushbutton, XmNactivateCallback,
                       dir_popup_cb, (XtPointer)DIR_INFO_SEL);
         XtManageChild(pushbutton);
         XmStringFree(x_string);
      }
   }

   XtAddEventHandler(line_window_w,
                     ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
                     False, (XtEventHandler)popup_dir_menu_cb, popupmenu);

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
   XtAddCallback(lw[FILE_LOAD_W], XmNactivateCallback, dir_popup_cb,
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
   XtAddCallback(lw[KBYTE_LOAD_W], XmNactivateCallback, dir_popup_cb,
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
   XtAddCallback(lw[CONNECTION_LOAD_W], XmNactivateCallback, dir_popup_cb,
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
   XtAddCallback(lw[TRANSFER_LOAD_W], XmNactivateCallback, dir_popup_cb,
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
         XtAddCallback(fw[i], XmNvalueChangedCallback, change_dir_font_cb,
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
      XtAddCallback(rw[i], XmNvalueChangedCallback, change_dir_rows_cb,
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
   XtAddCallback(lsw[STYLE_0_W], XmNvalueChangedCallback, change_dir_style_cb,
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
   XtAddCallback(lsw[STYLE_1_W], XmNvalueChangedCallback, change_dir_style_cb,
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
   XtAddCallback(lsw[STYLE_2_W], XmNvalueChangedCallback, change_dir_style_cb,
		 (XtPointer)2);
   XtManageChild(lsw[STYLE_2_W]);
   XmStringFree(x_string);

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
      dcp.dir_ctrl_list      = NULL;
      dcp.info               = YES;
      dcp.info_list          = NULL;
      dcp.disable            = YES;
      dcp.disable_list       = NULL;
      dcp.show_slog          = YES;   /* View the system log   */
      dcp.show_slog_list     = NULL;
      dcp.show_rlog          = YES;   /* View the receive log  */
      dcp.show_rlog_list     = NULL;
      dcp.show_tlog          = YES;   /* View the transfer log */
      dcp.show_tlog_list     = NULL;
      dcp.show_ilog          = YES;   /* View the input log    */
      dcp.show_ilog_list     = NULL;
      dcp.show_olog          = YES;   /* View the output log   */
      dcp.show_olog_list     = NULL;
      dcp.show_elog          = YES;   /* View the delete log   */
      dcp.show_elog_list     = NULL;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, DIR_CTRL_PERM)) == NULL)
      {
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         free(perm_buffer);
         exit(INCORRECT);
      }
      else
      {
         /*
          * For future use. Allow to limit for directories as well.
          */
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            store_host_names(dcp.dir_ctrl_list, ptr + 1);
         }
      }

      /* May the user view the information of a directory? */
      if ((ptr = posi(perm_buffer, DIR_INFO_PERM)) == NULL)
      {
         /* The user may NOT view any information. */
         dcp.info = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.info = store_host_names(dcp.info_list, ptr + 1);
         }
         else
         {
            dcp.info = NO_LIMIT;
         }
      }

      /* May the user use the disable button for a particular directory? */
      if ((ptr = posi(perm_buffer, DISABLE_DIR_PERM)) == NULL)
      {
         /* The user may NOT use the disable button. */
         dcp.disable = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.disable = store_host_names(dcp.disable_list, ptr + 1);
         }
         else
         {
            dcp.disable = NO_LIMIT;
         }
      }

      /* May the user view the system log? */
      if ((ptr = posi(perm_buffer, SHOW_SLOG_PERM)) == NULL)
      {
         /* The user may NOT view the system log. */
         dcp.show_slog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.show_slog = store_host_names(dcp.show_slog_list, ptr + 1);
         }
         else
         {
            dcp.show_slog = NO_LIMIT;
         }
      }

      /* May the user view the receive log? */
      if ((ptr = posi(perm_buffer, SHOW_RLOG_PERM)) == NULL)
      {
         /* The user may NOT view the receive log. */
         dcp.show_rlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.show_rlog = store_host_names(dcp.show_rlog_list, ptr + 1);
         }
         else
         {
            dcp.show_rlog = NO_LIMIT;
         }
      }

      /* May the user view the transfer log? */
      if ((ptr = posi(perm_buffer, SHOW_TLOG_PERM)) == NULL)
      {
         /* The user may NOT view the transfer log. */
         dcp.show_tlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.show_tlog = store_host_names(dcp.show_tlog_list, ptr + 1);
         }
         else
         {
            dcp.show_tlog = NO_LIMIT;
         }
      }

      /* May the user view the input log? */
      if ((ptr = posi(perm_buffer, SHOW_ILOG_PERM)) == NULL)
      {
         /* The user may NOT view the input log. */
         dcp.show_ilog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.show_ilog = store_host_names(dcp.show_ilog_list, ptr + 1);
         }
         else
         {
            dcp.show_ilog = NO_LIMIT;
         }
      }

      /* May the user view the output log? */
      if ((ptr = posi(perm_buffer, SHOW_OLOG_PERM)) == NULL)
      {
         /* The user may NOT view the output log. */
         dcp.show_olog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.show_olog = store_host_names(dcp.show_olog_list, ptr + 1);
         }
         else
         {
            dcp.show_olog = NO_LIMIT;
         }
      }

      /* May the user view the delete log? */
      if ((ptr = posi(perm_buffer, SHOW_ELOG_PERM)) == NULL)
      {
         /* The user may NOT view the delete log. */
         dcp.show_elog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            dcp.show_elog = store_host_names(dcp.show_elog_list, ptr + 1);
         }
         else
         {
            dcp.show_elog = NO_LIMIT;
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ dir_ctrl_exit() +++++++++++++++++++++++++++*/
static void
dir_ctrl_exit(void)
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
