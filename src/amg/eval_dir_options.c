/*
 *  eval_dir_options.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 2000 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_dir_options - evaluates the directory options
 **
 ** SYNOPSIS
 **   void eval_dir_options(int  dir_pos,
 **                         char *dir_options,
 **                         char *old_dir_options)
 **
 ** DESCRIPTION
 **   Reads and evaluates the directory options from one directory
 **   of the DIR_CONFIG file. It currently knows the following options:
 **
 **        delete unknown files [<value in hours>]
 **        delete queued files [<value in hours>]
 **        delete old locked files <value in hours>
 **        do not delete unknown files           [DEFAULT]
 **        report unknown files                  [DEFAULT]
 **        do not report unknown files
 **        old file time <value in hours>        [DEFAULT 24]
 **        end character <decimal number>
 **        ignore size [=|>|<] <decimal number>
 **        ignore file time [=|>|<] <decimal number>
 **        important dir
 **        time * * * * *
 **        keep connected <value in seconds>
 **        do not get dir list
 **        do not remove
 **        store retrieve list [once]
 **        priority <value>                      [DEFAULT 9]
 **        force rereads
 **        max process <value>                   [DEFAULT 10]
 **        max files <value>                     [DEFAULT ?]
 **        max size <value>                      [DEFAULT ?]
 **        wait for <file name|pattern>
 **        warn time <value in seconds>
 **        accumulate <value>
 **        accumulate size <value>
 **        dupcheck[ <timeout in secs>[ <check type>[ <action>[ <CRC type>]]]]
 **        accept dot files
 **        inotify <value>                       [DEFAULT 0]
 **
 **   For the string old_dir_options it is possible to define the
 **   following values:
 **        <hours> <DIRS*>
 **                 |||||
 **                 ||||+----> important directory
 **                 |||+-----> do not report
 **                 ||+------> report
 **                 |+-------> do not delete
 **                 +--------> delete
 **
 ** RETURN VALUES
 **   None, will just enter the values found into the structure
 **   dir_data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.03.2000 H.Kiehl Created
 **   12.08.2000 H.Kiehl Addition of priority.
 **   31.08.2000 H.Kiehl Addition of option "force rereads".
 **   20.07.2001 H.Kiehl New option "delete queued files".
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   14.08.2002 H.Kiehl Added "ignore size" option.
 **   06.02.2003 H.Kiehl Added "max files" and "max size" option.
 **   16.08.2003 H.Kiehl Added "wait for", accumulate and "accumulate size"
 **                      options.
 **   02.09.2004 H.Kiehl Added "ignore file time" option.
 **   28.11.2004 H.Kiehl Added "delete old locked files" option.
 **   07.06.2005 H.Kiehl Added "dupcheck" option.
 **   30.06.2006 H.Kiehl Added "accept dot files" option.
 **   19.08.2006 H.Kiehl Added "do not get dir list" option.
 **   10.11.2006 H.Kiehl Added "warn time" option.
 **   13.11.2006 H.Kiehl Added "keep connected" option.
 **   24.02.2007 H.Kiehl Added inotify support.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* stderr, fprintf()                   */
#include <stdlib.h>               /* atoi(), malloc(), free(), strtoul() */
#include <string.h>               /* strcmp(), strncmp(), strerror()     */
#include <ctype.h>                /* isdigit()                           */
#include <sys/types.h>
#include <sys/stat.h>             /* fstat()                             */
#include <unistd.h>               /* read(), close(), setuid()           */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>               /* O_RDONLY, etc                       */
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int             default_delete_files_flag,
#ifdef WITH_INOTIFY
                       default_inotify_flag,
#endif
                       default_old_file_time,
                       max_process_per_dir;
extern unsigned int    max_copied_files;
extern time_t          default_warn_time;
extern off_t           max_copied_file_size;
extern struct dir_data *dd;

#define DEL_UNKNOWN_FILES_FLAG           1
#define OLD_FILE_TIME_FLAG               2
#define DONT_REP_UNKNOWN_FILES_FLAG      4
#define DIRECTORY_PRIORITY_FLAG          8
#define END_CHARACTER_FLAG               16
#define TIME_FLAG                        32
#define MAX_PROCESS_FLAG                 64
#define DO_NOT_REMOVE_FLAG               128
#define STORE_RETRIEVE_LIST_FLAG         256
#define DEL_QUEUED_FILES_FLAG            512
#define DONT_DEL_UNKNOWN_FILES_FLAG      1024
#define REP_UNKNOWN_FILES_FLAG           2048
#define FORCE_REREAD_FLAG                4096
#define IMPORTANT_DIR_FLAG               8192
#define IGNORE_SIZE_FLAG                 16384
#define MAX_FILES_FLAG                   32768
#define MAX_SIZE_FLAG                    65536
#define WAIT_FOR_FILENAME_FLAG           131072
#define ACCUMULATE_FLAG                  262144
#define ACCUMULATE_SIZE_FLAG             524288
#define IGNORE_FILE_TIME_FLAG            1048576
#define DEL_OLD_LOCKED_FILES_FLAG        2097152
#ifdef WITH_DUP_CHECK
# define DUPCHECK_FLAG                   4194304
#endif
#define ACCEPT_DOT_FILES_FLAG            8388608
#define DO_NOT_GET_DIR_LIST_FLAG         16777216
#define DIR_WARN_TIME_FLAG               33554432
#define KEEP_CONNECTED_FLAG              67108864
#ifdef WITH_INOTIFY
# define INOTIFY_FLAG                    134217728
#endif


/*########################## eval_dir_options() #########################*/
void
eval_dir_options(int  dir_pos,
                 char *dir_options,
                 char *old_dir_options)
{
   int  old_file_time,
        used = 0;          /* Used to see whether option has */
                           /* already been set.              */
   char *ptr,
        *end_ptr = NULL,
        byte_buf;

   /* Set some default directory options. */
   if (default_old_file_time == -1)
   {
      old_file_time = DEFAULT_OLD_FILE_TIME * 3600;
   }
   else
   {
      old_file_time = default_old_file_time * 3600;
   }
   dd[dir_pos].delete_files_flag = default_delete_files_flag;
   dd[dir_pos].unknown_file_time = -1;
   dd[dir_pos].queued_file_time = -1;
   dd[dir_pos].locked_file_time = -1;
   dd[dir_pos].report_unknown_files = YES;
   dd[dir_pos].end_character = -1;
#ifndef _WITH_PTHREAD
   dd[dir_pos].important_dir = NO;
#endif
   dd[dir_pos].time_option = NO;
   dd[dir_pos].max_process = max_process_per_dir;
   dd[dir_pos].remove = YES;
   dd[dir_pos].stupid_mode = YES;
   dd[dir_pos].priority = DEFAULT_PRIORITY;
   dd[dir_pos].force_reread = NO;
   dd[dir_pos].gt_lt_sign = 0;
   dd[dir_pos].ignore_size = 0;
   dd[dir_pos].ignore_file_time = 0;
   dd[dir_pos].max_copied_files = max_copied_files;
   dd[dir_pos].max_copied_file_size = max_copied_file_size;
   dd[dir_pos].wait_for_filename[0] = '\0';
   dd[dir_pos].accumulate = 0;
   dd[dir_pos].accumulate_size = 0;
#ifdef WITH_DUP_CHECK
   dd[dir_pos].dup_check_flag = 0;
   dd[dir_pos].dup_check_timeout = 0L;
#endif
   dd[dir_pos].accept_dot_files = NO;
   dd[dir_pos].do_not_get_dir_list = NO;
   dd[dir_pos].max_errors = 10;
   dd[dir_pos].warn_time = default_warn_time;
   dd[dir_pos].keep_connected = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_INOTIFY
   dd[dir_pos].inotify_flag = default_inotify_flag;
#endif

   /*
    * First evaluate the old directory option so we
    * so we can later override them with the new options.
    */
   if ((*old_dir_options != '\n') && (*old_dir_options != '\0'))
   {
      int  length = 0;
      char number[MAX_INT_LENGTH + 1];

      ptr = old_dir_options;
      while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
      {
         number[length] = *ptr;
         ptr++; length++;
      }
      if ((length > 0) && (length != MAX_INT_LENGTH))
      {
         number[length] = '\0';
         old_file_time = atoi(number) * 3600;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         end_ptr = ptr;
      }
      else
      {
         end_ptr = ptr;
      }
      while ((*end_ptr != '\n') && (*end_ptr != '\0'))
      {
        switch (*end_ptr)
        {
           case 'd' :
           case 'D' : /* Delete unknown files. */
              if ((dd[dir_pos].delete_files_flag & UNKNOWN_FILES) == 0)
              {
                 dd[dir_pos].delete_files_flag |= UNKNOWN_FILES;
                 dd[dir_pos].in_dc_flag |= UNKNOWN_FILES_IDC;
              }
              break;

           case 'i' :
           case 'I' : /* Do NOT delete unknown files. */
              dd[dir_pos].delete_files_flag = 0;
              break;

           case 'r' :
           case 'R' : /* Report unknown files. */
              dd[dir_pos].report_unknown_files = YES;
              dd[dir_pos].in_dc_flag |= REPUKW_FILES_IDC;
              break;

           case 's' :
           case 'S' : /* Do NOT report unknown files. */
              dd[dir_pos].report_unknown_files = NO;
              dd[dir_pos].in_dc_flag |= DONT_REPUKW_FILES_IDC;
              break;

           case 'E' : /* Check end character of file. */
              if ((*(end_ptr + 1) == 'C') && (*(end_ptr + 2) == '='))
              {
                 end_ptr += 3;
                 length = 0;
                 while ((isdigit((int)(*end_ptr))) &&
                        (length < MAX_INT_LENGTH))
                 {
                    number[length] = *end_ptr;
                    end_ptr++; length++;
                 }
                 if (length != 0)
                 {
                    number[length] = '\0';
                    dd[dir_pos].end_character = atoi(number);
                 }
              }
              break;

#ifndef _WITH_PTHREAD
           case '*' : /* This is an important directory! */
              dd[dir_pos].important_dir = YES;
              break;
#endif

           case '\t':
           case ' ' : /* Ignore any spaces. */
              break;

           default : /* Give a warning about an unknown */
                     /* character option.               */
             system_log(WARN_SIGN, __FILE__, __LINE__,
                        "Unknown option character %c <%d> for directory option.",
                        *end_ptr, (int)*end_ptr);
             break;
        }
        end_ptr++;
      }
   }

   /*
    * Now for the new directory options.
    */
   ptr = dir_options;
   while (*ptr != '\0')
   {
      if (((used & DEL_UNKNOWN_FILES_FLAG) == 0) &&
          (strncmp(ptr, DEL_UNKNOWN_FILES_ID, DEL_UNKNOWN_FILES_ID_LENGTH) == 0))
      {
         used |= DEL_UNKNOWN_FILES_FLAG;
         ptr += DEL_UNKNOWN_FILES_ID_LENGTH;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            int  length = 0;
            char number[MAX_INT_LENGTH + 1];

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
            {
               number[length] = *ptr;
               ptr++; length++;
            }
            if ((length > 0) && (length != MAX_INT_LENGTH))
            {
               number[length] = '\0';
               dd[dir_pos].unknown_file_time = atoi(number) * 3600;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
            }
         }
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if ((dd[dir_pos].delete_files_flag & UNKNOWN_FILES) == 0)
         {
            dd[dir_pos].delete_files_flag |= UNKNOWN_FILES;
            dd[dir_pos].in_dc_flag |= UNKNOWN_FILES_IDC;
         }
      }
#ifdef WITH_INOTIFY
      else if (((used & INOTIFY_FLAG) == 0) &&
               (strncmp(ptr, INOTIFY_FLAG_ID, INOTIFY_FLAG_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= INOTIFY_FLAG;
              ptr += INOTIFY_FLAG_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].inotify_flag = (unsigned int)atoi(number);
                 if (dd[dir_pos].inotify_flag > (INOTIFY_RENAME_FLAG | INOTIFY_CLOSE_FLAG))
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                              "Incorrect parameter %u for directory option `%s' for the %d directory entry. Resetting to %u.",
                              dd[dir_pos].inotify_flag, INOTIFY_FLAG_ID,
                              dir_pos, default_inotify_flag);
                    dd[dir_pos].inotify_flag = default_inotify_flag;
                 }
                 else
                 {
                    dd[dir_pos].in_dc_flag |= INOTIFY_FLAG_IDC;
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
#endif
      else if (((used & OLD_FILE_TIME_FLAG) == 0) &&
               (strncmp(ptr, OLD_FILE_TIME_ID, OLD_FILE_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= OLD_FILE_TIME_FLAG;
              ptr += OLD_FILE_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 old_file_time = atoi(number) * 3600;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DIRECTORY_PRIORITY_FLAG) == 0) &&
               (strncmp(ptr, PRIORITY_ID, PRIORITY_ID_LENGTH) == 0))
           {
              used |= DIRECTORY_PRIORITY_FLAG;
              ptr += PRIORITY_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\n'))
              {
                 ptr++;
              }
              if (isdigit((int)(*ptr)))
              {
                 dd[dir_pos].priority = *ptr;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DONT_REP_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, DONT_REP_UNKNOWN_FILES_ID, DONT_REP_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= DONT_REP_UNKNOWN_FILES_FLAG;
              ptr += DONT_REP_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].report_unknown_files = NO;
              dd[dir_pos].in_dc_flag |= DONT_REPUKW_FILES_IDC;
           }
      else if (((used & END_CHARACTER_FLAG) == 0) &&
               (strncmp(ptr, END_CHARACTER_ID, END_CHARACTER_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= END_CHARACTER_FLAG;
              ptr += END_CHARACTER_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].end_character = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_PROCESS_FLAG) == 0) &&
               (strncmp(ptr, MAX_PROCESS_ID, MAX_PROCESS_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_PROCESS_FLAG;
              ptr += MAX_PROCESS_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_process = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & TIME_FLAG) == 0) &&
               (strncmp(ptr, TIME_ID, TIME_ID_LENGTH) == 0))
           {
              char tmp_char;

              used |= TIME_FLAG;
              ptr += TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              end_ptr = ptr;
              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
              {
                 end_ptr++;
              }
              tmp_char = *end_ptr;
              *end_ptr = '\0';
              if (eval_time_str(ptr, &dd[dir_pos].te) == SUCCESS)
              {
                 dd[dir_pos].time_option = YES;
              }
              else
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Invalid %s string <%s>", TIME_ID, ptr);
              }
              *end_ptr = tmp_char;
              ptr = end_ptr;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DO_NOT_REMOVE_FLAG) == 0) &&
               (strncmp(ptr, DO_NOT_REMOVE_ID, DO_NOT_REMOVE_ID_LENGTH) == 0))
           {
              used |= DO_NOT_REMOVE_FLAG;
              ptr += DO_NOT_REMOVE_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].remove = NO;
           }
      else if (((used & STORE_RETRIEVE_LIST_FLAG) == 0) &&
               (strncmp(ptr, STORE_RETRIEVE_LIST_ID, STORE_RETRIEVE_LIST_ID_LENGTH) == 0))
           {
              used |= STORE_RETRIEVE_LIST_FLAG;
              ptr += STORE_RETRIEVE_LIST_ID_LENGTH;
              while (*ptr == ' ')
              {
                 ptr++;
              }
              if ((*ptr == 'o') && (*(ptr + 1) == 'n') &&
                  (*(ptr + 2) == 'c') && (*(ptr + 3) == 'e') &&
                  ((*(ptr + 4) == '\n') || (*(ptr + 4) == '\0')))
              {
                 dd[dir_pos].stupid_mode = GET_ONCE_ONLY;
                 ptr += 4;
              }
              else
              {
                 dd[dir_pos].stupid_mode = NO;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & STORE_RETRIEVE_LIST_FLAG) == 0) &&
               (strncmp(ptr, STORE_REMOTE_LIST, STORE_REMOTE_LIST_LENGTH) == 0))
           {
              used |= STORE_RETRIEVE_LIST_FLAG;
              ptr += STORE_REMOTE_LIST_LENGTH;
              while (*ptr == ' ')
              {
                 ptr++;
              }
              if ((*ptr == 'o') && (*(ptr + 1) == 'n') &&
                  (*(ptr + 2) == 'c') && (*(ptr + 3) == 'e') &&
                  ((*(ptr + 4) == '\n') || (*(ptr + 4) == '\0')))
              {
                 dd[dir_pos].stupid_mode = GET_ONCE_ONLY;
                 ptr += 4;
              }
              else
              {
                 dd[dir_pos].stupid_mode = NO;
              }
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "The directory option 'store remote list' is depreciated! Please use 'store retrieve list' instead.");
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DEL_QUEUED_FILES_FLAG) == 0) &&
               (strncmp(ptr, DEL_QUEUED_FILES_ID, DEL_QUEUED_FILES_ID_LENGTH) == 0))
           {
              used |= DEL_QUEUED_FILES_FLAG;
              ptr += DEL_QUEUED_FILES_ID_LENGTH;
              if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 int  length = 0;
                 char number[MAX_INT_LENGTH + 1];

                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
                 {
                    number[length] = *ptr;
                    ptr++; length++;
                 }
                 if ((length > 0) && (length != MAX_INT_LENGTH))
                 {
                    number[length] = '\0';
                    dd[dir_pos].queued_file_time = atoi(number) * 3600;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              if ((dd[dir_pos].delete_files_flag & QUEUED_FILES) == 0)
              {
                 dd[dir_pos].delete_files_flag |= QUEUED_FILES;
                 dd[dir_pos].in_dc_flag |= QUEUED_FILES_IDC;
              }
           }
      else if (((used & DEL_OLD_LOCKED_FILES_FLAG) == 0) &&
               (strncmp(ptr, DEL_OLD_LOCKED_FILES_ID, DEL_OLD_LOCKED_FILES_ID_LENGTH) == 0))
           {
              used |= DEL_OLD_LOCKED_FILES_FLAG;
              ptr += DEL_OLD_LOCKED_FILES_ID_LENGTH;
              if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 int  length = 0;
                 char number[MAX_INT_LENGTH + 1];

                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
                 {
                    number[length] = *ptr;
                    ptr++; length++;
                 }
                 if ((length > 0) && (length != MAX_INT_LENGTH))
                 {
                    number[length] = '\0';
                    dd[dir_pos].locked_file_time = atoi(number) * 3600;
                 }
                 if ((dd[dir_pos].delete_files_flag & OLD_LOCKED_FILES) == 0)
                 {
                    dd[dir_pos].delete_files_flag |= OLD_LOCKED_FILES;
                    dd[dir_pos].in_dc_flag |= OLD_LOCKED_FILES_IDC;
                 }
              }
              else
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                           "No time given for option `%s' for the %d directory entry.",
                           DEL_OLD_LOCKED_FILES_ID, dir_pos);
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DONT_DEL_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, DONT_DEL_UNKNOWN_FILES_ID, DONT_DEL_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= DONT_DEL_UNKNOWN_FILES_FLAG;
              ptr += DONT_DEL_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              /*
               * This is dafault, actually this option is not needed.
               */
           }
      else if (((used & REP_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, REP_UNKNOWN_FILES_ID, REP_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= REP_UNKNOWN_FILES_FLAG;
              ptr += REP_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].report_unknown_files = YES;
              dd[dir_pos].in_dc_flag |= REPUKW_FILES_IDC;
           }
#ifdef WITH_DUP_CHECK
      else if (((used & DUPCHECK_FLAG) == 0) &&
               (strncmp(ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0))
           {
              used |= DUPCHECK_FLAG;
              ptr = eval_dupcheck_options(ptr, &dd[dir_pos].dup_check_timeout,
                                          &dd[dir_pos].dup_check_flag);

           }
#endif /* WITH_DUP_CHECK */
      else if (((used & ACCEPT_DOT_FILES_FLAG) == 0) &&
               (strncmp(ptr, ACCEPT_DOT_FILES_ID, ACCEPT_DOT_FILES_ID_LENGTH) == 0))
           {
              used |= ACCEPT_DOT_FILES_FLAG;
              ptr += ACCEPT_DOT_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].accept_dot_files = YES;
           }
      else if (((used & DO_NOT_GET_DIR_LIST_FLAG) == 0) &&
               (strncmp(ptr, DO_NOT_GET_DIR_LIST_ID, DO_NOT_GET_DIR_LIST_ID_LENGTH) == 0))
           {
              used |= DO_NOT_GET_DIR_LIST_FLAG;
              ptr += DO_NOT_GET_DIR_LIST_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].do_not_get_dir_list = YES;
           }
      else if (((used & DIR_WARN_TIME_FLAG) == 0) &&
               (strncmp(ptr, DIR_WARN_TIME_ID, DIR_WARN_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_LONG_LENGTH + 1];

              used |= DIR_WARN_TIME_FLAG;
              ptr += DIR_WARN_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_LONG_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_LONG_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].warn_time = (time_t)atol(number);
                 if (dd[dir_pos].warn_time < 0)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_TIME_T == 4
                               "A value less then 0 for directory option `%s' is no possible, setting default %ld.",
#else
                               "A value less then 0 for directory option `%s' is no possible, setting default %lld.",
#endif
                               DIR_WARN_TIME_ID, (pri_time_t)default_warn_time);
                    dd[dir_pos].warn_time = default_warn_time;
                 }
                 else
                 {
                    dd[dir_pos].in_dc_flag |= WARN_TIME_IDC;
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & KEEP_CONNECTED_FLAG) == 0) &&
               (strncmp(ptr, KEEP_CONNECTED_ID, KEEP_CONNECTED_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= KEEP_CONNECTED_FLAG;
              ptr += KEEP_CONNECTED_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].keep_connected = (unsigned int)atoi(number);
                 dd[dir_pos].in_dc_flag |= KEEP_CONNECTED_IDC;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & WAIT_FOR_FILENAME_FLAG) == 0) &&
               (strncmp(ptr, WAIT_FOR_FILENAME_ID, WAIT_FOR_FILENAME_ID_LENGTH) == 0))
           {
              int length = 0;

              used |= WAIT_FOR_FILENAME_FLAG;
              ptr += WAIT_FOR_FILENAME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isascii((int)(*ptr))) && (length < MAX_WAIT_FOR_LENGTH))
              {
                 dd[dir_pos].wait_for_filename[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_WAIT_FOR_LENGTH))
              {
                 dd[dir_pos].wait_for_filename[length] = '\0';
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 dd[dir_pos].wait_for_filename[0] = '\0';
                 if (length > 0)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                              "File name|pattern to long for directory option `%s' for the %d directory entry.",
                              WAIT_FOR_FILENAME_ID, dir_pos);
                    system_log(WARN_SIGN, NULL, 0,
                              "May only be %d bytes long.",
                              MAX_WAIT_FOR_LENGTH);
                 }
                 else
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                              "No file name|pattern for directory option `%s' for the %d directory entry.",
                              WAIT_FOR_FILENAME_ID, dir_pos);
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & ACCUMULATE_FLAG) == 0) &&
               (strncmp(ptr, ACCUMULATE_ID, ACCUMULATE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= ACCUMULATE_FLAG;
              ptr += ACCUMULATE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].accumulate = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & ACCUMULATE_SIZE_FLAG) == 0) &&
               (strncmp(ptr, ACCUMULATE_SIZE_ID, ACCUMULATE_SIZE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= ACCUMULATE_SIZE_FLAG;
              ptr += ACCUMULATE_SIZE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].accumulate_size = (off_t)strtoul(number, NULL, 10);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & FORCE_REREAD_FLAG) == 0) &&
               (strncmp(ptr, FORCE_REREAD_ID, FORCE_REREAD_ID_LENGTH) == 0))
           {
              used |= REP_UNKNOWN_FILES_FLAG;
              ptr += FORCE_REREAD_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].force_reread = YES;
           }
      else if (((used & IGNORE_SIZE_FLAG) == 0) &&
               (strncmp(ptr, IGNORE_SIZE_ID, IGNORE_SIZE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= IGNORE_SIZE_FLAG;
              ptr += IGNORE_SIZE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '>')
              {
                 dd[dir_pos].gt_lt_sign |= ISIZE_GREATER_THEN;
                 ptr++;
              }
              else if (*ptr == '<')
                   {
                      dd[dir_pos].gt_lt_sign |= ISIZE_LESS_THEN;
                      ptr++;
                   }
              else if ((*ptr == '=') || (isdigit((int)(*ptr))))
                   {
                      dd[dir_pos].gt_lt_sign |= ISIZE_EQUAL;
                      ptr++;
                   }
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 if ((dd[dir_pos].ignore_size = strtoul(number, NULL, 10)) == ULONG_MAX)
                 {
                    dd[dir_pos].ignore_size = 0;
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Value %s for option <%s> in DIR_CONFIG, to large causing overflow. Ignoring.",
                               number, IGNORE_SIZE_ID);
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & IGNORE_FILE_TIME_FLAG) == 0) &&
               (strncmp(ptr, IGNORE_FILE_TIME_ID, IGNORE_FILE_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= IGNORE_FILE_TIME_FLAG;
              ptr += IGNORE_FILE_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '>')
              {
                 dd[dir_pos].gt_lt_sign |= IFTIME_GREATER_THEN;
                 ptr++;
              }
              else if (*ptr == '<')
                   {
                      dd[dir_pos].gt_lt_sign |= IFTIME_LESS_THEN;
                      ptr++;
                   }
              else if ((*ptr == '=') || (isdigit((int)(*ptr))))
                   {
                      dd[dir_pos].gt_lt_sign |= IFTIME_EQUAL;
                      ptr++;
                   }
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].ignore_file_time = (unsigned int)strtoul(number, NULL, 10);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_FILES_FLAG) == 0) &&
               (strncmp(ptr, MAX_FILES_ID, MAX_FILES_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_FILES_FLAG;
              ptr += MAX_FILES_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_copied_files = (unsigned int)strtoul(number, NULL, 10);
                 dd[dir_pos].in_dc_flag |= MAX_CP_FILES_IDC;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_SIZE_FLAG) == 0) &&
               (strncmp(ptr, MAX_SIZE_ID, MAX_SIZE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_SIZE_FLAG;
              ptr += MAX_SIZE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_copied_file_size = (off_t)strtoul(number, NULL, 10) * MAX_COPIED_FILE_SIZE_UNIT;
                 dd[dir_pos].in_dc_flag |= MAX_CP_FILE_SIZE_IDC;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
#ifndef _WITH_PTHREAD
      else if (((used & IMPORTANT_DIR_FLAG) == 0) &&
               (strncmp(ptr, IMPORTANT_DIR_ID, IMPORTANT_DIR_ID_LENGTH) == 0))
           {
              used |= IMPORTANT_DIR_FLAG;
              ptr += IMPORTANT_DIR_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].important_dir = YES;
           }
#endif /* _WITH_PTHREAD */
           else
           {
              /* Ignore this option */
              end_ptr = ptr;
              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
              {
                 end_ptr++;
              }
              byte_buf = *end_ptr;
              *end_ptr = '\0';
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Unknown or duplicate option <%s> in DIR_CONFIG file for %d directory entry.",
                         ptr, dir_pos);
              *end_ptr = byte_buf;
              ptr = end_ptr;
           }

      while (*ptr == '\n')
      {
         ptr++;
      }
   } /* while (*ptr != '\0') */

   if (dd[dir_pos].unknown_file_time == -1)
   {
      dd[dir_pos].unknown_file_time = old_file_time;
   }
   if (dd[dir_pos].queued_file_time == -1)
   {
      dd[dir_pos].queued_file_time = old_file_time;
   }
   if (dd[dir_pos].locked_file_time == -1)
   {
      dd[dir_pos].locked_file_time = old_file_time;
   }

   return;
}
