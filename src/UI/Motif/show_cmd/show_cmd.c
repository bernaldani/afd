/*
 *  show_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_cmd - displays log result of any command
 **
 ** SYNOPSIS
 **   show_cmd [--version]
 **                OR
 **   show_cmd [-w <AFD working directory>] [-f <font name>] <command>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.12.1999 H.Kiehl Created
 **   18.03.2000 H.Kiehl Added print button.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <sys/wait.h>            /* wait()                               */
#include <unistd.h>              /* gethostname(), STDERR_FILENO         */
#include <stdlib.h>              /* getenv()                             */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "afd_ctrl.h"
#include "show_cmd.h"
#include "version.h"

/* Global variables. */
Display        *display;
XtAppContext   app;
XmTextPosition wpr_position;
XtInputId      cmd_input_id;
Widget         appshell,
               cmd_output,
               statusbox_w;
int            cmd_fd,
               go_to_beginning = NO,
               sys_log_fd = STDERR_FILENO;
pid_t          cmd_pid;
char           cmd[MAX_PATH_LENGTH],
               work_dir[MAX_PATH_LENGTH],
               font_name[40],
               *p_work_dir;
const char     *sys_log_name = SYSTEM_LOG_FIFO;

/* Local functions. */
static void    init_cmd(int *, char **, char *),
               sig_bus(int),
               sig_segv(int),
               usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[MAX_TITLE_CMD_LENGTH + 1 + 25 + 1];
   static String   fallback_res[] =
                   {
                      ".show_cmd*mwmDecorations : 110",
                      ".show_cmd*mwmFunctions : 30",
                      ".show_cmd.mainform_w.cmd_outputSW*XmText.fontList : fixed",
                      ".show_cmd*background : NavajoWhite2",
                      ".show_cmd.mainform_w.cmd_outputSW.cmd_output.background : NavajoWhite1",
                      ".show_cmd.mainform_w.buttonbox_w*background : PaleVioletRed2",
                      ".show_cmd.mainform_w.buttonbox_w*foreground : Black",
                      ".show_cmd.mainform_w.buttonbox_w*highlightColor : Black",
                      ".show_cmd.Print Data*background : NavajoWhite2",
                      ".show_cmd.Print Data*XmText.background : NavajoWhite1",
                      ".show_cmd.Print Data.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_cmd.Print Data.main_form.buttonbox*foreground : Black",
                      ".show_cmd.Print Data.main_form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          mainform_w,
                   button,
                   buttonbox_w,
                   separator_w;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XmFontList      fontlist;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   p_work_dir = work_dir;
   init_cmd(&argc, argv, window_title);

   /*
    * SSH wants to look at .Xauthority and with setuid flag
    * set we cannot do that. So when we initialize X lets temporaly
    * disable it. After XtAppInitialize() we set it back.
    */
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }

   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);

   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }

   if ((display = XtDisplay(appshell)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Create managing widget. */
   mainform_w = XmCreateForm(appshell, "mainform_w", NULL, 0);

   /* Prepare the font. */
   entry = XmFontListEntryLoad(XtDisplay(appshell), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase, 31);
   argcount++;
   buttonbox_w = XmCreateForm(mainform_w, "buttonbox_w", args, argcount);
   button = XtVaCreateManagedWidget("Repeat",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     1,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    10,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   30,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)repeat_button, 0);
   button = XtVaCreateManagedWidget("Print",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     11,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    20,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   30,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)print_button, 0);
   button = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     21,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    30,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   30,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);                                              

/*-----------------------------------------------------------------------*/
/*                            Status Box                                 */
/*                            ----------                                 */
/* The status of the output log is shown here. If eg. no files are found */
/* it will be shown here.                                                */
/*-----------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",                              
                        xmLabelWidgetClass,  mainform_w,
                        XmNfontList,         fontlist,  
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget,     separator_w,
                        NULL);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     statusbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /* Create cmd_output as a ScrolledText window */
   argcount = 0;
   XtSetArg(args[argcount], XmNrows,                   18);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       True);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  True);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           separator_w);
   argcount++;
   cmd_output = XmCreateScrolledText(mainform_w, "cmd_output", args, argcount);
   XtManageChild(cmd_output);
   XtManageChild(mainform_w);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                      _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);


   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 SHOW_CMD, strerror(errno));
   }

   xexec_cmd(cmd);

   /* We want the keyboard focus on the cmd output. */
   XmProcessTraversal(cmd_output, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++ init_cmd() ++++++++++++++++++++++++++++++*/
static void
init_cmd(int *argc, char *argv[], char *title_cmd)
{
   int  length = 0;
   char *ptr;

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }

   /* Get working directory for the AFD. */
   if (get_afd_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }
   if (get_arg(argc, argv, "-b", NULL, 0) == SUCCESS)
   {
      go_to_beginning = YES;
   }
   if (*argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   if (argv[1][0] == '"')
   {
      (void)strcpy(cmd, &argv[1][1]);
      length = strlen(cmd);
      if (cmd[length - 1] == '"')
      {
         cmd[length - 1] = '\0';
      }
   }
   else
   {
      (void)strcpy(cmd, argv[1]);
   }
   if (*argc > 2)
   {
      register int j;

      for (j = 1; j < *argc; j++)
      {
         argv[j] = argv[j + 1];
      }
      argv[j] = NULL;
   }
   else
   {
      argv[1] = NULL;
   }
   (*argc)--;

   /* Cut out command for title of window. */
   ptr = cmd;
   if ((cmd[0] == '/') || (cmd[0] == '.') || (cmd[0] == '~'))
   {
      char *p_end;

      while ((*ptr != ' ') && (*ptr != '\0'))
      {
         ptr++;
      }
      p_end = ptr;
      while ((*ptr != '/') && (*ptr != '.') && (*ptr != '~'))
      {
         ptr--;
      }
      ptr++;
      length = p_end - ptr;
      if (length > MAX_TITLE_CMD_LENGTH)
      {
         length = MAX_TITLE_CMD_LENGTH;
      }
      (void)my_strncpy(title_cmd, ptr, length);
   }
   else
   {
      length = 0;
      while ((*ptr != ' ') && (*ptr != '\0') &&
             (length < MAX_TITLE_CMD_LENGTH))
      {
         title_cmd[length] = *ptr;
         ptr++; length++;
      }
   }
   title_cmd[length] = ' ';
   title_cmd[length + 1] = '\0';

   /* Cut out target hostname. */
   while (*ptr != '\0')
   {
      ptr++;
   }
   while ((*ptr != ' ') && (ptr != cmd))
   {
      ptr--;
   }
   if (*ptr == ' ')
   {
      *ptr = '\0';
      ptr++;
   }
   (void)strncat(title_cmd, ptr, 25);

   return;
}


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s [options] <command to execute>\n", progname);
   (void)fprintf(stderr, "              --version\n");
   (void)fprintf(stderr, "              -b\n");
   (void)fprintf(stderr, "              -f <font name>\n");
   (void)fprintf(stderr, "              -w <working directory>\n");
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
