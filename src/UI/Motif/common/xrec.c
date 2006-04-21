/*
 *  xrec.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   xrec - pops up a message dialog
 **
 ** SYNOPSIS
 **   int xrec(Widget parent, char type, char *fmt, ...)
 **
 ** DESCRIPTION
 **   This function pops up a message dialog with 'fmt' as contents.
 **   The following types of message dialogs have been implemented:
 **
 **    +----------------+-----------------------+-------+---------+--------+
 **    |                |                       | Block |         |        |
 **    |     Type       |      Description      | Input | Buttons | Action |
 **    +----------------+-----------------------+-------+---------+--------+
 **    |INFO_DIALOG     | Information.          |  Yes  | OK      | None   |
 **    |WARN_DIALOG     | Warning.              |  Yes  | OK      | None   |
 **    |ERROR_DIALOG    | Error.                |  Yes  | OK      | None   |
 **    |FATAL_DIALOG    | Fatal error.          |  Yes  | OK      | exit() |
 **    |ABORT_DIALOG    | Fatal error.          |  Yes  | OK      | abort()|
 **    |QUESTION_DIALOG | Question.             |  Yes  | YES, NO | None   |
 **    +----------------+-----------------------+-------+---------+--------+
 **
 ** RETURN VALUES
 **   When using QUESTION_DIALOG either YES or no will be returned. All
 **   other dialogs return NEITHER.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>            /* abort(), exit()                        */
#include <stdarg.h>
#include <Xm/MessageB.h>
#include "x_common_defs.h"

/* External global variables */
extern XtAppContext app;

/* Local function prototype */
static void         question_callback(Widget, XtPointer, XtPointer);


/*############################### xrec() ################################*/
int
xrec(Widget parent, char type, char *fmt, ...)
{
   int      n = 0;
   char     buf[MAX_LINE_LENGTH];
   va_list  ap;
   Widget   dialog;
   XmString xstring;
   Arg      arg[2];

   va_start(ap, fmt);
   (void)vsprintf(buf, fmt, ap);
   xstring = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG);
   XtSetArg(arg[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
   n++;
   XtSetArg(arg[n], XmNmessageString, xstring);
   n++;
   dialog = XmCreateMessageDialog(parent, "Message", arg, n);
   XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
   switch (type)
   {
      case INFO_DIALOG    : /* Information and OK button */
         XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
         XtVaSetValues(dialog,
                       XmNdialogType, XmDIALOG_INFORMATION,
                       NULL);
         break;

      case WARN_DIALOG    : /* Warning message and OK button */
         XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
         XtVaSetValues(dialog,
                       XmNdialogType, XmDIALOG_WARNING,
                       NULL);
         break;

      case ERROR_DIALOG   : /* Error message and OK button */
         XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
         XtVaSetValues(dialog,
                       XmNdialogType, XmDIALOG_ERROR,
                       NULL);
         break;

      case FATAL_DIALOG   :
      case ABORT_DIALOG   : /* Fatal error message and OK button */
         {
            static int answer;

            answer = NEITHER;
            XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
            XtAddCallback(dialog, XmNokCallback, question_callback, &answer);
            XtVaSetValues(dialog,
                          XmNdialogType,        XmDIALOG_QUESTION,
                          XmNdefaultButtonType, XmDIALOG_CANCEL_BUTTON,
                          NULL);
            XtManageChild(dialog);
            XtPopup(XtParent(dialog), XtGrabNone);
            XmStringFree(xstring);
            va_end(ap);

            while (answer == NEITHER)
            {
               XtAppProcessEvent(app, XtIMAll);
            }
            XSync(XtDisplay(dialog), 0);
            XmUpdateDisplay(parent);

            if (type == ABORT_DIALOG)
            {
               abort();
            }
            else
            {
               exit(INCORRECT);
            }
         }

      case QUESTION_DIALOG: /* Message and YES+NO button */
         {
            static int answer;
            XmString   yes_string,
                       no_string;

            answer = NEITHER;
            yes_string = XmStringCreateLocalized("Yes");
            no_string = XmStringCreateLocalized("No");
            XtAddCallback(dialog, XmNokCallback, question_callback, &answer);
            XtAddCallback(dialog, XmNcancelCallback, question_callback, &answer);
            XtVaSetValues(dialog,
                          XmNdialogType,        XmDIALOG_QUESTION,
                          XmNokLabelString,     yes_string,
                          XmNcancelLabelString, no_string,
                          XmNdefaultButtonType, XmDIALOG_CANCEL_BUTTON,
                          NULL);
            XmStringFree(yes_string);
            XmStringFree(no_string);
            XtManageChild(dialog);
            XtPopup(XtParent(dialog), XtGrabNone);
            XmStringFree(xstring);
            va_end(ap);

            while (answer == NEITHER)
            {
               XtAppProcessEvent(app, XtIMAll);
            }
            XSync(XtDisplay(dialog), 0);
            XmUpdateDisplay(parent);

            return(answer);
         }

      default             : /* Undefined. Should not happen! */
         break;
   }

   XtManageChild(dialog);
   XtPopup(XtParent(dialog), XtGrabNone);
   XmStringFree(xstring);
   va_end(ap);

   return(NEITHER);
}


/*++++++++++++++++++++++++++ question_callback() ++++++++++++++++++++++++*/
static void
question_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
   int *answer = (int *)client_data;
   XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *)call_data;

   if (cbs->reason == XmCR_OK)
   {
      *answer = YES;
   }
   else if (cbs->reason == XmCR_CANCEL)
        {
           *answer = NO;
        }

   return;
}
