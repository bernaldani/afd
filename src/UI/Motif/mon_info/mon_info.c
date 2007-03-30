/*
 *  mon_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mon_info - displays information on a single AFD
 **
 ** SYNOPSIS
 **   mon_info [--version] [-w <work dir>] [-f <font name>] -a AFD-name
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.1999 H.Kiehl Created
 **   10.09.2000 H.Kiehl Added top transfer and top file rate.
 **   07.08.2004 H.Kiehl Write window ID to a common file.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <time.h>                /* strftime(), localtime()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>              /* getcwd(), gethostname()              */
#include <stdlib.h>              /* getenv(), atexit()                   */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "mon_info.h"
#include "version.h"

/* Global variables */
Display                *display;
XtAppContext           app;
XtIntervalId           interval_id_host;
Widget                 appshell,
                       info_w,
                       text_wl[NO_OF_MSA_ROWS],
                       text_wr[NO_OF_MSA_ROWS],
                       label_l_widget[NO_OF_MSA_ROWS],
                       label_r_widget[NO_OF_MSA_ROWS];
int                    sys_log_fd = STDERR_FILENO,
                       no_of_afds,
                       msa_id,
                       msa_fd = -1,
                       afd_position = -1;
off_t                  msa_size;
char                   afd_name[MAX_AFD_NAME_LENGTH + 1],
                       *alias_info_file,
                       *central_info_file,
                       font_name[40],
                       *info_data = NULL,
                       *p_work_dir,
                       label_l[NO_OF_MSA_ROWS][21] =
                       {
                          "Real host name     :",
                          "TCP port           :",
                          "Last data time     :",
                          "Maximum connections:",
                          "AFD Version        :",
                          "Top transfer rate  :"
                       },
                       label_r[NO_OF_MSA_ROWS][17] =
                       {
                          "IP number      :",
                          "Remote work dir:",
                          "Poll interval  :",
                          "TOP connections:",
                          "Number of hosts:",
                          "Top file rate  :"
                       };
struct mon_status_area *msa;
struct prev_values     prev;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototypes */
static void            init_mon_info(int *, char **),
                       mon_info_exit(void),
                       usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int             i;
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH],
                   str_line[MAX_INFO_STRING_LENGTH],
                   tmp_str_line[MAX_INFO_STRING_LENGTH];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".mon_info.form*background : NavajoWhite2",
                      ".mon_info.form.msa_box.?.?.?.text_wl.background : NavajoWhite1",
                      ".mon_info.form.msa_box.?.?.?.text_wr.background : NavajoWhite1",
                      ".mon_info.form.host_infoSW.host_info.background : NavajoWhite1",
                      ".mon_info.form.buttonbox*background : PaleVioletRed2",
                      ".mon_info.form.buttonbox*foreground : Black",
                      ".mon_info.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form_w,
                   msa_box_w,
                   msa_box1_w,
                   msa_box2_w,
                   msa_text_w,
                   button_w,
                   buttonbox_w,
                   rowcol1_w,
                   rowcol2_w,
                   h_separator1_w,
                   h_separator2_w,
                   v_separator_w;
   XmFontListEntry entry;
   XmFontList      fontlist;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_mon_info(&argc, argv);

   /*
    * SSH uses wants to look at .Xauthority and with setuid flag
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

   (void)strcpy(window_title, afd_name);
   (void)strcat(window_title, " Info");
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

   display = XtDisplay(appshell);

   /* Create managing widget */
   form_w = XmCreateForm(appshell, "form", NULL, 0);

   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   msa_box_w = XmCreateForm(form_w, "msa_box", args, argcount);
   XtManageChild(msa_box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   msa_box1_w = XmCreateForm(msa_box_w, "msa_box1", args, argcount);
   XtManageChild(msa_box1_w);

   rowcol1_w = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                                 msa_box1_w, NULL);
   for (i = 0; i < NO_OF_MSA_ROWS; i++)
   {
      msa_text_w = XtVaCreateWidget("msa_text", xmFormWidgetClass,
                                     rowcol1_w,
                                     XmNfractionBase, 41,
                                     NULL);
      label_l_widget[i] = XtVaCreateManagedWidget(label_l[i],
                              xmLabelGadgetClass,  msa_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wl[i] = XtVaCreateManagedWidget("text_wl",
                              xmTextWidgetClass,        msa_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               MON_INFO_LENGTH,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_POSITION,
                              XmNleftPosition,          22,
                              NULL);
      XtManageChild(msa_text_w);
   }
   XtManageChild(rowcol1_w);

   /* Fill up the text widget with some values */
   (void)sprintf(str_line, "%*s", MON_INFO_LENGTH,
                 prev.real_hostname[(int)prev.afd_toggle]);
   XmTextSetString(text_wl[0], str_line);
   (void)sprintf(str_line, "%*d",
                 MON_INFO_LENGTH, prev.port[(int)prev.afd_toggle]);
   XmTextSetString(text_wl[1], str_line);
   (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                  localtime(&prev.last_data_time));
   (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, tmp_str_line);
   XmTextSetString(text_wl[2], str_line);
   (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.max_connections);
   XmTextSetString(text_wl[3], str_line);
   (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, prev.afd_version);
   XmTextSetString(text_wl[4], str_line);
   if (prev.top_tr > 1048576)
   {
      (void)sprintf(str_line, "%*u MB/s",
                    MON_INFO_LENGTH - 5, prev.top_tr / 1048576);
   }
   else if (prev.top_tr > 1024)               
        {
           (void)sprintf(str_line, "%*u KB/s",
                         MON_INFO_LENGTH - 5, prev.top_tr / 1024);
        }
        else
        {
           (void)sprintf(str_line, "%*u Bytes/s",
                         MON_INFO_LENGTH - 8, prev.top_tr);
        }
   XmTextSetString(text_wl[5], str_line);

   /* Create the first horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       msa_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   h_separator1_w = XmCreateSeparator(form_w, "h_separator1_w", args, argcount);
   XtManageChild(h_separator1_w);

   /* Create the vertical separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       msa_box1_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   v_separator_w = XmCreateSeparator(msa_box_w, "v_separator", args, argcount);
   XtManageChild(v_separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   msa_box2_w = XmCreateForm(msa_box_w, "msa_box2", args, argcount);
   XtManageChild(msa_box2_w);

   rowcol2_w = XtVaCreateWidget("rowcol2", xmRowColumnWidgetClass,
                                msa_box2_w, NULL);
   for (i = 0; i < NO_OF_MSA_ROWS; i++)
   {
      msa_text_w = XtVaCreateWidget("msa_text", xmFormWidgetClass,
                                    rowcol2_w,
                                    XmNfractionBase, 41,
                                    NULL);
      label_r_widget[i] = XtVaCreateManagedWidget(label_r[i],
                              xmLabelGadgetClass,  msa_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wr[i] = XtVaCreateManagedWidget("text_wr",
                              xmTextWidgetClass,        msa_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               MON_INFO_LENGTH,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_POSITION,
                              XmNleftPosition,          20,
                              NULL);
      XtManageChild(msa_text_w);
   }
   XtManageChild(rowcol2_w);

   /* Fill up the text widget with some values */
   get_ip_no(msa[afd_position].hostname[(int)prev.afd_toggle], tmp_str_line);
   (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, tmp_str_line);
   XmTextSetString(text_wr[0], str_line);
   (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, prev.r_work_dir);
   XmTextSetString(text_wr[1], str_line);
   (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.poll_interval);
   XmTextSetString(text_wr[2], str_line);
   (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.top_not);
   XmTextSetString(text_wr[3], str_line);
   (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.no_of_hosts);
   XmTextSetString(text_wr[4], str_line);
   (void)sprintf(str_line, "%*u files/s", MON_INFO_LENGTH - 8, prev.top_fr);
   XmTextSetString(text_wr[5], str_line);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox_w = XmCreateForm(form_w, "buttonbox", args, argcount);

   /* Create the second horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator2_w = XmCreateSeparator(form_w, "h_separator2", args, argcount);
   XtManageChild(h_separator2_w);

   button_w = XtVaCreateManagedWidget("Close",
                                      xmPushButtonWidgetClass, buttonbox_w,
                                      XmNfontList,         fontlist,
                                      XmNtopAttachment,    XmATTACH_POSITION,
                                      XmNtopPosition,      2,
                                      XmNbottomAttachment, XmATTACH_POSITION,
                                      XmNbottomPosition,   19,
                                      XmNleftAttachment,   XmATTACH_POSITION,
                                      XmNleftPosition,     1,
                                      XmNrightAttachment,  XmATTACH_POSITION,
                                      XmNrightPosition,    20,
                                      NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);

   /* Create log_text as a ScrolledText window */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNrows,                   10);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       False);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              h_separator1_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,              3);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             3);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            3);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator2_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           3);
   argcount++;
   info_w = XmCreateScrolledText(form_w, "host_info", args, argcount);
   XtManageChild(info_w);
   XtManageChild(form_w);

   /* Free font list */
   XmFontListFree(fontlist);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(appshell);
   wait_visible(appshell);

   /* Read and display the information file */
   check_info_file(afd_name);

   /* Call update_info() after UPDATE_INTERVAL ms */
   interval_id_host = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      form_w);

   /* We want the keyboard focus on the Done button */
   XmProcessTraversal(button_w, XmTRAVERSE_CURRENT);

   /* Write window ID, so mon_ctrl can set focus if it is called again. */
   write_window_id(XtWindow(appshell), getpid(), MON_INFO);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_mon_info() ++++++++++++++++++++++++++*/
static void
init_mon_info(int *argc, char *argv[])
{
   size_t length;
   int    i;

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }
   if (get_arg(argc, argv, "-a", afd_name, MAX_AFD_NAME_LENGTH + 1) == INCORRECT)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   if (get_mon_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD_MON. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Attach to the MSA */
   if (msa_attach() < 0)
   {
      (void)fprintf(stderr, "Failed to attach to MSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_afds; i++)
   {
      if (strcmp(msa[i].afd_alias, afd_name) == 0)
      {
         afd_position = i;
         break;
      }
   }
   if (afd_position < 0)
   {
      (void)fprintf(stderr,
                    "WARNING : Could not find AFD %s in MSA. (%s %d)\n",
                    afd_name, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialize values in MSA structure */
   (void)strcpy(prev.real_hostname[0], msa[afd_position].hostname[0]);
   (void)strcpy(prev.real_hostname[1], msa[afd_position].hostname[1]);
   (void)strcpy(prev.r_work_dir, msa[afd_position].r_work_dir);
   (void)strcpy(prev.afd_version, msa[afd_position].afd_version);
   prev.port[0] = msa[afd_position].port[0];
   prev.port[1] = msa[afd_position].port[1];
   prev.afd_toggle = msa[afd_position].afd_toggle;
   prev.poll_interval = msa[afd_position].poll_interval;
   prev.max_connections = msa[afd_position].max_connections;
   prev.no_of_hosts = msa[afd_position].no_of_hosts;
   prev.last_data_time = msa[afd_position].last_data_time;
   prev.top_not = msa[afd_position].top_no_of_transfers[0];
   prev.top_tr = msa[afd_position].top_tr[0];
   prev.top_fr = msa[afd_position].top_fr[0];

   /* Create name of alias and central info file. */
   length = strlen(p_work_dir) + 1 + strlen(ETC_DIR) + 1;
   i = strlen(INFO_IDENTIFIER) + strlen(afd_name) + 1;
   if ((alias_info_file = malloc((length + i))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)sprintf(alias_info_file, "%s%s/%s%s", p_work_dir,
                 ETC_DIR, INFO_IDENTIFIER, afd_name);
   i = strlen(AFD_INFO_FILE) + 1;
   if ((central_info_file = malloc((length + i))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)sprintf(central_info_file, "%s%s/%s", p_work_dir,
                 ETC_DIR, AFD_INFO_FILE);

   if (atexit(mon_info_exit) != 0)
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set exit handler for %s : %s",
                 MON_INFO, strerror(errno));
   }
   check_window_ids(MON_INFO);

   return;
}


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] -a AFD-name\n", progname);
   (void)fprintf(stderr, "           --version\n");
   (void)fprintf(stderr, "           -f <font name>]\n");
   (void)fprintf(stderr, "           -w <working directory>]\n");
   return;
}


/*--------------------------- mon_info_exit() ---------------------------*/
static void
mon_info_exit(void)
{                  
   remove_window_id(getpid(), MON_INFO);
   return;
}