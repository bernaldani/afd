/*
 *  edit_hc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   edit_hc - edits the AFD host configuration file
 **
 ** SYNOPSIS
 **   edit_hc [options]
 **            --version
 **            -w <AFD working directory>
 **            -f <font name>
 **            -h <host alias>
 **
 ** DESCRIPTION
 **   This dialog allows the user to change the following parameters
 **   for a given hostname:
 **       - Real Hostaname/IP Number
 **       - Transfer timeout
 **       - Retry interval
 **       - Maximum errors
 **       - Successful retries
 **       - Transfer rate limit
 **       - Max. parallel transfers
 **       - Transfer Blocksize
 **       - File size offset
 **       - Number of transfers that may not burst
 **       - Proxy name
 **   additionally some protocol specific options can be set:
 **       - FTP active/passive mode
 **       - set FTP idle time
 **       - send STAT to keep control connection alive (FTP)
 **       - FTP fast rename
 **       - FTP fast cd
 **
 **   In the list widget "Alias Hostname" the user can change the order
 **   of the host names in the afd_ctrl dialog by using drag & drop.
 **   During the drag operation the cursor will change into a bee. The
 **   hot spot of this cursor are the two feelers.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.08.1997 H.Kiehl Created
 **   28.02.1998 H.Kiehl Added host switching information.
 **   21.10.1998 H.Kiehl Added Remove button to remove hosts from the FSA.
 **   04.08.2001 H.Kiehl Added support for active or passive mode and
 **                      FTP idle time.
 **   11.09.2001 H.Kiehl Added support to send FTP STAT to keep control
 **                      connection alive.
 **   10.01.2004 H.Kiehl Added FTP fast rename option.
 **   17.04.2004 H.Kiehl Added option -h to select a host alias.
 **   10.06.2004 H.Kiehl Added transfer rate limit.
 **   17.02.2006 H.Kiehl Added option to change socket send and/or receive
 **                      buffer.
 **   28.02.2006 H.Kiehl Added option for setting the keep connected
 **                      parameter.
 **   16.03.2006 H.Kiehl Added duplicate check option.
 **   17.08.2006 H.Kiehl Added extended active or passive mode.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* malloc(), free()                     */
#include <ctype.h>               /* isdigit()                            */
#include <signal.h>              /* signal()                             */
#include <sys/stat.h>
#include <fcntl.h>               /* O_RDWR                               */
#include <unistd.h>              /* STDERR_FILENO                        */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"
#include "permission.h"

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <X11/Intrinsic.h>
#include "edit_hc.h"
#include "afd_ctrl.h"
#include "source.h"
#include "source_mask.h"
#include "no_source.h"
#include "no_source_mask.h"

/* Global variables */
XtAppContext               app;
Display                    *display;
Widget                     active_mode_w,
#ifdef _WITH_BURST_2
                           allow_burst_w,
#endif
                           appshell,
                           auto_toggle_w,
#ifdef WITH_DUP_CHECK
                           dc_delete_w,
                           dc_disable_w,
                           dc_enable_w,
                           dc_filecontent_w,
                           dc_filenamecontent_w,
                           dc_filename_w,
                           dc_nosuffix_w,
                           dc_store_w,
                           dc_timeout_label_w,
                           dc_timeout_w,
                           dc_type_w,
                           dc_warn_w,
#endif
                           extended_mode_w,
                           first_label_w,
                           ftp_fast_cd_w,
                           ftp_fast_rename_w,
                           ftp_idle_time_w,
                           ftp_ignore_bin_w,
                           ftp_mode_w,
                           host_1_w,
                           host_2_w,
                           host_1_label_w,
                           host_2_label_w,
                           host_list_w,
                           host_switch_toggle_w,
                           keep_connected_w,
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                           ftp_keepalive_w,
#endif
                           max_errors_w,
                           mode_label_w,
                           no_source_icon_w,
                           passive_mode_w,
                           passive_redirect_w,
                           proxy_box_w,
                           proxy_name_w,
                           real_hostname_1_w,
                           real_hostname_2_w,
                           retry_interval_w,
                           rm_button_w,
                           second_label_w,
                           socket_send_buffer_size_label_w,
                           socket_send_buffer_size_w,
                           socket_receive_buffer_size_label_w,
                           socket_receive_buffer_size_w,
                           source_icon_w,
                           start_drag_w,
                           statusbox_w,
                           successful_retries_label_w,
                           successful_retries_w,
                           transfer_rate_limit_label_w,
                           transfer_rate_limit_w,
                           transfer_timeout_w;
Atom                       compound_text;
int                        fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           host_alias_order_change = NO,
                           in_drop_site = -2,
                           no_of_dirs,
                           no_of_hosts,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       fake_user[MAX_FULL_USER_ID_LENGTH],
                           *p_work_dir,
                           last_selected_host[MAX_HOSTNAME_LENGTH + 1];
struct fileretrieve_status *fra;
struct filetransfer_status *fsa;
struct afd_status          *p_afd_status;
struct host_list           *hl = NULL; /* required by change_alias_order() */
struct changed_entry       *ce;
struct parallel_transfers  pt;
struct no_of_no_bursts     nob;
struct transfer_blocksize  tb;
struct file_size_offset    fso;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables */
static int                 seleted_host_no = 0;
static char                font_name[40],
                           translation_table[] = "#override <Btn2Down>: start_drag()";
static XtActionsRec        action_table[] =
                           {
                              { "start_drag", (XtActionProc)start_drag }
                           };

/* Local functions. */
static void                create_option_menu_fso(Widget, Widget, XmFontList),
                           create_option_menu_nob(Widget, Widget, XmFontList),
                           create_option_menu_pt(Widget, Widget, XmFontList),
                           create_option_menu_tb(Widget, Widget, XmFontList),
                           init_edit_hc(int *, char **, char *),
                           init_widget_data(void),
                           sig_bus(int),
                           sig_segv(int),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            label_str[HOST_ALIAS_LABEL_LENGTH + MAX_HOSTNAME_LENGTH],
                   window_title[100],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      ".edit_hc*mwmDecorations : 10",
                      ".edit_hc*mwmFunctions : 4",
                      ".edit_hc*background : NavajoWhite2",
                      ".edit_hc.form_w.host_list_box_w.host_list_wSW*background : NavajoWhite1",
                      ".edit_hc.form_w*XmText.background : NavajoWhite1",
                      ".edit_hc.form_w.button_box*background : PaleVioletRed2",
                      ".edit_hc.form_w.button_box.Remove.XmDialogShell*background : NavajoWhite2",
                      ".edit_hc.form_w.button_box*foreground : Black",
                      ".edit_hc.form_w.button_box*highlightColor : Black",
                      NULL
                   };
   Widget          box_w,
                   button_w,
#ifdef WITH_DUP_CHECK
                   dupcheck_w,
#endif
                   form_w,
                   label_w,
                   h_separator_bottom_w,
                   h_separator_top_w,
                   v_separator_w;
   XtTranslations  translations;
   Atom            targets[1];
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XmFontList      fontlist;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_edit_hc(&argc, argv, window_title);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif
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
   compound_text = XmInternAtom(display, "COMPOUND_TEXT", False);

   /* Create managing widget */
   form_w = XmCreateForm(appshell, "form_w", NULL, 0);

   /* Prepare the font */
   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

/*-----------------------------------------------------------------------*/
/*                            Button Box                                 */
/*                            ----------                                 */
/* Contains two buttons, one to activate the changes and the other to    */
/* close this window.                                                    */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     31);
   argcount++;
   box_w = XmCreateForm(form_w, "button_box", args, argcount);

   button_w = XtVaCreateManagedWidget("Update",
                                     xmPushButtonWidgetClass, box_w,
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
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)submite_button, NULL);
   rm_button_w = XtVaCreateManagedWidget("Remove",
                                     xmPushButtonWidgetClass, box_w,
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
   XtAddCallback(rm_button_w, XmNactivateCallback,
                 (XtCallbackProc)remove_button, NULL);
   button_w = XtVaCreateManagedWidget("Close",
                                     xmPushButtonWidgetClass, box_w,
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
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, NULL);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_bottom_w = XmCreateSeparator(form_w, "h_separator_bottom",
                                            args, argcount);
   XtManageChild(h_separator_bottom_w);

/*-----------------------------------------------------------------------*/
/*                            Status Box                                 */
/*                            ----------                                 */
/* Here any feedback from the program will be shown.                     */
/*-----------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                           xmLabelWidgetClass,       form_w,
                           XmNfontList,              fontlist,
                           XmNleftAttachment,        XmATTACH_FORM,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNbottomAttachment,      XmATTACH_WIDGET,
                           XmNbottomWidget,          h_separator_bottom_w,
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
   h_separator_bottom_w = XmCreateSeparator(form_w, "h_separator_bottom",
                                            args, argcount);
   XtManageChild(h_separator_bottom_w);

/*-----------------------------------------------------------------------*/
/*                             Host List Box                             */
/*                             -------------                            */
/* Lists all hosts that are stored in the FSA. They are listed in there  */
/* short form, ie MAX_HOSTNAME_LENGTH as displayed by afd_ctrl.          */
/* ----------------------------------------------------------------------*/
   /* Create managing widget for host list box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   box_w = XmCreateForm(form_w, "host_list_box_w", args, argcount);

   (void)memcpy(label_str, HOST_ALIAS_LABEL, HOST_ALIAS_LABEL_LENGTH);
   if (HOST_ALIAS_LABEL_LENGTH < MAX_HOSTNAME_LENGTH)
   {
      (void)memset(&label_str[HOST_ALIAS_LABEL_LENGTH], ' ',
                   (MAX_HOSTNAME_LENGTH - HOST_ALIAS_LABEL_LENGTH));
      label_str[MAX_HOSTNAME_LENGTH] = ':';
      label_str[MAX_HOSTNAME_LENGTH + 1] = '\0';
   }
   else
   {
      label_str[HOST_ALIAS_LABEL_LENGTH] = ':';
      label_str[HOST_ALIAS_LABEL_LENGTH + 1] = '\0';
   }
   label_w = XtVaCreateManagedWidget(label_str,
                             xmLabelGadgetClass,  box_w,
                             XmNfontList,         fontlist,
                             XmNtopAttachment,    XmATTACH_FORM,
                             XmNtopOffset,        SIDE_OFFSET,
                             XmNleftAttachment,   XmATTACH_FORM,
                             XmNleftOffset,       SIDE_OFFSET,
                             XmNrightAttachment,  XmATTACH_FORM,
                             XmNrightOffset,      SIDE_OFFSET,
                             XmNalignment,        XmALIGNMENT_BEGINNING,
                             NULL);

   /* Add actions */
   XtAppAddActions(app, action_table, XtNumber(action_table));
   translations = XtParseTranslationTable(translation_table);

   /* Create the host list widget */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              label_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNvisibleItemCount,       10);
   argcount++;
   XtSetArg(args[argcount], XmNselectionPolicy,        XmEXTENDED_SELECT);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtranslations,           translations);
   argcount++;
   host_list_w = XmCreateScrolledList(box_w, "host_list_w", args, argcount);
   XtManageChild(host_list_w);
   XtManageChild(box_w);
   XtAddCallback(host_list_w, XmNextendedSelectionCallback, selected, NULL);

   /* Set up host_list_w as a drop site. */
   targets[0] = XmInternAtom(display, "COMPOUND_TEXT", False);
   argcount = 0;
   XtSetArg(args[argcount], XmNimportTargets,      targets);
   argcount++;
   XtSetArg(args[argcount], XmNnumImportTargets,   1);
   argcount++;
   XtSetArg(args[argcount], XmNdropSiteOperations, XmDROP_MOVE);
   argcount++;
   XtSetArg(args[argcount], XmNdropProc,           accept_drop);
   argcount++;
   XmDropSiteRegister(box_w, args, argcount);

/*-----------------------------------------------------------------------*/
/*                          Vertical Separator                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   v_separator_w = XmCreateSeparator(form_w, "v_separator", args, argcount);
   XtManageChild(v_separator_w);

/*-----------------------------------------------------------------------*/
/*                           Host Switch Box                             */
/*                           ---------------                             */
/* Allows user to set host or auto host switching.                       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other real hostname box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "host_switch_box_w", args, argcount);

   host_switch_toggle_w = XtVaCreateManagedWidget("Host switching",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(host_switch_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)host_switch_toggle,
                 (XtPointer)HOST_SWITCHING);

   host_1_label_w = XtVaCreateManagedWidget("Host 1:",
                       xmLabelGadgetClass, box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_switch_toggle_w,
                       XmNleftOffset,       2 * SIDE_OFFSET,
                       XmNbottomAttachment, XmATTACH_FORM,
                       NULL);
   host_1_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          1,
                       XmNmaxLength,        1,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_1_label_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,     SIDE_OFFSET - 1,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(host_1_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(host_1_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_1_ID);
   host_2_label_w = XtVaCreateManagedWidget("Host 2:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_1_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       NULL);
   host_2_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          1,
                       XmNmaxLength,        1,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_2_label_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,     SIDE_OFFSET - 1,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(host_2_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(host_2_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_2_ID);
   auto_toggle_w = XtVaCreateManagedWidget("Auto",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             host_2_w,
                       XmNleftOffset,             2 * SIDE_OFFSET,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(auto_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)host_switch_toggle,
                 (XtPointer)AUTO_SWITCHING);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);


/*-----------------------------------------------------------------------*/
/*                           Real Hostname Box                           */
/*                           -----------------                           */
/* One text widget in which the user can enter the true host name or     */
/* IP-Address of the remote host. Another text widget is there for the   */
/* user to enter a proxy name.                                           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other real hostname box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    62);
   argcount++;
   box_w = XmCreateForm(form_w, "real_hostname_box_w", args, argcount);

   XtVaCreateManagedWidget("Real Hostname/IP Number:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNrightAttachment,  XmATTACH_POSITION,
                       XmNrightPosition,    40,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   30,
                       XmNalignment,        XmALIGNMENT_BEGINNING,
                       NULL);
   first_label_w = XtVaCreateManagedWidget("Host 1:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       NULL);
   real_hostname_1_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          16,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       first_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(real_hostname_1_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(real_hostname_1_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)REAL_HOST_NAME_1);

   second_label_w = XtVaCreateManagedWidget("Host 2:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       real_hostname_1_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       NULL);
   real_hostname_2_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          16,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       second_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNrightAttachment,  XmATTACH_FORM,
                       XmNrightOffset,      SIDE_OFFSET,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(real_hostname_2_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(real_hostname_2_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)REAL_HOST_NAME_2);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                         Text Input Box                                */
/*                         --------------                                */
/* Here more control parameters can be entered, such as: maximum number  */
/* of errors, transfer timeout, retry interval and successful retries.   */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other text input box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    63);
   argcount++;
   box_w = XmCreateForm(form_w, "text_input_box", args, argcount);

   label_w = XtVaCreateManagedWidget("Transfer timeout:",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   transfer_timeout_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(transfer_timeout_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(transfer_timeout_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(transfer_timeout_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)TRANSFER_TIMEOUT);

   label_w = XtVaCreateManagedWidget("Retry interval    :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     31,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   retry_interval_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(retry_interval_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(retry_interval_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(retry_interval_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RETRY_INTERVAL);

   label_w = XtVaCreateManagedWidget("Maximum errors  :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   41,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   max_errors_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   41,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(max_errors_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(max_errors_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(max_errors_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)MAXIMUM_ERRORS);

   successful_retries_label_w = XtVaCreateManagedWidget("Successful retries:",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     31,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   41,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   successful_retries_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       successful_retries_label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   41,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(successful_retries_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(successful_retries_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(successful_retries_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SUCCESSFUL_RETRIES);
   XtManageChild(box_w);

   label_w = XtVaCreateManagedWidget("Keep connected  :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      42,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   62,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   keep_connected_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          6,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      42,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   62,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(keep_connected_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(keep_connected_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(keep_connected_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)KEEP_CONNECTED);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                       General Transfer Parameters                     */
/*                       ---------------------------                     */
/* Here transfer control parameters can be entered such as the transfer  */
/* rate limit.                                                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     61);
   argcount++;
   box_w = XmCreateForm(form_w, "transfer_input_box", args, argcount);

   transfer_rate_limit_label_w = XtVaCreateManagedWidget(
                           "Transfer rate limit (in kilobytes):",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   transfer_rate_limit_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          7,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNrightAttachment,  XmATTACH_WIDGET,
                           XmNrightWidget,      box_w,
                           XmNrightOffset,      SIDE_OFFSET,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(transfer_rate_limit_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(transfer_rate_limit_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(transfer_rate_limit_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)TRANSFER_RATE_LIMIT);
   socket_send_buffer_size_label_w = XtVaCreateManagedWidget(
                           "Socket send buffer size (in kilobytes):",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   socket_send_buffer_size_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          7,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNrightAttachment,  XmATTACH_WIDGET,
                           XmNrightWidget,      box_w,
                           XmNrightOffset,      SIDE_OFFSET,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(socket_send_buffer_size_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(socket_send_buffer_size_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(socket_send_buffer_size_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SOCKET_SEND_BUFFER);
   socket_receive_buffer_size_label_w = XtVaCreateManagedWidget(
                           "Socket receive buffer size (in kilobytes):",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   socket_receive_buffer_size_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          7,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNrightAttachment,  XmATTACH_WIDGET,
                           XmNrightWidget,      box_w,
                           XmNrightOffset,      SIDE_OFFSET,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(socket_receive_buffer_size_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(socket_receive_buffer_size_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(socket_receive_buffer_size_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SOCKET_RECEIVE_BUFFER);
   XtManageChild(box_w);

#ifdef WITH_DUP_CHECK
/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                           Check for duplicates                        */
/*                           --------------------                        */
/* Parameters of how to perform the dupcheck, what action should be      */
/* taken and when to remove the checksum.                                */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "dupcheck_box_w", args, argcount);

   label_w = XtVaCreateManagedWidget("Check for duplicates :",
                            xmLabelGadgetClass,  box_w,
                            XmNfontList,         fontlist,
                            XmNalignment,        XmALIGNMENT_END,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNleftOffset,       5,
                            XmNbottomAttachment, XmATTACH_FORM,
                            NULL);
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   dupcheck_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   dc_enable_w = XtVaCreateManagedWidget("Enabled",
                            xmToggleButtonGadgetClass, dupcheck_w,
                            XmNfontList,               fontlist,
                            XmNset,                    True,
                            NULL);
   XtAddCallback(dc_enable_w, XmNdisarmCallback,
                 (XtCallbackProc)edc_radio_button,
                 (XtPointer)ENABLE_DUPCHECK_SEL);
   dc_disable_w = XtVaCreateManagedWidget("Disabled",
                            xmToggleButtonGadgetClass, dupcheck_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(dc_disable_w, XmNdisarmCallback,
                 (XtCallbackProc)edc_radio_button,
                 (XtPointer)DISABLE_DUPCHECK_SEL);
   XtManageChild(dupcheck_w);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "dupcheck_box_w", args, argcount);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   dc_type_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   dc_filename_w = XtVaCreateManagedWidget("Name",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    True,
                                   NULL);
   XtAddCallback(dc_filename_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NAME_SEL);
   dc_nosuffix_w = XtVaCreateManagedWidget("Name no suffix",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_nosuffix_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NOSUFFIX_SEL);
   dc_filecontent_w = XtVaCreateManagedWidget("Content",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_filecontent_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_CONTENT_SEL);
   dc_filenamecontent_w = XtVaCreateManagedWidget("Name + content",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_filenamecontent_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NAME_CONTENT_SEL);
   XtManageChild(dc_type_w);

   dc_delete_w = XtVaCreateManagedWidget("Delete",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    True,
                       XmNtopAttachment,          XmATTACH_WIDGET,
                       XmNtopWidget,              dc_type_w,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       NULL);
   XtAddCallback(dc_delete_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, (XtPointer)DC_DELETE_CHANGED);
   dc_store_w = XtVaCreateManagedWidget("Store",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_WIDGET,
                       XmNtopWidget,              dc_type_w,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             dc_delete_w,
                       NULL);
   XtAddCallback(dc_store_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, (XtPointer)DC_STORE_CHANGED);
   dc_warn_w = XtVaCreateManagedWidget("Warn",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_WIDGET,
                       XmNtopWidget,              dc_type_w,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             dc_store_w,
                       NULL);
   XtAddCallback(dc_warn_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, (XtPointer)DC_WARN_CHANGED);
   dc_timeout_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          7,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_WIDGET,
                       XmNtopWidget,        dc_type_w,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNrightAttachment,  XmATTACH_FORM,
                       XmNrightOffset,      SIDE_OFFSET,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(dc_timeout_w, XmNmodifyVerifyCallback, check_nummeric, NULL);
   XtAddCallback(dc_timeout_w, XmNvalueChangedCallback, value_change, NULL);
   XtAddCallback(dc_timeout_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)DC_TIMEOUT);
   dc_timeout_label_w = XtVaCreateManagedWidget(
                       "Timeout:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_WIDGET,
                       XmNtopWidget,        dc_type_w,
                       XmNrightAttachment,  XmATTACH_WIDGET,
                       XmNrightWidget,      dc_timeout_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNalignment,        XmALIGNMENT_BEGINNING,
                       NULL);
   XtManageChild(box_w);
#endif /* WITH_DUP_CHECK */

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                             Option Box                                */
/*                             ----------                                */
/* Here more control parameters can be selected, such as: maximum number */
/* of parallel transfers, transfer blocksize and file size offset.       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other text input box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     81);
   argcount++;
   box_w = XmCreateForm(form_w, "text_input_box", args, argcount);

   /* Parallel transfers */
   label_w = XtVaCreateManagedWidget("Max. parallel transfers     :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   20,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_pt(box_w, label_w, fontlist);

   /* Transfer blocksize */
   label_w = XtVaCreateManagedWidget("Transfer Blocksize          :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      21,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   40,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_tb(box_w, label_w, fontlist);

   /* File size offset */
   label_w = XtVaCreateManagedWidget("File size offset for append :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      41,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   60,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_fso(box_w, label_w, fontlist);

   /* File size offset */
   label_w = XtVaCreateManagedWidget("Number of no bursts         :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      61,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   80,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_nob(box_w, label_w, fontlist);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                       Protocol Specific Options                       */
/*                       -------------------------                       */
/* Select FTP active or passive mode and set FTP idle time for remote    */
/* FTP-server.                                                           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for the protocol specific option box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific1_box_w", args, argcount);

   mode_label_w = XtVaCreateManagedWidget("FTP Mode :",
                                xmLabelGadgetClass,  box_w,
                                XmNfontList,         fontlist,
                                XmNalignment,        XmALIGNMENT_END,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNleftOffset,       5,
                                XmNbottomAttachment, XmATTACH_FORM,
                                NULL);
   extended_mode_w = XtVaCreateManagedWidget("Extended",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             mode_label_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(extended_mode_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button,
                 (XtPointer)FTP_EXTENDED_MODE_CHANGED);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       extended_mode_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   ftp_mode_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   active_mode_w = XtVaCreateManagedWidget("Active",
                                   xmToggleButtonGadgetClass, ftp_mode_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    True,
                                   NULL);
   XtAddCallback(active_mode_w, XmNdisarmCallback,
                 (XtCallbackProc)ftp_mode_radio_button,
                 (XtPointer)FTP_ACTIVE_MODE_SEL);
   passive_mode_w = XtVaCreateManagedWidget("Passive",
                                   xmToggleButtonGadgetClass, ftp_mode_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(passive_mode_w, XmNdisarmCallback,
                 (XtCallbackProc)ftp_mode_radio_button,
                 (XtPointer)FTP_PASSIVE_MODE_SEL);
   XtManageChild(ftp_mode_w);
   passive_redirect_w = XtVaCreateManagedWidget("Redirect",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             ftp_mode_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(passive_redirect_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)FTP_PASSIVE_REDIRECT_CHANGED);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific2_box_w", args, argcount);
   ftp_idle_time_w = XtVaCreateManagedWidget("Set idle time",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_idle_time_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button,
                 (XtPointer)FTP_SET_IDLE_TIME_CHANGED);
   ftp_keepalive_w = XtVaCreateManagedWidget("Keepalive",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_idle_time_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_keepalive_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_KEEPALIVE_CHANGED);
   ftp_fast_rename_w = XtVaCreateManagedWidget("Fast rename",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_keepalive_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_fast_rename_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_FAST_RENAME_CHANGED);
   ftp_fast_cd_w = XtVaCreateManagedWidget("Fast cd",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_fast_rename_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_fast_cd_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_FAST_CD_CHANGED);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific2_box_w", args, argcount);
   ftp_ignore_bin_w = XtVaCreateManagedWidget("Ignore type I",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_ignore_bin_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_IGNORE_BIN_CHANGED);
#ifdef _WITH_BURST_2
   allow_burst_w = XtVaCreateManagedWidget("Allow burst",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    True,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_ignore_bin_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(allow_burst_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)ALLOW_BURST_CHANGED);
#endif
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                            Proxy Name Box                             */
/*                            --------------                             */
/* One text widget in which the user can enter the proxy name.           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for the proxy name box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   proxy_box_w = XmCreateForm(form_w, "proxy_name_box_w", args, argcount);

   label_w = XtVaCreateManagedWidget("Proxy Name:",
                           xmLabelGadgetClass,  proxy_box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNleftOffset,       5,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   proxy_name_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   proxy_box_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNrightOffset,      5,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(proxy_name_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(proxy_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)PROXY_NAME);
   XtManageChild(proxy_box_w);
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

   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(appshell, WARN_DIALOG,
                 "Failed to set signal handler's for %s : %s",
                 EDIT_HC, strerror(errno));
   }

   /* Initialize widgets with data */
   init_widget_data();

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_edit_hc() +++++++++++++++++++++++++++*/
static void
init_edit_hc(int *argc, char *argv[], char *window_title)
{
   char *perm_buffer,
        *p_user,
        hostname[MAX_AFD_NAME_LENGTH],
        selected_host[MAX_HOSTNAME_LENGTH + 1];

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      exit(INCORRECT);
   }
   if (get_arg(argc, argv, "-h", selected_host,
               MAX_HOSTNAME_LENGTH) == INCORRECT)
   {
      selected_host[0] = '\0';
   }

   /* Now lets see if user may use this program */
   check_fake_user(argc, argv, AFD_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          "Failed to access `%s', unable to determine users permissions.\n",
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE     : (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      if ((perm_buffer[0] == 'a') &&
                          (perm_buffer[1] == 'l') &&
                          (perm_buffer[2] == 'l') &&
                          ((perm_buffer[3] == '\0') ||
                           (perm_buffer[3] == ' ') ||
                           (perm_buffer[3] == '\t')))
                      {
                         free(perm_buffer);
                         break;
                      }
                      else if (posi(perm_buffer, EDIT_HC_PERM) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "%s\n", PERMISSION_DENIED_STR);
                              exit(INCORRECT);
                           }
                      free(perm_buffer);
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      break;

      default       : (void)fprintf(stderr,
                                    "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }

   /* Check that no one else is using this dialog. */
   if ((p_user = lock_proc(EDIT_HC_LOCK_ID, NO)) != NULL)
   {
      (void)fprintf(stderr,
                    "Only one user may use this dialog. Currently %s is using it.\n",
                    p_user);
      exit(INCORRECT);
   }

   /* Get the font if supplied. */
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (selected_host[0] != '\0')
   {
      int i;

      for (i = 0; i < no_of_hosts; i++)
      {
         if (CHECK_STRCMP(fsa[i].host_alias, selected_host) == 0)
         {
            seleted_host_no = i;
            break;
         }
      }
   }

   /* Allocate memory to store all changes */
   if ((ce = malloc(no_of_hosts * sizeof(struct changed_entry))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get display pointer */
   if ((display = XOpenDisplay(NULL)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Prepare title of this window. */
   (void)strcpy(window_title, "Host Config ");
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

   if (attach_afd_status(NULL) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to attach to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}


/*---------------------------------- usage() ----------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s [options]\n", progname);
   (void)fprintf(stderr, "              --version\n");
   (void)fprintf(stderr, "              -w <working directory>\n");
   (void)fprintf(stderr, "              -f <font name>\n");
   (void)fprintf(stderr, "              -h <host alias>\n");
   (void)fprintf(stderr, "              --version\n");
   return;
}


/*++++++++++++++++++++++++ create_option_menu_pt() +++++++++++++++++++++++*/
static void
create_option_menu_pt(Widget parent, Widget label_w, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        button_name[MAX_INT_LENGTH];
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;


   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      1);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   20);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   pt.option_menu_w = XmCreateOptionMenu(parent, "parallel_transfer",
                                         args, argcount);
   XtManageChild(pt.option_menu_w);

   /* Add all possible buttons. */
   for (i = 1; i <= MAX_NO_PARALLEL_JOBS; i++)
   {
#if SIZEOF_LONG == 4
      (void)sprintf(button_name, "%d", i);
#else
      (void)sprintf(button_name, "%ld", i);
#endif
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      pt.value[i - 1] = i;
      pt.button_w[i - 1] = XtCreateManagedWidget(button_name,
                                             xmPushButtonWidgetClass,
                                             pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(pt.button_w[i - 1], XmNactivateCallback, pt_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*++++++++++++++++++++++++ create_option_menu_tb() +++++++++++++++++++++++*/
static void
create_option_menu_tb(Widget parent, Widget label_w, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        *blocksize_name[MAX_TB_BUTTONS] =
               {
                  "256 B",
                  "512 B",
                  "1 KB",
                  "2 KB",
                  "4 KB",
                  "8 KB",
                  "16 KB",
                  "64 KB",
                  "128 KB",
                  "256 KB"
               };
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;

   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      21);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   40);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   tb.option_menu_w = XmCreateOptionMenu(parent, "transfer_blocksize",
                                         args, argcount);
   XtManageChild(tb.option_menu_w);

   /* Add all possible buttons. */
   for (i = 0; i < MAX_TB_BUTTONS; i++)
   {
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      tb.button_w[i] = XtCreateManagedWidget(blocksize_name[i],
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(tb.button_w[i], XmNactivateCallback, tb_option_changed,
                    (XtPointer)i);
   }

   tb.value[0] = 256; tb.value[1] = 512; tb.value[2] = 1024;
   tb.value[3] = 2048; tb.value[4] = 4096; tb.value[5] = 8192;
   tb.value[6] = 16384; tb.value[7] = 65536; tb.value[8] = 131072;
   tb.value[9] = 262144;

   return;
}


/*+++++++++++++++++++++++ create_option_menu_fso() +++++++++++++++++++++++*/
static void
create_option_menu_fso(Widget parent, Widget label_w, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        button_name[MAX_INT_LENGTH];
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;

   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId, pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      41);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   60);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   fso.option_menu_w = XmCreateOptionMenu(parent, "file_size_offset",
                                          args, argcount);
   XtManageChild(fso.option_menu_w);

   /* Add all possible buttons. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   fso.value[0] = -1;
   fso.button_w[0] = XtCreateManagedWidget("None",
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);
   XtAddCallback(fso.button_w[0], XmNactivateCallback, fso_option_changed,
                 (XtPointer)0);
   fso.value[1] = AUTO_SIZE_DETECT;
   fso.button_w[1] = XtCreateManagedWidget("Auto",
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);
   XtAddCallback(fso.button_w[1], XmNactivateCallback, fso_option_changed,
                 (XtPointer)1);
   for (i = 2; i < MAX_FSO_BUTTONS; i++)
   {
#if SIZEOF_LONG == 4
      (void)sprintf(button_name, "%d", i);
#else
      (void)sprintf(button_name, "%ld", i);
#endif
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      fso.value[i] = i;
      fso.button_w[i] = XtCreateManagedWidget(button_name,
                                              xmPushButtonWidgetClass,
                                              pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(fso.button_w[i], XmNactivateCallback, fso_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*+++++++++++++++++++++++ create_option_menu_nob() +++++++++++++++++++++++*/
static void
create_option_menu_nob(Widget parent, Widget label_w, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        button_name[MAX_INT_LENGTH];
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;


   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      61);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   80);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   nob.option_menu_w = XmCreateOptionMenu(parent, "no_of_no_burst",
                                          args, argcount);
   XtManageChild(nob.option_menu_w);

   /* Add all possible buttons. */
   for (i = 0; i <= MAX_NO_PARALLEL_JOBS; i++)
   {
#if SIZEOF_LONG == 4
      (void)sprintf(button_name, "%d", i);
#else
      (void)sprintf(button_name, "%ld", i);
#endif
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      nob.value[i] = i;
      nob.button_w[i] = XtCreateManagedWidget(button_name,
                                              xmPushButtonWidgetClass,
                                              pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(nob.button_w[i], XmNactivateCallback, nob_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*+++++++++++++++++++++++++++ init_widget_data() ++++++++++++++++++++++++*/
static void
init_widget_data(void)
{
   int           i;
   Arg           args[MAXARGS];
   Cardinal      argcount;
   Pixmap        icon,
                 iconmask;
   Display       *sdisplay = XtDisplay(host_list_w);
   Window        win = XtWindow(host_list_w);
   XmStringTable item_list;

   item_list = (XmStringTable)XtMalloc(no_of_hosts * sizeof(XmString));

   for (i = 0; i < no_of_hosts; i++)
   {
      item_list[i] = XmStringCreateLocalized(fsa[i].host_alias);

      /* Initialize array holding all changed entries. */
      ce[i].value_changed = 0;
      ce[i].value_changed2 = 0;
      ce[i].real_hostname[0][0] = -1;
      ce[i].real_hostname[1][0] = -1;
      ce[i].proxy_name[0] = -1;
      ce[i].transfer_timeout = -1L;
      ce[i].retry_interval = -1;
      ce[i].max_errors = -1;
      ce[i].max_successful_retries = -1;
      ce[i].allowed_transfers = -1;
      ce[i].block_size = -1;
      ce[i].file_size_offset = -3;
      ce[i].transfer_rate_limit = -1;
      ce[i].sndbuf_size = 0;
      ce[i].rcvbuf_size = 0;
      ce[i].keep_connected = 0;
#ifdef WITH_DUP_CHECK
      ce[i].dup_check_flag = 0;
      ce[i].dup_check_timeout = 0L;
#endif
      if (fsa[i].host_toggle_str[0] == '\0')
      {
         ce[i].host_toggle[0][0] = '1';
         ce[i].host_toggle[1][0] = '2';
         ce[i].host_switch_toggle = OFF;
         ce[i].auto_toggle = OFF;
      }
      else
      {
         ce[i].host_toggle[0][0] = fsa[i].host_toggle_str[HOST_ONE];
         ce[i].host_toggle[1][0] = fsa[i].host_toggle_str[HOST_TWO];
         ce[i].host_switch_toggle = ON;
         if (fsa[i].auto_toggle == ON)
         {
            ce[i].auto_toggle = ON;
         }
         else
         {
            ce[i].auto_toggle = OFF;
         }
      }
   }

   XtVaSetValues(host_list_w,
                 XmNitems,      item_list,
                 XmNitemCount,  no_of_hosts,
                 NULL);

   for (i = 0; i < no_of_hosts; i++)
   {
      XmStringFree(item_list[i]);
   }
   XtFree((char *)item_list);

   /*
    * Create source cursor for drag & drop
    */
   icon     = XCreateBitmapFromData(sdisplay, win,
                                    (char *)source_bits,
                                    source_width,
                                    source_height);
   iconmask = XCreateBitmapFromData(sdisplay, win,
                                    (char *)source_mask_bits,
                                    source_mask_width,
                                    source_mask_height);
   argcount = 0;
   XtSetArg(args[argcount], XmNwidth,  source_width);
   argcount++;
   XtSetArg(args[argcount], XmNheight, source_height);
   argcount++;
   XtSetArg(args[argcount], XmNpixmap, icon);
   argcount++;
   XtSetArg(args[argcount], XmNmask,   iconmask);
   argcount++;
   source_icon_w = XmCreateDragIcon(host_list_w, "source_icon",
                                    args, argcount);

   /*
    * Create invalid source cursor for drag & drop
    */
   icon     = XCreateBitmapFromData(sdisplay, win,
                                    (char *)no_source_bits,
                                    no_source_width,
                                    no_source_height);
   iconmask = XCreateBitmapFromData(sdisplay, win,
                                    (char *)no_source_mask_bits,
                                    no_source_mask_width,
                                    no_source_mask_height);
   argcount = 0;
   XtSetArg(args[argcount], XmNwidth,  no_source_width);
   argcount++;
   XtSetArg(args[argcount], XmNheight, no_source_height);
   argcount++;
   XtSetArg(args[argcount], XmNpixmap, icon);
   argcount++;
   XtSetArg(args[argcount], XmNmask,   iconmask);
   argcount++;
   no_source_icon_w = XmCreateDragIcon(host_list_w, "no_source_icon",
                      args, argcount);

   /* Select the first host */
   if (no_of_hosts > 0)
   {
      int top,
          visible;

      XmListSelectPos(host_list_w, seleted_host_no + 1, True);

      /*
       * This code was taken from the book Motif Programming Manual
       * Volume 6A by Dan Heller & Paula M. Ferguson.
       */
      XtVaGetValues(host_list_w,
                    XmNtopItemPosition,  &top,
                    XmNvisibleItemCount, &visible,
                    NULL);
      if ((seleted_host_no + 1) < top)
      {
         XmListSetPos(host_list_w, (seleted_host_no + 1));
      }
      else if ((seleted_host_no + 1) >= (top + visible))
           {
              XmListSetBottomPos(host_list_w, (seleted_host_no + 1));
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