/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   callbacks - all callback functions for module handle_event
 **
 ** SYNOPSIS
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void set_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void radio_button(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.06.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "handle_event.h"

/* External global variables. */
extern Widget                     text_w;
extern int                        acknowledge_type,
                                  fra_fd,
                                  no_of_alias,
                                  no_of_dirs,
                                  no_of_hosts;
extern char                       **dir_alias,
                                  **host_alias,
                                  user[];
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;


/*########################### close_button() ############################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(0);
}


/*############################ set_button() #############################*/
void
set_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int          flags_changed,
                flags_unchangeable,
                i,
                not_enough_errors,
                pos;
   unsigned int event_action;
   char         *reason_str,
                *text;

   if ((text = XmTextGetString(text_w)))
   {
      if (text[0] == '\0')
      {
         reason_str = NULL;
      }
      else
      {
         size_t length;

         length = strlen(text);
         if ((reason_str = malloc((3 * length))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s\n", strerror(errno));
            exit(INCORRECT);
         }
         else
         {
            int j = 0;

            for (i = 0; i < length; i++)
            {
               if (text[i] >= ' ')
               {
                  if (text[i] == '%')
                  {
                     reason_str[j++] = text[i];
                     j += sprintf(&reason_str[j], "%02x", (int)text[i]);
                  }
                  else
                  {
                     reason_str[j++] = text[i];
                  }
               }
               else
               {
                  reason_str[j++] = '%';
                  j += sprintf(&reason_str[j], "%02x", (int)text[i]);
               }
            }
            reason_str[j] = '\0';
         }
      }
      XtFree(text);
   }
   else
   {
      reason_str = NULL;
   }

   flags_changed = flags_unchangeable = not_enough_errors = 0;
   for (i = 0; i < no_of_alias; i++)
   {
      event_action = 0;
      if (fra_fd == -1)
      {
         pos = get_host_position(fsa, host_alias[i], no_of_hosts);
         if (pos == INCORRECT)
         {
            (void)fprintf(stderr, "Failed to locate `%s' in FSA.",
                          host_alias[i]);
         }
         else
         {
            int flag_changed = NO;

            if ((fsa[pos].error_counter > 0) ||
                (acknowledge_type == UNSET_SELECT))
            {
               if (acknowledge_type == ACKNOWLEDGE_SELECT)
               {
                  if ((fsa[pos].host_status & HOST_ERROR_ACKNOWLEDGED) == 0)
                  {
                     fsa[pos].host_status |= HOST_ERROR_ACKNOWLEDGED;
                     flag_changed = YES;
                     flags_changed++;
                  }
                  else
                  {
                     flags_unchangeable++;
                  }
                  event_action = EA_ACKNOWLEDGE;
               }
               else if (acknowledge_type == OFFLINE_SELECT)
                    {
                       if ((fsa[pos].host_status & HOST_ERROR_OFFLINE) == 0)
                       {
                          fsa[pos].host_status |= HOST_ERROR_OFFLINE;
                          flag_changed = YES;
                          flags_changed++;
                       }
                       else
                       {
                          flags_unchangeable++;
                       }
                       event_action = EA_OFFLINE;
                    }
                    else /* Unset all the flags. */
                    {
                       if (fsa[pos].host_status & HOST_ERROR_OFFLINE)
                       {
                          fsa[pos].host_status &= ~HOST_ERROR_OFFLINE;
                          flag_changed = YES;
                       }
                       if (fsa[pos].host_status & HOST_ERROR_ACKNOWLEDGED)
                       {
                          fsa[pos].host_status &= ~HOST_ERROR_ACKNOWLEDGED;
                          flag_changed = YES;
                       }
                       if (fsa[pos].host_status & HOST_ERROR_OFFLINE_T)
                       {
                          fsa[pos].host_status &= ~HOST_ERROR_OFFLINE_T;
                          flag_changed = YES;
                       }
                       if (fsa[pos].host_status & HOST_ERROR_ACKNOWLEDGED_T)
                       {
                          fsa[pos].host_status &= ~HOST_ERROR_ACKNOWLEDGED_T;
                          flag_changed = YES;
                       }
                       if (flag_changed == NO)
                       {
                          flags_unchangeable++;
                       }
                       else
                       {
                          flags_changed++;
                       }
                       event_action = EA_UNSET_ACK_OFFL;
                    }
            }
            else
            {
               not_enough_errors++;
            }
            if (event_action != 0)
            {
               if (reason_str == NULL)
               {
                  if (flag_changed == YES)
                  {
                     event_log(0L, EC_HOST, ET_MAN, event_action, "%s%c%s",
                               host_alias[i], SEPARATOR_CHAR, user);
                  }
               }
               else
               {
                  if ((flag_changed == YES) ||
                      (event_action != EA_UNSET_ACK_OFFL))
                  {
                     event_log(0L, EC_HOST, ET_MAN, event_action, "%s%c%s%c%s",
                               host_alias[i], SEPARATOR_CHAR, user,
                               SEPARATOR_CHAR, reason_str);
                  }
               }
            }
         }
      }
      else
      {
         pos = get_dir_position(fra, dir_alias[i], no_of_dirs);
         if (pos == INCORRECT)
         {
            (void)fprintf(stderr, "Failed to locate `%s' in FRA.",
                          dir_alias[i]);
         }
         else
         {
            int flag_changed = NO;

            if ((fra[pos].error_counter > 0) ||
                (acknowledge_type == UNSET_SELECT))
            {
               if (acknowledge_type == ACKNOWLEDGE_SELECT)
               {
                  if ((fra[pos].dir_flag & DIR_ERROR_ACKN) == 0)
                  {
                     fra[pos].dir_flag |= DIR_ERROR_ACKN;
                     flag_changed = YES;
                     flags_changed++;
                  }
                  else
                  {
                     flags_unchangeable++;
                  }
                  event_action = EA_ACKNOWLEDGE;
               }
               else if (acknowledge_type == OFFLINE_SELECT)
                    {
                       if ((fra[pos].dir_flag & DIR_ERROR_OFFLINE) == 0)
                       {
                          fra[pos].dir_flag |= DIR_ERROR_OFFLINE;
                          flag_changed = YES;
                          flags_changed++;
                       }
                       else
                       {
                          flags_unchangeable++;
                       }
                       event_action = EA_OFFLINE;
                    }
                    else /* Unset all the flags. */
                    {
                       if (fra[pos].dir_flag & DIR_ERROR_ACKN)
                       {
                          fra[pos].dir_flag &= ~DIR_ERROR_ACKN;
                          flag_changed = YES;
                       }
                       if (fra[pos].dir_flag & DIR_ERROR_OFFLINE)
                       {
                          fra[pos].dir_flag &= ~DIR_ERROR_OFFLINE;
                          flag_changed = YES;
                       }
                       if (fra[pos].dir_flag & DIR_ERROR_ACKN_T)
                       {
                          fra[pos].dir_flag &= ~DIR_ERROR_ACKN_T;
                          flag_changed = YES;
                       }
                       if (fra[pos].dir_flag & DIR_ERROR_OFFL_T)
                       {
                          fra[pos].dir_flag &= ~DIR_ERROR_OFFL_T;
                          flag_changed = YES;
                       }
                       if (flag_changed == NO)
                       {
                          flags_unchangeable++;
                       }
                       else
                       {
                          flags_changed++;
                       }
                       event_action = EA_UNSET_ACK_OFFL;
                    }
            }
            else
            {
               not_enough_errors++;
            }
            if (event_action != 0)
            {
               if (reason_str == NULL)
               {
                  if (flag_changed == YES)
                  {
                     event_log(0L, EC_DIR, ET_MAN, event_action, "%s%c%s",
                               dir_alias[i], SEPARATOR_CHAR, user);
                  }
               }
               else
               {
                  if ((flag_changed == YES) ||
                      (event_action != EA_UNSET_ACK_OFFL))
                  {
                     event_log(0L, EC_DIR, ET_MAN, event_action, "%s%c%s%c%s",
                               dir_alias[i], SEPARATOR_CHAR, user,
                               SEPARATOR_CHAR, reason_str);
                  }
               }
            }
         }
      }
   } /* for (i = 0; i < no_of_alias; i++) */

   if (flags_changed > 0)
   {
      if (acknowledge_type == UNSET_SELECT)
      {
         if (flags_unchangeable > 0)
         {
            (void)xrec(INFO_DIALOG,
                       "Unset acknowledge/offline for %d instances, %d already unset.",
                       flags_changed, flags_unchangeable);
         }
         else
         {
            (void)xrec(INFO_DIALOG,
                       "Unset acknowledge/offline for %d instances.",
                       flags_changed);
         }
      }
      else
      {
         if (flags_unchangeable > 0)
         {
            if (not_enough_errors > 0)
            {
               (void)xrec(INFO_DIALOG,
                          "Set acknowledge/offline for %d instances, %d already set. For %d there are not enough errors.",
                          flags_changed, flags_unchangeable, not_enough_errors);
            }
            else
            {
               (void)xrec(INFO_DIALOG,
                          "Set acknowledge/offline for %d instances, %d already set.",
                          flags_changed, flags_unchangeable);
            }
         }
         else
         {
            if (not_enough_errors > 0)
            {
               (void)xrec(INFO_DIALOG,
                          "Set acknowledge/offline for %d instances. For %d there are not enough errors.",
                          flags_changed, not_enough_errors);
            }
            else
            {
               (void)xrec(INFO_DIALOG,
                          "Set acknowledge/offline for %d instances.",
                          flags_changed);
            }
         }
      }
   }
   else if (flags_unchangeable > 0)
        {
           if (acknowledge_type == UNSET_SELECT)
           {
              (void)xrec(INFO_DIALOG,
                         "Acknowledge/offline for %d instances already unset.",
                         flags_unchangeable);
           }
           else
           {
              if (not_enough_errors > 0)
              {
                 (void)xrec(INFO_DIALOG,
                            "Acknowledge/offline for %d instances already set. For %d there are not enough errors.",
                            flags_unchangeable, not_enough_errors);
              }
              else
              {
                 if (reason_str == NULL)
                 {
                    (void)xrec(INFO_DIALOG,
                               "Acknowledge/offline for %d instances already set.",
                               flags_unchangeable);
                 }
              }
           }
        }
   else if (not_enough_errors > 0)
        {
           (void)xrec(INFO_DIALOG,
                      "Not enough errors for %d instances.", not_enough_errors);
        }
   else if ((reason_str == NULL) || (acknowledge_type == UNSET_SELECT))
        {
           (void)xrec(INFO_DIALOG, "No changes.");
        }

   return;
}


/*########################### radio_button() ############################*/
void
radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
#if SIZEOF_LONG == 4
   acknowledge_type = (int)client_data;
#else
   union intlong
         {
            int  ival[2];
            long lval;
         } il;
   int   byte_order = 1;

   il.lval = (long)client_data;
   if (*(char *)&byte_order == 1)
   {
      acknowledge_type = il.ival[0];
   }
   else
   {
      acknowledge_type = il.ival[1];
   }
#endif

   return;
}
