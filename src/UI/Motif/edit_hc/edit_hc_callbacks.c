/*
 *  edit_hc_callbacks.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   edit_hc_callbacks - all callback functions for edit_hc
 **
 ** SYNOPSIS
 **   edit_hc_callbacks
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.08.1997 H.Kiehl Created
 **   28.02.1998 H.Kiehl Added host switching information.
 **   16.07.2000 H.Kiehl Disable any input fields when they are not
 **                      available.
 **   10.06.2004 H.Kiehl Added transfer rate limit.
 **   17.02.2006 H.Kiehl Added option to change socket send and/or receive
 **                      buffer.
 **   28.02.2006 H.Kiehl Added option for setting the keep connected
 **                      parameter.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strlen()                             */
#include <stdlib.h>              /* atoi(), atol()                       */
#include <unistd.h>              /* close()                              */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>               /* open()                               */
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#ifdef WITH_DUP_CHECK
# include <Xm/ToggleB.h>
#endif
#include "edit_hc.h"
#include "afd_ctrl.h"

/* External global variables */
extern Display                    *display;
extern Widget                     active_mode_w,
#ifdef _WITH_BURST_2
                                  allow_burst_w,
#endif
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
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                                  ftp_keepalive_w,
#endif
                                  ftp_mode_w,
                                  host_1_w,
                                  host_2_w,
                                  host_1_label_w,
                                  host_2_label_w,
                                  host_list_w,
                                  host_switch_toggle_w,
                                  keep_connected_w,
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
                                  transfer_timeout_w,
                                  transfer_rate_limit_label_w,
                                  transfer_rate_limit_w;
extern int                        fsa_id,
                                  host_alias_order_change,
                                  in_drop_site,
                                  sys_log_fd,
                                  no_of_hosts;
extern char                       fake_user[],
                                  *p_work_dir,
                                  last_selected_host[];
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;
extern struct changed_entry       *ce;
extern struct parallel_transfers  pt;
extern struct no_of_no_bursts     nob;
extern struct transfer_blocksize  tb;
extern struct file_size_offset    fso;

/* Local global variables */
static int                        cur_pos,
                                  value_changed = NO;
static char                       label_str[8] =
                                  {
                                     'H', 'o', 's', 't', ' ', '1', ':', '\0'
                                  };


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int i;

   for (i = 0; i < no_of_hosts; i++)
   {
      if ((ce[i].value_changed != 0) || (ce[i].value_changed2 != 0))
      {
         if (xrec(w, QUESTION_DIALOG,
                  "There are unsaved changes!\nDo you want to discarde these?") != YES)
         {
            return;
         }
      }
   }

   (void)detach_afd_status();
   exit(0);
}


/*############################ remove_button() ##########################*/
void
remove_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int  last_removed_position = -1,
        no_selected,
        removed_hosts = 0,
        *select_list;
   char msg[MAX_MESSAGE_LENGTH];

   if (XmListGetSelectedPos(host_list_w, &select_list, &no_selected) == True)
   {
      int           fsa_pos,
                    i;
      char          *host_selected;
      XmStringTable all_items;

      XtVaGetValues(host_list_w, XmNitems, &all_items, NULL);
      for (i = (no_selected - 1); i > -1; i--)
      {
         XmStringGetLtoR(all_items[select_list[i] - 1],
                         XmFONTLIST_DEFAULT_TAG, &host_selected);
         if ((fsa_pos = get_host_position(fsa, host_selected,
                                          no_of_hosts)) < 0)
         {
            (void)xrec(w, WARN_DIALOG,
                       "Could not find host %s in FSA. Assume it has already been removed. (%s %d)",
                       host_selected, __FILE__, __LINE__);
         }
         else
         {
            if (fsa[fsa_pos].special_flag & HOST_IN_DIR_CONFIG)
            {
               (void)xrec(w, WARN_DIALOG,
                          "Host %s is still in the DIR_CONFIG. Will NOT remove it! (%s %d)",
                          host_selected, __FILE__, __LINE__);
            }
            else
            {
               if (xrec(w, QUESTION_DIALOG,
                        "Removing host %s will destroy all statistic information for it!\nAre you really sure?",
                        host_selected) == YES)
               {
                  if (remove_host(host_selected) == SUCCESS)
                  {
                     last_removed_position = XmListItemPos(host_list_w, all_items[select_list[i] - 1]);
                     XmListDeleteItem(host_list_w, all_items[select_list[i] - 1]);
                     removed_hosts++;
                  }
               }
            }
         }
         XtFree(host_selected);
      }
      XtFree((char *)select_list);
   }

   if (removed_hosts > 0)
   {
      int  db_update_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  db_update_readfd;
#endif
      char db_update_fifo[MAX_PATH_LENGTH];

      (void)strcpy(db_update_fifo, p_work_dir);
      (void)strcat(db_update_fifo, FIFO_DIR);
      (void)strcat(db_update_fifo, DB_UPDATE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(db_update_fifo, &db_update_readfd, &db_update_fd) == -1)
#else
      if ((db_update_fd = open(db_update_fifo, O_RDWR)) == -1)
#endif
      {
         (void)xrec(w, WARN_DIALOG, "Failed to open() %s : %s (%s %d)",
                    db_update_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         int  ret;

         if ((ret = send_cmd(REREAD_HOST_CONFIG, db_update_fd)) != SUCCESS)
         {
            (void)xrec(w, ERROR_DIALOG,
                       "Failed to REREAD_HOST_CONFIG message to AMG : %s (%s %d)",
                       strerror(-ret), __FILE__, __LINE__);
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (close(db_update_readfd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
#endif
            if (close(db_update_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
         }
         else
         {
            int sleep_counter = 0;

#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (close(db_update_readfd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
#endif
            if (close(db_update_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }

            /* Wait for AMG to update the FSA. */
            while ((check_fsa(NO) == NO) && (sleep_counter < 12))
            {
               (void)sleep(1);
               sleep_counter++;
            }
         }

         if (removed_hosts == no_selected)
         {
            if (last_removed_position != -1)
            {
               if ((last_removed_position - 1) == 0)
               {
                  XmListSelectPos(host_list_w, 1, True);
               }
               else
               {
                  XmListSelectPos(host_list_w, last_removed_position - 1, False);
               }
            }
         }
         else
         {
            XmStringTable xmsel;

            XtVaGetValues(host_list_w,
                          XmNselectedItemCount, &no_selected,
                          XmNselectedItems,     &xmsel,
                          NULL);
            if (no_selected > 0)
            {
               XmListSelectItem(host_list_w, xmsel[no_selected - 1], False);
            }
         }
      }
   } /* if (removed_hosts > 0) */

   (void)sprintf(msg, "Removed %d hosts from FSA.", removed_hosts);
   show_message(statusbox_w, msg);

   return;
}


/*######################## fso_option_changed() #########################*/
void
fso_option_changed(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (fso.value[item_no] != fsa[cur_pos].file_size_offset)
   {
      ce[cur_pos].value_changed |= FILE_SIZE_OFFSET_CHANGED;
      ce[cur_pos].file_size_offset = (signed char)fso.value[item_no];
   }

   return;
}


/*######################### host_switch_toggle() ########################*/
void
host_switch_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE toggles_set = (XT_PTR_TYPE)client_data;

   if (toggles_set == HOST_SWITCHING)
   {
      if (ce[cur_pos].host_switch_toggle == ON)
      {
         XtSetSensitive(second_label_w, False);
         XtSetSensitive(real_hostname_2_w, False);
         ce[cur_pos].host_switch_toggle = OFF;
         XtSetSensitive(host_1_label_w, False);
         XtSetSensitive(host_1_w, False);
         XtSetSensitive(host_2_label_w, False);
         XtSetSensitive(host_2_w, False);
         XtSetSensitive(auto_toggle_w, False);
         XtSetSensitive(successful_retries_label_w, False);
         XtSetSensitive(successful_retries_w, False);
         if (strncmp(fsa[cur_pos].real_hostname[0],
                     fsa[cur_pos].host_alias,
                     strlen(fsa[cur_pos].host_alias)) == 0)
         {
            char real_hostname[MAX_REAL_HOSTNAME_LENGTH];

            (void)strcpy(real_hostname, fsa[cur_pos].host_alias);
            XtVaSetValues(real_hostname_1_w,
                          XmNvalue, real_hostname,
                          NULL);
         }
      }
      else
      {
         int  toggle_pos;
         char host_dsp_name[MAX_HOSTNAME_LENGTH + 1],
              toggle_str[2];

         XtSetSensitive(second_label_w, True);
         XtSetSensitive(real_hostname_2_w, True);
         ce[cur_pos].host_switch_toggle = ON;
         XtSetSensitive(host_1_label_w, True);
         XtSetSensitive(host_1_w, True);
         XtSetSensitive(host_2_label_w, True);
         XtSetSensitive(host_2_w, True);
         XtSetSensitive(auto_toggle_w, True);
         if (ce[cur_pos].auto_toggle == OFF)
         {
            XtSetSensitive(successful_retries_label_w, False);
            XtSetSensitive(successful_retries_w, False);
         }
         else
         {
            XtSetSensitive(successful_retries_label_w, True);
            XtSetSensitive(successful_retries_w, True);
         }

         (void)strcpy(host_dsp_name, fsa[cur_pos].host_alias);
         toggle_pos = strlen(host_dsp_name);
         host_dsp_name[toggle_pos] = ce[cur_pos].host_toggle[(int)(fsa[cur_pos].host_toggle - 1)][0];
         host_dsp_name[toggle_pos + 1] = '\0';
         if (strncmp(fsa[cur_pos].real_hostname[0], fsa[cur_pos].host_alias, toggle_pos) == 0)
         {
            char real_hostname[MAX_REAL_HOSTNAME_LENGTH];

            (void)strcpy(real_hostname, host_dsp_name);
            XtVaSetValues(real_hostname_1_w,
                          XmNvalue, real_hostname,
                          NULL);
         }
         if ((fsa[cur_pos].real_hostname[1][0] == '\0') ||
             (strncmp(fsa[cur_pos].real_hostname[1], fsa[cur_pos].host_alias, toggle_pos) == 0))
         {
            char real_hostname[MAX_REAL_HOSTNAME_LENGTH];

            (void)strcpy(real_hostname, host_dsp_name);
            if (fsa[cur_pos].host_toggle == HOST_ONE)
            {
               real_hostname[toggle_pos] = ce[cur_pos].host_toggle[1][0];
            }
            else
            {
               real_hostname[toggle_pos] = ce[cur_pos].host_toggle[0][0];
            }
            XtVaSetValues(real_hostname_2_w,
                          XmNvalue, real_hostname,
                          NULL);
         }
         ce[cur_pos].host_toggle[0][0] = '1';
         ce[cur_pos].value_changed |= HOST_1_ID_CHANGED;
         ce[cur_pos].host_toggle[1][0] = '2';
         ce[cur_pos].value_changed |= HOST_2_ID_CHANGED;
         toggle_str[1] = '\0';
         toggle_str[0] = ce[cur_pos].host_toggle[0][0];
         XtVaSetValues(host_1_w, XmNvalue, toggle_str, NULL);
         toggle_str[0] = ce[cur_pos].host_toggle[1][0];
         XtVaSetValues(host_2_w, XmNvalue, toggle_str, NULL);
      }
      ce[cur_pos].value_changed |= HOST_SWITCH_TOGGLE_CHANGED;
   }
   else if (toggles_set == AUTO_SWITCHING)
        {
           if (ce[cur_pos].auto_toggle == ON)
           {
              XtSetSensitive(successful_retries_label_w, False);
              XtSetSensitive(successful_retries_w, False);
              ce[cur_pos].auto_toggle = OFF;
           }
           else
           {
              XtSetSensitive(successful_retries_label_w, True);
              XtSetSensitive(successful_retries_w, True);
              ce[cur_pos].auto_toggle = ON;
           }
           ce[cur_pos].value_changed |= AUTO_TOGGLE_CHANGED;
        }
        else
        {
            (void)xrec(w, WARN_DIALOG,
#if SIZEOF_LONG == 4
                       "Unknown toggle set [%d] : (%s %d)",
#else
                       "Unknown toggle set [%ld] : (%s %d)",
#endif
                       toggles_set, __FILE__, __LINE__);
        }

   return;
}


/*######################### pt_option_changed() #########################*/
void
pt_option_changed(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (pt.value[item_no - 1] != fsa[cur_pos].allowed_transfers)
   {
      ce[cur_pos].value_changed |= ALLOWED_TRANSFERS_CHANGED;
      ce[cur_pos].allowed_transfers = pt.value[item_no - 1];
   }

   return;
}


/*######################## nob_option_changed() #########################*/
void
nob_option_changed(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   ce[cur_pos].value_changed |= NO_OF_NO_BURST_CHANGED;
   ce[cur_pos].no_of_no_bursts = nob.value[item_no];

   return;
}


/*####################### ftp_mode_radio_button() #######################*/
void
ftp_mode_radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   ce[cur_pos].value_changed |= FTP_MODE_CHANGED;
   ce[cur_pos].ftp_mode = (XT_PTR_TYPE)client_data;
   if (ce[cur_pos].ftp_mode == FTP_PASSIVE_MODE_SEL)
   {
      XtSetSensitive(passive_redirect_w, True);
   }
   else
   {
      XtSetSensitive(passive_redirect_w, False);
   }

   return;
}


#ifdef WITH_DUP_CHECK
/*########################## edc_radio_button() #########################*/
void
edc_radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char numeric_str[MAX_LONG_LENGTH];

   if ((XT_PTR_TYPE)client_data == ENABLE_DUPCHECK_SEL)
   {
      ce[cur_pos].dup_check_timeout = DEFAULT_DUPCHECK_TIMEOUT;
      ce[cur_pos].dup_check_flag = (DC_FILENAME_ONLY | DC_CRC32 | DC_DELETE);
      XtSetSensitive(dc_type_w, True);
      XtVaSetValues(dc_filename_w, XmNset, True, NULL);
      XtVaSetValues(dc_nosuffix_w, XmNset, False, NULL);
      XtVaSetValues(dc_filecontent_w, XmNset, False, NULL);
      XtVaSetValues(dc_filenamecontent_w, XmNset, False, NULL);
      XtSetSensitive(dc_delete_w, True);
      XtVaSetValues(dc_delete_w, XmNset, True, NULL);
      XtVaSetValues(dc_store_w, XmNset, False, NULL);
      XtSetSensitive(dc_store_w, False);
      XtSetSensitive(dc_warn_w, True);
      XtSetSensitive(dc_timeout_w, True);
      XtSetSensitive(dc_timeout_label_w, True);
      (void)sprintf(numeric_str, "%ld", ce[cur_pos].dup_check_timeout);
      XtVaSetValues(dc_timeout_w, XmNvalue, numeric_str, NULL);
   }
   else
   {
      ce[cur_pos].dup_check_timeout = 0L;
      ce[cur_pos].dup_check_flag = 0;
      XtSetSensitive(dc_type_w, False);
      XtSetSensitive(dc_delete_w, False);
      XtSetSensitive(dc_store_w, False);
      XtSetSensitive(dc_warn_w, False);
      XtSetSensitive(dc_timeout_w, False);
      XtSetSensitive(dc_timeout_label_w, False);
   }
   ce[cur_pos].value_changed |= DC_TYPE_CHANGED;
   ce[cur_pos].value_changed |= DC_DELETE_CHANGED;
   ce[cur_pos].value_changed |= DC_STORE_CHANGED;
   ce[cur_pos].value_changed |= DC_WARN_CHANGED;
   ce[cur_pos].value_changed |= DC_TIMEOUT_CHANGED;

   return;
}


/*######################## dc_type_radio_button() #######################*/
void
dc_type_radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   ce[cur_pos].value_changed |= DC_TYPE_CHANGED;
   if ((XT_PTR_TYPE)client_data == FILE_CONTENT_SEL)
   {
      if (ce[cur_pos].dup_check_flag & DC_FILENAME_ONLY)
      {
         ce[cur_pos].dup_check_flag ^= DC_FILENAME_ONLY;
      }
      if (ce[cur_pos].dup_check_flag & DC_NAME_NO_SUFFIX)
      {
         ce[cur_pos].dup_check_flag ^= DC_NAME_NO_SUFFIX;
      }
      if (ce[cur_pos].dup_check_flag & DC_FILE_CONT_NAME)
      {
         ce[cur_pos].dup_check_flag ^= DC_FILE_CONT_NAME;
      }
      ce[cur_pos].dup_check_flag |= DC_FILE_CONTENT;
   }
   else if ((XT_PTR_TYPE)client_data == FILE_NAME_CONTENT_SEL)
        {
           if (ce[cur_pos].dup_check_flag & DC_FILENAME_ONLY)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILENAME_ONLY;
           }
           if (ce[cur_pos].dup_check_flag & DC_NAME_NO_SUFFIX)
           {
              ce[cur_pos].dup_check_flag ^= DC_NAME_NO_SUFFIX;
           }
           if (ce[cur_pos].dup_check_flag & DC_FILE_CONTENT)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILE_CONTENT;
           }
           ce[cur_pos].dup_check_flag |= DC_FILE_CONT_NAME;
        }
   else if ((XT_PTR_TYPE)client_data == FILE_NOSUFFIX_SEL)
        {
           if (ce[cur_pos].dup_check_flag & DC_FILENAME_ONLY)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILENAME_ONLY;
           }
           if (ce[cur_pos].dup_check_flag & DC_FILE_CONT_NAME)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILE_CONT_NAME;
           }
           if (ce[cur_pos].dup_check_flag & DC_FILE_CONTENT)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILE_CONTENT;
           }
           ce[cur_pos].dup_check_flag |= DC_NAME_NO_SUFFIX;
        }
        else
        {
           if (ce[cur_pos].dup_check_flag & DC_NAME_NO_SUFFIX)
           {
              ce[cur_pos].dup_check_flag ^= DC_NAME_NO_SUFFIX;
           }
           if (ce[cur_pos].dup_check_flag & DC_FILE_CONTENT)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILE_CONTENT;
           }
           if (ce[cur_pos].dup_check_flag & DC_FILE_CONT_NAME)
           {
              ce[cur_pos].dup_check_flag ^= DC_FILE_CONT_NAME;
           }
           ce[cur_pos].dup_check_flag |= DC_FILENAME_ONLY;
        }

   return;
}
#endif /* WITH_DUP_CHECK */


/*########################### toggle_button() ###########################*/
void
toggle_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   ce[cur_pos].value_changed |= (XT_PTR_TYPE)client_data;
   if ((XT_PTR_TYPE)client_data == FTP_EXTENDED_MODE_CHANGED)
   {
      if (XmToggleButtonGetState(w) == True)
      {
         XtSetSensitive(passive_redirect_w, False);
      }
      else
      {
         if (XmToggleButtonGetState(passive_mode_w) == True)
         {
            XtSetSensitive(passive_redirect_w, True);
         }
         else
         {
            XtSetSensitive(passive_redirect_w, False);
         }
      }
   }
#ifdef WITH_DUP_CHECK
   else if ((XT_PTR_TYPE)client_data == DC_DELETE_CHANGED)
        {
           if (XmToggleButtonGetState(w) == True)
           {
              XtVaSetValues(dc_store_w, XmNset, False, NULL);
              XtSetSensitive(dc_store_w, False);
              if ((ce[cur_pos].dup_check_flag & DC_DELETE) == 0)
              {
                 ce[cur_pos].dup_check_flag |= DC_DELETE;
              }
           }
           else
           {
              XtSetSensitive(dc_store_w, True);
              if (ce[cur_pos].dup_check_flag & DC_DELETE)
              {
                 ce[cur_pos].dup_check_flag ^= DC_DELETE;
              }
           }
        }
   else if ((XT_PTR_TYPE)client_data == DC_STORE_CHANGED)
        {
           if (XmToggleButtonGetState(w) == True)
           {
              XtVaSetValues(dc_delete_w, XmNset, False, NULL);
              XtSetSensitive(dc_delete_w, False);
              if ((ce[cur_pos].dup_check_flag & DC_STORE) == 0)
              {
                 ce[cur_pos].dup_check_flag |= DC_STORE;
              }
           }
           else
           {
              XtSetSensitive(dc_delete_w, True);
              if (ce[cur_pos].dup_check_flag & DC_STORE)
              {
                 ce[cur_pos].dup_check_flag ^= DC_STORE;
              }
           }
        }
   else if ((XT_PTR_TYPE)client_data == DC_WARN_CHANGED)
        {
           if (XmToggleButtonGetState(w) == True)
           {
              if ((ce[cur_pos].dup_check_flag & DC_WARN) == 0)
              {
                 ce[cur_pos].dup_check_flag |= DC_WARN;
              }
           }
           else
           {
              if (ce[cur_pos].dup_check_flag & DC_WARN)
              {
                 ce[cur_pos].dup_check_flag ^= DC_WARN;
              }
           }
        }
#endif

   return;
}


/*########################### toggle_button2() ##########################*/
void
toggle_button2(Widget w, XtPointer client_data, XtPointer call_data)
{
   ce[cur_pos].value_changed2 |= (XT_PTR_TYPE)client_data;

   return;
}


/*########################### value_change() ############################*/
void
value_change(Widget w, XtPointer client_data, XtPointer call_data)
{
   value_changed = YES;

   return;
}


/*############################ save_input() #############################*/
void
save_input(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (value_changed == YES)
   {
      XT_PTR_TYPE choice = (XT_PTR_TYPE)client_data;
      char        *input_data = XmTextGetString(w);

      value_changed = NO;
      switch (choice)
      {
         case REAL_HOST_NAME_1 :
            if (input_data[0] != '\0')
            {
               (void)strcpy(ce[cur_pos].real_hostname[0], input_data);
            }
            else
            {
               ce[cur_pos].real_hostname[0][0] = '\0';
            }
            ce[cur_pos].value_changed |= REAL_HOSTNAME_1_CHANGED;
            break;

         case REAL_HOST_NAME_2 :
            if (input_data[0] != '\0')
            {
               (void)strcpy(ce[cur_pos].real_hostname[1], input_data);
            }
            else
            {
               ce[cur_pos].real_hostname[1][0] = '\0';
            }
            ce[cur_pos].value_changed |= REAL_HOSTNAME_2_CHANGED;
            break;

         case HOST_1_ID :
            if (input_data[0] != '\0')
            {
               ce[cur_pos].host_toggle[0][0] = *input_data;
            }
            else
            {
               ce[cur_pos].host_toggle[0][0] = '1';
            }
            ce[cur_pos].value_changed |= HOST_1_ID_CHANGED;
            break;

         case HOST_2_ID :
            if (input_data[0] != '\0')
            {
               ce[cur_pos].host_toggle[1][0] = *input_data;
            }
            else
            {
               ce[cur_pos].host_toggle[1][0] = '2';
            }
            ce[cur_pos].value_changed |= HOST_2_ID_CHANGED;
            break;

         case PROXY_NAME :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].proxy_name[0] = '\0';
            }
            else
            {
               int length = strlen(input_data);

               if (length > MAX_PROXY_NAME_LENGTH)
               {
                  (void)memcpy(ce[cur_pos].proxy_name, input_data,
                               MAX_PROXY_NAME_LENGTH);
                  ce[cur_pos].proxy_name[MAX_PROXY_NAME_LENGTH] = '\0';
                  XmTextSetString(w, ce[cur_pos].proxy_name);
                  XFlush(display);
                  (void)xrec(w, INFO_DIALOG,
                             "Proxy length to long. Cutting off extra length.");
               }
               else
               {
                  (void)memcpy(ce[cur_pos].proxy_name, input_data, length);
                  ce[cur_pos].proxy_name[length] = '\0';
               }
            }
            ce[cur_pos].value_changed |= PROXY_NAME_CHANGED;
            break;

         case TRANSFER_TIMEOUT :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
            }
            else
            {
               ce[cur_pos].transfer_timeout = atol(input_data);
            }
            ce[cur_pos].value_changed |= TRANSFER_TIMEOUT_CHANGED;
            break;

         case RETRY_INTERVAL :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].retry_interval = DEFAULT_RETRY_INTERVAL;
            }
            else
            {
               ce[cur_pos].retry_interval = atoi(input_data);
            }
            ce[cur_pos].value_changed |= RETRY_INTERVAL_CHANGED;
            break;

         case MAXIMUM_ERRORS :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].max_errors = DEFAULT_MAX_ERRORS;
            }
            else
            {
               ce[cur_pos].max_errors = atoi(input_data);
            }
            ce[cur_pos].value_changed |= MAX_ERRORS_CHANGED;
            break;

         case SUCCESSFUL_RETRIES :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].max_successful_retries = DEFAULT_SUCCESSFUL_RETRIES;
            }
            else
            {
               ce[cur_pos].max_successful_retries = atoi(input_data);
            }
            ce[cur_pos].value_changed |= SUCCESSFUL_RETRIES_CHANGED;
            break;

         case TRANSFER_RATE_LIMIT :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].transfer_rate_limit = 0;
            }
            else
            {
               ce[cur_pos].transfer_rate_limit = (off_t)strtoul(input_data, NULL, 0) * 1024;
            }
            ce[cur_pos].value_changed |= TRANSFER_RATE_LIMIT_CHANGED;
            break;

         case SOCKET_SEND_BUFFER :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].sndbuf_size = 0;
            }
            else
            {
               ce[cur_pos].sndbuf_size = (unsigned int)strtoul(input_data, NULL, 10) * 1024;
            }
            ce[cur_pos].value_changed |= SOCKET_SEND_BUFFER_CHANGED;
            break;

         case SOCKET_RECEIVE_BUFFER :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].rcvbuf_size = 0;
            }
            else
            {
               ce[cur_pos].rcvbuf_size = (unsigned int)strtoul(input_data, NULL, 10) * 1024;
            }
            ce[cur_pos].value_changed |= SOCKET_RECEIVE_BUFFER_CHANGED;
            break;

         case KEEP_CONNECTED :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].keep_connected = 0;
            }
            else
            {
               ce[cur_pos].keep_connected = (unsigned int)strtoul(input_data, NULL, 10);
            }
            ce[cur_pos].value_changed |= KEEP_CONNECTED_CHANGED;
            break;

#ifdef WITH_DUP_CHECK
         case DC_TIMEOUT :
            if (input_data[0] == '\0')
            {
               ce[cur_pos].dup_check_timeout = 0L;
            }
            else
            {
               ce[cur_pos].dup_check_timeout = atol(input_data);
            }
            ce[cur_pos].value_changed |= DC_TIMEOUT_CHANGED;
            break;
#endif

         default :
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Please inform programmer he is doing something wrong here!");
            break;
      }

      XtFree(input_data);
   } /* if (value_changed == YES) */

   return;
}


/*############################# selected() ##############################*/
void
selected(Widget w, XtPointer client_data, XtPointer call_data)
{
   static int           last_select = -1;
   char                 *host_selected;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   /* Clear message area when clicking on a host alias. */
   reset_message(statusbox_w);

   /* Get the selected hostname and the position in the FSA. */
   XmStringGetLtoR(cbs->item, XmFONTLIST_DEFAULT_TAG, &host_selected);
   (void)strcpy(last_selected_host, host_selected);
   if ((cur_pos = get_host_position(fsa, host_selected, no_of_hosts)) < 0)
   {
      (void)xrec(w, FATAL_DIALOG,
                 "AAAaaaarrrrghhhh!!! Could not find host %s in FSA. (%s %d)",
                 host_selected, __FILE__, __LINE__);
      return;
   }
   XtFree(host_selected);

   /*
    * Don't always show the same data!!!!
    */
   if (cur_pos != last_select)
   {
      int      choice;
      char     *tmp_ptr,
               numeric_str[MAX_LONG_LENGTH];
      XmString xstr;

      last_select = cur_pos;

      if ((fsa[cur_pos].protocol & FTP_FLAG) ||
          (fsa[cur_pos].protocol & SFTP_FLAG) ||
          (fsa[cur_pos].protocol & HTTP_FLAG) ||
#ifdef _WITH_SCP_SUPPORT
          (fsa[cur_pos].protocol & SCP_FLAG) ||
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
          (fsa[cur_pos].protocol & WMO_FLAG) ||
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
          (fsa[cur_pos].protocol & MAP_FLAG) ||
#endif /* _WITH_MAP_SUPPORT */
          (fsa[cur_pos].protocol & SMTP_FLAG))
      {
         XtSetSensitive(host_switch_toggle_w, True);
         XtSetSensitive(real_hostname_1_w, True);
         XtSetSensitive(transfer_timeout_w, True);

         /* Activate/Deactivate 2nd host name string. */
         if (fsa[cur_pos].host_toggle_str[0] == '\0')
         {
            XtSetSensitive(second_label_w, False);
            XtSetSensitive(real_hostname_2_w, False);
            label_str[5] = '1';
            xstr = XmStringCreateLtoR(label_str, XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(first_label_w,
                          XmNlabelString, xstr,
                          NULL);
            XmStringFree(xstr);

            ce[cur_pos].host_switch_toggle = OFF;
            XtSetSensitive(host_1_label_w, False);
            XtSetSensitive(host_1_w, False);
            XtSetSensitive(host_2_label_w, False);
            XtSetSensitive(host_2_w, False);
            XtSetSensitive(auto_toggle_w, False);
            XtVaSetValues(host_switch_toggle_w, XmNset, False, NULL);
         }
         else
         {
            char toggle_str[2];

            XtSetSensitive(second_label_w, True);
            XtSetSensitive(real_hostname_2_w, True);
            label_str[5] = fsa[cur_pos].host_toggle_str[HOST_ONE];
            xstr = XmStringCreateLtoR(label_str, XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(first_label_w,
                          XmNlabelString, xstr,
                          NULL);
            XmStringFree(xstr);
            label_str[5] = fsa[cur_pos].host_toggle_str[HOST_TWO];
            xstr = XmStringCreateLtoR(label_str, XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(second_label_w,
                          XmNlabelString, xstr,
                          NULL);
            XmStringFree(xstr);

            XtVaSetValues(host_switch_toggle_w, XmNset, True, NULL);
            ce[cur_pos].host_switch_toggle = ON;
            XtSetSensitive(host_1_label_w, True);
            XtSetSensitive(host_1_w, True);
            toggle_str[1] = '\0';
            toggle_str[0] = fsa[cur_pos].host_toggle_str[HOST_ONE];
            XtVaSetValues(host_1_w, XmNvalue, toggle_str, NULL);
            XtSetSensitive(host_2_label_w, True);
            XtSetSensitive(host_2_w, True);
            toggle_str[0] = fsa[cur_pos].host_toggle_str[HOST_TWO];
            XtVaSetValues(host_2_w, XmNvalue, toggle_str, NULL);
            XtSetSensitive(auto_toggle_w, True);
         }

         if (ce[cur_pos].value_changed & REAL_HOSTNAME_1_CHANGED)
         {
            tmp_ptr = ce[cur_pos].real_hostname[0];
         }
         else
         {
            tmp_ptr = fsa[cur_pos].real_hostname[0];
         }
         XtVaSetValues(real_hostname_1_w, XmNvalue, tmp_ptr, NULL);

         if (ce[cur_pos].value_changed & REAL_HOSTNAME_2_CHANGED)
         {
            tmp_ptr = ce[cur_pos].real_hostname[1];
         }
         else
         {
            tmp_ptr = fsa[cur_pos].real_hostname[1];
         }
         XtVaSetValues(real_hostname_2_w, XmNvalue, tmp_ptr, NULL);

         if (ce[cur_pos].value_changed & TRANSFER_TIMEOUT_CHANGED)
         {
            (void)sprintf(numeric_str, "%ld", ce[cur_pos].transfer_timeout);
         }
         else
         {
            (void)sprintf(numeric_str, "%ld", fsa[cur_pos].transfer_timeout);
         }
         XtVaSetValues(transfer_timeout_w, XmNvalue, numeric_str, NULL);

         if (fsa[cur_pos].auto_toggle == ON)
         {
            XtSetSensitive(successful_retries_label_w, True);
            XtSetSensitive(successful_retries_w, True);
            XtVaSetValues(auto_toggle_w, XmNset, True, NULL);
            ce[cur_pos].auto_toggle = ON;
            if (ce[cur_pos].value_changed & SUCCESSFUL_RETRIES_CHANGED)
            {
               (void)sprintf(numeric_str, "%d",
                             ce[cur_pos].max_successful_retries);
            }
            else
            {
               (void)sprintf(numeric_str, "%d",
                             fsa[cur_pos].max_successful_retries);
            }
            XtVaSetValues(successful_retries_w, XmNvalue, numeric_str, NULL);
         }
         else
         {
            XtSetSensitive(successful_retries_label_w, False);
            XtSetSensitive(successful_retries_w, False);
            XtVaSetValues(auto_toggle_w, XmNset, False, NULL);
            ce[cur_pos].auto_toggle = OFF;
         }

         XtSetSensitive(transfer_rate_limit_label_w, True);
         XtSetSensitive(transfer_rate_limit_w, True);
         if (ce[cur_pos].value_changed & TRANSFER_RATE_LIMIT_CHANGED)
         {
            if (ce[cur_pos].transfer_rate_limit < 1024)
            {
               numeric_str[0] = '0';
               numeric_str[1] = '\0';
            }
            else
            {
#if SIZEOF_OFF_T == 4
               (void)sprintf(numeric_str, "%ld",
#else
               (void)sprintf(numeric_str, "%lld",
#endif
                             (pri_off_t)(ce[cur_pos].transfer_rate_limit / 1024));
            }
         }
         else
         {
            if (fsa[cur_pos].transfer_rate_limit < 1024)
            {
               numeric_str[0] = '0';
               numeric_str[1] = '\0';
            }
            else
            {
#if SIZEOF_OFF_T == 4
               (void)sprintf(numeric_str, "%ld",
#else
               (void)sprintf(numeric_str, "%lld",
#endif
                             (pri_off_t)(fsa[cur_pos].transfer_rate_limit / 1024));
            }
         }
         XtVaSetValues(transfer_rate_limit_w, XmNvalue, numeric_str, NULL);

         XtSetSensitive(socket_send_buffer_size_label_w, True);
         XtSetSensitive(socket_send_buffer_size_w, True);
         if (ce[cur_pos].value_changed & SOCKET_SEND_BUFFER_CHANGED)
         {
            if (ce[cur_pos].sndbuf_size < 1024)
            {
               numeric_str[0] = '0';
               numeric_str[1] = '\0';
            }
            else
            {
               (void)sprintf(numeric_str, "%u",
                             ce[cur_pos].sndbuf_size / 1024U);
            }
         }
         else
         {
            if (fsa[cur_pos].socksnd_bufsize < 1024)
            {
               numeric_str[0] = '0';
               numeric_str[1] = '\0';
            }
            else
            {
               (void)sprintf(numeric_str, "%u",
                             fsa[cur_pos].socksnd_bufsize / 1024U);
            }
         }
         XtVaSetValues(socket_send_buffer_size_w, XmNvalue, numeric_str, NULL);

         XtSetSensitive(socket_receive_buffer_size_label_w, True);
         XtSetSensitive(socket_receive_buffer_size_w, True);
         if (ce[cur_pos].value_changed & SOCKET_SEND_BUFFER_CHANGED)
         {
            if (ce[cur_pos].rcvbuf_size < 1024)
            {
               numeric_str[0] = '0';
               numeric_str[1] = '\0';
            }
            else
            {
               (void)sprintf(numeric_str, "%u",
                             ce[cur_pos].rcvbuf_size / 1024U);
            }
         }
         else
         {
            if (fsa[cur_pos].sockrcv_bufsize < 1024)
            {
               numeric_str[0] = '0';
               numeric_str[1] = '\0';
            }
            else
            {
               (void)sprintf(numeric_str, "%u",
                             fsa[cur_pos].sockrcv_bufsize / 1024U);
            }
         }
         XtVaSetValues(socket_receive_buffer_size_w, XmNvalue, numeric_str, NULL);
      }
      else
      {
         XtSetSensitive(host_switch_toggle_w, False);
         XtSetSensitive(host_1_label_w, False);
         XtSetSensitive(host_1_w, False);
         XtSetSensitive(host_2_label_w, False);
         XtSetSensitive(host_2_w, False);
         XtSetSensitive(auto_toggle_w, False);
         XtSetSensitive(real_hostname_1_w, False);
         XtSetSensitive(real_hostname_2_w, False);
         XtSetSensitive(transfer_timeout_w, False);
         XtSetSensitive(successful_retries_label_w, False);
         XtSetSensitive(successful_retries_w, False);
         XtSetSensitive(transfer_rate_limit_label_w, False);
         XtSetSensitive(transfer_rate_limit_w, False);
         XtSetSensitive(socket_send_buffer_size_label_w, False);
         XtSetSensitive(socket_send_buffer_size_w, False);
         XtSetSensitive(socket_receive_buffer_size_label_w, False);
         XtSetSensitive(socket_receive_buffer_size_w, False);
      }

      if (fsa[cur_pos].protocol & FTP_FLAG)
      {
         XtSetSensitive(proxy_box_w, True);
         if (ce[cur_pos].value_changed & PROXY_NAME_CHANGED)
         {
            tmp_ptr = ce[cur_pos].proxy_name;
         }
         else
         {
            tmp_ptr = fsa[cur_pos].proxy_name;
         }
         XtVaSetValues(proxy_name_w, XmNvalue, tmp_ptr, NULL);
         XtSetSensitive(mode_label_w, True);
         XtSetSensitive(extended_mode_w, True);
         if (fsa[cur_pos].protocol_options & FTP_EXTENDED_MODE)
         {
            XtVaSetValues(extended_mode_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(extended_mode_w, XmNset, False, NULL);
         }
         XtSetSensitive(ftp_mode_w, True);
         if (fsa[cur_pos].protocol_options & FTP_PASSIVE_MODE)
         {
            XtVaSetValues(passive_mode_w, XmNset, True, NULL);
            XtVaSetValues(active_mode_w, XmNset, False, NULL);
            if ((fsa[cur_pos].protocol_options & FTP_EXTENDED_MODE) == 0)
            {
               XtSetSensitive(passive_redirect_w, True);
               if (fsa[cur_pos].protocol_options & FTP_ALLOW_DATA_REDIRECT)
               {
                  XtVaSetValues(passive_redirect_w, XmNset, True, NULL);
               }
               else
               {
                  XtVaSetValues(passive_redirect_w, XmNset, False, NULL);
               }
            }
            else
            {
               XtSetSensitive(passive_redirect_w, False);
               XtVaSetValues(passive_redirect_w, XmNset, False, NULL);
            }
         }
         else
         {
            XtSetSensitive(passive_redirect_w, False);
            XtVaSetValues(passive_mode_w, XmNset, False, NULL);
            XtVaSetValues(active_mode_w, XmNset, True, NULL);
         }
         XtSetSensitive(ftp_idle_time_w, True);
         if (fsa[cur_pos].protocol_options & SET_IDLE_TIME)
         {
            XtVaSetValues(ftp_idle_time_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(ftp_idle_time_w, XmNset, False, NULL);
         }
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         XtSetSensitive(ftp_keepalive_w, True);
         if (fsa[cur_pos].protocol_options & STAT_KEEPALIVE)
         {
            XtVaSetValues(ftp_keepalive_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(ftp_keepalive_w, XmNset, False, NULL);
         }
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
         XtSetSensitive(ftp_fast_rename_w, True);
         if (fsa[cur_pos].protocol_options & FTP_FAST_MOVE)
         {
            XtVaSetValues(ftp_fast_rename_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(ftp_fast_rename_w, XmNset, False, NULL);
         }
         XtSetSensitive(ftp_fast_cd_w, True);
         if (fsa[cur_pos].protocol_options & FTP_FAST_CD)
         {
            XtVaSetValues(ftp_fast_cd_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(ftp_fast_cd_w, XmNset, False, NULL);
         }
         XtSetSensitive(ftp_ignore_bin_w, True);
         if (fsa[cur_pos].protocol_options & FTP_IGNORE_BIN)
         {
            XtVaSetValues(ftp_ignore_bin_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(ftp_ignore_bin_w, XmNset, False, NULL);
         }
      }
      else
      {
         XtSetSensitive(proxy_box_w, False);
         XtSetSensitive(mode_label_w, False);
         XtSetSensitive(extended_mode_w, False);
         XtSetSensitive(ftp_mode_w, False);
         XtSetSensitive(passive_redirect_w, False);
         XtSetSensitive(ftp_idle_time_w, False);
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         XtSetSensitive(ftp_keepalive_w, False);
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
         XtSetSensitive(ftp_fast_rename_w, False);
         if (fsa[cur_pos].protocol & SFTP_FLAG)
         {
            XtSetSensitive(ftp_fast_cd_w, True);
            if (fsa[cur_pos].protocol_options & FTP_FAST_CD)
            {
               XtVaSetValues(ftp_fast_cd_w, XmNset, True, NULL);
            }
            else
            {
               XtVaSetValues(ftp_fast_cd_w, XmNset, False, NULL);
            }
         }
         else
         {
            XtSetSensitive(ftp_fast_cd_w, False);
         }
         XtSetSensitive(ftp_ignore_bin_w, False);
      }

#ifdef _WITH_BURST_2
      if (fsa[cur_pos].protocol_options & DISABLE_BURSTING)
      {
         XtVaSetValues(allow_burst_w, XmNset, False, NULL);
      }
      else
      {
         XtVaSetValues(allow_burst_w, XmNset, True, NULL);
      }
#endif

      if (ce[cur_pos].value_changed & RETRY_INTERVAL_CHANGED)
      {
         (void)sprintf(numeric_str, "%d", ce[cur_pos].retry_interval);
      }
      else
      {
         (void)sprintf(numeric_str, "%d", fsa[cur_pos].retry_interval);
      }
      XtVaSetValues(retry_interval_w, XmNvalue, numeric_str, NULL);

      if (ce[cur_pos].value_changed & MAX_ERRORS_CHANGED)
      {
         (void)sprintf(numeric_str, "%d", ce[cur_pos].max_errors);
      }
      else
      {
         (void)sprintf(numeric_str, "%d", fsa[cur_pos].max_errors);
      }
      XtVaSetValues(max_errors_w, XmNvalue, numeric_str, NULL);

      if (ce[cur_pos].value_changed & KEEP_CONNECTED_CHANGED)
      {
         (void)sprintf(numeric_str, "%u", ce[cur_pos].keep_connected);
      }
      else
      {
         (void)sprintf(numeric_str, "%u", fsa[cur_pos].keep_connected);
      }
      XtVaSetValues(keep_connected_w, XmNvalue, numeric_str, NULL);

#ifdef WITH_DUP_CHECK
      if (((ce[cur_pos].value_changed & DC_TYPE_CHANGED) == 0) &&
          ((ce[cur_pos].value_changed & DC_DELETE_CHANGED) == 0) &&
          ((ce[cur_pos].value_changed & DC_STORE_CHANGED) == 0) &&
          ((ce[cur_pos].value_changed & DC_WARN_CHANGED) == 0) &&
          ((ce[cur_pos].value_changed & DC_TIMEOUT_CHANGED) == 0))
      {
         if (fsa[cur_pos].dup_check_timeout == 0L)
         {
            ce[cur_pos].dup_check_timeout = 0L;
            ce[cur_pos].dup_check_flag = 0;
            XtSetSensitive(dc_type_w, False);
            XtSetSensitive(dc_delete_w, False);
            XtSetSensitive(dc_store_w, False);
            XtSetSensitive(dc_warn_w, False);
            XtSetSensitive(dc_timeout_w, False);
            XtSetSensitive(dc_timeout_label_w, False);
            XmToggleButtonSetState(dc_disable_w, True, True);
         }
         else
         {
            ce[cur_pos].dup_check_timeout = fsa[cur_pos].dup_check_timeout;
            ce[cur_pos].dup_check_flag = fsa[cur_pos].dup_check_flag;
            XtSetSensitive(dc_type_w, True);
            XtSetSensitive(dc_delete_w, True);
            XtSetSensitive(dc_store_w, True);
            XtSetSensitive(dc_warn_w, True);
            XtSetSensitive(dc_timeout_w, True);
            XtSetSensitive(dc_timeout_label_w, True);
            XmToggleButtonSetState(dc_enable_w, True, True);
         }
      }

      if ((ce[cur_pos].value_changed & DC_TYPE_CHANGED) == 0)
      {
         if (fsa[cur_pos].dup_check_flag & DC_FILE_CONTENT)
         {
            XtVaSetValues(dc_filename_w, XmNset, False, NULL);
            XtVaSetValues(dc_nosuffix_w, XmNset, False, NULL);
            XtVaSetValues(dc_filecontent_w, XmNset, True, NULL);
            XtVaSetValues(dc_filenamecontent_w, XmNset, False, NULL);
         }
         else if (fsa[cur_pos].dup_check_flag & DC_FILE_CONT_NAME)
              {
                 XtVaSetValues(dc_filename_w, XmNset, False, NULL);
                 XtVaSetValues(dc_nosuffix_w, XmNset, False, NULL);
                 XtVaSetValues(dc_filecontent_w, XmNset, False, NULL);
                 XtVaSetValues(dc_filenamecontent_w, XmNset, True, NULL);
              }
         else if (fsa[cur_pos].dup_check_flag & DC_NAME_NO_SUFFIX)
              {
                 XtVaSetValues(dc_filename_w, XmNset, False, NULL);
                 XtVaSetValues(dc_nosuffix_w, XmNset, True, NULL);
                 XtVaSetValues(dc_filecontent_w, XmNset, False, NULL);
                 XtVaSetValues(dc_filenamecontent_w, XmNset, False, NULL);
              }
              else
              {
                 XtVaSetValues(dc_filename_w, XmNset, True, NULL);
                 XtVaSetValues(dc_nosuffix_w, XmNset, False, NULL);
                 XtVaSetValues(dc_filecontent_w, XmNset, False, NULL);
                 XtVaSetValues(dc_filenamecontent_w, XmNset, False, NULL);
              }
      }

      if ((ce[cur_pos].value_changed & DC_DELETE_CHANGED) == 0)
      {
         if (fsa[cur_pos].dup_check_flag & DC_DELETE)
         {
            XtVaSetValues(dc_delete_w, XmNset, True, NULL);
            XtVaSetValues(dc_store_w, XmNset, False, NULL);
            XtSetSensitive(dc_store_w, False);
         }
         else
         {
            XtVaSetValues(dc_delete_w, XmNset, False, NULL);
         }
      }

      if ((ce[cur_pos].value_changed & DC_STORE_CHANGED) == 0)
      {
         if (fsa[cur_pos].dup_check_flag & DC_STORE)
         {
            XtVaSetValues(dc_store_w, XmNset, True, NULL);
            XtVaSetValues(dc_delete_w, XmNset, False, NULL);
            XtSetSensitive(dc_delete_w, False);
         }
         else
         {
            XtVaSetValues(dc_store_w, XmNset, False, NULL);
         }
      }

      if ((ce[cur_pos].value_changed & DC_WARN_CHANGED) == 0)
      {
         if (fsa[cur_pos].dup_check_flag & DC_WARN)
         {
            XtVaSetValues(dc_warn_w, XmNset, True, NULL);
         }
         else
         {
            XtVaSetValues(dc_warn_w, XmNset, False, NULL);
         }
      }

      if (ce[cur_pos].value_changed & DC_TIMEOUT_CHANGED)
      {
         (void)sprintf(numeric_str, "%ld", ce[cur_pos].dup_check_timeout);
      }
      else
      {
         (void)sprintf(numeric_str, "%ld", fsa[cur_pos].dup_check_timeout);
      }
      XtVaSetValues(dc_timeout_w, XmNvalue, numeric_str, NULL);
#endif /* WITH_DUP_CHECK */

      /* Set option menu for Parallel Transfers */
      if (ce[cur_pos].value_changed & ALLOWED_TRANSFERS_CHANGED)
      {
         choice = ce[cur_pos].allowed_transfers - 1;
      }
      else
      {
         choice = fsa[cur_pos].allowed_transfers - 1;
      }
      if (choice < 0)
      {
         choice = 0;
      }
      XtVaSetValues(pt.option_menu_w,
                    XmNmenuHistory, pt.button_w[choice],
                    NULL);
      XmUpdateDisplay(pt.option_menu_w);

      /* Set option menu for Transfer Blocksize */
      if (ce[cur_pos].value_changed & BLOCK_SIZE_CHANGED)
      {
         if (ce[cur_pos].block_size == tb.value[0])
         {
            choice = 0;
         }
         else if (ce[cur_pos].block_size == tb.value[1])
              {
                 choice = 1;
              }
         else if (ce[cur_pos].block_size == tb.value[3])
              {
                 choice = 3;
              }
         else if (ce[cur_pos].block_size == tb.value[4])
              {
                 choice = 4;
              }
         else if (ce[cur_pos].block_size == tb.value[5])
              {
                 choice = 5;
              }
         else if (ce[cur_pos].block_size == tb.value[6])
              {
                 choice = 6;
              }
         else if (ce[cur_pos].block_size == tb.value[7])
              {
                 choice = 7;
              }
         else if (ce[cur_pos].block_size == tb.value[8])
              {
                 choice = 8;
              }
         else if (ce[cur_pos].block_size == tb.value[9])
              {
                 choice = 9;
              }
              else
              {
                 choice = 2;
              }
      }
      else
      {
         if (fsa[cur_pos].block_size == tb.value[0])
         {
            choice = 0;
         }
         else if (fsa[cur_pos].block_size == tb.value[1])
              {
                 choice = 1;
              }
         else if (fsa[cur_pos].block_size == tb.value[3])
              {
                 choice = 3;
              }
         else if (fsa[cur_pos].block_size == tb.value[4])
              {
                 choice = 4;
              }
         else if (fsa[cur_pos].block_size == tb.value[5])
              {
                 choice = 5;
              }
         else if (fsa[cur_pos].block_size == tb.value[6])
              {
                 choice = 6;
              }
         else if (fsa[cur_pos].block_size == tb.value[7])
              {
                 choice = 7;
              }
         else if (fsa[cur_pos].block_size == tb.value[8])
              {
                 choice = 8;
              }
         else if (fsa[cur_pos].block_size == tb.value[9])
              {
                 choice = 9;
              }
              else
              {
                 choice = 2;
              }
      }
      XtVaSetValues(tb.option_menu_w,
                    XmNmenuHistory, tb.button_w[choice],
                    NULL);
      XmUpdateDisplay(tb.option_menu_w);

      /* Set option menu for Filesize Offset. */
      if ((fsa[cur_pos].protocol & FTP_FLAG) ||
          (fsa[cur_pos].protocol & SFTP_FLAG))
      {
         int     i,
                 max_fso_buttons;
         Boolean sensitive;

         XtSetSensitive(fso.button_w[0], True);
         XtSetSensitive(fso.button_w[1], True);
         if (fsa[cur_pos].protocol & FTP_FLAG)
         {
            max_fso_buttons = MAX_FSO_BUTTONS;
            sensitive = True;
         }
         else
         {
            max_fso_buttons = MAX_FSO_SFTP_BUTTONS;
            sensitive = False;
         }
         for (i = 2; i < MAX_FSO_BUTTONS; i++)
         {
            XtSetSensitive(fso.button_w[i], sensitive);
         }
         XtSetSensitive(fso.option_menu_w, True);
         if (ce[cur_pos].value_changed & FILE_SIZE_OFFSET_CHANGED)
         {
            if ((ce[cur_pos].file_size_offset == -1) ||
                (ce[cur_pos].file_size_offset > (max_fso_buttons - 1)))
            {
               choice = 0;
            }
            else if (ce[cur_pos].file_size_offset == AUTO_SIZE_DETECT)
                 {
                    choice = 1;
                 }
                 else
                 {
                    choice = ce[cur_pos].file_size_offset;
                 }
         }
         else
         {
            if ((fsa[cur_pos].file_size_offset == -1) ||
                (fsa[cur_pos].file_size_offset > (max_fso_buttons - 1)))
            {
               choice = 0;
            }
            else if (fsa[cur_pos].file_size_offset == AUTO_SIZE_DETECT)
                 {
                    choice = 1;
                 }
                 else
                 {
                    choice = fsa[cur_pos].file_size_offset;
                 }
         }
         XtVaSetValues(fso.option_menu_w,
                       XmNmenuHistory, fso.button_w[choice],
                       NULL);
         XmUpdateDisplay(fso.option_menu_w);
      }
      else
      {
         XtSetSensitive(fso.option_menu_w, False);
      }

      /* Set option menu for number of no bursts */
      if ((fsa[cur_pos].protocol & FTP_FLAG)
          || (fsa[cur_pos].protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
          || (fsa[cur_pos].protocol & WMO_FLAG)
#endif
#ifdef _WITH_SCP_SUPPORT
          || (fsa[cur_pos].protocol & SCP_FLAG))
#else
         )
#endif
      {
         XtSetSensitive(nob.option_menu_w, True);
         if (ce[cur_pos].value_changed & NO_OF_NO_BURST_CHANGED)
         {
            choice = ce[cur_pos].no_of_no_bursts;
         }
         else
         {
            choice = (fsa[cur_pos].special_flag & NO_BURST_COUNT_MASK);
         }
         XtVaSetValues(nob.option_menu_w,
                       XmNmenuHistory, nob.button_w[choice],
                       NULL);
         XmUpdateDisplay(nob.option_menu_w);
      }
      else
      {
         XtSetSensitive(nob.option_menu_w, False);
      }

      /* See if we need to disable the remove button. */
      if (fsa[cur_pos].special_flag & HOST_IN_DIR_CONFIG)
      {
         XtSetSensitive(rm_button_w, False);
      }
      else
      {
         XtSetSensitive(rm_button_w, True);
      }
   } /* if (cur_pos != last_select) */

   return;
}


/*########################### submite_button() ##########################*/
void
submite_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int  i,
        changes = 0,
        changed_hosts = 0,
        prev_changes;
   char **host_list = NULL,
        msg[MAX_MESSAGE_LENGTH];

   /* Ensure that the FSA we are mapped to is up to date */
   if (check_fsa(NO) == YES)
   {
      char user[MAX_FULL_USER_ID_LENGTH];

      get_user(user, fake_user);
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "%s was using edit_hc while someone changed the DIR_CONFIG!",
                 user);
      (void)xrec(w, FATAL_DIALOG, "DO NOT EDIT THE DIR_CONFIG FILE WHILE USING edit_hc!!!!");
      return;
   }

   RT_ARRAY(host_list, no_of_hosts, (MAX_HOSTNAME_LENGTH + 1), char);

   /*
    * See if any data was changed by the user. Only change those that
    * really did change. The reason why I am using this complex method is
    * that I am not certain what happens to a process that is reading at
    * the same time that this function is writing the data. Locking is
    * also no solution since there are lots of processes that use these
    * variables and would cost to much CPU time (especially on SMP
    * machines).
    */
   for (i = 0; i < no_of_hosts; i++)
   {
      if ((ce[i].value_changed != 0) || (ce[i].value_changed2 != 0))
      {
         prev_changes = changes;
         if (ce[i].value_changed & REAL_HOSTNAME_1_CHANGED)
         {
            if (ce[i].real_hostname[0][0] != '\0')
            {
               (void)strcpy(fsa[i].real_hostname[0], ce[i].real_hostname[0]);
               ce[i].real_hostname[0][0] = -1;
               changes++;
            }
            else
            {
               show_message(statusbox_w, REAL_HOST_NAME_WRONG);
               if (host_list != NULL)
               {
                  FREE_RT_ARRAY(host_list);
               }
               return;
            }
         }
         if (ce[i].value_changed & REAL_HOSTNAME_2_CHANGED)
         {
            if ((ce[i].real_hostname[1][0] != '\0') ||
                ((ce[i].real_hostname[1][0] == '\0') &&
                 (ce[i].host_switch_toggle != ON)))
            {
               (void)strcpy(fsa[i].real_hostname[1], ce[i].real_hostname[1]);
               ce[i].real_hostname[1][0] = -1;
               changes++;
            }
            else
            {
               show_message(statusbox_w, REAL_HOST_NAME_WRONG);
               if (host_list != NULL)
               {
                  FREE_RT_ARRAY(host_list);
               }
               return;
            }
         }
         if (ce[i].value_changed & HOST_1_ID_CHANGED)
         {
            fsa[i].host_toggle_str[HOST_ONE] = ce[i].host_toggle[0][0];
            if (fsa[i].host_toggle == HOST_ONE)
            {
               if ((fsa[i].toggle_pos = strlen(fsa[i].host_alias)) >= MAX_HOSTNAME_LENGTH)
               {
                  fsa[i].toggle_pos = MAX_HOSTNAME_LENGTH;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
            }
            changes++;
         }
         if (ce[i].value_changed & HOST_2_ID_CHANGED)
         {
            fsa[i].host_toggle_str[HOST_TWO] = ce[i].host_toggle[1][0];
            if (fsa[i].host_toggle == HOST_TWO)
            {
               if ((fsa[i].toggle_pos = strlen(fsa[i].host_alias)) >= MAX_HOSTNAME_LENGTH)
               {
                  fsa[i].toggle_pos = MAX_HOSTNAME_LENGTH;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
            }
            changes++;
         }
         if (ce[i].value_changed & PROXY_NAME_CHANGED)
         {
            if (ce[i].proxy_name[0] == '\0')
            {
               fsa[i].proxy_name[0] = '\0';
            }
            else
            {
               (void)strcpy(fsa[i].proxy_name, ce[i].proxy_name);
            }
            ce[i].proxy_name[0] = -1;
            changes++;
         }
         if (ce[i].value_changed & AUTO_TOGGLE_CHANGED)
         {
            fsa[i].auto_toggle = ce[i].auto_toggle;
            ce[i].auto_toggle = -1;
            if (fsa[i].auto_toggle == ON)
            {
               fsa[i].host_toggle_str[0]  = AUTO_TOGGLE_OPEN;
               fsa[i].host_toggle_str[3]  = AUTO_TOGGLE_CLOSE;
            }
            else
            {
               fsa[i].host_toggle_str[0]  = STATIC_TOGGLE_OPEN;
               fsa[i].host_toggle_str[3]  = STATIC_TOGGLE_CLOSE;
            }
            changes++;
         }
         if (ce[i].value_changed & HOST_SWITCH_TOGGLE_CHANGED)
         {
            if (ce[i].host_switch_toggle == ON)
            {
               fsa[i].host_toggle         = DEFAULT_TOGGLE_HOST;
               fsa[i].original_toggle_pos = NONE;
               if (fsa[i].auto_toggle == ON)
               {
                  fsa[i].host_toggle_str[0]  = AUTO_TOGGLE_OPEN;
                  fsa[i].host_toggle_str[3]  = AUTO_TOGGLE_CLOSE;
               }
               else
               {
                  fsa[i].host_toggle_str[0]  = STATIC_TOGGLE_OPEN;
                  fsa[i].host_toggle_str[3]  = STATIC_TOGGLE_CLOSE;
               }
               fsa[i].host_toggle_str[HOST_ONE] = ce[i].host_toggle[0][0];
               fsa[i].host_toggle_str[HOST_TWO] = ce[i].host_toggle[1][0];
               fsa[i].host_toggle_str[4]  = '\0';
               if ((fsa[i].toggle_pos = strlen(fsa[i].host_alias)) >= MAX_HOSTNAME_LENGTH)
               {
                  fsa[i].toggle_pos = MAX_HOSTNAME_LENGTH;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
               fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
               if (strncmp(fsa[i].real_hostname[0], fsa[i].host_alias, fsa[i].toggle_pos) == 0)
               {
                  (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
               }
               if ((fsa[i].real_hostname[1][0] == '\0') ||
                   (strncmp(fsa[i].real_hostname[1], fsa[i].host_alias, fsa[i].toggle_pos) == 0))
               {
                  (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
                  if (fsa[i].host_toggle == HOST_ONE)
                  {
                     fsa[i].real_hostname[1][(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
                  }
                  else
                  {
                     fsa[i].real_hostname[1][(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
                  }
               }
            }
            else
            {
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = ' ';
               if (strncmp(fsa[i].real_hostname[0], fsa[i].host_alias, fsa[i].toggle_pos) == 0)
               {
                  fsa[i].real_hostname[0][(int)fsa[i].toggle_pos] = '\0';
               }
               fsa[i].real_hostname[1][0] = '\0';
               fsa[i].host_toggle_str[0] = '\0';
               fsa[i].host_toggle = HOST_ONE;
               fsa[i].auto_toggle = OFF;
            }
            changes++;
         }
         if (ce[i].value_changed & TRANSFER_TIMEOUT_CHANGED)
         {
            fsa[i].transfer_timeout = ce[i].transfer_timeout;
            ce[i].transfer_timeout = -1L;
            changes++;
         }
         if (ce[i].value_changed & RETRY_INTERVAL_CHANGED)
         {
            fsa[i].retry_interval = ce[i].retry_interval;
            ce[i].retry_interval = -1;
            changes++;
         }
         if (ce[i].value_changed & MAX_ERRORS_CHANGED)
         {
            fsa[i].max_errors = ce[i].max_errors;
            ce[i].max_errors = -1;
            changes++;
         }
         if (ce[i].value_changed & SUCCESSFUL_RETRIES_CHANGED)
         {
            fsa[i].max_successful_retries = ce[i].max_successful_retries;
            ce[i].max_successful_retries = -1;
            changes++;
         }
         if (ce[i].value_changed & KEEP_CONNECTED_CHANGED)
         {
            fsa[i].keep_connected = ce[i].keep_connected;
            ce[i].keep_connected = 0;
            changes++;
         }
         if (ce[i].value_changed & TRANSFER_RATE_LIMIT_CHANGED)
         {
            fsa[i].transfer_rate_limit = ce[i].transfer_rate_limit;
            ce[i].transfer_rate_limit = -1;
            changes++;
         }
         if (ce[i].value_changed & SOCKET_SEND_BUFFER_CHANGED)
         {
            fsa[i].socksnd_bufsize = ce[i].sndbuf_size;
            ce[i].sndbuf_size = 0;
            changes++;
         }
         if (ce[i].value_changed & SOCKET_RECEIVE_BUFFER_CHANGED)
         {
            fsa[i].sockrcv_bufsize = ce[i].rcvbuf_size;
            ce[i].rcvbuf_size = 0;
            changes++;
         }

#ifdef WITH_DUP_CHECK
         if (ce[i].value_changed & DC_TYPE_CHANGED)
         {
            if (ce[i].dup_check_flag & DC_FILE_CONTENT)
            {
               if ((fsa[i].dup_check_flag & DC_FILE_CONTENT) == 0)
               {
                  fsa[i].dup_check_flag |= DC_FILE_CONTENT;
               }
               if (fsa[i].dup_check_flag & DC_FILENAME_ONLY)
               {
                  fsa[i].dup_check_flag ^= DC_FILENAME_ONLY;
               }
               if (fsa[i].dup_check_flag & DC_NAME_NO_SUFFIX)
               {
                  fsa[i].dup_check_flag ^= DC_NAME_NO_SUFFIX;
               }
               if (fsa[i].dup_check_flag & DC_FILE_CONT_NAME)
               {
                  fsa[i].dup_check_flag ^= DC_FILE_CONT_NAME;
               }
            }
            else if (ce[i].dup_check_flag & DC_FILE_CONT_NAME)
                 {
                    if ((fsa[i].dup_check_flag & DC_FILE_CONT_NAME) == 0)
                    {
                       fsa[i].dup_check_flag |= DC_FILE_CONT_NAME;
                    }
                    if (fsa[i].dup_check_flag & DC_FILENAME_ONLY)
                    {
                       fsa[i].dup_check_flag ^= DC_FILENAME_ONLY;
                    }
                    if (fsa[i].dup_check_flag & DC_NAME_NO_SUFFIX)
                    {
                       fsa[i].dup_check_flag ^= DC_NAME_NO_SUFFIX;
                    }
                    if (fsa[i].dup_check_flag & DC_FILE_CONTENT)
                    {
                       fsa[i].dup_check_flag ^= DC_FILE_CONTENT;
                    }
                 }
            else if (ce[i].dup_check_flag & DC_NAME_NO_SUFFIX)
                 {
                    if ((fsa[i].dup_check_flag & DC_NAME_NO_SUFFIX) == 0)
                    {
                       fsa[i].dup_check_flag |= DC_NAME_NO_SUFFIX;
                    }
                    if (fsa[i].dup_check_flag & DC_FILENAME_ONLY)
                    {
                       fsa[i].dup_check_flag ^= DC_FILENAME_ONLY;
                    }
                    if (fsa[i].dup_check_flag & DC_FILE_CONTENT)
                    {
                       fsa[i].dup_check_flag ^= DC_FILE_CONTENT;
                    }
                    if (fsa[i].dup_check_flag & DC_FILE_CONT_NAME)
                    {
                       fsa[i].dup_check_flag ^= DC_FILE_CONT_NAME;
                    }
                 }
                 else
                 {
                    if ((fsa[i].dup_check_flag & DC_FILENAME_ONLY) == 0)
                    {
                       fsa[i].dup_check_flag |= DC_FILENAME_ONLY;
                    }
                    if (fsa[i].dup_check_flag & DC_NAME_NO_SUFFIX)
                    {
                       fsa[i].dup_check_flag ^= DC_NAME_NO_SUFFIX;
                    }
                    if (fsa[i].dup_check_flag & DC_FILE_CONTENT)
                    {
                       fsa[i].dup_check_flag ^= DC_FILE_CONTENT;
                    }
                    if (fsa[i].dup_check_flag & DC_FILE_CONT_NAME)
                    {
                       fsa[i].dup_check_flag ^= DC_FILE_CONT_NAME;
                    }
                 }
            changes++;
         }
         if (ce[i].value_changed & DC_DELETE_CHANGED)
         {
            if (ce[i].dup_check_flag & DC_DELETE)
            {
               if ((fsa[i].dup_check_flag & DC_DELETE) == 0)
               {
                  fsa[i].dup_check_flag |= DC_DELETE;
               }
               if (fsa[i].dup_check_flag & DC_STORE)
               {
                  fsa[i].dup_check_flag ^= DC_STORE;
               }
            }
            else
            {
               if (fsa[i].dup_check_flag & DC_DELETE)
               {
                  fsa[i].dup_check_flag ^= DC_DELETE;
               }
            }
            changes++;
         }
         if (ce[i].value_changed & DC_STORE_CHANGED)
         {
            if (ce[i].dup_check_flag & DC_STORE)
            {
               if ((fsa[i].dup_check_flag & DC_STORE) == 0)
               {
                  fsa[i].dup_check_flag |= DC_STORE;
               }
               if (fsa[i].dup_check_flag & DC_DELETE)
               {
                  fsa[i].dup_check_flag ^= DC_DELETE;
               }
            }
            else
            {
               if (fsa[i].dup_check_flag & DC_STORE)
               {
                  fsa[i].dup_check_flag ^= DC_STORE;
               }
            }
            changes++;
         }
         if (ce[i].value_changed & DC_WARN_CHANGED)
         {
            if (ce[i].dup_check_flag & DC_WARN)
            {
               if ((fsa[i].dup_check_flag & DC_WARN) == 0)
               {
                  fsa[i].dup_check_flag |= DC_WARN;
               }
            }
            else
            {
               if (fsa[i].dup_check_flag & DC_WARN)
               {
                  fsa[i].dup_check_flag ^= DC_WARN;
               }
            }
            changes++;
         }
         if ((ce[i].value_changed & DC_TYPE_CHANGED) ||
             (ce[i].value_changed & DC_DELETE_CHANGED) ||
             (ce[i].value_changed & DC_STORE_CHANGED) ||
             (ce[i].value_changed & DC_WARN_CHANGED))
         {
            ce[i].dup_check_flag = 0;
         }
         if (ce[i].value_changed & DC_TIMEOUT_CHANGED)
         {
            fsa[i].dup_check_timeout = ce[i].dup_check_timeout;
            ce[i].dup_check_timeout = 0;
            changes++;
         }
         if (fsa[i].dup_check_timeout > 0L)
         {
            if ((fsa[i].dup_check_flag & DC_CRC32) == 0)
            {
               fsa[i].dup_check_flag |= DC_CRC32;
            }
         }
#endif /* WITH_DUP_CHECK */

         if (ce[i].value_changed & ALLOWED_TRANSFERS_CHANGED)
         {
            /*
             * NOTE: When we increase the number of parallel transfers
             *       we have to initialize the values for job_status.
             *       But NOT when we decrease the number. It could be
             *       that a job is still transmitting data, which will
             *       overwrite the data we just have send or we overwrite
             *       its data.
             */
            if (ce[i].allowed_transfers > fsa[i].allowed_transfers)
            {
               int j;

               for (j = fsa[i].allowed_transfers; j < ce[i].allowed_transfers; j++)
               {
                  if (fsa[i].job_status[j].connect_status == 0)
                  {
                     fsa[i].job_status[j].connect_status = DISCONNECT;
                     fsa[i].job_status[j].job_id = NO_ID;
                  }
               }
            }
            fsa[i].allowed_transfers = ce[i].allowed_transfers;
            ce[i].allowed_transfers = -1;
            changes++;
         }
         if (ce[i].value_changed & BLOCK_SIZE_CHANGED)
         {
            fsa[i].block_size = ce[i].block_size;
            ce[i].block_size = -1;
            changes++;
         }
         if (ce[i].value_changed & FILE_SIZE_OFFSET_CHANGED)
         {
            fsa[i].file_size_offset = ce[i].file_size_offset;
            ce[i].file_size_offset = -3;
            changes++;
         }
         if (ce[i].value_changed & NO_OF_NO_BURST_CHANGED)
         {
            fsa[i].special_flag = (fsa[i].special_flag & (~NO_BURST_COUNT_MASK)) | ce[i].no_of_no_bursts;
            changes++;
         }
         if (ce[i].value_changed & FTP_MODE_CHANGED)
         {
            if (((fsa[i].protocol_options & FTP_PASSIVE_MODE) &&
                 (ce[i].ftp_mode == FTP_ACTIVE_MODE_SEL)) ||
                (((fsa[i].protocol_options & FTP_PASSIVE_MODE) == 0) &&
                 (ce[i].ftp_mode == FTP_PASSIVE_MODE_SEL)))
            {
               fsa[i].protocol_options ^= FTP_PASSIVE_MODE;
               changes++;
            }
         }
         if (ce[i].value_changed & FTP_SET_IDLE_TIME_CHANGED)
         {
            fsa[i].protocol_options ^= SET_IDLE_TIME;
            changes++;
         }
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (ce[i].value_changed & FTP_KEEPALIVE_CHANGED)
         {
            fsa[i].protocol_options ^= STAT_KEEPALIVE;
            changes++;
         }
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
         if (ce[i].value_changed & FTP_FAST_RENAME_CHANGED)
         {
            fsa[i].protocol_options ^= FTP_FAST_MOVE;
            changes++;
         }
         if (ce[i].value_changed & FTP_FAST_CD_CHANGED)
         {
            fsa[i].protocol_options ^= FTP_FAST_CD;
            changes++;
         }
         if (ce[i].value_changed & FTP_IGNORE_BIN_CHANGED)
         {
            fsa[i].protocol_options ^= FTP_IGNORE_BIN;
            changes++;
         }
         if (ce[i].value_changed & FTP_EXTENDED_MODE_CHANGED)
         {
            fsa[i].protocol_options ^= FTP_EXTENDED_MODE;
            changes++;
         }
#ifdef _WITH_BURST_2
         if (ce[i].value_changed2 & ALLOW_BURST_CHANGED)
         {
            fsa[i].protocol_options ^= DISABLE_BURSTING;
            changes++;
         }
#endif
         if (ce[i].value_changed2 & FTP_PASSIVE_REDIRECT_CHANGED)
         {
            fsa[i].protocol_options ^= FTP_ALLOW_DATA_REDIRECT;
            changes++;
         }

         ce[i].value_changed = 0;
         ce[i].value_changed2 = 0;

         if (prev_changes != changes)
         {
            (void)strcpy(host_list[changed_hosts], fsa[i].host_dsp_name);
            changed_hosts++;
         }
      } /* if ((ce[i].value_changed != 0) || (ce[i].value_changed2 != 0)) */
   } /* for (i = 0; i < no_of_hosts; i++) */

   /*
    * NOTE: Change order as the last point, otherwise we might not
    *       know where the change has occured.
    */
   if (host_alias_order_change == YES)
   {
      size_t        new_size = no_of_hosts * sizeof(char *);
      char          **p_host_names = NULL;
      XmStringTable item_list;

      XtVaGetValues(host_list_w,
                    XmNitems, &item_list,
                    NULL);
      if ((p_host_names = malloc(new_size)) == NULL)
      {
         (void)xrec(w, FATAL_DIALOG, "malloc() error [%d Bytes] : %s (%s %d)",
                    new_size, strerror(errno), __FILE__, __LINE__);
         if (host_list != NULL)
         {
            FREE_RT_ARRAY(host_list);
         }
         return;
      }
      for (i = 0; i < no_of_hosts; i++)
      {
         XmStringGetLtoR(item_list[i], XmFONTLIST_DEFAULT_TAG, &p_host_names[i]);
      }
      if ((p_afd_status->amg_jobs & REREADING_DIR_CONFIG) == 0)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      inform_fd_about_fsa_change();
      change_alias_order(p_host_names, -1);
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      for (i = 0; i < no_of_hosts; i++)
      {
         XtFree(p_host_names[i]);
      }
      free(p_host_names);

      if (changes > 1)
      {
         (void)sprintf(msg, "Changed alias order and submitted %d changes to FSA",
                       changes);
      }
      else if (changes == 1)
           {
              (void)sprintf(msg, "Changed alias order and submitted one change to FSA");
           }
           else
           {
              char user[MAX_FULL_USER_ID_LENGTH];

              (void)sprintf(msg, "Changed alias order in FSA");
              get_user(user, fake_user);
              system_log(CONFIG_SIGN, NULL, 0, "%s (%s)", msg, user);
           }
   }
   else
   {
      if (changes == 1)
      {
         (void)sprintf(msg, "Submitted one change to FSA");
      }
      else if (changes > 1)
           {
              (void)sprintf(msg, "Submitted %d changes to FSA", changes);
           }
           else
           {
              (void)sprintf(msg, "No values have been changed!");
           }
   }
   show_message(statusbox_w, msg);
   if (changes != 0)
   {
      int  line_length;
      char user[MAX_FULL_USER_ID_LENGTH];

      get_user(user, fake_user);
      system_log(CONFIG_SIGN, NULL, 0, "%s (%s)", msg, user);

      /*
       * Show the hosts that where changed. But ensure that the line
       * does not get longer then MAX_CHARS_IN_LINE.
       */
      i = 0;
      line_length = sprintf(msg, "Hosts changed: ");
      do
      {
         if (msg[0] == '\0')
         {
            line_length = sprintf(msg, "               ");;
         }
         do
         {
            line_length += sprintf(&msg[line_length], "%s ",
                                   host_list[i]);
            i++;
         } while ((line_length <= MAX_CHARS_IN_LINE) && (changed_hosts > i));

         msg[line_length] = '\0';
         system_log(INFO_SIGN, NULL, 0, "%s", msg);
         msg[0] = '\0';
      } while (changed_hosts > i);
   }

   if ((host_alias_order_change == YES) || (changes > 0))
   {
      int  ret,
#ifdef WITHOUT_FIFO_RW_SUPPORT
           db_update_readfd,
#endif
           db_update_fd;
      char db_update_fifo[MAX_PATH_LENGTH];

      host_alias_order_change = NO;
      (void)strcpy(db_update_fifo, p_work_dir);
      (void)strcat(db_update_fifo, FIFO_DIR);
      (void)strcat(db_update_fifo, DB_UPDATE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(db_update_fifo, &db_update_readfd, &db_update_fd) == -1)
#else
      if ((db_update_fd = open(db_update_fifo, O_RDWR)) == -1)
#endif
      {
         (void)xrec(w, WARN_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    db_update_fifo, strerror(errno),
                    __FILE__, __LINE__);
      }
      else
      {
         if ((ret = send_cmd(HOST_CONFIG_UPDATE, db_update_fd)) != SUCCESS)
         {
            (void)xrec(w, ERROR_DIALOG,
                       "Failed to send update message to AMG : %s (%s %d)",
                       strerror(-ret), __FILE__, __LINE__);
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(db_update_readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
#endif
         if (close(db_update_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   }

   if (host_list != NULL)
   {
      FREE_RT_ARRAY(host_list);
   }

   return;
}


/*######################### tb_option_changed() #########################*/
void
tb_option_changed(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (tb.value[item_no] != fsa[cur_pos].block_size)
   {
      ce[cur_pos].value_changed |= BLOCK_SIZE_CHANGED;
      ce[cur_pos].block_size = tb.value[item_no];
   }

   return;
}


/*############################ leave_notify() ###########################*/
void
leave_notify(Widget w, XtPointer client_data, XtPointer call_data)
{
   if ((in_drop_site != -2) && (in_drop_site == YES))
   {
      in_drop_site = NO;
      XtVaSetValues(start_drag_w,
                    XmNsourceCursorIcon, no_source_icon_w,
                    NULL);
   }

   return;
}


/*############################ enter_notify() ###########################*/
void
enter_notify(Widget w, XtPointer client_data, XtPointer call_data)
{
   if ((in_drop_site != -2) && (in_drop_site == NO))
   {
      in_drop_site = YES;
      XtVaSetValues(start_drag_w,
                    XmNsourceCursorIcon, source_icon_w,
                    NULL);
   }

   return;
}