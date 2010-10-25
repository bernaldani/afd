/*
 *  handle_options.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_options - handles all options which are to be done by
 **                    the AMG
 **
 ** SYNOPSIS
 **   int handle_options(int          position,
 **                      time_t       creation_time,
 **                      unsigned int unique_number,
 **                      unsigned int split_job_counter,
 **                      char         *file_path,
 **                      int          *files_to_send,
 **                      off_t        *file_size)
 **
 ** DESCRIPTION
 **   This functions executes the options for AMG. The following
 **   option are so far known:
 **   basename       - Here we rename all files to their basename.
 **                    This is defined to be from the first character
 **                    to the first dot being encountered.
 **                    (eg. file.1.ext -> file)
 **   extension      - Only the extension of the file name is cut off.
 **                    (eg. file.1.ext -> file.1)
 **   prefix add XXX - This adds a prefix to the file name.
 **                    (eg. file.1 -> XXXfile.1)
 **   prefix del XXX - Removes the prefix from the file name.
 **                    (eg. XXXfile.1 -> file.1)
 **   toupper          Convert all letters of a file name to upper case.
 **   tolower          Convert all letters of a file name to lower case.
 **   rename <rule>  - Files are being renamed by the rule <rule>.
 **                    Where <rule> is a simple string by which
 **                    the AFD can identify which rule to use in the
 **                    file $AFD_WORK_DIR/etc/rename.rule.
 **   exec <command> - The <command> is executed for each file
 **                    found. The current file name can be inserted
 **                    with %s. This can be done up to 10 times.
 **   tiff2gts       - This converts TIFF files to files having a
 **                    GTS header and only the TIFF data.
 **   gts2tiff       - This converts GTS T4 encoded files to TIFF
 **                    files.
 **   grib2wmo       - Extract GRIBS and write them in a WMO file
 **                    with TTAAii_CCCC_YYGGgg headers and 8 byte ASCII
 **                    length indicator and 2 byte type indicator.
 **   extract XXX    - Extracts WMO bulletins from a file and creates
 **                    a file for each bulletin it finds. The following
 **                    WMO files for XXX are recognised:
 **                    MRZ   - Special format used for the exchange
 **                            of bulletins between EZMW and EDZW.
 **                    VAX   - Bulletins with a two byte length indicator.
 **                            (Low Byte First)
 **                    LBF   - Bulletins with a four byte length indicator
 **                            and low byte first.
 **                    HBF   - Bulletins with a four byte length indicator
 **                            and high byte first.
 **                    MSS   - Bulletins with a special four byte length
 **                            indicator used in conjunction with the MSS.
 **                    WMO   - Bulletins with 8 byte ASCII length indicator
 **                            and 2 byte type indicator.
 **                    ASCII - Normal ASCII bulletins where SOH and ETX
 **                            are start and end of one bulletin.
 **                    GRIB  - Standard GRIB files, however since there
 **                            are so many different GRIB types and
 **                            limited namespace for WMO header, many
 **                            GRIB's will overwrite each other.
 **   assemble XXX   - Assembles WMO bulletins that are stored as 'one file
 **                    per bulletin' into one large file. The original files
 **                    will be deleted. It can create the same file
 **                    structures that the option 'extract' can handle,
 **                    except for MRZ.
 **   convert XXX    - Converts a file from one format into another.
 **                    Currently the following is implemented:
 **                    sohetx      - Adds <SOH><CR><CR><LF> to the beginning of
 **                                  the file and <CR><CR><LF><ETX> to the end
 **                                  of the file.
 **                    wmo         - Just add WMO 8 byte ascii length and 2
 **                                  bytes type indicator. If the message
 **                                  contains <SOH><CR><CR><LF> at start
 **                                  and <CR><CR><LF><ETX> at the end, these
 **                                  will be removed.
 **                    sohetxwmo   - Adds WMO 8 byte ascii length and 2 bytes
 **                                  type indicator plus <SOH><CR><CR><LF> at
 **                                  start and <CR><CR><LF><ETX> to end if they
 **                                  are not there.
 **                    sohetx2wmo0 - converts a file that contains many ascii
 **                                  bulletins starting with SOH and ending
 **                                  with ETX to the WMO standart (8 byte
 **                                  ascii length and 2 bytes type
 **                                  indicators). The SOH and ETX will NOT be
 **                                  copied to the new file.
 **                    sohetx2wmo1 - same as above only that here the SOH and
 **                                  ETX will copied to the new file.
 **                    mrz2wmo     - Converts GRIB, BUFR and BLOK files to
 **                                  files with 8 byte ascii length and 2 bytes
 **                                  type indicator plus  <SOH><CR><CR><LF>
 **                                  at start and <CR><CR><LF><ETX> to the
 **                                  end, for the individual fields.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to perform the action. Otherwise
 **   0 is returned and the file size 'file_size' if the size of the
 **   files have been changed due to the action (compress, uncompress,
 **   gzip, gunzip, tiff2gts and extract).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.10.1995 H.Kiehl Created
 **   21.01.1997 H.Kiehl Added rename option.
 **   16.09.1997 H.Kiehl Add some more extract options (VAX, LBF, HBF, MSS).
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   05.04.1999 H.Kiehl Added WMO to extract option.
 **   26.03.2000 H.Kiehl Removed (un)gzip and (un)compress options.
 **   14.07.2000 H.Kiehl Return the exit status of the process started
 **                      with exec_cmd() when it has failed.
 **   03.05.2002 H.Kiehl Added option execd (exec and remove original file).
 **   16.06.2002 H.Kiehl Added option execD and remove any sub directories
 **                      in the job directory.
 **   29.06.2002 H.Kiehl Added options toupper and tolower.
 **   25.08.2002 H.Kiehl Log the renaming of files.
 **   28.02.2003 H.Kiehl Added grib2wmo.
 **   01.10.2003 H.Kiehl Added convert.
 **   29.01.2005 H.Kiehl Exec option now supports parameters.
 **   15.08.2005 H.Kiehl Show files we delete due to an exec error in
 **                      DELETE_LOG.
 **   20.07.2006 H.Kiehl Added 'convert mrz2wmo'.
 **   11.02.2007 H.Kiehl Added locking option to exec.
 **   30.04.2007 H.Kiehl Recount files when we rename and overwrite files.
 **   08.03.2008 H.Kiehl Added -s to exec option.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf(), popen(), pclose()       */
#include <string.h>                /* strlen(), strcpy(), strcmp(),      */
                                   /* strncmp(), strerror()              */
#include <ctype.h>                 /* toupper(), tolower()               */
#include <unistd.h>                /* unlink()                           */
#include <stdlib.h>                /* system()                           */
#include <time.h>                  /* time(), gmtime()                   */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                /* open()                             */
#endif
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern time_t                     default_exec_timeout;
extern int                        fra_fd,
                                  no_of_rule_headers,
                                  receive_log_fd;
#ifndef _WITH_PTHREAD
extern int                        max_copied_files;
# ifdef _DELETE_LOG
extern off_t                      *file_size_pool;
# endif
extern char                       *file_name_buffer;
#endif
extern char                       *p_work_dir;
extern struct rule                *rule;
extern struct instant_db          *db;
extern struct fileretrieve_status *fra,
                                  *p_fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local global variables. */
static int                        counter_fd = -1,
                                  *unique_counter;
#ifndef _WITH_PTHREAD
static char                       *p_file_name;
#endif

/* Local function prototypes. */
static void                       prepare_rename_ow(int, char **),
                                  rename_ow(int, int, char *, off_t *,
#ifdef _DELETE_LOG
                                            time_t, unsigned int,
                                            unsigned int, int,
#endif
                                            char *, char *, char *, char *),
                                  create_assembled_name(char *, char *),
                                  delete_all_files(char *,
#ifdef _DELETE_LOG
                                                   int, int, time_t,
                                                   unsigned int,
                                                   unsigned int,
#endif
                                                   int, char *, char *);
static int                        cleanup_rename_ow(int,
#ifdef _PRODUCTION_LOG
                                                    int, time_t, unsigned int,
                                                    unsigned int, char *,
#endif
                                                    char **);
#ifdef _WITH_PTHREAD
static int                        get_file_names(char *, char **, char **);
#else
static int                        restore_files(char *, off_t *);
#endif
#if defined (_WITH_PTHREAD) || !defined (_PRODUCTION_LOG)
static int                        recount_files(char *, off_t *);
#endif
#ifdef _PRODUCTION_LOG
static int                        check_changes(time_t, unsigned int,
                                                unsigned int, int, char *,
                                                char *, int, int, int,
                                                char *, off_t *);
#endif


/*############################ handle_options() #########################*/
int
handle_options(int          position,
               time_t       creation_time,
               unsigned int unique_number,
               unsigned int split_job_counter,
               char         *file_path,
               int          *files_to_send,
               off_t        *file_size)
{
   int   i,
         j;
   off_t size;
   char  *options,
#ifdef _PRODUCTION_LOG
         *p_option,
#endif
         *ptr,
         *p_prefix,
#ifdef _WITH_PTHREAD
         *file_name_buffer = NULL,
         *p_file_name,
#endif
         *p_newname,
         filename[MAX_FILENAME_LENGTH],
         fullname[MAX_PATH_LENGTH],
         newname[MAX_PATH_LENGTH];

   options = db[position].loptions;
   for (i = 0; i < db[position].no_of_loptions; i++)
   {
      p_file_name = file_name_buffer;

      /* Check if we have to rename. */
      if ((db[position].loptions_flag & RENAME_ID_FLAG) &&
          (CHECK_STRNCMP(options, RENAME_ID, RENAME_ID_LENGTH) == 0))
      {
#ifdef _PRODUCTION_LOG
         p_option = options;
#endif
         /*
          * To rename a file we first look in the rename table 'rule' to
          * see if we find a rule for the renaming. If no rule is
          * found, no renaming will be done and the files get distributed
          * in their original names. The rename table is created by
          * the 'dir_check' from a rename rules file.
          */

         /*
          * Make sure we do have a renaming file. If not lets ignore
          * the renaming option. Lets hope this is correct ;-)
          */
         if (no_of_rule_headers == 0)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "You want to do renaming, but there is no valid file with rules for renaming. Ignoring this option.");
         }
         else
         {
            char *p_rule;

            /* Extract rule from local options. */
            p_rule = options + RENAME_ID_LENGTH;
            while ((*p_rule == ' ') || (*p_rule == '\t'))
            {
               p_rule++;
            }
            if (*p_rule == '\0')
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "No rule specified for renaming. Ignoring this option.");
            }
            else
            {
               int rule_pos;

               if ((rule_pos = get_rule(p_rule, no_of_rule_headers)) < 0)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Could NOT find rule `%s'. Ignoring this option.",
                              p_rule);
               }
               else
               {
#ifndef _WITH_PTHREAD
                  int  file_counter = *files_to_send;
#else
                  int  file_counter = 0;

                  if ((file_counter = get_file_names(file_path,
                                                     &file_name_buffer,
                                                     &p_file_name)) > 0)
                  {
#endif
                     register int k,
                                  overwrite,
                                  ret;
                     char         changed_name[MAX_FILENAME_LENGTH],
                                  *new_name_buffer,
                                  *p_overwrite;

                     /*
                      * Check if we want to overwrite the file
                      * if rename would lead to an overwrite.
                      */
                     p_overwrite = p_rule;
                     while ((*p_overwrite != '\0') && (*p_overwrite != ' ') &&
                            (*p_overwrite != '\t'))
                     {
                        p_overwrite++;
                     }
                     if ((*p_overwrite == ' ') || (*p_overwrite == '\t'))
                     {
                        while ((*p_overwrite == ' ') || (*p_overwrite == '\t'))
                        {
                           p_overwrite++;
                        }
                        if (((*p_overwrite == 'o') || (*p_overwrite == 'O')) &&
                            (*(p_overwrite + 1) == 'v') &&
                            (*(p_overwrite + 2) == 'e') &&
                            (*(p_overwrite + 3) == 'r') &&
                            (*(p_overwrite + 4) == 'w') &&
                            (*(p_overwrite + 5) == 'r') &&
                            (*(p_overwrite + 6) == 'i') &&
                            (*(p_overwrite + 7) == 't') &&
                            (*(p_overwrite + 8) == 'e') &&
                            ((*(p_overwrite + 9) == '\0') ||
                             (*(p_overwrite + 9) == ' ') ||
                             (*(p_overwrite + 9) == '\t')))
                        {
                           overwrite = YES;
                        }
                        else
                        {
                           overwrite = NO;
                        }
                     }                     
                     else
                     {
                        overwrite = NO;
                     }

                     (void)strcpy(newname, file_path);
                     p_newname = newname + strlen(newname);
                     *p_newname++ = '/';
                     (void)strcpy(fullname, file_path);
                     ptr = fullname + strlen(fullname);
                     *ptr++ = '/';

                     prepare_rename_ow(file_counter, &new_name_buffer);

                     for (j = 0; j < file_counter; j++)
                     {
                        for (k = 0; k < rule[rule_pos].no_of_rules; k++)
                        {
                           /*
                            * Filtering is necessary here since you can
                            * can have different rename rules for different
                            * files.
                            */
                           if ((ret = pmatch(rule[rule_pos].filter[k],
                                             p_file_name, NULL)) == 0)
                           {
                              /* We found a rule, what more do you want? */
                              /* Now lets get the new name.              */
                              change_name(p_file_name,
                                          rule[rule_pos].filter[k],
                                          rule[rule_pos].rename_to[k],
                                          changed_name, &counter_fd,
                                          &unique_counter,
                                          db[position].job_id);
                              (void)strcpy(ptr, p_file_name);
                              (void)strcpy(p_newname, changed_name);

                              /*
                               * Could be that we overwrite an existing
                               * file. If thats the case, we need to
                               * redo the file_name_buffer. But
                               * the changed name may not be the same
                               * name as the new one, else we get the
                               * counters wrong in afd_ctrl. Worth, when
                               * we only have one file to send, we will
                               * loose it!
                               */
                              rename_ow(overwrite, file_counter,
                                        new_name_buffer, file_size,
#ifdef _DELETE_LOG
                                        creation_time, unique_number,
                                        split_job_counter, position,
#endif
                                        newname, p_newname, fullname,
                                        p_file_name);
                              break;
                           }
                           else if (ret == 1)
                                {
                                   /*
                                    * This file is definitely NOT wanted,
                                    * no matter what the following filters
                                    * say.
                                    */
                                   break;
                                }
                        } /* for (k = 0; k < rule[rule_pos].no_of_rules; k++) */

                        p_file_name += MAX_FILENAME_LENGTH;
                     } /* for (j = 0; j < file_counter; j++) */

                     *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                                        position, creation_time,
                                                        unique_number,
                                                        split_job_counter,
                                                        p_option,
#endif
                                                        &new_name_buffer);

#ifdef _WITH_PTHREAD
                  }

                  free(file_name_buffer);
#endif
               }
            }
         }

         NEXT(options);
         continue;
      }

      /* Check if we have to execute a command. */
      if ((db[position].loptions_flag & EXEC_ID_FLAG) &&
          (CHECK_STRNCMP(options, EXEC_ID, EXEC_ID_LENGTH) == 0))
      {
#ifndef _WITH_PTHREAD
         int    file_counter = *files_to_send;
#else
         int    file_counter = 0;
#endif
         int    lock_all_jobs = NO,
                lock_one_job_only = NO,
                delete_original_file = NO,
                on_error_delete_all = NO,
                on_error_save = NO;
         time_t exec_timeout;
         char   *p_command,
                *del_orig_file = NULL,
                *p_del_orig_file = NULL,
                *p_save_orig_file = NULL,
                *save_orig_file = NULL;

         /*
          * First lets get the command rule. The full file name
          * can be entered with %s.
          */

         /* Determine the type of exec command, first the old one. */
#ifdef _PRODUCTION_LOG
         p_option = options;
#endif
         p_command = options + EXEC_ID_LENGTH;
         exec_timeout = default_exec_timeout;
         if (*p_command == 'd')
         {
            p_command++;
            if ((del_orig_file = malloc(MAX_PATH_LENGTH)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "malloc() error : %s", strerror(errno));
            }
            else
            {
               p_del_orig_file = del_orig_file +
                                 sprintf(del_orig_file, "%s/", file_path);
            }
            delete_original_file = YES;
         }
         else if (*p_command == 'D')
              {
                 p_command++;
                 on_error_delete_all = YES;
              }

         /* Now check for new type of exec command with parameters. */
         while ((*p_command != '\0') && (*(p_command + 1) != '\0') &&
                (*(p_command + 2) != '\0') && (*(p_command + 3) != '\0'))
         {
            if (((*p_command == ' ') || (*p_command == '\t')) &&
                (*(p_command + 1) == '-') &&
                ((*(p_command + 3) == ' ') || (*(p_command + 3) == '\t')))
            {
               if (*(p_command + 2) == 'd')
               {
                  if (del_orig_file == NULL)
                  {
                     if ((del_orig_file = malloc(MAX_PATH_LENGTH)) == NULL)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "malloc() error : %s", strerror(errno));
                     }
                     else
                     {
                        p_del_orig_file = del_orig_file +
                                          sprintf(del_orig_file, "%s/",
                                                  file_path);
                     }
                  }
                  delete_original_file = YES;
                  p_command += 3;
               }
               else if (*(p_command + 2) == 'D')
                    {
                       on_error_delete_all = YES;
                       p_command += 3;
                    }
               else if (*(p_command + 2) == 't')
                    {
                       int  i;
                       char str_number[MAX_INT_LENGTH];

                       p_command += 4;
                       i = 0;
                       while ((isdigit((int)(*p_command))) &&
                              (i < MAX_INT_LENGTH))
                       {
                          str_number[i] = *p_command;
                          i++; p_command++;
                       }
                       if (i > 0)
                       {
                          if (i < MAX_INT_LENGTH)
                          {
                             str_number[i] = '\0';
                             exec_timeout = (time_t)str2timet(str_number,
                                                              (char **)NULL, 10);
                          }
                          else
                          {
                             while ((*p_command != ' ') &&
                                    (*p_command != '\t') &&
                                    (*p_command != '\0'))
                             {
                                p_command++;
                             }
                             receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                         "exec timeout value to long.");
                          }
                       }
                    }
               else if (*(p_command + 2) == 'l')
                    {
                       lock_one_job_only = YES;
                       p_command += 3;
                    }
               else if (*(p_command + 2) == 'L')
                    {
                       lock_all_jobs = YES;
                       p_command += 3;
                    }
               else if (*(p_command + 2) == 's')
                    {
                       if (save_orig_file == NULL)
                       {
                          if ((save_orig_file = malloc(MAX_PATH_LENGTH)) == NULL)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "malloc() error : %s", strerror(errno));
                          }
                          else
                          {
                             p_save_orig_file = save_orig_file +
                                                sprintf(save_orig_file,
#if SIZEOF_TIME_T == 4
                                                        "%s%s%s/%lx_%x_%x/",
#else
                                                        "%s%s%s/%llx_%x_%x/",
#endif
                                                        p_work_dir,
                                                        AFD_FILE_DIR, STORE_DIR,
                                                        (pri_time_t)creation_time,
                                                        unique_number,
                                                        split_job_counter);
                             if (del_orig_file == NULL)
                             {
                                if ((del_orig_file = malloc(MAX_PATH_LENGTH)) == NULL)
                                {
                                   system_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "malloc() error : %s",
                                              strerror(errno));
                                }
                                else
                                {
                                   p_del_orig_file = del_orig_file +
                                                     sprintf(del_orig_file, "%s/",
                                                             file_path);
                                }
                             }
                          }
                       }
                       on_error_save = YES;
                       p_command += 3;
                    }
                    else
                    {
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                   "Unknown exec option -%c", *(p_command + 2));
                       break;
                    }
            }
            else
            {
               break;
            }
         }

         /* Extract command from local options. */
         while (((*p_command == ' ') || (*p_command == '\t')) &&
                (*p_command != '\0'))
         {
            p_command++;
         }
         if (*p_command == '\0')
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "No command specified for executing. Ignoring this option.");
         }
         else
         {
            register int ii = 0,
                         k = 0;
            char         *p_end = p_command,
                         *insert_list[10],
                         tmp_char,
                         tmp_option[1024],
                         command_str[1024],
                         *return_str = NULL;
            off_t        lock_offset;

            if (lock_all_jobs == YES)
            {
               lock_offset = AFD_WORD_OFFSET +
                             (db[position].fra_pos * sizeof(struct fileretrieve_status));
#ifdef LOCK_DEBUG
               lock_region_w(fra_fd, lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
               lock_region_w(fra_fd, lock_offset + LOCK_EXEC);
#endif
            }
            while ((*p_end != '\0') && (ii < 10))
            {
               if ((*p_end == '%') && (*(p_end + 1) == 's'))
               {
                  tmp_option[k] = *p_end;
                  tmp_option[k + 1] = *(p_end + 1);
                  insert_list[ii] = &tmp_option[k];
                  ii++;
                  p_end += 2;
                  k += 2;
               }
               else
               {
                  tmp_option[k] = *p_end;
                  p_end++; k++;
               }
            }
            if (ii >= 10)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "To many %%s in exec option. Can oly handle 10.");
            }
            tmp_option[k] = '\0';
            p_command = tmp_option;

            if (ii > 0)
            {
#ifdef _WITH_PTHREAD
               if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                                  &p_file_name)) > 0)
               {
#endif
                  int  file_removed,
                       length,
                       length_start,
                       mask_file_name,
                       ret,
                       save_dir_created = NO;
                  char *fnp;
#ifdef _PRODUCTION_LOG
#endif

                  insert_list[ii] = &tmp_option[k];
                  tmp_char = *insert_list[0];
                  *insert_list[0] = '\0';
                  length_start = sprintf(command_str, "cd %s && %s",
                                         file_path, p_command);
                  *insert_list[0] = tmp_char;

                  if ((lock_one_job_only == YES) && (lock_all_jobs == NO))
                  {
                     lock_offset = AFD_WORD_OFFSET +
                                   (db[position].fra_pos * sizeof(struct fileretrieve_status));
                  }
                  for (j = 0; j < file_counter; j++)
                  {
                     fnp = p_file_name;
                     mask_file_name = NO;
                     do
                     {
                        if ((*fnp == ';') || (*fnp == ' '))
                        {
                           mask_file_name = YES;
                           break;
                        }
                        fnp++;
                     } while (*fnp != '\0');

                     /* Generate command string with file name(s). */
                     length = 0;
                     for (k = 1; k < (ii + 1); k++)
                     {
                        tmp_char = *insert_list[k];
                        *insert_list[k] = '\0';
                        if (mask_file_name == YES)
                        {
                           length += sprintf(command_str + length_start + length,
                                             "\"%s\"%s", p_file_name,
                                             insert_list[k - 1] + 2);
                        }
                        else
                        {
                           length += sprintf(command_str + length_start + length,
                                             "%s%s", p_file_name,
                                             insert_list[k - 1] + 2);
                        }
                        *insert_list[k] = tmp_char;
                     }

                     if ((lock_one_job_only == YES) && (lock_all_jobs == NO))
                     {
#ifdef LOCK_DEBUG
                        lock_region_w(fra_fd, lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
                        lock_region_w(fra_fd, lock_offset + LOCK_EXEC);
#endif
                     }
                     if ((ret = exec_cmd(command_str, &return_str,
                                         receive_log_fd, p_fra->dir_alias,
                                         MAX_DIR_ALIAS_LENGTH, "",
                                         exec_timeout, YES,
                                         YES)) != 0) /* ie != SUCCESS */
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to execute command %s [Return code = %d]",
                                    command_str, ret);
                        if (return_str[0] != '\0')
                        {
                           char *end_ptr = return_str,
                                *start_ptr;

                           do
                           {
                              start_ptr = end_ptr;
                              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                              {
                                 end_ptr++;
                              }
                              if (*end_ptr == '\n')
                              {
                                 *end_ptr = '\0';
                                 end_ptr++;
                              }
                              receive_log(WARN_SIGN, NULL, 0, 0L,
                                          "%s", start_ptr);
                           } while (*end_ptr != '\0');
                        }
                     }
                     if ((lock_one_job_only == YES) && (lock_all_jobs == NO))
                     {
#ifdef LOCK_DEBUG
                        unlock_region(fra_fd, lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
                        unlock_region(fra_fd, lock_offset + LOCK_EXEC);
#endif
                     }
                     if (return_str != NULL)
                     {
                        free(return_str);
                        return_str = NULL;
                     }
                     file_removed = NO;
                     if ((ret != 0) && (on_error_save == YES))
                     {
                        if (save_dir_created == NO)
                        {
                           if ((mkdir(save_orig_file, DIR_MODE) == -1) &&
                               (errno != EEXIST))
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to mkdir() %s : %s",
                                         save_orig_file, strerror(errno));
                              free(save_orig_file);
                              save_orig_file = p_save_orig_file = NULL;
                              on_error_save = NO;
                           }
                           else
                           {
                              save_dir_created = YES;
                           }
                        }
                        if (on_error_save == YES)
                        {
                           (void)strcpy(p_del_orig_file, p_file_name);
                           (void)strcpy(p_save_orig_file, p_file_name);
                           if (delete_original_file == YES)
                           {
                              if (rename(del_orig_file, save_orig_file) == -1)
                              {
                                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                             "Failed to rename() %s to %s : %s",
                                             del_orig_file, save_orig_file,
                                             strerror(errno));
                              }
                              else
                              {
#ifdef _DELETE_LOG
                                 size_t dl_real_size;

                                 (void)strcpy(dl.file_name, p_file_name);
                                 (void)sprintf(dl.host_name, "%-*s %03x",
                                               MAX_HOSTNAME_LENGTH,
                                               db[position].host_alias,
                                               EXEC_FAILED_STORED);
                                 *dl.file_size = file_size_pool[j];
                                 *dl.job_id = db[position].job_id;
                                 *dl.dir_id = db[position].dir_id;
                                 *dl.input_time = creation_time;
                                 *dl.split_job_counter = split_job_counter;
                                 *dl.unique_number = unique_number;
                                 *dl.file_name_length = strlen(p_file_name);
                                 dl_real_size = *dl.file_name_length + dl.size +
                                                sprintf((dl.file_name + *dl.file_name_length + 1),
                                                        "%s%creturn code = %d (%s)",
                                                        DIR_CHECK, SEPARATOR_CHAR,
                                                        ret, save_orig_file);
                                 if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               "write() error : %s",
                                               strerror(errno));
                                 }
#endif
                                 file_removed = YES;
                              }
                           }
                           else
                           {
                              if (copy_file(del_orig_file, save_orig_file, NULL) != SUCCESS)
                              {
                                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                             "Failed to copy_file() %s to %s : %s",
                                             del_orig_file, save_orig_file,
                                             strerror(errno));
                              }
                           }
                        }
                     }
                     if ((file_removed == NO) && (delete_original_file == YES))
                     {
                        (void)strcpy(p_del_orig_file, p_file_name);
                        if ((unlink(del_orig_file) == -1) && (errno != ENOENT))
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() `%s' : %s",
                                       del_orig_file, strerror(errno));
                        }
#ifdef _DELETE_LOG
                        else
                        {
                           if (ret != 0)
                           {
                              size_t dl_real_size;

                              (void)strcpy(dl.file_name, p_file_name);
                              (void)sprintf(dl.host_name, "%-*s %03x",
                                            MAX_HOSTNAME_LENGTH,
                                            db[position].host_alias,
                                            EXEC_FAILED_DEL);
                              *dl.file_size = file_size_pool[j];
                              *dl.job_id = db[position].job_id;
                              *dl.dir_id = db[position].dir_id;
                              *dl.input_time = creation_time;
                              *dl.split_job_counter = split_job_counter;
                              *dl.unique_number = unique_number;
                              *dl.file_name_length = strlen(p_file_name);
                              dl_real_size = *dl.file_name_length + dl.size +
                                             sprintf((dl.file_name + *dl.file_name_length + 1),
                                                     "%s%creturn code = %d (%s %d)",
                                                     DIR_CHECK, SEPARATOR_CHAR,
                                                     ret, __FILE__, __LINE__);
                              if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            "write() error : %s", strerror(errno));
                              }
                           }
                        }
#endif
                     }
#ifdef _PRODUCTION_LOG
                     if (j == (file_counter - 1))
                     {
                        *files_to_send = check_changes(creation_time,
                                                       unique_number,
                                                       split_job_counter,
                                                       position,
                                                       p_file_name,
                                                       p_option,
                                                       ret,
                                                       file_counter,
                                                       YES,
                                                       file_path,
                                                       file_size);

                        /* Since check_changes() has overwritten       */
                        /* file_name_buffer array we must update j and */
                        /* file_counter so we do leave the for loop.   */
                        file_counter = *files_to_send;
                        j = file_counter;
                     }
                     else
                     {
                        *files_to_send = check_changes(creation_time,
                                                       unique_number,
                                                       split_job_counter,
                                                       position,
                                                       p_file_name,
                                                       p_option,
                                                       ret,
                                                       file_counter,
                                                       NO,
                                                       file_path,
                                                       file_size);
                     }
#endif

                     p_file_name += MAX_FILENAME_LENGTH;
                  } /* for (j = 0; j < file_counter; j++) */

#ifdef _WITH_PTHREAD
# ifndef _PRODUCTION_LOG
                  *files_to_send = recount_files(file_path, file_size);
# endif
               }

               free(file_name_buffer);
#else
# ifndef _PRODUCTION_LOG
                  *files_to_send = restore_files(file_path, file_size);
# endif
#endif
            }
            else
            {
               int ret;

               (void)sprintf(command_str, "cd %s && %s", file_path, p_command);

               if ((lock_one_job_only == YES) && (lock_all_jobs == NO))
               {
                  lock_offset = AFD_WORD_OFFSET +
                                (db[position].fra_pos * sizeof(struct fileretrieve_status));
#ifdef LOCK_DEBUG
                  lock_region_w(fra_fd, lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
                  lock_region_w(fra_fd, lock_offset + LOCK_EXEC);
#endif
               }
               if ((ret = exec_cmd(command_str, &return_str,
                                   receive_log_fd, p_fra->dir_alias,
                                   MAX_DIR_ALIAS_LENGTH, "",
                                   exec_timeout, YES, YES)) != 0)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to execute command %s [Return code = %d]",
                              command_str, ret);
                  if (return_str[0] != '\0')
                  {
                     char *end_ptr = return_str,
                          *start_ptr;

                     do
                     {
                        start_ptr = end_ptr;
                        while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                        {
                           end_ptr++;
                        }
                        if (*end_ptr == '\n')
                        {
                           *end_ptr = '\0';
                           end_ptr++;
                        }
                        receive_log(WARN_SIGN, NULL, 0, 0L, "%s", start_ptr);
                     } while (*end_ptr != '\0');
                  }
               }
               if ((lock_one_job_only == YES) && (lock_all_jobs == NO))
               {
#ifdef LOCK_DEBUG
                  unlock_region(fra_fd, lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
                  unlock_region(fra_fd, lock_offset + LOCK_EXEC);
#endif
               }
               if (return_str != NULL)
               {
                  free(return_str);
                  return_str = NULL;
               }

               if (delete_original_file == YES)
               {
                  int do_rename,
                      ren_unlink_ret;

#ifdef _WITH_PTHREAD
                  if ((file_counter = get_file_names(file_path,
                                                     &file_name_buffer,
                                                     &p_file_name)) > 0)
                  {
#endif
                  if ((ret != 0) && (on_error_save == YES))
                  {
                     if ((mkdir(save_orig_file, DIR_MODE) == -1) &&
                         (errno != EEXIST))
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to mkdir() %s : %s",
                                   save_orig_file, strerror(errno));
                        free(save_orig_file);
                        save_orig_file = p_save_orig_file = NULL;
                        on_error_save = NO;
                        do_rename = NO;
                     }
                     else
                     {
                        do_rename = YES;
                     }
                  }
                  else
                  {
                     do_rename = NO;
                  }
                  for (j = 0; j < file_counter; j++)
                  {
                     (void)strcpy(p_del_orig_file, p_file_name);
                     if (do_rename == YES)
                     {
                        (void)strcpy(p_save_orig_file, p_file_name);
                        ren_unlink_ret = rename(del_orig_file, save_orig_file);
                     }
                     else
                     {
                        ren_unlink_ret = unlink(del_orig_file);
                     }
                     if ((ren_unlink_ret == -1) && (errno != ENOENT))
                     {
                        if (do_rename == YES)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to rename() `%s' to `%s' : %s",
                                       del_orig_file, save_orig_file,
                                       strerror(errno));
                        }
                        else
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() `%s' : %s",
                                       del_orig_file, strerror(errno));
                        }
                     }
#ifdef _DELETE_LOG
                     else
                     {
                        if (ret != 0)
                        {
                           size_t dl_real_size;

                           (void)strcpy(dl.file_name, p_file_name);
                           (void)sprintf(dl.host_name, "%-*s %03x",
                                         MAX_HOSTNAME_LENGTH,
                                         db[position].host_alias,
                                         (do_rename == NO) ? EXEC_FAILED_DEL : EXEC_FAILED_STORED);
                           *dl.file_size = file_size_pool[j];
                           *dl.job_id = db[position].job_id;
                           *dl.dir_id = db[position].dir_id;
                           *dl.input_time = creation_time;
                           *dl.split_job_counter = split_job_counter;
                           *dl.unique_number = unique_number;
                           *dl.file_name_length = strlen(p_file_name);
                           if (do_rename == NO)
                           {
                              dl_real_size = *dl.file_name_length + dl.size +
                                             sprintf((dl.file_name + *dl.file_name_length + 1),
                                                     "%s%creturn code = %d (%s %d)",
                                                     DIR_CHECK, SEPARATOR_CHAR,
                                                     ret, __FILE__, __LINE__);
                           }
                           else
                           {
                              dl_real_size = *dl.file_name_length + dl.size +
                                             sprintf((dl.file_name + *dl.file_name_length + 1),
                                                     "%s%creturn code = %d (%s)",
                                                     DIR_CHECK, SEPARATOR_CHAR,
                                                     ret, save_orig_file);
                           }
                           if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "write() error : %s", strerror(errno));
                           }
                        }
                     }
#endif
                     p_file_name += MAX_FILENAME_LENGTH;
                  } /* for (j = 0; j < file_counter; j++) */
#ifdef _WITH_PTHREAD
                  }
#endif
               }

               if ((ret != 0) &&
                   ((on_error_delete_all == YES) || (on_error_save == YES)))
               {
                  if (on_error_save == YES)
                  {
                     if ((mkdir(save_orig_file, DIR_MODE) == -1) &&
                         (errno != EEXIST))
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to mkdir() %s : %s",
                                   save_orig_file, strerror(errno));
                        free(save_orig_file);
                        save_orig_file = p_save_orig_file = NULL;
                        on_error_save = NO;
                     }
                  }
                  delete_all_files(file_path,
#ifdef _DELETE_LOG
                                   position, ret, creation_time,
                                   unique_number, split_job_counter,
#endif
                                   on_error_delete_all,
                                   (on_error_save == YES) ? p_save_orig_file : NULL,
                                   save_orig_file);
               }
               if ((ret != 0) && (on_error_delete_all == YES))
               {
                  *files_to_send = 0;
                  *file_size = 0;
#ifdef _PRODUCTION_LOG
# ifdef _WITH_PTHREAD
                  if ((file_counter = get_file_names(file_path,
                                                     &file_name_buffer,
                                                     &p_file_name)) > 0)
                  {
# endif
                     for (j = 0; j < file_counter; j++)
                     {
                        production_log(creation_time, file_counter, 0,
                                       unique_number, split_job_counter,
                                       db[position].job_id, db[position].dir_id,
                                       "%s%c%c%c%d%c%s",
                                       p_file_name, SEPARATOR_CHAR,
                                       SEPARATOR_CHAR, SEPARATOR_CHAR,
                                       ret, SEPARATOR_CHAR, p_command);
                        p_file_name += MAX_FILENAME_LENGTH;
                     }
# ifdef _WITH_PTHREAD
                     free(file_name_buffer);
                  }
# endif
#endif
               }
               else
               {
#ifdef _PRODUCTION_LOG
                  *files_to_send = check_changes(creation_time,
                                                 unique_number,
                                                 split_job_counter,
                                                 position,
                                                 NULL,
                                                 p_option,
                                                 ret,
                                                 file_counter,
                                                 YES,
                                                 file_path,
                                                 file_size);
                  file_counter = *files_to_send;
#else
                  /*
                   * Recount the files regardless if the previous function
                   * call was successful or not. Even if exec_cmd() returns
                   * with an error we do not know if it did after all do
                   * somethings with the files.
                   */
# ifdef _WITH_PTHREAD
                  *files_to_send = recount_files(file_path, file_size);
# else
                  if ((i + 1) == db[position].no_of_loptions)
                  {
                     *files_to_send = recount_files(file_path, file_size);
                  }
                  else
                  {
                     *files_to_send = restore_files(file_path, file_size);
                  }
# endif
#endif
               }
            }
            if (del_orig_file != NULL)
            {
               free(del_orig_file);
            }
            if (save_orig_file != NULL)
            {
               free(save_orig_file);
            }
            if (lock_all_jobs == YES)
            {
#ifdef LOCK_DEBUG
               unlock_region(fra_fd, lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
               unlock_region(fra_fd, lock_offset + LOCK_EXEC);
#endif
            }
         }
         NEXT(options);
         continue;
      }

      /* Check if we just want to send the basename. */
      if ((db[position].loptions_flag & BASENAME_ID_FLAG) &&
          (CHECK_STRNCMP(options, BASENAME_ID, BASENAME_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  overwrite;
            char *new_name_buffer,
                 *p_overwrite,
                 *p_search;

            p_overwrite = options + BASENAME_ID_LENGTH;
            if ((*p_overwrite == ' ') || (*p_overwrite == '\t'))
            {
               while ((*p_overwrite == ' ') || (*p_overwrite == '\t'))
               {
                  p_overwrite++;
               }
               if (((*p_overwrite == 'o') || (*p_overwrite == 'O')) &&
                   (*(p_overwrite + 1) == 'v') &&
                   (*(p_overwrite + 2) == 'e') &&
                   (*(p_overwrite + 3) == 'r') &&
                   (*(p_overwrite + 4) == 'w') &&
                   (*(p_overwrite + 5) == 'r') &&
                   (*(p_overwrite + 6) == 'i') &&
                   (*(p_overwrite + 7) == 't') &&
                   (*(p_overwrite + 8) == 'e') &&
                   ((*(p_overwrite + 9) == '\0') ||
                    (*(p_overwrite + 9) == ' ') ||
                    (*(p_overwrite + 9) == '\t')))
               {
                  overwrite = YES;
               }
               else
               {
                  overwrite = NO;
               }
            }
            else
            {
               overwrite = NO;
            }
            (void)strcpy(fullname, file_path);
            ptr = fullname + strlen(fullname);
            *ptr++ = '/';
            *ptr = '\0';
            (void)strcpy(newname, fullname);
            p_newname = newname + (ptr - fullname);

            prepare_rename_ow(file_counter, &new_name_buffer);

            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);

               /* Generate name for the new file. */
               (void)strcpy(filename, p_file_name);
               p_search = filename;
               while ((*p_search != '.') && (*p_search != '\0'))
               {
                  p_search++;
               }
               if (*p_search == '.')
               {
                  *p_search = '\0';
                  (void)strcpy(p_newname, filename);

                  rename_ow(overwrite, file_counter, new_name_buffer, file_size,
#ifdef _DELETE_LOG
                            creation_time, unique_number,
                            split_job_counter, position,
#endif
                            newname, p_newname, fullname, p_file_name);
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }

            *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                               position, creation_time,
                                               unique_number,
                                               split_job_counter, options,
#endif
                                               &new_name_buffer);
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we have to truncate the extension. */
      if ((db[position].loptions_flag & EXTENSION_ID_FLAG) &&
          (CHECK_STRNCMP(options, EXTENSION_ID, EXTENSION_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  overwrite;
            char *new_name_buffer,
                 *p_overwrite,
                 *p_search;

            p_overwrite = options + EXTENSION_ID_LENGTH;
            if ((*p_overwrite == ' ') || (*p_overwrite == '\t'))
            {
               while ((*p_overwrite == ' ') || (*p_overwrite == '\t'))
               {
                  p_overwrite++;
               }
               if (((*p_overwrite == 'o') || (*p_overwrite == 'O')) &&
                   (*(p_overwrite + 1) == 'v') &&
                   (*(p_overwrite + 2) == 'e') &&
                   (*(p_overwrite + 3) == 'r') &&
                   (*(p_overwrite + 4) == 'w') &&
                   (*(p_overwrite + 5) == 'r') &&
                   (*(p_overwrite + 6) == 'i') &&
                   (*(p_overwrite + 7) == 't') &&
                   (*(p_overwrite + 8) == 'e') &&
                   ((*(p_overwrite + 9) == '\0') ||
                    (*(p_overwrite + 9) == ' ') ||
                    (*(p_overwrite + 9) == '\t')))
               {
                  overwrite = YES;
               }
               else
               {
                  overwrite = NO;
               }
            }
            else
            {
               overwrite = NO;
            }
            (void)strcpy(fullname, file_path);
            ptr = fullname + strlen(fullname);
            *ptr++ = '/';
            *ptr = '\0';
            (void)strcpy(newname, fullname);
            p_newname = newname + (ptr - fullname);

            prepare_rename_ow(file_counter, &new_name_buffer);

            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);

               /* Generate name for the new file. */
               (void)strcpy(filename, p_file_name);
               p_search = filename + strlen(filename);
               while ((*p_search != '.') && (p_search != filename))
               {
                  p_search--;
               }
               if (*p_search == '.')
               {
                  *p_search = '\0';
                  (void)strcpy(p_newname, filename);

                  rename_ow(overwrite, file_counter, new_name_buffer, file_size,
#ifdef _DELETE_LOG
                            creation_time, unique_number,
                            split_job_counter, position,
#endif
                            newname, p_newname, fullname, p_file_name);
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                               position, creation_time,
                                               unique_number,
                                               split_job_counter, options,
#endif
                                               &new_name_buffer);
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if it's the add prefix option. */
      if ((db[position].loptions_flag & ADD_PREFIX_ID_FLAG) &&
          (CHECK_STRNCMP(options, ADD_PREFIX_ID, ADD_PREFIX_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;
#else
         int  file_counter = *files_to_send;
#endif
         char *new_name_buffer,
              *p_prefix_newname;

         /* Get the prefix. */
#ifdef _PRODUCTION_LOG
         p_option = options;
#endif
         p_prefix = options + ADD_PREFIX_ID_LENGTH;
         while ((*p_prefix == ' ') || (*p_prefix == '\t'))
         {
            p_prefix++;
         }

         (void)strcpy(newname, file_path);
         p_newname = newname + strlen(newname);
         *p_newname++ = '/';
         p_prefix_newname = p_newname;
         while (*p_prefix != '\0')
         {
            *p_newname = *p_prefix;
            p_newname++; p_prefix++;
         }
         (void)strcpy(fullname, file_path);
         ptr = fullname + strlen(fullname);
         *ptr++ = '/';

         prepare_rename_ow(file_counter, &new_name_buffer);

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);

               /* Generate name with prefix. */
               (void)strcpy(p_newname, p_file_name);

               rename_ow(YES, file_counter, new_name_buffer, file_size,
#ifdef _DELETE_LOG
                         creation_time, unique_number,
                         split_job_counter, position,
#endif
                         newname, p_prefix_newname, fullname, p_file_name);
               p_file_name += MAX_FILENAME_LENGTH;
            }
            *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                               position, creation_time,
                                               unique_number,
                                               split_job_counter, p_option,
#endif
                                               &new_name_buffer);
#ifdef _WITH_PTHREAD
         }
         else
         {
            free(new_name_buffer);
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if it's the delete prefix option. */
      if ((db[position].loptions_flag & DEL_PREFIX_ID_FLAG) &&
          (CHECK_STRNCMP(options, DEL_PREFIX_ID, DEL_PREFIX_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;
#else
         int  file_counter = *files_to_send;
#endif
         int  length;
         char *new_name_buffer;

         /* Get the prefix. */
#ifdef _PRODUCTION_LOG
         p_option = options;
#endif
         p_prefix = options + DEL_PREFIX_ID_LENGTH;
         while ((*p_prefix == ' ') || (*p_prefix == '\t'))
         {
            p_prefix++;
         }

         (void)strcpy(newname, file_path);
         p_newname = newname + strlen(newname);
         *p_newname++ = '/';
         (void)strcpy(fullname, file_path);
         ptr = fullname + strlen(fullname);
         *ptr++ = '/';
         length = strlen(p_prefix);

         prepare_rename_ow(file_counter, &new_name_buffer);

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            for (j = 0; j < file_counter; j++)
            {
               if (CHECK_STRNCMP(p_file_name, p_prefix, length) == 0)
               {
                  (void)strcpy(ptr, p_file_name);

                  /* Generate name without prefix. */
                  (void)strcpy(p_newname, p_file_name + length);

                  rename_ow(YES, file_counter, new_name_buffer, file_size,
#ifdef _DELETE_LOG
                            creation_time, unique_number,
                            split_job_counter, position,
#endif
                            newname, p_newname, fullname, p_file_name);
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                               position, creation_time,
                                               unique_number,
                                               split_job_counter, p_option,
#endif
                                               &new_name_buffer);
#ifdef _WITH_PTHREAD
         }
         else
         {
            free(new_name_buffer);
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if it's the toupper option. */
      if ((db[position].loptions_flag & TOUPPER_ID_FLAG) &&
          (CHECK_STRNCMP(options, TOUPPER_ID, TOUPPER_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;
#else
         int  file_counter = *files_to_send;
#endif
         char *convert_ptr,
              *new_name_buffer;

         (void)strcpy(newname, file_path);
         p_newname = newname + strlen(newname);
         *p_newname++ = '/';
         (void)strcpy(fullname, file_path);
         ptr = fullname + strlen(fullname);
         *ptr++ = '/';

         prepare_rename_ow(file_counter, &new_name_buffer);

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);
               (void)strcpy(p_newname, p_file_name);
               convert_ptr = p_newname;
               while (*convert_ptr != '\0')
               {
                  *convert_ptr = toupper((int)*convert_ptr);
                  convert_ptr++;
               }

               rename_ow(YES, file_counter, new_name_buffer, file_size,
#ifdef _DELETE_LOG
                         creation_time, unique_number,
                         split_job_counter, position,
#endif
                         newname, p_newname, fullname, p_file_name);
               p_file_name += MAX_FILENAME_LENGTH;
            }
            *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                               position, creation_time,
                                               unique_number,
                                               split_job_counter, options,
#endif
                                               &new_name_buffer);
#ifdef _WITH_PTHREAD
         }
         else
         {
            free(new_name_buffer);
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if it's the tolower option. */
      if ((db[position].loptions_flag & TOLOWER_ID_FLAG) &&
          (CHECK_STRNCMP(options, TOLOWER_ID, TOLOWER_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;
#else
         int  file_counter = *files_to_send;
#endif
         char *convert_ptr,
              *new_name_buffer;

         (void)strcpy(newname, file_path);
         p_newname = newname + strlen(newname);
         *p_newname++ = '/';
         (void)strcpy(fullname, file_path);
         ptr = fullname + strlen(fullname);
         *ptr++ = '/';

         prepare_rename_ow(file_counter, &new_name_buffer);

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);
               (void)strcpy(p_newname, p_file_name);
               convert_ptr = p_newname;
               while (*convert_ptr != '\0')
               {
                  *convert_ptr = tolower((int)*convert_ptr);
                  convert_ptr++;
               }

               rename_ow(YES, file_counter, new_name_buffer, file_size,
#ifdef _DELETE_LOG
                         creation_time, unique_number,
                         split_job_counter, position,
#endif
                         newname, p_newname, fullname, p_file_name);
               p_file_name += MAX_FILENAME_LENGTH;
            }
            *files_to_send = cleanup_rename_ow(file_counter,
#ifdef _PRODUCTION_LOG
                                               position, creation_time,
                                               unique_number,
                                               split_job_counter, options,
#endif
                                               &new_name_buffer);
#ifdef _WITH_PTHREAD
         }
         else
         {
            free(new_name_buffer);
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

#ifdef _WITH_AFW2WMO
      /* Check if we want to convert AFW format to WMO format. */
      if ((db[position].loptions_flag & AFW2WMO_ID_FLAG) &&
          (CHECK_STRCMP(options, AFW2WMO_ID) == 0))
      {
# ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
# else
            int  file_counter = *files_to_send;
# endif
            int  length,
                 ret;
            char *buffer;

            *file_size = 0;

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);

               buffer = NULL;
               if ((length = (int)read_file(fullname, &buffer)) != INCORRECT)
               {
                  char *wmo_buffer = NULL;

                  if ((ret = afw2wmo(buffer, &length, &wmo_buffer,
                                     p_file_name)) < 0)
                  {
                     char error_name[MAX_PATH_LENGTH];

                     (void)sprintf(error_name, "%s%s/error/%s", p_work_dir,
                                   AFD_FILE_DIR, p_file_name);
                     if (rename(fullname, error_name) == -1)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to rename file `%s' to `%s' : %s",
                                    fullname, error_name, strerror(errno));
                     }
                     else
                     {
                        (*files_to_send)--;
                     }
                  }
                  else
                  {
                     if (ret == SUCCESS)
                     {
                        int fd;

# ifdef O_LARGEFILE
                        if ((fd = open(fullname, O_WRONLY | O_TRUNC | O_LARGEFILE)) == -1)
# else
                        if ((fd = open(fullname, O_WRONLY | O_TRUNC)) == -1)
# endif
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to open() `%s' : %s",
                                       fullname, strerror(errno));
                           if ((unlink(fullname) == -1) && (errno != ENOENT))
                           {
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                          "Failed to unlink() `%s' : %s",
                                          p_file_name, strerror(errno));
                           }
                           else
                           {
                              (*files_to_send)--;
                           }
                        }
                        else
                        {
                           if (write(fd, wmo_buffer, length) != length)
                           {
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                          "Failed to write() to `%s' : %s",
                                          p_file_name, strerror(errno));
                              if ((unlink(fullname) == -1) && (errno != ENOENT))
                              {
                                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                             "Failed to unlink() `%s' : %s",
                                             p_file_name, strerror(errno));
                              }
                              else
                              {
                                 (*files_to_send)--;
                              }
                           }
                           else
                           {
                              *file_size += length;
# ifdef _PRODUCTION_LOG
                              production_log(creation_time, 1, 1, unique_number,
                                             split_job_counter,
                                             db[position].job_id,
                                             db[position].dir_id,
                                             "%s%c%s%c%x%c0%cafw2wmo()",
                                             p_file_name, SEPARATOR_CHAR,
                                             p_file_name, SEPARATOR_CHAR,
                                             length, SEPARATOR_CHAR,
                                             SEPARATOR_CHAR);
# endif
                           }

                           if (close(fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "close() error : %s", strerror(errno));
                           }
                        }
                        free(wmo_buffer);
                     } /* if (ret == SUCCESS) */
                     else if (ret == WMO_MESSAGE)
                          {
                             *file_size += length;
# ifdef _PRODUCTION_LOG
                              production_log(creation_time, 1, 1, unique_number,
                                             split_job_counter,
                                             db[position].job_id,
                                             db[position].dir_id,
                                             "%s%c%s%c%x%c0%cafw2wmo()",
                                             p_file_name, SEPARATOR_CHAR,
                                             p_file_name, SEPARATOR_CHAR,
                                             length, SEPARATOR_CHAR,
                                             SEPARATOR_CHAR);
# endif
                          }
                  }
                  free(buffer);
               }
               else
               {
                  char error_name[MAX_PATH_LENGTH];

                  (void)sprintf(error_name, "%s%s/%s", p_work_dir,
                                AFD_FILE_DIR, p_file_name);
                  if (rename(fullname, error_name) == -1)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Failed to rename file `%s' to `%s' : %s",
                                 fullname, error_name, strerror(errno));
                  }
                  else
                  {
                     (*files_to_send)--;
                  }
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
# ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
# endif
         NEXT(options);
         continue;
      }
#endif /* _WITH_AFW2WMO */

      /* Check if we want to convert TIFF files to GTS format. */
      if (((db[position].loptions_flag & TIFF2GTS_ID_FLAG) ||
           (db[position].loptions_flag & FAX2GTS_ID_FLAG)) &&
          ((CHECK_STRCMP(options, TIFF2GTS_ID) == 0) ||
           (CHECK_STRNCMP(options, FAX2GTS_ID, FAX2GTS_ID_LENGTH) == 0)))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int fax_format,
                recount_files_var = NO;

            *file_size = 0;
            if (db[position].loptions_flag & TIFF2GTS_ID_FLAG)
            {
               fax_format = 0;
            }
            else
            {
               if (*(options + FAX2GTS_ID_LENGTH) == ' ')
               {
                  fax_format = atoi(options + FAX2GTS_ID_LENGTH + 1);
                  switch (fax_format)
                  {
                     case 1 : /* DFAX1062 */
                     case 2 : /* DFAX1064 */
                     case 3 : /* DFAX1074 */
                     case 4 : /* DFAX1084 */
                     case 5 : /* DFAX1099 */
                        break;
                     default:
                        fax_format = 2;
                        break;
                  }
               }
               else
               {
                  fax_format = 2;
               }
            }

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
               if (fax_format == 0)
               {
                  size = tiff2gts(file_path, p_file_name);
               }
               else
               {
                  size = fax2gts(file_path, p_file_name, fax_format);
               }
               if (size <= 0)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to unlink() file `%s' : %s",
                                    fullname, strerror(errno));
                     }
                  }
                  else
                  {
#ifdef _DELETE_LOG
                     size_t dl_real_size;
#endif

                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Removing corrupt file `%s'", p_file_name);
                     recount_files_var = YES;
#ifdef _PRODUCTION_LOG
                     production_log(creation_time, 1, 0, unique_number,
                                    split_job_counter, db[position].job_id,
                                    db[position].dir_id, "%s%c%c%c-1%c%s",
                                    p_file_name, SEPARATOR_CHAR,
                                    SEPARATOR_CHAR, SEPARATOR_CHAR,
                                    SEPARATOR_CHAR,
                                    (fax_format == 0) ? TIFF2GTS_ID : FAX2GTS_ID);
#endif
#ifdef _DELETE_LOG
                     (void)strcpy(dl.file_name, p_file_name);
                     (void)sprintf(dl.host_name, "%-*s %03x",
                                   MAX_HOSTNAME_LENGTH, db[position].host_alias,
                                   CONVERSION_FAILED);
                     *dl.file_size = file_size_pool[j];
                     *dl.job_id = db[position].job_id;
                     *dl.dir_id = db[position].dir_id;
                     *dl.input_time = creation_time;
                     *dl.split_job_counter = split_job_counter;
                     *dl.unique_number = unique_number;
                     *dl.file_name_length = strlen(p_file_name);
                     dl_real_size = *dl.file_name_length + dl.size +
                                    sprintf((dl.file_name + *dl.file_name_length + 1),
                                            "%s", DIR_CHECK);
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "write() error : %s",
                                   strerror(errno));
                     }
#endif
                  }
               }
               else
               {
                  *file_size += size;
#ifdef _PRODUCTION_LOG
                  production_log(creation_time, 1, 1, unique_number,
                                 split_job_counter, db[position].job_id,
# if SIZEOF_OFF_T == 4
                                 db[position].dir_id, "%s%c%s%c%lx%c0%c%s",
# else
                                 db[position].dir_id, "%s%c%s%c%llx%c0%c%s",
# endif
                                 p_file_name, SEPARATOR_CHAR, p_file_name,
                                 SEPARATOR_CHAR, (pri_off_t)size,
                                 SEPARATOR_CHAR, SEPARATOR_CHAR,
                                 (fax_format == 0) ? TIFF2GTS_ID : FAX2GTS_ID);
#endif
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files_var == YES)
            {
#ifdef _WITH_PTHREAD
               *files_to_send = recount_files(file_path, file_size);
#else
               *files_to_send = restore_files(file_path, file_size);
#endif
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want to convert GTS T4 encoded files to TIFF files. */
      if ((db[position].loptions_flag & GTS2TIFF_ID_FLAG) &&
          (CHECK_STRCMP(options, GTS2TIFF_ID) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  recount_files_var = NO;
#ifdef _PRODUCTION_LOG
            char orig_file_name[MAX_FILENAME_LENGTH];
#endif

            *file_size = 0;

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
#ifdef _PRODUCTION_LOG
               (void)strcpy(orig_file_name, p_file_name);
#endif
               if ((size = gts2tiff(file_path, p_file_name)) < 0)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to unlink() file `%s' : %s",
                                    fullname, strerror(errno));
                     }
                  }
                  else
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Removing corrupt file `%s'", p_file_name);
                     recount_files_var = YES;
#ifdef _PRODUCTION_LOG
                     production_log(creation_time, 1, 0, unique_number,
                                    split_job_counter, db[position].job_id,
                                    db[position].dir_id,
                                    "%s%c%c%c-1%c%s",
                                    orig_file_name, SEPARATOR_CHAR,
                                    SEPARATOR_CHAR, SEPARATOR_CHAR,
                                    SEPARATOR_CHAR, GTS2TIFF_ID);
#endif
                  }
               }
               else
               {
                  *file_size += size;
#ifdef _PRODUCTION_LOG
                  production_log(creation_time, 1, 1, unique_number,
                                 split_job_counter, db[position].job_id,
                                 db[position].dir_id,
# if SIZEOF_OFF_T == 4
                                 "%s%c%s%c%lx%c0%c%s",
# else
                                 "%s%c%s%c%llx%c0%c%s",
# endif
                                 orig_file_name, SEPARATOR_CHAR, p_file_name,
                                 SEPARATOR_CHAR, (pri_off_t)size,
                                 SEPARATOR_CHAR, SEPARATOR_CHAR, GTS2TIFF_ID);
#endif
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files_var == YES)
            {
#ifdef _WITH_PTHREAD
               *files_to_send = recount_files(file_path, file_size);
#else
               *files_to_send = restore_files(file_path, file_size);
#endif
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want to convert GRIB files to WMO files. */
      if ((db[position].loptions_flag & GRIB2WMO_ID_FLAG) &&
          (CHECK_STRNCMP(options, GRIB2WMO_ID, GRIB2WMO_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  recount_files_var = NO;
            char cccc[4],
                 *p_cccc;

#ifdef _PRODUCTION_LOG
            p_option = options;
#endif
            if ((*(options + GRIB2WMO_ID_LENGTH) == ' ') ||
                (*(options + GRIB2WMO_ID_LENGTH) == '\t'))
            {
               ptr = options + GRIB2WMO_ID_LENGTH;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               j = 0;
               while ((isalpha((int)(*ptr))) && (j < 4))
               {
                  cccc[j] = *ptr;
                  j++; ptr++;
               }
               if (j == 4)
               {
                  p_cccc = cccc;
               }
               else             
               {
                  p_cccc = NULL;
               }
            }                
            else
            {
               p_cccc = NULL;
            }
            *file_size = 0;

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
               size = 0;
               (void)convert_grib2wmo(fullname, &size, p_cccc);
               if (size == 0)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to unlink() file `%s' : %s",
                                    fullname, strerror(errno));
                     }
                  }
                  else
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Unable to convert, removed file `%s'", p_file_name);
                     recount_files_var = YES;
#ifdef _PRODUCTION_LOG
                     production_log(creation_time, 1, 0, unique_number,
                                    split_job_counter, db[position].job_id,
                                    db[position].dir_id,
                                    "%s%c%c%c-1%c%s",
                                    p_file_name, SEPARATOR_CHAR,
                                    SEPARATOR_CHAR, SEPARATOR_CHAR,
                                    SEPARATOR_CHAR, p_option);
#endif
                  }
               }
               else
               {
                  *file_size += size;
#ifdef _PRODUCTION_LOG
                  production_log(creation_time, 1, 1, unique_number,
                                 split_job_counter, db[position].job_id,
                                 db[position].dir_id,
# if SIZEOF_OFF_T == 4
                                 "%s%c%s%c%lx%c0%c%s",
# else
                                 "%s%c%s%c%llx%c0%c%s",
# endif
                                 p_file_name, SEPARATOR_CHAR, p_file_name,
                                 SEPARATOR_CHAR, (pri_off_t)size,
                                 SEPARATOR_CHAR, SEPARATOR_CHAR, p_option);
#endif
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files_var == YES)
            {
#ifdef _WITH_PTHREAD
               *files_to_send = recount_files(file_path, file_size);
#else
               *files_to_send = restore_files(file_path, file_size);
#endif
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want extract bulletins from a file. */
      if ((db[position].loptions_flag & EXTRACT_ID_FLAG) &&
          (CHECK_STRNCMP(options, EXTRACT_ID, EXTRACT_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0,
#else
         int file_counter = *files_to_send,
#endif
              extract_options = DEFAULT_EXTRACT_OPTIONS,
              extract_typ;
         char *p_extract_id,
              *p_filter;

#ifdef _PRODUCTION_LOG
         p_option = options;
#endif
         p_extract_id = options + EXTRACT_ID_LENGTH + 1;

         if (*p_extract_id == '-')
         {
            do
            {
               switch (*(p_extract_id + 1))
               {
                  case 'b' : /* Extract reports. */
                     if ((extract_options & EXTRACT_REPORTS) == 0)
                     {
                        extract_options |= EXTRACT_REPORTS;
                     }
                     break;

                  case 'B' : /* Do not extract reports. */
                     if (extract_options & EXTRACT_REPORTS)
                     {
                        extract_options ^= EXTRACT_REPORTS;
                     }
                     break;

                  case 'c' : /* Add CRC checksum. */
                     if ((extract_options & EXTRACT_ADD_CRC_CHECKSUM) == 0)
                     {
                        extract_options |= EXTRACT_ADD_CRC_CHECKSUM;
                     }
                     break;

                  case 'C' : /* Do not add CRC checksum. */
                     if (extract_options & EXTRACT_ADD_CRC_CHECKSUM)
                     {
                        extract_options ^= EXTRACT_ADD_CRC_CHECKSUM;
                     }
                     break;

                  case 'n' : /* Add unique number. */
                     if ((extract_options & EXTRACT_ADD_UNIQUE_NUMBER) == 0)
                     {
                        extract_options |= EXTRACT_ADD_UNIQUE_NUMBER;
                     }
                     break;

                  case 'H' : /* Remove WMO header. */
                     extract_options |= EXTRACT_REMOVE_WMO_HEADER;
                     break;

                  case 'N' : /* Do not add unique number. */
                     if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
                     {
                        extract_options ^= EXTRACT_ADD_UNIQUE_NUMBER;
                     }
                     break;

                  case 's' : /* Add SOH and ETX. */
                     if ((extract_options & EXTRACT_ADD_SOH_ETX) == 0)
                     {
                        extract_options |= EXTRACT_ADD_SOH_ETX;
                     }
                     break;

                  case 'S' : /* Remove SOH and ETX. */
                     if (extract_options & EXTRACT_ADD_SOH_ETX)
                     {
                        extract_options ^= EXTRACT_ADD_SOH_ETX;
                     }
                     break;

                  default  : /* Unknown. */
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Unknown extract option -%c", *(p_extract_id + 1));
                     break;
               }
               p_extract_id += 2;
               while (*p_extract_id == ' ')
               {
                  p_extract_id++;
               }
            } while (*p_extract_id == '-');
         }
         if ((*p_extract_id == 'V') && (*(p_extract_id + 1) == 'A') &&
             (*(p_extract_id + 2) == 'X') &&
             ((*(p_extract_id + 3) == ' ') || (*(p_extract_id + 3) == '\0')))
         {
            extract_typ = TWO_BYTE;
            p_extract_id += 3;
         }
         else if ((*p_extract_id == 'L') && (*(p_extract_id + 1) == 'B') &&
                  (*(p_extract_id + 2) == 'F') &&
                  ((*(p_extract_id + 3) == ' ') || (*(p_extract_id + 3) == '\0')))
              {
                 extract_typ = FOUR_BYTE_LBF;
                 p_extract_id += 3;
              }
         else if ((*p_extract_id == 'H') && (*(p_extract_id + 1) == 'B') &&
                  (*(p_extract_id + 2) == 'F') &&
                  ((*(p_extract_id + 3) == ' ') || (*(p_extract_id + 3) == '\0')))
              {
                 extract_typ = FOUR_BYTE_HBF;
                 p_extract_id += 3;
              }
         else if ((*p_extract_id == 'M') && (*(p_extract_id + 1) == 'S') &&
                  (*(p_extract_id + 2) == 'S') &&
                  ((*(p_extract_id + 3) == ' ') || (*(p_extract_id + 3) == '\0')))
              {
                 extract_typ = FOUR_BYTE_MSS;
                 p_extract_id += 3;
              }
         else if ((*p_extract_id == 'M') && (*(p_extract_id + 1) == 'R') &&
                  (*(p_extract_id + 2) == 'Z') &&
                  ((*(p_extract_id + 3) == ' ') || (*(p_extract_id + 3) == '\0')))
              {
                 extract_typ = FOUR_BYTE_MRZ;
                 p_extract_id += 3;
              }
         else if ((*p_extract_id == 'G') && (*(p_extract_id + 1) == 'R') &&
                  (*(p_extract_id + 2) == 'I') &&
                  (*(p_extract_id + 3) == 'B') &&
                  ((*(p_extract_id + 4) == ' ') || (*(p_extract_id + 4) == '\0')))
              {
                 extract_typ = FOUR_BYTE_GRIB;
                 p_extract_id += 4;
              }
         else if ((*p_extract_id == 'W') && (*(p_extract_id + 1) == 'M') &&
                  (*(p_extract_id + 2) == 'O') &&
                  ((*(p_extract_id + 3) == ' ') || (*(p_extract_id + 3) == '\0')))
              {
                 extract_typ = WMO_STANDARD;
                 p_extract_id += 3;
              }
         else if ((*p_extract_id == 'A') && (*(p_extract_id + 1) == 'S') &&
                  (*(p_extract_id + 2) == 'C') &&
                  (*(p_extract_id + 3) == 'I') &&
                  (*(p_extract_id + 4) == 'I') &&
                  ((*(p_extract_id + 5) == ' ') || (*(p_extract_id + 5) == '\0')))
              {
                 extract_typ = ASCII_STANDARD;
                 p_extract_id += 5;
              }
         else if ((*p_extract_id == 'Z') && (*(p_extract_id + 1) == 'C') &&
                  (*(p_extract_id + 2) == 'Z') &&
                  (*(p_extract_id + 3) == 'C') &&
                  ((*(p_extract_id + 4) == ' ') || (*(p_extract_id + 4) == '\0')))
              {
                 extract_typ = ZCZC_NNNN;
                 p_extract_id += 4;
              }
         else if (*(p_extract_id - 1) == '\0')
              {
                 /* To stay compatible to Version 0.8.x */
                 extract_typ = FOUR_BYTE_MRZ;
                 p_extract_id -= 1;
              }
              else
              {
                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                             "Unknown extract ID (%s) in DIR_CONFIG file.",
                             p_extract_id);
                 NEXT(options);
                 continue;
              }

          while (*p_extract_id == ' ')
          {
             p_extract_id++;
          }
          if (*p_extract_id != '\0')
          {
             p_filter = p_extract_id;
          }
          else
          {
             p_filter = NULL;
          }

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            /*
             * Hopefully we have read all file names! Now it is save
             * to chop the files up.
             */
            if ((extract_typ == FOUR_BYTE_MRZ) ||
                (extract_typ == FOUR_BYTE_GRIB))
            {
               for (j = 0; j < file_counter; j++)
               {
                  (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
                  if (bin_file_chopper(fullname, files_to_send, file_size,
                                       p_filter,
#ifdef _PRODUCTION_LOG
                                       (extract_typ == FOUR_BYTE_MRZ) ? NO : YES,
                                       creation_time, unique_number,
                                       split_job_counter, db[position].job_id,
                                       db[position].dir_id, p_option,
                                       p_file_name) < 0)
#else
                                       (extract_typ == FOUR_BYTE_MRZ) ? NO : YES) < 0)
#endif
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "An error occurred when extracting bulletins from file `%s', deleting file!",
                                 fullname);

                     if (unlink(fullname) == -1)
                     {
                        if (errno != ENOENT)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() file `%s' : %s",
                                       fullname, strerror(errno));
                        }
                     }
                     else
                     {
                        struct stat stat_buf;

                        if (stat(fullname, &stat_buf) == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Can't access file `%s' : %s",
                                       fullname, strerror(errno));
                        }
                        else
                        {
                           *file_size = *file_size - stat_buf.st_size;
                        }
                        (*files_to_send)--;
                     }
                  }
                  p_file_name += MAX_FILENAME_LENGTH;
               }
            }
            else
            {
               for (j = 0; j < file_counter; j++)
               {
                  (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
                  if (extract(p_file_name, file_path, p_filter,
#ifdef _PRODUCTION_LOG
                              creation_time, unique_number, split_job_counter,
                              db[position].job_id, db[position].dir_id,
                              p_option,
#endif
                              extract_typ, extract_options,
                              files_to_send, file_size) < 0)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "An error occurred when extracting bulletins from file `%s', deleting file!",
                                 fullname);

                     if (unlink(fullname) == -1)
                     {
                        if (errno != ENOENT)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() file `%s' : %s",
                                       fullname, strerror(errno));
                        }
                     }
                     else
                     {
                        struct stat stat_buf;

                        if (stat(fullname, &stat_buf) == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Can't access file `%s' : %s",
                                       fullname, strerror(errno));
                        }
                        else
                        {
                           *file_size = *file_size - stat_buf.st_size;
                        }
                        (*files_to_send)--;
                     }
                  }
                  p_file_name += MAX_FILENAME_LENGTH;
               }
            }
#ifdef _WITH_PTHREAD
            if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
            {
               *files_to_send = recount_files(file_path, file_size);
            }
#else
            *files_to_send = restore_files(file_path, file_size);
#endif
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want assemble bulletins to a file. */
      if ((db[position].loptions_flag & ASSEMBLE_ID_FLAG) &&
          (CHECK_STRNCMP(options, ASSEMBLE_ID, ASSEMBLE_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0,
#else
         int file_counter = *files_to_send,
#endif
              assemble_typ;
         char *p_assemble_id,
              assembled_name[MAX_FILENAME_LENGTH];

#ifdef _PRODUCTION_LOG
         p_option = options;
#endif
         p_assemble_id = options + ASSEMBLE_ID_LENGTH + 1;
         if ((*p_assemble_id == 'V') && (*(p_assemble_id + 1) == 'A') &&
             (*(p_assemble_id + 2) == 'X'))
         {
            assemble_typ = TWO_BYTE;
         }
         else if ((*p_assemble_id == 'L') && (*(p_assemble_id + 1) == 'B') &&
                  (*(p_assemble_id + 2) == 'F'))
              {
                 assemble_typ = FOUR_BYTE_LBF;
              }
         else if ((*p_assemble_id == 'H') && (*(p_assemble_id + 1) == 'B') &&
                  (*(p_assemble_id + 2) == 'F'))
              {
                 assemble_typ = FOUR_BYTE_HBF;
              }
         else if ((*p_assemble_id == 'D') && (*(p_assemble_id + 1) == 'W') &&
                  (*(p_assemble_id + 2) == 'D'))
              {
                 assemble_typ = FOUR_BYTE_DWD;
              }
         else if ((*p_assemble_id == 'A') && (*(p_assemble_id + 1) == 'S') &&
                  (*(p_assemble_id + 2) == 'C') &&
                  (*(p_assemble_id + 3) == 'I') &&
                  (*(p_assemble_id + 4) == 'I'))
              {
                 assemble_typ = ASCII_STANDARD;
              }
         else if ((*p_assemble_id == 'M') && (*(p_assemble_id + 1) == 'S') &&
                  (*(p_assemble_id + 2) == 'S'))
              {
                 assemble_typ = FOUR_BYTE_MSS;
              }
         else if ((*p_assemble_id == 'W') && (*(p_assemble_id + 1) == 'M') &&
                  (*(p_assemble_id + 2) == 'O'))
              {
                 assemble_typ = WMO_STANDARD;
              }
              else
              {
                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                             "Unknown assemble ID (%s) in DIR_CONFIG file.",
                             p_assemble_id);
                 NEXT(options);
                 continue;
              }

         /*
          * Now we need to get the rule for creating the file name of
          * the assembled file.
          */
         while ((*p_assemble_id != ' ') && (*p_assemble_id != '\t') &&
                (*p_assemble_id != '\0'))
         {
            p_assemble_id++;
         }
         if ((*p_assemble_id == ' ') || (*p_assemble_id == '\t'))
         {
            while ((*p_assemble_id == ' ') || (*p_assemble_id == '\t'))
            {
               p_assemble_id++;
            }
            if (*p_assemble_id != '\0')
            {
               int  ii = 0;
               char assemble_rule[MAX_FILENAME_LENGTH];

               while ((*p_assemble_id != '\0') && (*p_assemble_id != ' ') &&
                      (*p_assemble_id != '\t'))
               {
                  assemble_rule[ii] = *p_assemble_id;
                  p_assemble_id++; ii++;
               }
               assemble_rule[ii] = '\0';

               create_assembled_name(assembled_name, assemble_rule);
            }
         }
         else
         {
            (void)strcpy(assembled_name, "no_file_name");
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "No file name set for assemble option in DIR_CONFIG file, set to <no_file_name>.");
         }

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path,
                                            &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
#ifdef _PRODUCTION_LOG
            size = *file_size;
#endif
            (void)sprintf(fullname, "%s/%s", file_path, assembled_name);
            if (assemble(file_path, p_file_name, file_counter,
                         fullname, assemble_typ, unique_number,
                         files_to_send, file_size) < 0)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "An error occurred when assembling bulletins!");
            }
            else
            {
#ifdef _PRODUCTION_LOG
               int  ii;

               ptr = p_file_name;
               for (ii = 0; ii < file_counter; ii++)
               {
                  production_log(creation_time, file_counter, 1, unique_number,
                                 split_job_counter, db[position].job_id,
                                 db[position].dir_id,
# if SIZEOF_OFF_T == 4
                                 "%s%c%s%c%lx%c0%c%s",
# else
                                 "%s%c%s%c%llx%c0%c%s",
# endif
                                 ptr, SEPARATOR_CHAR, assembled_name,
                                 SEPARATOR_CHAR, (pri_off_t)(*file_size - size),
                                 SEPARATOR_CHAR, SEPARATOR_CHAR, p_option);
                  ptr += MAX_FILENAME_LENGTH;
               }
#endif
#ifndef _WITH_PTHREAD
               *files_to_send = restore_files(file_path, file_size);
#endif
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want convert a file from one format to another. */
      if ((db[position].loptions_flag & CONVERT_ID_FLAG) &&
          (CHECK_STRNCMP(options, CONVERT_ID, CONVERT_ID_LENGTH) == 0))
      {
#ifdef _WITH_PTHREAD
         int file_counter = 0;
                                                                    
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  convert_type,
                 ret;
            char *p_convert_id;

#ifdef _PRODUCTION_LOG
            p_option = options;
#endif
            p_convert_id = options + CONVERT_ID_LENGTH + 1;
            if ((*p_convert_id == 's') && (*(p_convert_id + 1) == 'o') &&
                (*(p_convert_id + 2) == 'h') && (*(p_convert_id + 3) == 'e') &&
                (*(p_convert_id + 4) == 't') && (*(p_convert_id + 5) == 'x') &&
                (*(p_convert_id + 6) == '2') && (*(p_convert_id + 7) == 'w') &&
                (*(p_convert_id + 8) == 'm') && (*(p_convert_id + 9) == 'o') &&
                (*(p_convert_id + 10) == '0'))
            {
               convert_type = SOHETX2WMO0;
            }
            else if ((*p_convert_id == 's') && (*(p_convert_id + 1) == 'o') &&
                     (*(p_convert_id + 2) == 'h') &&
                     (*(p_convert_id + 3) == 'e') &&
                     (*(p_convert_id + 4) == 't') &&
                     (*(p_convert_id + 5) == 'x') &&
                     (*(p_convert_id + 6) == '2') &&
                     (*(p_convert_id + 7) == 'w') &&
                     (*(p_convert_id + 8) == 'm') &&
                     (*(p_convert_id + 9) == 'o') &&
                     (*(p_convert_id + 10) == '1'))
                 {
                    convert_type = SOHETX2WMO1;
                 }
            else if ((*p_convert_id == 's') && (*(p_convert_id + 1) == 'o') &&
                     (*(p_convert_id + 2) == 'h') &&
                     (*(p_convert_id + 3) == 'e') &&
                     (*(p_convert_id + 4) == 't') &&
                     (*(p_convert_id + 5) == 'x') &&
                     (*(p_convert_id + 6) == '\0'))
                 {
                    convert_type = SOHETX;
                 }
            else if ((*p_convert_id == 's') && (*(p_convert_id + 1) == 'o') &&
                     (*(p_convert_id + 2) == 'h') &&
                     (*(p_convert_id + 3) == 'e') &&
                     (*(p_convert_id + 4) == 't') &&
                     (*(p_convert_id + 5) == 'x') &&
                     (*(p_convert_id + 6) == 'w') &&
                     (*(p_convert_id + 7) == 'm') &&
                     (*(p_convert_id + 8) == 'o'))
                 {
                    convert_type = SOHETXWMO;
                 }
            else if ((*p_convert_id == 'w') && (*(p_convert_id + 1) == 'm') &&
                     (*(p_convert_id + 2) == 'o'))
                 {
                    convert_type = ONLY_WMO;
                 }
            else if ((*p_convert_id == 'm') && (*(p_convert_id + 1) == 'r') &&
                     (*(p_convert_id + 2) == 'z') &&
                     (*(p_convert_id + 3) == '2') &&
                     (*(p_convert_id + 4) == 'w') &&
                     (*(p_convert_id + 5) == 'm') &&
                     (*(p_convert_id + 6) == 'o'))
                 {
                    convert_type = MRZ2WMO;
                 }
            else if ((*p_convert_id == 'd') && (*(p_convert_id + 1) == 'o') &&
                     (*(p_convert_id + 2) == 's') &&
                     (*(p_convert_id + 3) == '2') &&
                     (*(p_convert_id + 4) == 'u') &&
                     (*(p_convert_id + 5) == 'n') &&
                     (*(p_convert_id + 6) == 'i') &&
                     (*(p_convert_id + 7) == 'x'))
                 {
                    convert_type = DOS2UNIX;
                 }
            else if ((*p_convert_id == 'u') && (*(p_convert_id + 1) == 'n') &&
                     (*(p_convert_id + 2) == 'i') &&
                     (*(p_convert_id + 3) == 'x') &&
                     (*(p_convert_id + 4) == '2') &&
                     (*(p_convert_id + 5) == 'd') &&
                     (*(p_convert_id + 6) == 'o') &&
                     (*(p_convert_id + 7) == 's'))
                 {
                    convert_type = UNIX2DOS;
                 }
            else if ((*p_convert_id == 'l') && (*(p_convert_id + 1) == 'f') &&
                     (*(p_convert_id + 2) == '2') &&
                     (*(p_convert_id + 3) == 'c') &&
                     (*(p_convert_id + 4) == 'r') &&
                     (*(p_convert_id + 5) == 'c') &&
                     (*(p_convert_id + 6) == 'r') &&
                     (*(p_convert_id + 7) == 'l') &&
                     (*(p_convert_id + 8) == 'f'))
                 {
                    convert_type = LF2CRCRLF;
                 }
            else if ((*p_convert_id == 'c') && (*(p_convert_id + 1) == 'r') &&
                     (*(p_convert_id + 2) == 'c') &&
                     (*(p_convert_id + 3) == 'r') &&
                     (*(p_convert_id + 4) == 'l') &&
                     (*(p_convert_id + 5) == 'f') &&
                     (*(p_convert_id + 6) == '2') &&
                     (*(p_convert_id + 7) == 'l') &&
                     (*(p_convert_id + 8) == 'f'))
                 {
                    convert_type = CRCRLF2LF;
                 }
                 else
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "Unknown convert ID (%s) in DIR_CONFIG file.",
                                p_convert_id);
                    NEXT(options);
                    continue;
                 }
            for (j = 0; j < file_counter; j++)
            {
#ifdef _PRODUCTION_LOG
               size = *file_size;
#endif
               ret = convert(file_path, p_file_name, convert_type, file_size);
               if (ret < 0)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Unable to convert file %s", p_file_name);
               }
#ifdef _PRODUCTION_LOG
               production_log(creation_time, 1, 1, unique_number,
                              split_job_counter, db[position].job_id,
                              db[position].dir_id,
# if SIZEOF_OFF_T == 4
                              "%s%c%s%c%lx%c%d%c%s",
# else
                              "%s%c%s%c%llx%c%d%c%s",
# endif
                              p_file_name, SEPARATOR_CHAR, p_file_name,
                              SEPARATOR_CHAR, (pri_off_t)(*file_size - size),
                              SEPARATOR_CHAR, ret, SEPARATOR_CHAR, p_option);
#endif
               p_file_name += MAX_FILENAME_LENGTH;
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want to convert WMO to ASCII. */
      if ((db[position].loptions_flag & WMO2ASCII_ID_FLAG) &&
          (CHECK_STRCMP(options, WMO2ASCII_ID) == 0))
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int recount_files_var = NO;

            *file_size = 0;

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               if ((wmo2ascii(file_path, p_file_name, &size)) < 0)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "wmo2ascii(): Removing corrupt file `%s'",
                              p_file_name);
                  recount_files_var = YES;
#ifdef _PRODUCTION_LOG
                  production_log(creation_time, 1, 0, unique_number,
                                 split_job_counter, db[position].job_id,
                                 db[position].dir_id,
                                 "%s%c%c%c-1%c%s",
                                 p_file_name, SEPARATOR_CHAR, SEPARATOR_CHAR,
                                 SEPARATOR_CHAR, SEPARATOR_CHAR, WMO2ASCII_ID);
#endif
               }
               else
               {
                  *file_size += size;
#ifdef _PRODUCTION_LOG
                  production_log(creation_time, 1, 1, unique_number,
                                 split_job_counter, db[position].job_id,
                                 db[position].dir_id,
# if SIZEOF_OFF_T == 4
                                 "%s%c%s%c%lx%c0%c%s",
# else
                                 "%s%c%s%c%llx%c0%c%s",
# endif
                                 p_file_name, SEPARATOR_CHAR, p_file_name,
                                 SEPARATOR_CHAR, (pri_off_t)size,
                                 SEPARATOR_CHAR, SEPARATOR_CHAR, WMO2ASCII_ID);
#endif
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files_var == YES)
            {
#ifdef _WITH_PTHREAD
               *files_to_send = recount_files(file_path, file_size);
#else
               *files_to_send = restore_files(file_path, file_size);
#endif
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /*
       * If we do not find any action for this option, lets simply
       * ignore it.
       */
      NEXT(options);
   }

   return(0);
}


/*+++++++++++++++++++++++++++++ rename_ow() +++++++++++++++++++++++++++++*/
static void
rename_ow(int          overwrite,
          int          file_counter,
          char         *new_name_buffer,
          off_t        *file_size,
#ifdef _DELETE_LOG
          time_t       creation_time,
          unsigned int unique_number,
          unsigned int split_job_counter,
          int          position,
#endif
          char         *newname,
          char         *p_newname,
          char         *oldname,
          char         *p_file_name)
{
   int   gotcha,
         i,
         rename_overwrite;
   char  *p_new_name_buffer;
   off_t rename_overwrite_size;

   if (overwrite == NO)
   {
      int  dup_count = 0;
      char *p_end = NULL;

      gotcha = YES;
      while ((dup_count < 1000) && (gotcha == YES))
      {
         gotcha = NO;
         p_new_name_buffer = new_name_buffer;
         for (i = 0; i < file_counter; i++)
         {
            if (CHECK_STRCMP(p_new_name_buffer, p_newname) == 0)
            {
               gotcha = YES;
               break;
            }
            p_new_name_buffer += MAX_FILENAME_LENGTH;
         }
         if (gotcha == YES)
         {
            if (p_end == NULL)
            {
               p_end = p_newname + strlen(p_newname);
            }
            (void)sprintf(p_end, "-%d", dup_count);
            dup_count++;
         }
      }

      if ((dup_count >= 1000) && (gotcha == YES))
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to add a unique number to filename (%s) to avoid rename overwritting a source file because we retried 1000 times.",
                     p_newname);
         *p_new_name_buffer = '\0';
         rename_overwrite = YES;
      }
      else
      {
         rename_overwrite = NO;
      }
   }
   else
   {
      gotcha = NO;
      p_new_name_buffer = new_name_buffer;
      for (i = 0; i < file_counter; i++)
      {
         if (CHECK_STRCMP(p_new_name_buffer, p_newname) == 0)
         {
            gotcha = YES;
            *p_new_name_buffer = '\0';
            break;
         }
         p_new_name_buffer += MAX_FILENAME_LENGTH;
      }
      if (gotcha == YES)
      {
         rename_overwrite = YES;
      }
      else
      {
         rename_overwrite = NO;
      }
   }

   if (rename_overwrite == YES)
   {
      struct stat stat_buf;

      if (stat(newname, &stat_buf) != -1)
      {
         rename_overwrite_size = stat_buf.st_size;
      }
      else
      {
         rename_overwrite_size = 0;
      }
   }
   if (rename(oldname, newname) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  "Failed to rename() `%s' to `%s' : %s",
                  oldname, newname, strerror(errno));
   }
   else
   {
      if (rename_overwrite == YES)
      {
#ifdef _DELETE_LOG
         size_t dl_real_size;
#endif

         *file_size -= rename_overwrite_size;
#ifdef _DELETE_LOG
         (void)strcpy(dl.file_name, p_newname);
         (void)sprintf(dl.host_name, "%-*s %03x", MAX_HOSTNAME_LENGTH,
                       db[position].host_alias, RENAME_OVERWRITE);
         *dl.file_size = rename_overwrite_size;
         *dl.job_id = db[position].job_id;
         *dl.dir_id = db[position].dir_id;
         *dl.input_time = creation_time;
         *dl.split_job_counter = split_job_counter;
         *dl.unique_number = unique_number;
         *dl.file_name_length = strlen(p_newname);
         dl_real_size = *dl.file_name_length + dl.size +
                        sprintf((dl.file_name + *dl.file_name_length + 1),
                                "%s", DIR_CHECK);
         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
#endif
      }
      (void)strcpy(new_name_buffer + (p_file_name - file_name_buffer),
                   p_newname);
   }

   return;
}


/*++++++++++++++++++++++++++ prepare_rename_ow() ++++++++++++++++++++++++*/
static void
prepare_rename_ow(int file_counter, char **new_name_buffer)
{
   size_t new_size = file_counter * MAX_FILENAME_LENGTH;

   if ((*new_name_buffer = malloc(new_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not malloc() memory [%d bytes] : %s",
                 new_size, strerror(errno));
      exit(INCORRECT);
   }
   (void)memcpy(*new_name_buffer, file_name_buffer, new_size);

   return;
}


/*++++++++++++++++++++++++++ cleanup_rename_ow() ++++++++++++++++++++++++*/
static int
cleanup_rename_ow(int          file_counter,
#ifdef _PRODUCTION_LOG
                  int          position,
                  time_t       creation_time,
                  unsigned int unique_number,
                  unsigned int split_job_counter,
                  char         *p_option,
#endif
                  char         **new_name_buffer)
{
   int  files_deleted = 0,
        files_to_send,
#ifdef _PRODUCTION_LOG
        j,
#endif
        i;
   char *p_file_name,
        *p_new_name;

#ifdef _PRODUCTION_LOG
   p_new_name = *new_name_buffer;
   p_file_name = file_name_buffer;
   for (i = 0; i < file_counter; i++)
   {
      if (*p_new_name == '\0')
      {
         j = 0;
         files_deleted++;
      }
      else
      {
         j = 1;
      }
      production_log(creation_time, 1, j, unique_number, split_job_counter,
                     db[position].job_id, db[position].dir_id,
                     "%s%c%s%c%c0%c%s",
                     p_file_name, SEPARATOR_CHAR, p_new_name, SEPARATOR_CHAR,
                     SEPARATOR_CHAR, SEPARATOR_CHAR, p_option);
      p_new_name += MAX_FILENAME_LENGTH;
      p_file_name += MAX_FILENAME_LENGTH;
   }
#else
   for (i = 0; i < file_counter; i++)
   {
      if (*p_new_name == '\0')
      {
         files_deleted++;
      }
   }
#endif

   if (files_deleted)
   {
      size_t new_size;

      free(file_name_buffer);
      files_to_send = file_counter - files_deleted;
      new_size = ((files_to_send / FILE_NAME_STEP_SIZE) + 1) * FILE_NAME_STEP_SIZE *
                 MAX_FILENAME_LENGTH;
      if ((file_name_buffer = malloc(new_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not malloc() memory [%d bytes] : %s",
                    new_size, strerror(errno));
         exit(INCORRECT);
      }
      p_new_name = *new_name_buffer;
      p_file_name = file_name_buffer;
      for (i = 0; i < file_counter; i++)
      {
         if (*p_new_name != '\0')
         {
            (void)strcpy(p_file_name, p_new_name);
            p_file_name += MAX_FILENAME_LENGTH;
         }
         p_new_name += MAX_FILENAME_LENGTH;
      }
   }
   else
   {
      files_to_send = file_counter;
      p_new_name = *new_name_buffer;
      p_file_name = file_name_buffer;
      for (i = 0; i < file_counter; i++)
      {
         (void)strcpy(p_file_name, p_new_name);
         p_file_name += MAX_FILENAME_LENGTH;
         p_new_name += MAX_FILENAME_LENGTH;
      }
   }
   free(*new_name_buffer);
   *new_name_buffer = NULL;

   return(files_to_send);
}


#if defined (_WITH_PTHREAD) || !defined (_PRODUCTION_LOG)
/*+++++++++++++++++++++++++++ recount_files() +++++++++++++++++++++++++++*/
static int
recount_files(char *file_path, off_t *file_size)
{
   int file_counter = 0;
   DIR *dp;

   *file_size = 0;
   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Can't access directory `%s' : %s",
                 file_path, strerror(errno));
   }
   else
   {
      char          *ptr,
                    fullname[MAX_PATH_LENGTH];
      struct stat   stat_buf;
      struct dirent *p_dir;

      (void)strcpy(fullname, file_path);
      ptr = fullname + strlen(fullname);
      *ptr++ = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
             ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
             (p_dir->d_name[2] == '\0')))
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Can't access file `%s' : %s",
                       fullname, strerror(errno));
         }
         else
         {
            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               *file_size += stat_buf.st_size;
               file_counter++;
            }
            else if (S_ISDIR(stat_buf.st_mode))
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "Currently unable to handle directories in job directories. Removing `%s'.",
                                fullname);
                    (void)rec_rmdir(fullname);
                 }
         }
         errno = 0;
      }
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s",
                    file_path, strerror(errno));
      }

      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s",
                    file_path, strerror(errno));
      }
   }

   return(file_counter);
}
#endif


/*+++++++++++++++++++++++++ delete_all_files() ++++++++++++++++++++++++++*/
static void
delete_all_files(char *file_path,
#ifdef _DELETE_LOG
                 int          position,
                 int          ret,
                 time_t       creation_time,
                 unsigned int unique_number,
                 unsigned int split_job_counter,
#endif
                 int          on_error_delete_all,
                 char         *p_save_file,
                 char         *save_orig_file)
{
   DIR *dp;

   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Can't access directory `%s' : %s",
                 file_path, strerror(errno));
   }
   else
   {
      int           rename_unlink_ret;
      char          *ptr,
                    fullname[MAX_PATH_LENGTH];
      struct stat   stat_buf;
      struct dirent *p_dir;

      (void)strcpy(fullname, file_path);
      ptr = fullname + strlen(fullname);
      *ptr++ = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
             ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
             (p_dir->d_name[2] == '\0')))
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Can't access file `%s' : %s",
                       fullname, strerror(errno));
         }
         else
         {
            if (S_ISDIR(stat_buf.st_mode))
            {
               (void)rec_rmdir(fullname);
            }
            else
            {
               if (p_save_file == NULL)
               {
                  rename_unlink_ret = unlink(fullname);
               }
               else
               {
                  (void)strcpy(p_save_file, p_dir->d_name);
                  if (on_error_delete_all == YES)
                  {
                     rename_unlink_ret = rename(fullname, save_orig_file);
                  }
                  else
                  {
                     rename_unlink_ret = copy_file(fullname, save_orig_file,
                                                   NULL);
                  }
               }
               if (rename_unlink_ret == -1)
               {
                  if (p_save_file == NULL)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Failed to unlink() `%s' : %s",
                                 fullname, strerror(errno));
                  }
                  else
                  {
                     if (on_error_delete_all == YES)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to rename() `%s' to `%s' : %s",
                                    fullname, save_orig_file, strerror(errno));
                     }
                     else
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to copy `%s' to `%s'",
                                    fullname, save_orig_file);
                     }
                  }
               }
#ifdef _DELETE_LOG
               else
               {
                  if (on_error_delete_all == YES)
                  {
                     size_t dl_real_size;

                     (void)strcpy(dl.file_name, p_dir->d_name);
                     (void)sprintf(dl.host_name, "%-*s %03x",
                                   MAX_HOSTNAME_LENGTH, db[position].host_alias,
                                   (p_save_file == NULL) ? EXEC_FAILED_DEL : EXEC_FAILED_STORED);
                     *dl.file_size = stat_buf.st_size;
                     *dl.job_id = db[position].job_id;
                     *dl.dir_id = db[position].dir_id;
                     *dl.input_time = creation_time;
                     *dl.split_job_counter = split_job_counter;
                     *dl.unique_number = unique_number;
                     *dl.file_name_length = strlen(p_dir->d_name);
                     if (p_save_file == NULL)
                     {
                        dl_real_size = *dl.file_name_length + dl.size +
                                       sprintf((dl.file_name + *dl.file_name_length + 1),
                                               "%s%creturn code = %d (%s %d)",
                                               DIR_CHECK, SEPARATOR_CHAR, ret,
                                               __FILE__, __LINE__);
                     }
                     else
                     {
                        dl_real_size = *dl.file_name_length + dl.size +
                                       sprintf((dl.file_name + *dl.file_name_length + 1),
                                               "%s%creturn code = %d (%s)",
                                               DIR_CHECK, SEPARATOR_CHAR, ret,
                                               save_orig_file);
                     }
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "write() error : %s", strerror(errno));
                     }
                  }
               }
#endif
            }
         }
         errno = 0;
      }
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s",
                    file_path, strerror(errno));
      }

      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s",
                    file_path, strerror(errno));
      }
   }

   return;
}


#ifdef _PRODUCTION_LOG
/*+++++++++++++++++++++++++++ check_changes() +++++++++++++++++++++++++++*/
static int
check_changes(time_t       creation_time,
              unsigned int unique_number,
              unsigned int split_job_counter,
              int          position,
              char         *exec_name,
              char         *exec_cmd,
              int          exec_ret,
              int          old_file_counter,
              int          overwrite,
              char         *file_path,
              off_t        *file_size)
{
   int file_counter = 0;
   DIR *dp;

   *file_size = 0;
   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Can't access directory `%s' : %s",
                 file_path, strerror(errno));
   }
   else
   {
      int           gotcha,
                    i,
                    j,
                    log_entries;
      static int    prev_file_counter = 0;
      off_t         *new_file_size_buffer = NULL;
      char          fullname[MAX_PATH_LENGTH],
                    *p_new_file_name,
                    *p_old_file_name,
                    *ptr,
                    *new_file_name_buffer = NULL;
      static char   *old_file_name_buffer = NULL;
      struct stat   stat_buf;
      struct dirent *p_dir;

      p_new_file_name = new_file_name_buffer;

      (void)strcpy(fullname, file_path);
      ptr = fullname + strlen(fullname);
      *ptr++ = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
             ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
             (p_dir->d_name[2] == '\0')))
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Can't access file `%s' : %s",
                       fullname, strerror(errno));
         }
         else
         {
            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               if ((file_counter % FILE_NAME_STEP_SIZE) == 0)
               {
                  int    offset;
                  size_t new_size;

                  /* Calculate new size of file name buffer. */
                  new_size = ((file_counter / FILE_NAME_STEP_SIZE) + 1) *
                             FILE_NAME_STEP_SIZE * MAX_FILENAME_LENGTH;

                  /* Increase the space for the file name buffer. */
                  offset = p_new_file_name - new_file_name_buffer;
                  if ((new_file_name_buffer = realloc(new_file_name_buffer,
                                                      new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory [%d bytes] : %s",
                                new_size, strerror(errno));
                     exit(INCORRECT);
                  }

                  /* After realloc, don't forget to position */
                  /* pointer correctly.                      */
                  p_new_file_name = new_file_name_buffer + offset;

                  /* Calculate new size of file size buffer. */
                  new_size = ((file_counter / FILE_NAME_STEP_SIZE) + 1) *
                             FILE_NAME_STEP_SIZE * sizeof(off_t);

                  /* Increase the space for the file size buffer. */
                  if ((new_file_size_buffer = realloc(new_file_size_buffer,
                                                      new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory [%d bytes] : %s",
                                new_size, strerror(errno));
                     exit(INCORRECT);
                  }
               }
               (void)strcpy(p_new_file_name, p_dir->d_name);
               new_file_size_buffer[file_counter] = stat_buf.st_size;
               p_new_file_name += MAX_FILENAME_LENGTH;
               *file_size += stat_buf.st_size;
               file_counter++;
            }
            else if (S_ISDIR(stat_buf.st_mode))
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "Currently unable to handle directories in job directories. Removing `%s'.",
                                fullname);
                    (void)rec_rmdir(fullname);
                 }
         }
         errno = 0;
      }
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s",
                    file_path, strerror(errno));
      }

      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s",
                    file_path, strerror(errno));
      }

      /*
       * We have the new list. We now compare this with the old list
       * and see what is new so we can log this in the production log.
       */
      if (prev_file_counter == 0)
      {
         prev_file_counter = old_file_counter;
      }

      /* First lets just count the number of log entries. */
      p_new_file_name = new_file_name_buffer;
      log_entries = 0;
      for (i = 0; i < file_counter; i++)
      {
         gotcha = NO;
         if (exec_name != NULL)
         {
            if (old_file_name_buffer == NULL)
            {
               size_t new_size;

               new_size = old_file_counter * MAX_FILENAME_LENGTH;
               if ((old_file_name_buffer = malloc(new_size)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Could not malloc() memory [%d bytes] : %s",
                             new_size, strerror(errno));
                  exit(INCORRECT);
               }
               (void)memcpy(old_file_name_buffer, file_name_buffer, new_size);
            }
            p_old_file_name = old_file_name_buffer;
         }
         else
         {
            p_old_file_name = file_name_buffer;
         }

         for (j = 0; j < prev_file_counter; j++)
         {
            if (CHECK_STRCMP(p_new_file_name, p_old_file_name) == 0)
            {
               gotcha = YES;
               break;
            }
            p_old_file_name += MAX_FILENAME_LENGTH;
         }
         if (gotcha == NO)
         {
            if (exec_name == NULL)
            {
               log_entries += old_file_counter;
            }
            else
            {
               log_entries++;
            }
         }
         p_new_file_name += MAX_FILENAME_LENGTH;
      }

      if (log_entries != 0)
      {
         p_new_file_name = new_file_name_buffer;
         for (i = 0; i < file_counter; i++)
         {
            gotcha = NO;
            if (exec_name != NULL)
            {
               p_old_file_name = old_file_name_buffer;
            }
            else
            {
               p_old_file_name = file_name_buffer;
            }

            for (j = 0; j < prev_file_counter; j++)
            {
               if (CHECK_STRCMP(p_new_file_name, p_old_file_name) == 0)
               {
                  gotcha = YES;
                  break;
               }
               p_old_file_name += MAX_FILENAME_LENGTH;
            }
            if (gotcha == NO)
            {
               if (exec_name == NULL)
               {
                  char *p_tmp_file_name;

                  p_tmp_file_name = file_name_buffer;
                  for (j = 0; j < old_file_counter; j++)
                  {
                     production_log(creation_time, log_entries, file_counter,
                                    unique_number, split_job_counter,
                                    db[position].job_id, db[position].dir_id,
# if SIZEOF_OFF_T == 4
                                    "%s%c%s%c%lx%c%d%c%s",
# else
                                    "%s%c%s%c%llx%c%d%c%s",
# endif
                                    p_tmp_file_name, SEPARATOR_CHAR,
                                    p_new_file_name, SEPARATOR_CHAR,
                                    (pri_off_t)new_file_size_buffer[i],
                                    SEPARATOR_CHAR, exec_ret, SEPARATOR_CHAR,
                                    exec_cmd);
                     p_tmp_file_name += MAX_FILENAME_LENGTH;
                  }
               }
               else
               {
                  production_log(creation_time, 1, log_entries, unique_number,
                                 split_job_counter, db[position].job_id,
# if SIZEOF_OFF_T == 4
                                 db[position].dir_id, "%s%c%s%c%lx%c%d%c%s",
# else
                                 db[position].dir_id, "%s%c%s%c%llx%c%d%c%s",
# endif
                                 exec_name, SEPARATOR_CHAR, p_new_file_name,
                                 SEPARATOR_CHAR, (pri_off_t)new_file_size_buffer[i],
                                 SEPARATOR_CHAR, exec_ret, SEPARATOR_CHAR,
                                 exec_cmd);
               }
            }
            p_new_file_name += MAX_FILENAME_LENGTH;
         }
      }
      else
      {
         if (exec_name == NULL)
         {
            /*
             * Lets first search for the old file names and see if they
             * still exists.
             */
            p_old_file_name = file_name_buffer;
            for (i = 0; i < old_file_counter; i++)
            {
               gotcha = NO;
               p_new_file_name = new_file_name_buffer;
               for (j = 0; j < file_counter; j++)
               {
                  if (CHECK_STRCMP(p_new_file_name, p_old_file_name) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
                  p_new_file_name += MAX_FILENAME_LENGTH;
               }
               if (gotcha == NO)
               {
                  production_log(creation_time, 1, 0, unique_number,
                                 split_job_counter, db[position].job_id,
                                 db[position].dir_id, "%s%c%c%c%d%c%s",
                                 p_old_file_name, SEPARATOR_CHAR,
                                 SEPARATOR_CHAR, SEPARATOR_CHAR, exec_ret,
                                 SEPARATOR_CHAR, exec_cmd);
               }
               else
               {
                  production_log(creation_time, 1, 1, unique_number,
                                 split_job_counter, db[position].job_id,
# if SIZEOF_OFF_T == 4
                                 db[position].dir_id, "%s%c%s%c%lx%c%d%c%s",
# else
                                 db[position].dir_id, "%s%c%s%c%llx%c%d%c%s",
# endif
                                 p_old_file_name, SEPARATOR_CHAR,
                                 p_old_file_name, SEPARATOR_CHAR,
                                 (pri_off_t)new_file_size_buffer[j],
                                 SEPARATOR_CHAR, exec_ret, SEPARATOR_CHAR,
                                 exec_cmd);
               }
               p_old_file_name += MAX_FILENAME_LENGTH;
            }
         }
         else
         {
            gotcha = NO;
            p_new_file_name = new_file_name_buffer;
            for (i = 0; i < file_counter; i++)
            {
               if (CHECK_STRCMP(p_new_file_name, exec_name) == 0)
               {
                  production_log(creation_time, 1, 1, unique_number,
                                 split_job_counter, db[position].job_id,
# if SIZEOF_OFF_T == 4
                                 db[position].dir_id, "%s%c%s%c%lx%c%d%c%s",
# else
                                 db[position].dir_id, "%s%c%s%c%llx%c%d%c%s",
# endif
                                 exec_name, SEPARATOR_CHAR, exec_name,
                                 SEPARATOR_CHAR,
                                 (pri_off_t)new_file_size_buffer[i],
                                 SEPARATOR_CHAR, exec_ret, SEPARATOR_CHAR,
                                 exec_cmd);
                  log_entries++;
                  break;
               }
               p_new_file_name += MAX_FILENAME_LENGTH;
            }
            if (log_entries == 0)
            {
               production_log(creation_time, 1, 0, unique_number,
                              split_job_counter, db[position].job_id,
                              db[position].dir_id, "%s%c%c%c%d%c%s",
                              exec_name, SEPARATOR_CHAR, SEPARATOR_CHAR,
                              SEPARATOR_CHAR, exec_ret, SEPARATOR_CHAR,
                              exec_cmd);
            }
         }
      }
      if (overwrite == YES)
      {
         if ((exec_name != NULL) && (old_file_name_buffer != NULL))
         {
            free(old_file_name_buffer);
            old_file_name_buffer = NULL;
         }
         free(file_name_buffer);
         file_name_buffer = new_file_name_buffer;
         p_file_name = file_name_buffer;
         prev_file_counter = 0;
      }
      else
      {
         if (exec_name != NULL)
         {
            if (old_file_name_buffer != NULL)
            {
               free(old_file_name_buffer);
            }
            old_file_name_buffer = new_file_name_buffer;
            prev_file_counter = file_counter;
         }
         else
         {
            free(new_file_name_buffer);
            prev_file_counter = 0;
         }
      }
      if (new_file_size_buffer != NULL)
      {
         free(new_file_size_buffer);
      }
   }

   return(file_counter);
}
#endif


#ifndef _WITH_PTHREAD
/*+++++++++++++++++++++++++++ restore_files() +++++++++++++++++++++++++++*/
static int
restore_files(char *file_path, off_t *file_size)
{
   int file_counter = 0;
   DIR *dp;

   *file_size = 0;
   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Can't access directory `%s' : %s",
                 file_path, strerror(errno));
   }
   else
   {
      char          *ptr,
                    fullname[MAX_PATH_LENGTH];
      struct stat   stat_buf;
      struct dirent *p_dir;

      if (file_name_buffer != NULL)
      {
         free(file_name_buffer);
         file_name_buffer = NULL;
      }
      p_file_name = file_name_buffer;

      (void)strcpy(fullname, file_path);
      ptr = fullname + strlen(fullname);
      *ptr++ = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
             ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
             (p_dir->d_name[2] == '\0')))
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Can't access file `%s' : %s",
                       fullname, strerror(errno));
         }
         else
         {
            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               if ((file_counter % FILE_NAME_STEP_SIZE) == 0)
               {
                  int    offset;
                  size_t new_size;

                  /* Calculate new size of file name buffer. */
                  new_size = ((file_counter / FILE_NAME_STEP_SIZE) + 1) * FILE_NAME_STEP_SIZE *
                             MAX_FILENAME_LENGTH;

                  /* Increase the space for the file name buffer. */
                  offset = p_file_name - file_name_buffer;
                  if ((file_name_buffer = realloc(file_name_buffer,
                                                  new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory [%d bytes] : %s",
                                new_size, strerror(errno));
                     exit(INCORRECT);
                  }

                  /* After realloc, don't forget to position */
                  /* pointer correctly.                      */
                  p_file_name = file_name_buffer + offset;
               }
               (void)strcpy(p_file_name, p_dir->d_name);
               p_file_name += MAX_FILENAME_LENGTH;
               *file_size += stat_buf.st_size;
               file_counter++;
            }
            else if (S_ISDIR(stat_buf.st_mode))
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "Currently unable to handle directories in job directories. Removing `%s'.",
                                fullname);
                    (void)rec_rmdir(fullname);
                 }
         }
         errno = 0;
      }
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s",
                    file_path, strerror(errno));
      }

      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s",
                    file_path, strerror(errno));
      }
   }

   return(file_counter);
}
#endif /* !_WITH_PTHREAD */


#ifdef _WITH_PTHREAD
/*++++++++++++++++++++++++++ get_file_names() +++++++++++++++++++++++++++*/
static int
get_file_names(char *file_path, char **file_name_buffer, char **p_file_name)
{
   int           file_counter = 0,
                 offset;
   DIR           *dp;
   struct dirent *p_dir;

   *file_name_buffer = NULL;
   *p_file_name = *file_name_buffer;

   if ((dp = opendir(file_path)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Can't access directory `%s' : %s",
                 file_path, strerror(errno));
      return(INCORRECT);
   }

   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }

      /*
       * Since on some implementations we might take a file
       * we just have created with the bin_file_chopper(), it
       * is better to count all the files and remember their
       * names, before doing anything with them.
       */
      if ((file_counter % FILE_NAME_STEP_SIZE) == 0)
      {
         size_t new_size;

         /* Calculate new size of file name buffer. */
         new_size = ((file_counter / FILE_NAME_STEP_SIZE) + 1) * FILE_NAME_STEP_SIZE * MAX_FILENAME_LENGTH;

         /* Increase the space for the file name buffer. */
         offset = *p_file_name - *file_name_buffer;
         if ((*file_name_buffer = realloc(*file_name_buffer, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory : %s", strerror(errno));
            exit(INCORRECT);
         }

         /* After realloc, don't forget to position */
         /* pointer correctly.                      */
         *p_file_name = *file_name_buffer + offset;
      }
      (void)strcpy(*p_file_name, p_dir->d_name);
      *p_file_name += MAX_FILENAME_LENGTH;
      file_counter++;
   }

   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not closedir() `%s' : %s", file_path, strerror(errno));
   }

   *p_file_name = *file_name_buffer;

   return(file_counter);
}
#endif /* _WITH_PTHREAD */


/*++++++++++++++++++++++++ create_assembled_name() ++++++++++++++++++++++*/
static void
create_assembled_name(char *name, char *rule)
{
   char *p_name = name,
        *p_rule = rule;

   do
   {
      while ((*p_rule != '%') && (*p_rule != '\0'))
      {
         *p_name = *p_rule;
         p_name++; p_rule++;
      }
      if (*p_rule != '\0')
      {
         p_rule++;
         switch (*p_rule)
         {
            case 'n' : /* Generate a 4 character unique number. */
               if (counter_fd == -1)
               {
                  if ((counter_fd = open_counter_file(COUNTER_FILE, &unique_counter)) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to open unique counter file.");
                     break;
                  }
               }
               next_counter(counter_fd, unique_counter);
               (void)sprintf(p_name, "%04x", *unique_counter);
               p_name += 4;
               p_rule++;
               break;

            case 't' : /* Insert actual time. */
               p_rule++;
               if (*p_rule == '\0')
               {
                  name[0] = '\0';
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Time option without any parameter for option assemble `%s'",
                             rule);
                  return;
               }
               else
               {
                  int    n;
                  time_t current_time;

                  current_time = time(NULL);
                  switch (*p_rule)
                  {
                     case 'a' : /* Short day eg. Tue. */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%a",
                                     gmtime(&current_time));
                        break;

                     case 'A' : /* Long day eg. Tuesday. */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%A",
                                     gmtime(&current_time));
                        break;

                     case 'b' : /* Short month eg. Jan. */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%b",
                                     gmtime(&current_time));
                        break;

                     case 'B' : /* Long month eg. January. */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%B",
                                     gmtime(&current_time));
                        break;

                     case 'd' : /* Day of month (01 - 31). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%d",
                                     gmtime(&current_time));
                        break;

                     case 'j' : /* Day of year (001 - 366). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%j",
                                     gmtime(&current_time));
                        break;

                     case 'y' : /* Year (01 - 99). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%y",
                                     gmtime(&current_time));
                        break;

                     case 'Y' : /* Year eg. 1999. */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%Y",
                                     gmtime(&current_time));
                        break;

                     case 'm' : /* Month (01 - 12). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%m",
                                     gmtime(&current_time));
                        break;

                     case 'H' : /* Hour (00 - 23). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%H",
                                     gmtime(&current_time));
                        break;

                     case 'M' : /* Minute (00 - 59). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%M",
                                     gmtime(&current_time));
                        break;

                     case 'S' : /* Second (00 - 60). */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%S",
                                     gmtime(&current_time));
                        break;

                     case 'U' : /* Unix time. */
#if SIZEOF_TIME_T == 4
                        n = sprintf(p_name, "%ld",
#else
                        n = sprintf(p_name, "%lld",
#endif
                                    (pri_time_t)current_time);
                        break;

                     default  : /* Error in timeformat. */
                        name[0] = '\0';
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Unknown parameter %c for timeformat for option assemble `%s'",
                                   *p_rule, rule);
                        return;
                  }
                  p_name += n;
                  p_rule++;
               }

               break;

            default  : /* Unknown. */
               name[0] = '\0';
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Unknown format in rule `%s' for option assemble.",
                          rule);
               return;
         }
      }
   } while (*p_rule != '\0');

   *p_name = '\0';

   return;
}
