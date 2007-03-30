/*
 *  fra_view.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   fra_view - shows all information in the FRA about a specific
 **              directory
 **
 ** SYNOPSIS
 **   fra_view [--version] [-w <working directory>] dir-alias|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific directory in
 **   the FRA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.03.2000 H.Kiehl Created
 **   20.07.2001 H.Kiehl Show which input files are to be deleted, unknown
 **                      and/or queued.
 **   03.05.2002 H.Kiehl Show number of files, bytes and queues.
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   17.08.2003 H.Kiehl wait_for_filename, accumulate and accumulate_size.
 **   09.02.2005 H.Kiehl Added additional time entry structure.
 **   07.06.2005 H.Kiehl Added dupcheck entries.
 **   05.10.2005 H.Kiehl Added in_dc_flag entry.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs = 0;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void convert2bin(char *, unsigned char),
            show_time_entry(struct bd_time_entry *),
            usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i,
        last = 0,
        position = -1;
   char dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
        *ptr,
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         (void)strcpy(dir_alias, argv[1]);
      }
   }
   else if (argc == 1)
        {
           position = -2;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if (fra_attach_passive() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FRA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (position == -1)
   {
      if ((position = get_dir_position(fra, dir_alias, no_of_dirs)) < 0)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not find directory %s in FRA. (%s %d)\n",
                       dir_alias, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_dirs;
           position = 0;
        }
   else if (position >= no_of_dirs)
        {
           (void)fprintf(stderr,
                         "WARNING : There are only %d directories in the FRA. (%s %d)\n",
                         no_of_dirs, __FILE__, __LINE__);
           exit(INCORRECT);
        }

   ptr =(char *)fra;
   ptr -= AFD_WORD_OFFSET;
   (void)fprintf(stdout,
                 "     Number of directories: %d   FRA ID: %d  Struct Version: %d\n\n",
                 no_of_dirs, fra_id, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
   for (i = position; i < last; i++)
   {
      (void)fprintf(stdout, "=============================> %s (%d) <=============================\n",
                    fra[i].dir_alias, i);
      (void)fprintf(stdout, "Directory alias      : %s\n", fra[i].dir_alias);
      (void)fprintf(stdout, "Directory ID         : %x\n", fra[i].dir_id);
      (void)fprintf(stdout, "URL                  : %s\n", fra[i].url);
      (void)fprintf(stdout, "Host alias           : %s\n", fra[i].host_alias);
      (void)fprintf(stdout, "Wait for             : %s\n", fra[i].wait_for_filename);
      (void)fprintf(stdout, "FSA position         : %d\n", fra[i].fsa_pos);
      (void)fprintf(stdout, "Priority             : %c\n", fra[i].priority);
      (void)fprintf(stdout, "Number of process    : %d\n", fra[i].no_of_process);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Bytes received       : %lu\n", fra[i].bytes_received);
#else
      (void)fprintf(stdout, "Bytes received       : %llu\n", fra[i].bytes_received);
#endif
      (void)fprintf(stdout, "Files received       : %u\n", fra[i].files_received);
      (void)fprintf(stdout, "Files in directory   : %u\n", fra[i].files_in_dir);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Bytes in directory   : %ld\n", (pri_off_t)fra[i].bytes_in_dir);
#else
      (void)fprintf(stdout, "Bytes in directory   : %lld\n", (pri_off_t)fra[i].bytes_in_dir);
#endif
      (void)fprintf(stdout, "Files in queue(s)    : %u\n", fra[i].files_queued);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Bytes in queue(s)    : %ld\n", (pri_off_t)fra[i].bytes_in_queue);
#else
      (void)fprintf(stdout, "Bytes in queue(s)    : %lld\n", (pri_off_t)fra[i].bytes_in_queue);
#endif
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Accumulate size      : %ld\n", (pri_off_t)fra[i].accumulate_size);
#else
      (void)fprintf(stdout, "Accumulate size      : %lld\n", (pri_off_t)fra[i].accumulate_size);
#endif
      (void)fprintf(stdout, "Accumulate           : %u\n", fra[i].accumulate);
      (void)fprintf(stdout, "gt_lt_sign           : %u\n", fra[i].gt_lt_sign);
      (void)fprintf(stdout, "Max errors           : %d\n", fra[i].max_errors);
      (void)fprintf(stdout, "Error counter        : %u\n", fra[i].error_counter);
#if SIZEOF_TIME_T == 4
      (void)fprintf(stdout, "Warn time            : %ld\n", (pri_time_t)fra[i].warn_time);
#else
      (void)fprintf(stdout, "Warn time            : %lld\n", (pri_time_t)fra[i].warn_time);
#endif
      (void)fprintf(stdout, "Keep connected       : %u\n", fra[i].keep_connected);
      if (fra[i].ignore_size == 0)
      {
         (void)fprintf(stdout, "Ignore size          : 0\n");
      }
      else
      {
         if (fra[i].gt_lt_sign & ISIZE_EQUAL)
         {
#if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "Ignore size          : %ld\n",
#else
            (void)fprintf(stdout, "Ignore size          : %lld\n",
#endif
                          (pri_off_t)fra[i].ignore_size);
         }
         else if (fra[i].gt_lt_sign & ISIZE_LESS_THEN)
              {
#if SIZEOF_OFF_T == 4
                 (void)fprintf(stdout, "Ignore size          : < %ld\n",
#else
                 (void)fprintf(stdout, "Ignore size          : < %lld\n",
#endif
                               (pri_off_t)fra[i].ignore_size);
              }
         else if (fra[i].gt_lt_sign & ISIZE_GREATER_THEN)
              {
#if SIZEOF_OFF_T == 4
                 (void)fprintf(stdout, "Ignore size          : > %ld\n",
#else
                 (void)fprintf(stdout, "Ignore size          : > %lld\n",
#endif
                               (pri_off_t)fra[i].ignore_size);
              }
              else
              {
#if SIZEOF_OFF_T == 4
                 (void)fprintf(stdout, "Ignore size          : ? %ld\n",
#else
                 (void)fprintf(stdout, "Ignore size          : ? %lld\n",
#endif
                               (pri_off_t)fra[i].ignore_size);
              }
      }
      if (fra[i].ignore_file_time == 0)
      {
         (void)fprintf(stdout, "Ignore file time     : 0\n");
      }
      else
      {
         if (fra[i].gt_lt_sign & IFTIME_EQUAL)
         {
            (void)fprintf(stdout, "Ignore file time     : %u\n",
                          fra[i].ignore_file_time);
         }
         else if (fra[i].gt_lt_sign & IFTIME_LESS_THEN)
              {
                 (void)fprintf(stdout, "Ignore file time     : < %u\n",
                               fra[i].ignore_file_time);
              }
         else if (fra[i].gt_lt_sign & IFTIME_GREATER_THEN)
              {
                 (void)fprintf(stdout, "Ignore file time     : > %u\n",
                               fra[i].ignore_file_time);
              }
              else
              {
                 (void)fprintf(stdout, "Ignore file time     : ? %u\n",
                               fra[i].ignore_file_time);
              }
      }
      (void)fprintf(stdout, "Max files            : %u\n",
                    fra[i].max_copied_files);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Max size             : %ld\n",
#else
      (void)fprintf(stdout, "Max size             : %lld\n",
#endif
                    (pri_off_t)fra[i].max_copied_file_size);
      if (fra[i].dir_status == NORMAL_STATUS)
      {
         (void)fprintf(stdout, "Directory status(%3d): NORMAL_STATUS\n",
                       fra[i].dir_status);
      }
      else if (fra[i].dir_status == WARNING_ID)
           {
              (void)fprintf(stdout, "Directory status(%3d): WARN TIME REACHED\n",
                            fra[i].dir_status);
           }
      else if (fra[i].dir_status == NOT_WORKING2)
           {
              (void)fprintf(stdout, "Directory status(%3d): NOT WORKING\n",
                            fra[i].dir_status);
           }
      else if (fra[i].dir_status == DISABLED)
           {
              (void)fprintf(stdout, "Directory status(%3d): DISABLED\n",
                            fra[i].dir_status);
           }
           else
           {
              (void)fprintf(stdout, "Directory status(%3d): UNKNOWN\n",
                            fra[i].dir_status);
           }
      if (fra[i].dir_flag == 0)
      {
         (void)fprintf(stdout, "Directory flag(  0)  : None\n");
      }
      else
      {
         (void)fprintf(stdout, "Directory flag(%3d)  : ", fra[i].dir_flag);
         if (fra[i].dir_flag & MAX_COPIED)
         {
            (void)fprintf(stdout, "MAX_COPIED ");
         }
         if (fra[i].dir_flag & FILES_IN_QUEUE)
         {
            (void)fprintf(stdout, "FILES_IN_QUEUE ");
         }
         if (fra[i].dir_flag & ADD_TIME_ENTRY)
         {
            (void)fprintf(stdout, "ADD_TIME_ENTRY ");
         }
         if (fra[i].dir_flag & LINK_NO_EXEC)
         {
            (void)fprintf(stdout, "LINK_NO_EXEC ");
         }
         if (fra[i].dir_flag & DIR_DISABLED)
         {
            (void)fprintf(stdout, "DIR_DISABLED ");
         }
#ifdef WITH_INOTIFY
         if (fra[i].dir_flag & INOTIFY_RENAME)
         {
            (void)fprintf(stdout, "INOTIFY_RENAME ");
         }
         if (fra[i].dir_flag & INOTIFY_CLOSE)
         {
            (void)fprintf(stdout, "INOTIFY_CLOSE ");
         }
#endif
         if (fra[i].dir_flag & ACCEPT_DOT_FILES)
         {
            (void)fprintf(stdout, "ACCEPT_DOT_FILES ");
         }
         if (fra[i].dir_flag & DONT_GET_DIR_LIST)
         {
            (void)fprintf(stdout, "DONT_GET_DIR_LIST ");
         }
         if (fra[i].dir_flag & DIR_ERROR_SET)
         {
            (void)fprintf(stdout, "DIR_ERROR_SET ");
         }
         if (fra[i].dir_flag & WARN_TIME_REACHED)
         {
            (void)fprintf(stdout, "WARN_TIME_REACHED");
         }
         (void)fprintf(stdout, "\n");
      }
      if (fra[i].in_dc_flag == 0)
      {
         (void)fprintf(stdout, "In DIR_CONFIG flag   : None\n");
      }
      else
      {
         (void)fprintf(stdout, "In DIR_CONFIG flag   : ");
         if (fra[i].in_dc_flag & DIR_ALIAS_IDC)
         {
            (void)fprintf(stdout, "DIR_ALIAS ");
         }
         if (fra[i].in_dc_flag & UNKNOWN_FILES_IDC)
         {
            (void)fprintf(stdout, "UNKNOWN_FILES ");
         }
         if (fra[i].in_dc_flag & QUEUED_FILES_IDC)
         {
            (void)fprintf(stdout, "QUEUED_FILES ");
         }
         if (fra[i].in_dc_flag & OLD_LOCKED_FILES_IDC)
         {
            (void)fprintf(stdout, "OLD_LOCKED_FILES ");
         }
         if (fra[i].in_dc_flag & REPUKW_FILES_IDC)
         {
            (void)fprintf(stdout, "REPORT_UNKNOWN_FILES ");
         }
         if (fra[i].in_dc_flag & DONT_REPUKW_FILES_IDC)
         {
            (void)fprintf(stdout, "DONT_REPORT_UNKNOWN_FILES ");
         }
#ifdef WITH_INOTIFY
         if (fra[i].in_dc_flag & INOTIFY_FLAG_IDC)
         {
            (void)fprintf(stdout, "INOTIFY_FLAG ");
         }
#endif
         if (fra[i].in_dc_flag & MAX_CP_FILES_IDC)
         {
            (void)fprintf(stdout, "MAX_COPIED_FILES ");
         }
         if (fra[i].in_dc_flag & MAX_CP_FILE_SIZE_IDC)
         {
            (void)fprintf(stdout, "MAX_COPIED_FILE_SIZE ");
         }
         if (fra[i].in_dc_flag & WARN_TIME_IDC)
         {
            (void)fprintf(stdout, "WARN_TIME ");
         }
         if (fra[i].in_dc_flag & KEEP_CONNECTED_IDC)
         {
            (void)fprintf(stdout, "KEEP_CONNECTED");
         }
         (void)fprintf(stdout, "\n");
      }
#ifdef WITH_DUP_CHECK
      if (fra[i].dup_check_timeout == 0L)
      {
         (void)fprintf(stdout, "Dupcheck timeout     : Disabled\n");
      }
      else
      {
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "Dupcheck timeout     : %ld\n",
#else
         (void)fprintf(stdout, "Dupcheck timeout     : %lld\n",
#endif
                       (pri_time_t)fra[i].dup_check_timeout);
         (void)fprintf(stdout, "Dupcheck flag        : ");
         if (fra[i].dup_check_flag & DC_FILENAME_ONLY)
         {
            (void)fprintf(stdout, "FILENAME_ONLY ");
         }
         else if (fra[i].dup_check_flag & DC_NAME_NO_SUFFIX)
              {
                 (void)fprintf(stdout, "NAME_NO_SUFFIX ");
              }
         else if (fra[i].dup_check_flag & DC_FILE_CONTENT)
              {
                 (void)fprintf(stdout, "FILE_CONTENT ");
              }
         else if (fra[i].dup_check_flag & DC_FILE_CONT_NAME)
              {
                 (void)fprintf(stdout, "FILE_CONT_NAME ");
              }
              else
              {
                 (void)fprintf(stdout, "UNKNOWN_TYPE ");
              }
         if (fra[i].dup_check_flag & DC_CRC32)
         {
            (void)fprintf(stdout, "CRC32");
         }
         else
         {
            (void)fprintf(stdout, "UNKNOWN_CRC");
         }
      }
#endif
      if (fra[i].force_reread == NO)
      {
         (void)fprintf(stdout, "Force reread         : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Force reread         : YES\n");
      }
      if (fra[i].queued == NO)
      {
         (void)fprintf(stdout, "Queued               : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Queued               : YES\n");
      }
      if (fra[i].remove == NO)
      {
         (void)fprintf(stdout, "Remove files         : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Remove files         : YES\n");
      }
      if (fra[i].stupid_mode == NO)
      {
         (void)fprintf(stdout, "Stupid mode          : NO\n");
      }
      else if (fra[i].stupid_mode == GET_ONCE_ONLY)
           {
              (void)fprintf(stdout, "Stupid mode          : GET_ONCE_ONLY\n");
           }
           else
           {
              (void)fprintf(stdout, "Stupid mode          : YES\n");
           }
      (void)fprintf(stdout, "Protocol (%4d)      : ", fra[i].protocol);
      if (fra[i].protocol == FTP)
      {
         (void)fprintf(stdout, "FTP\n");
      }
      else if (fra[i].protocol == LOC)
           {
              (void)fprintf(stdout, "LOC\n");
           }
      else if (fra[i].protocol == HTTP)
           {
              (void)fprintf(stdout, "HTTP\n");
           }
      else if (fra[i].protocol == SMTP)
           {
              (void)fprintf(stdout, "SMTP\n");
           }
#ifdef _WITH_WMO_SUPPORT
      else if (fra[i].protocol == WMO)
           {
              (void)fprintf(stdout, "WMO\n");
           }
#endif
           else
           {
              (void)fprintf(stdout, "Unknown\n");
           }
      if (fra[i].delete_files_flag == 0)
      {
         (void)fprintf(stdout, "Delete input files   : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Delete input files   : ");
         if (fra[i].delete_files_flag & UNKNOWN_FILES)
         {
            (void)fprintf(stdout, "UNKNOWN ");
         }
         if (fra[i].delete_files_flag & QUEUED_FILES)
         {
            (void)fprintf(stdout, "QUEUED ");
         }
         if (fra[i].delete_files_flag & OLD_LOCKED_FILES)
         {
            (void)fprintf(stdout, "LOCKED\n");
         }
         else
         {
            (void)fprintf(stdout, "\n");
         }
         if (fra[i].delete_files_flag & UNKNOWN_FILES)
         {
            (void)fprintf(stdout, "Unknown file time (h): %d\n",
                          fra[i].unknown_file_time / 3600);
         }
         if (fra[i].delete_files_flag & QUEUED_FILES)
         {
            (void)fprintf(stdout, "Queued file time (h) : %d\n",
                          fra[i].queued_file_time / 3600);
         }
         if (fra[i].delete_files_flag & OLD_LOCKED_FILES)
         {
            (void)fprintf(stdout, "Old lck file time (h): %d\n",
                          fra[i].locked_file_time / 3600);
         }
      }
      if (fra[i].report_unknown_files == NO)
      {
         (void)fprintf(stdout, "Report unknown files : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Report unknown files : YES\n");
      }
      if (fra[i].important_dir == NO)
      {
         (void)fprintf(stdout, "Important directory  : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Important directory  : YES\n");
      }
      if (fra[i].end_character == -1)
      {
         (void)fprintf(stdout, "End character        : NONE\n");
      }
      else
      {
         (void)fprintf(stdout, "End character        : %d\n",
                       fra[i].end_character);
      }
      if (fra[i].time_option == NO)
      {
         (void)fprintf(stdout, "Time option          : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Time option          : YES\n");
         (void)fprintf(stdout, "Next check time      : %s",
                       ctime(&fra[i].next_check_time));
         show_time_entry(&fra[i].te);
      }
      show_time_entry(&fra[i].ate);
      (void)fprintf(stdout, "Last retrieval       : %s",
                    ctime(&fra[i].last_retrieval));
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ show_time_entry() ++++++++++++++++++++++++++*/
static void
show_time_entry(struct bd_time_entry *te)
{
   int  byte_order = 1;
   char binstr[72],
        *ptr;

#ifdef _WORKING_LONG_LONG
   ptr = (char *)&te->minute;
   if (*(char *)&byte_order == 1)
   {
      ptr += 7;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[27], *ptr);
   binstr[35] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[36], *ptr);
   binstr[44] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[45], *ptr);
   binstr[53] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[54], *ptr);
   binstr[62] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[63], *ptr);
   binstr[71] = '\0';
   (void)fprintf(stdout, "Minute (long long)   : %s\n", binstr);
   (void)fprintf(stdout, "Continues (long long): %s\n", binstr);
#else
   ptr = (char *)&te->minute[7];
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   ptr--;
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   ptr--;
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   ptr--;
   convert2bin(&binstr[27], *ptr);
   binstr[35] = ' ';
   ptr--;
   convert2bin(&binstr[36], *ptr);
   binstr[44] = ' ';
   ptr--;
   convert2bin(&binstr[45], *ptr);
   binstr[53] = ' ';
   ptr--;
   convert2bin(&binstr[54], *ptr);
   binstr[62] = ' ';
   ptr--;
   convert2bin(&binstr[63], *ptr);
   binstr[71] = '\0';
   (void)fprintf(stdout, "Minute (uchar[8])    : %s\n", binstr);
   (void)fprintf(stdout, "Continues (uchar[8]) : %s\n", binstr);
#endif /* _WORKING_LONG_LONG */
   ptr = (char *)&te->hour;
   if (*(char *)&byte_order == 1)
   {
      ptr += 3;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[27], *ptr);
   binstr[35] = '\0';
   (void)fprintf(stdout, "Hour (uint)          : %s\n", binstr);
   ptr = (char *)&te->day_of_month;
   if (*(char *)&byte_order == 1)
   {
      ptr += 3;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[27], *ptr);
   binstr[35] = '\0';
   (void)fprintf(stdout, "Day of month (uint)  : %s\n", binstr);
   ptr = (char *)&te->month;
   if (*(char *)&byte_order == 1)
   {
      ptr++;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = '\0';
   (void)fprintf(stdout, "Month (short)        : %s\n", binstr);
   convert2bin(binstr, te->day_of_week);
   binstr[8] = '\0';
   (void)fprintf(stdout, "Day of week (uchar)  : %s\n", binstr);

   return;
}


/*--------------------------- convert2bin() ----------------------------*/ 
static void
convert2bin(char *buf, unsigned char val)
{
   register int i;

   for (i = 0; i < 8; i++)
   {
      if (val & 1)
      {
         buf[7 - i] = '1';
      }
      else
      {
         buf[7 - i] = '0';
      }
      val = val >> 1;
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : fra_view [--version] [-w working directory] dir-alias|position\n");
   return;
}
