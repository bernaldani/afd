/*
 *  handle_options.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int handle_options(int   no_of_options,
 **                      char  *options,
 **                      char  *file_path,
 **                      int   *files_to_send,
 **                      off_t *file_size)
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
 **                    MRZ - Special format used for the exchange
 **                          of bulletins between EZMW and EDZW.
 **                    VAX - Bulletins with a two byte length indicator.
 **                          (Low Byte First)
 **                    LBF - Bulletins with a four byte length indicator
 **                          and low byte first.
 **                    HBF - Bulletins with a four byte length indicator
 **                          and high byte first.
 **                    MSS - Bulletins with a special four byte length
 **                          indicator used in conjunction with the MSS.
 **                    WMO - Bulletins with 8 byte ASCII length indicator
 **                          and 2 byte type indicator.
 **   assemble XXX   - Assembles WMO bulletins that are stored as 'one file
 **                    per bulletin' into one large file. The original files
 **                    will be deleted. It can create the same file
 **                    structures that the option 'extract' can handle,
 **                    except for MRZ. In addition it can generate the
 **                    ASCII format.
 **   convert XXX    - Converts a file from one format into another.
 **                    Currently the following is implemented:
 **                    sohetx2wmo0 - converts a file that contains many ascii
 **                                  bulletins starting with SOH and ending
 **                                  with ETX to the WMO standart (8 byte
 **                                  ascii length and 2 bytes type
 **                                  indicators). The SOH and ETX will NOT be
 **                                  copied to the new file.
 **                    sohetx2wmo1 - same as above only that here the SOH and
 **                                  ETX will copied to the new file.
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
 **   28.02.2003 H.Kiehl Added grib2wmo.
 **   01.10.2003 H.Kiehl Added convert.
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
#include <fcntl.h>                 /* open()                             */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int         sys_log_fd,
                   no_of_rule_headers;
#ifndef _WITH_PTHREAD
extern int         max_copied_files;
extern char        *file_name_buffer;
#endif
extern char        *p_work_dir;
extern struct rule *rule;

#ifndef _WITH_PTHREAD
/* Local global variables. */
static char        *p_file_name;
#endif

/* Local functions */
static void        create_assembled_name(char *, char *),
                   delete_all_files(char *);
#ifdef _WITH_PTHREAD
static int         get_file_names(char *, char **, char **),
#else
static int         restore_files(char *, off_t *),
#endif
                   recount_files(char *, off_t *);


/*############################ handle_options() #########################*/
int
handle_options(int   no_of_options,
               char  *options,
               char  *file_path,
               int   *files_to_send,
               off_t *file_size)
{
   int         i,
               j,
               ext_counter,
               first_time;
   off_t       size;
   char        *ptr = NULL,
               *p_prefix,
#ifdef _WITH_PTHREAD
               *file_name_buffer = NULL,
               *p_file_name,
#endif
               *p_newname,
               filename[MAX_FILENAME_LENGTH],
               fullname[MAX_PATH_LENGTH],
               newname[MAX_PATH_LENGTH];
   struct stat stat_buf;

   for (i = 0; i < no_of_options; i++)
   {
      p_file_name = file_name_buffer;

      /* Check if we have to rename */
      if (strncmp(options, RENAME_ID, RENAME_ID_LENGTH) == 0)
      {
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
            char *p_rule = NULL;

            /* Extract rule from local options */
            p_rule = options + RENAME_ID_LENGTH;
            while (((*p_rule == ' ') || (*p_rule == '\t')) &&
                   ((*p_rule != '\n') && (*p_rule != '\0')))
            {
               p_rule++;
            }
            if ((*p_rule == '\n') || (*p_rule == '\0'))
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
                              "Could NOT find rule %s. Ignoring this option.",
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
                                  *p_overwrite;

                     /*
                      * Check if we want to overwrite the file
                      * if rename would lead to an overwrite.
                      */
                     p_overwrite = p_rule;
                     while ((*p_overwrite != '\n') && (*p_overwrite != ' ') &&
                            (*p_overwrite != '\t') && (*p_overwrite != '\0'))
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
                            ((*(p_overwrite + 9) == '\n') ||
                             (*(p_overwrite + 9) == '\0') ||
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
                                             p_file_name)) == 0)
                           {
                              /* We found a rule, what more do you want? */
                              /* Now lets get the new name.              */
                              change_name(p_file_name,
                                          rule[rule_pos].filter[k],
                                          rule[rule_pos].rename_to[k],
                                          changed_name);
                              (void)strcpy(ptr, p_file_name);

#ifndef _WITH_PTHREAD
                              if (overwrite == NO)
                              {
                                 int  dup_count = 0,
                                      ii;
                                 char *p_end = NULL,
                                      *p_search_name;

                                 /*
                                  * Lets try to avoid a system call stat()
                                  * to see if the file does exists. Hope that
                                  * this is cheaper by going through the own
                                  * buffer and check if this name is there.
                                  */
search_again:
                                 p_search_name = file_name_buffer;
                                 for (ii = 0; ii < *files_to_send; ii++)
                                 {
                                    if (p_search_name != p_file_name)
                                    {
                                       if (CHECK_STRCMP(p_search_name, changed_name) == 0)
                                       {
                                          if (p_end == NULL)
                                          {
                                             p_end = changed_name + strlen(changed_name);
                                          }
                                          (void)sprintf(p_end, ";%d", dup_count);
                                          dup_count++;
                                          goto search_again;
                                       }
                                    }
                                    p_search_name += MAX_FILENAME_LENGTH;
                                 }
                              }
#endif /* !_WITH_PTHREAD */
                              (void)strcpy(p_newname, changed_name);

#ifdef _WITH_PTHREAD
                              /*
                               * Could be that we overwrite an existing
                               * file. If thats the case, subtract this
                               * from file_size and files_to_send. But
                               * the changed name may not be the same
                               * name as the new one, else we get the
                               * counters wrong in afd_ctrl. Worth, when
                               * we only have one file to send, we will
                               * loose it!
                               */
                              if (((overwrite == YES) ||
                                   (CHECK_STRCMP(p_file_name, changed_name) != 0)) &&
#else
                              if ((overwrite == YES) &&
#endif
                                  (stat(newname, &stat_buf) != -1))
                              {
                                 (*files_to_send)--;
                                 *file_size -= stat_buf.st_size;
                              }
                              if (rename(fullname, newname) < 0)
                              {
                                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                             "Failed to rename() %s to %s : %s",
                                             fullname, newname, strerror(errno));
                              }
#ifndef _WITH_PTHREAD
                              else
                              {
                                 (void)strcpy(p_file_name, changed_name);
                              }
#endif /* !_WITH_PTHREAD */

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
                        }

                        p_file_name += MAX_FILENAME_LENGTH;
                     }
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

      /* Check if we have to execute a command */
      if (strncmp(options, EXEC_ID, EXEC_ID_LENGTH) == 0)
      {
#ifndef _WITH_PTHREAD
         int  file_counter = *files_to_send;
#else
         int  file_counter = 0;
#endif
         int  on_error_delete_all;
         char *p_command,
              *del_orig_file,
              *p_del_orig_file;

         /*
          * First lets get the command rule. The full file name
          * can be entered with %s.
          */

         /* Extract command from local options */
         p_command = options + EXEC_ID_LENGTH;
         if (*p_command == 'd')
         {
            p_command++;
            if ((del_orig_file = malloc(MAX_PATH_LENGTH)) == NULL)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "malloc() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               p_del_orig_file = del_orig_file +
                                 sprintf(del_orig_file, "%s/", file_path);
            }
         }
         else if (*p_command == 'D')
              {
                 p_command++;
                 del_orig_file = NULL;
                 on_error_delete_all = YES;
              }
              else
              {
                 del_orig_file = NULL;
                 on_error_delete_all = NO;
              }
         while (((*p_command == ' ') || (*p_command == '\t')) &&
                ((*p_command != '\n') && (*p_command != '\0')))
         {
            p_command++;
         }
         if ((*p_command == '\n') || (*p_command == '\0'))
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
                         return_str[MAX_PATH_LENGTH + MAX_PATH_LENGTH + 1];

            while ((*p_end != '\n') && (*p_end != '\0'))
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
            tmp_option[k] = '\0';
            p_command = tmp_option;

            if (ii > 0)
            {
#ifdef _WITH_PTHREAD
               if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                                  &p_file_name)) > 0)
               {
#endif
                  int  length,
                       length_start,
                       mask_file_name,
                       ret;
                  char *fnp;

                  insert_list[ii] = &tmp_option[k];
                  tmp_char = *insert_list[0];
                  *insert_list[0] = '\0';
                  length_start = sprintf(command_str, "cd %s && %s",
                                         file_path, p_command);
                  *insert_list[0] = tmp_char;

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

                     /* Generate command string with file name(s) */
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

                     if ((ret = exec_cmd(command_str, return_str)) != 0) /* ie != SUCCESS */
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
                              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                          "%s", start_ptr);
                           } while (*end_ptr != '\0');
                        }
                     }
                     if (del_orig_file != NULL)
                     {
                        (void)strcpy(p_del_orig_file, p_file_name);
                        if ((unlink(del_orig_file) == -1) && (errno != ENOENT))
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() %s : %s",
                                       del_orig_file, strerror(errno));
                        }
                     }

                     p_file_name += MAX_FILENAME_LENGTH;
                  }

#ifdef _WITH_PTHREAD
                  *files_to_send = recount_files(file_path, file_size);
               }

               free(file_name_buffer);
#else
                  if ((i + 1) == no_of_options)
                  {
                     *files_to_send = recount_files(file_path, file_size);
                  }
                  else
                  {
                     *files_to_send = restore_files(file_path, file_size);
                  }
#endif
            }
            else
            {
               int ret;

               (void)sprintf(command_str, "cd %s && %s", file_path, p_command);

               if ((ret = exec_cmd(command_str, return_str)) != 0)
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
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "%s", start_ptr);
                     } while (*end_ptr != '\0');
                  }
               }

               if (del_orig_file != NULL)
               {
                  (void)strcpy(p_del_orig_file, p_file_name);
                  if ((unlink(del_orig_file) == -1) && (errno != ENOENT))
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Failed to unlink() %s : %s",
                                 del_orig_file, strerror(errno));
                  }
               }

               if ((ret != 0) && (on_error_delete_all == YES))
               {
                  delete_all_files(file_path);
                  *files_to_send = 0;
                  *file_size = 0;
               }
               else
               {
                  /*
                   * Recount the files regardless if the previous function
                   * call was successful or not. Even if exec_cmd() returns
                   * with an error we do not know if it did after all do
                   * somethings with the files.
                   */
#ifdef _WITH_PTHREAD
                  *files_to_send = recount_files(file_path, file_size);
#else
                  if ((i + 1) == no_of_options)
                  {
                     *files_to_send = recount_files(file_path, file_size);
                  }
                  else
                  {
                     *files_to_send = restore_files(file_path, file_size);
                  }
#endif
               }
            }
            if (del_orig_file != NULL)
            {
               free(del_orig_file);
            }
         }
         NEXT(options);
         continue;
      }

      /* Check if we just want to send the basename */
      if (CHECK_STRCMP(options, BASENAME_ID) == 0)
      {
#ifdef _WITH_PTHREAD
         int file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int  recount_files = NO;

            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);

               /* Generate name for the new file */
               (void)strcpy(filename, p_file_name);
               ptr = filename;
               while ((*ptr != '.') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '.')
               {
                  *ptr = '\0';
               }
               (void)sprintf(newname, "%s/%s", file_path, filename);

               if (CHECK_STRCMP(fullname, newname) == 0)
               {
                  p_file_name += MAX_FILENAME_LENGTH;
                  continue;
               }
               ext_counter = 1;
               first_time = YES;
               while (stat(newname, &stat_buf) == 0)
               {
                  if (first_time == YES)
                  {
                     (void)strcat(newname, ";");
                     ptr = newname + strlen(newname);
                     first_time = NO;
                  }
                  *ptr = '\0';
                  (void)sprintf(ptr, "%d", ext_counter);
                  ext_counter++;
               }

               if (rename(fullname, newname) == -1)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to rename() %s to %s : %s",
                              fullname, newname, strerror(errno));
               }
               else
               {
                  if (recount_files == NO)
                  {
                     int  k;
                     char *p_tmp_file_name;

                     p_tmp_file_name = file_name_buffer;
                     for (k = 0; k < file_counter; k++)
                     {
                        if (k != j)
                        {
                           if (strcmp(p_tmp_file_name, p_file_name) == 0)
                           {
                              recount_files = YES;
                              break;
                           }
                        }
                     }
                  }
#ifndef _WITH_PTHREAD
                  (void)strcpy(p_file_name, filename);
#endif /* !_WITH_PTHREAD */
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if we have to truncate the extension */
      if (CHECK_STRCMP(options, EXTENSION_ID) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int  recount_files = NO;

            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);

               /* Generate name for the new file */
               (void)strcpy(filename, p_file_name);
               ptr = filename + strlen(filename);
               while ((*ptr != '.') && (ptr != filename))
               {
                  ptr--;
               }
               if (*ptr == '.')
               {
                  *ptr = '\0';
               }
               (void)sprintf(newname, "%s/%s", file_path, filename);

               if (CHECK_STRCMP(fullname, newname) == 0)
               {
                  p_file_name += MAX_FILENAME_LENGTH;
                  continue;
               }
               ext_counter = 1;
               first_time = YES;
               while (stat(newname, &stat_buf) == 0)
               {
                  if (first_time == YES)
                  {
                     (void)strcat(newname, ";");
                     ptr = newname + strlen(newname);
                     first_time = NO;
                  }
                  *ptr = '\0';
                  (void)sprintf(ptr, "%d", ext_counter);
                  ext_counter++;
               }
 
               if (rename(fullname, newname) == -1)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to rename() %s to %s : %s",
                              fullname, newname, strerror(errno));
               }
               else
               {
                  if (recount_files == NO)
                  {
                     int  k;
                     char *p_tmp_file_name;

                     p_tmp_file_name = file_name_buffer;
                     for (k = 0; k < file_counter; k++)
                     {
                        if (k != j)
                        {
                           if (strcmp(p_tmp_file_name, p_file_name) == 0)
                           {
                              recount_files = YES;
                              break;
                           }
                        }
                     }
                  }
#ifndef _WITH_PTHREAD
                  (void)strcpy(p_file_name, filename);
#endif /* !_WITH_PTHREAD */
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if it's the add prefix option */
      if (strncmp(options, ADD_PREFIX_ID, ADD_PREFIX_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int file_counter = 0;
#else
         int file_counter = *files_to_send;
#endif
         int  recount_files = NO;

         /* Get the prefix */
         p_prefix = options + ADD_PREFIX_ID_LENGTH;
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

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);

               /* Generate name with prefix */
               (void)strcpy(p_newname, p_prefix);
               (void)strcat(p_newname, p_file_name);

               if (rename(fullname, newname) == -1)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to rename() %s to %s : %s",
                              fullname, newname, strerror(errno));
               }
               else
               {
                  if (recount_files == NO)
                  {
                     int  k;
                     char *p_tmp_file_name;

                     p_tmp_file_name = file_name_buffer;
                     for (k = 0; k < file_counter; k++)
                     {
                        if (k != j)
                        {
                           if (strcmp(p_tmp_file_name, p_file_name) == 0)
                           {
                              recount_files = YES;
                              break;
                           }
                        }
                     }
                  }
#ifndef _WITH_PTHREAD
                  (void)strcpy(p_file_name, p_newname);
#endif /* !_WITH_PTHREAD */
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if it's the delete prefix option */
      if (strncmp(options, DEL_PREFIX_ID, DEL_PREFIX_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int file_counter = 0;
#else
         int file_counter = *files_to_send;
#endif
         int  recount_files = NO;

         /* Get the prefix */
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

#ifdef _WITH_PTHREAD
         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#endif
            for (j = 0; j < file_counter; j++)
            {
               (void)strcpy(ptr, p_file_name);
               if (strncmp(p_file_name, p_prefix, strlen(p_prefix)) == 0)
               {
                  /* Generate name with prefix */
                  (void)strcpy(p_newname, p_file_name + strlen(p_prefix));

                  if (rename(fullname, newname) == -1)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Failed to rename() %s to %s : %s",
                                 fullname, newname, strerror(errno));
                  }
                  else
                  {
                     if (recount_files == NO)
                     {
                        int  k;
                        char *p_tmp_file_name;

                        p_tmp_file_name = file_name_buffer;
                        for (k = 0; k < file_counter; k++)
                        {
                           if (k != j)
                           {
                              if (strcmp(p_tmp_file_name, p_file_name) == 0)
                              {
                                 recount_files = YES;
                                 break;
                              }
                           }
                        }
                     }
#ifndef _WITH_PTHREAD
                     (void)strcpy(p_file_name, p_newname);
#endif /* !_WITH_PTHREAD */
                  }
               }

               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if it's the toupper option */
      if (strncmp(options, TOUPPER_ID, TOUPPER_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;
#else
         int  file_counter = *files_to_send;
#endif
         int  recount_files = NO;
         char *convert_ptr;

         (void)strcpy(newname, file_path);
         p_newname = newname + strlen(newname);
         *p_newname++ = '/';
         (void)strcpy(fullname, file_path);
         ptr = fullname + strlen(fullname);
         *ptr++ = '/';

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

               if (rename(fullname, newname) == -1)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to rename() %s to %s : %s",
                              fullname, newname, strerror(errno));
               }
               else
               {
                  if (recount_files == NO)
                  {
                     int  k;
                     char *p_tmp_file_name;

                     p_tmp_file_name = file_name_buffer;
                     for (k = 0; k < file_counter; k++)
                     {
                        if (k != j)
                        {
                           if (strcmp(p_tmp_file_name, p_file_name) == 0)
                           {
                              recount_files = YES;
                              break;
                           }
                        }
                     }
                  }
#ifndef _WITH_PTHREAD
                  (void)strcpy(p_file_name, p_newname);
#endif /* !_WITH_PTHREAD */
               }

               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if it's the tolower option */
      if (strncmp(options, TOLOWER_ID, TOLOWER_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;
#else
         int  file_counter = *files_to_send;
#endif
         int  recount_files = NO;
         char *convert_ptr;

         (void)strcpy(newname, file_path);
         p_newname = newname + strlen(newname);
         *p_newname++ = '/';
         (void)strcpy(fullname, file_path);
         ptr = fullname + strlen(fullname);
         *ptr++ = '/';

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

               if (rename(fullname, newname) == -1)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to rename() %s to %s : %s",
                              fullname, newname, strerror(errno));
               }
               else
               {
                  if (recount_files == NO)
                  {
                     int  k;
                     char *p_tmp_file_name;

                     p_tmp_file_name = file_name_buffer;
                     for (k = 0; k < file_counter; k++)
                     {
                        if (k != j)
                        {
                           if (strcmp(p_tmp_file_name, p_file_name) == 0)
                           {
                              recount_files = YES;
                              break;
                           }
                        }
                     }
                  }
#ifndef _WITH_PTHREAD
                  (void)strcpy(p_file_name, p_newname);
#endif /* !_WITH_PTHREAD */
               }

               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

#ifdef _WITH_AFW2WMO
      /* Check if we want to convert AFW format to WMO format. */
      if (CHECK_STRCMP(options, AFW2WMO_ID) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
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

                     (void)sprintf(error_name, "%s%s%s/%s", p_work_dir,
                                   AFD_FILE_DIR, ERROR_DIR, p_file_name);
                     if (rename(fullname, error_name) == -1)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to rename file %s to %s : %s",
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

                        if ((fd = open(fullname, O_WRONLY | O_TRUNC)) == -1)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to open() %s : %s",
                                       fullname, strerror(errno));
                           if ((unlink(fullname) == -1) && (errno != ENOENT))
                           {
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                          "Failed to unlink() %s : %s",
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
                                          "Failed to write() to %s : %s",
                                          p_file_name, strerror(errno));
                              if ((unlink(fullname) == -1) && (errno != ENOENT))
                              {
                                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                             "Failed to unlink() %s : %s",
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
                           }

                           if (close(fd) == -1)
                           {
                              (void)rec(sys_log_fd, DEBUG_SIGN,
                                        "close() error : %s (%s %d)\n",
                                        strerror(errno), __FILE__, __LINE__);
                           }
                        }
                        free(wmo_buffer);
                     } /* if (ret == SUCCESS) */
                     else if (ret == WMO_MESSAGE)
                          {
                             *file_size += length;
                          }
                  }
                  free(buffer);
               }
               else
               {
                  char error_name[MAX_PATH_LENGTH];

                  (void)sprintf(error_name, "%s%s%s/%s", p_work_dir,
                                AFD_FILE_DIR, ERROR_DIR, p_file_name);
                  if (rename(fullname, error_name) == -1)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Failed to rename file %s to %s : %s",
                                 fullname, error_name, strerror(errno));
                  }
                  else
                  {
                     (*files_to_send)--;
                  }
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }
#endif /* _WITH_AFW2WMO */

      /* Check if we want to convert TIFF files to GTS format */
      if (CHECK_STRCMP(options, TIFF2GTS_ID) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int recount_files = NO;

            *file_size = 0;

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
               if ((size = tiff2gts(file_path, p_file_name)) < 0)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to unlink() file %s : %s",
                                    fullname, strerror(errno));
                     }
                  }
                  else
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Removing corrupt file %s", p_file_name);
                     recount_files = YES;
                  }
               }
               else
               {
                  *file_size += size;
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if we want to convert GTS T4 encoded files to TIFF files */
      if (CHECK_STRCMP(options, GTS2TIFF_ID) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int recount_files = NO;

            *file_size = 0;

            /*
             * Hopefully we have read all file names! Now it is save
             * to convert the files.
             */
            for (j = 0; j < file_counter; j++)
            {
               (void)sprintf(fullname, "%s/%s", file_path, p_file_name);
               if ((size = gts2tiff(file_path, p_file_name)) < 0)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to unlink() file %s : %s",
                                    fullname, strerror(errno));
                     }
                  }
                  else
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Removing corrupt file %s", p_file_name);
                     recount_files = YES;
                  }
               }
               else
               {
                  *file_size += size;
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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

      /* Check if we want to convert GRIB files to WMO files */
      if (strncmp(options, GRIB2WMO_ID, GRIB2WMO_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  recount_files = NO;
            char cccc[4],
                 *p_cccc;

            if ((*(options + GRIB2WMO_ID_LENGTH) == ' ') ||
                (*(options + GRIB2WMO_ID_LENGTH) == '\t'))
            {
               char *ptr = options + GRIB2WMO_ID_LENGTH;

               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               j = 0;
               while ((isalpha(*ptr)) && (j < 4))
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
                                    "Failed to unlink() file %s : %s",
                                    fullname, strerror(errno));
                     }
                  }
                  else
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "Unable to convert, removed file %s",
                                 p_file_name);
                     recount_files = YES;                              
                  }
               }
               else
               {
                  *file_size += size;
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }   
            if (recount_files == YES)
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

      /* Check if we want extract bulletins from a file */
      if (strncmp(options, EXTRACT_ID, EXTRACT_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0,
#else
         int file_counter = *files_to_send,
#endif
              extract_typ;
         char *p_extract_id;

         p_extract_id = options + EXTRACT_ID_LENGTH + 1;
         if ((*p_extract_id == 'V') && (*(p_extract_id + 1) == 'A') &&
             (*(p_extract_id + 2) == 'X'))
         {
            extract_typ = TWO_BYTE;
         }
         else if ((*p_extract_id == 'L') && (*(p_extract_id + 1) == 'B') &&
                  (*(p_extract_id + 2) == 'F'))
              {
                 extract_typ = FOUR_BYTE_LBF;
              }
         else if ((*p_extract_id == 'H') && (*(p_extract_id + 1) == 'B') &&
                  (*(p_extract_id + 2) == 'F'))
              {
                 extract_typ = FOUR_BYTE_HBF;
              }
         else if ((*p_extract_id == 'M') && (*(p_extract_id + 1) == 'S') &&
                  (*(p_extract_id + 2) == 'S'))
              {
                 extract_typ = FOUR_BYTE_MSS;
              }
         else if ((*p_extract_id == 'M') && (*(p_extract_id + 1) == 'R') &&
                  (*(p_extract_id + 2) == 'Z'))
              {
                 extract_typ = FOUR_BYTE_MRZ;
              }
         else if ((*p_extract_id == 'G') && (*(p_extract_id + 1) == 'R') &&
                  (*(p_extract_id + 2) == 'I') && (*(p_extract_id + 3) == 'B'))
              {
                 extract_typ = FOUR_BYTE_GRIB;
              }
         else if ((*p_extract_id == 'W') && (*(p_extract_id + 1) == 'M') &&
                  (*(p_extract_id + 2) == 'O'))
              {
                 extract_typ = WMO_STANDARD;
              }
         else if (*(p_extract_id - 1) == '\0')
              {
                 /* To stay compatible to Version 0.8.x */
                 extract_typ = FOUR_BYTE_MRZ;
              }
              else
              {
                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                             "Unknown extract ID (%s) in DIR_CONFIG file.",
                             p_extract_id);
                 NEXT(options);
                 continue;
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
                      (extract_typ == FOUR_BYTE_MRZ) ? NO : YES) < 0)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "An error occurred when extracting bulletins from file %s, deleting file!",
                                 fullname);

                     if (unlink(fullname) == -1)
                     {
                        if (errno != ENOENT)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() file %s : %s",
                                       fullname, strerror(errno));
                        }
                     }
                     else
                     {
                        if (stat(fullname, &stat_buf) == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Can't access file %s : %s",
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
                  if (extract(p_file_name, file_path, extract_typ,
                              files_to_send, file_size) < 0)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 "An error occurred when extracting bulletins from file %s, deleting file!",
                                 fullname);

                     if (unlink(fullname) == -1)
                     {
                        if (errno != ENOENT)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Failed to unlink() file %s : %s",
                                       fullname, strerror(errno));
                        }
                     }
                     else
                     {
                        if (stat(fullname, &stat_buf) == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       "Can't access file %s : %s",
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
#ifndef _WITH_UNIQUE_NUMBERS
            *files_to_send = recount_files(file_path, file_size);
#endif
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

      /* Check if we want extract bulletins from a file */
      if (strncmp(options, ASSEMBLE_ID, ASSEMBLE_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0,
#else
         int file_counter = *files_to_send,
#endif
              assemble_typ;
         char *p_assemble_id,
              assembled_name[MAX_FILENAME_LENGTH];

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
                (*p_assemble_id != '\n') && (*p_assemble_id != '\0'))
         {
            p_assemble_id++;
         }
         if ((*p_assemble_id == ' ') || (*p_assemble_id == '\t'))
         {
            while ((*p_assemble_id == ' ') || (*p_assemble_id == '\t'))
            {
               p_assemble_id++;
            }
            if ((*p_assemble_id != '\n') || (*p_assemble_id != '\0'))
            {
               int  ii = 0;
               char assemble_rule[MAX_FILENAME_LENGTH];

               while ((*p_assemble_id != '\n') && (*p_assemble_id != ' ') &&
                      (*p_assemble_id != '\t') && (*p_assemble_id != '\0'))
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
            (void)sprintf(fullname, "%s/%s", file_path, assembled_name);
            if (assemble(file_path, p_file_name, file_counter,
                         fullname, assemble_typ, files_to_send,
                         file_size) < 0)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "An error occurred when assembling bulletins!");
            }
#ifndef _WITH_PTHREAD
            else
            {
               *files_to_send = restore_files(file_path, file_size);
            }
#else
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want convert a file from one format to another. */
      if (strncmp(options, CONVERT_ID, CONVERT_ID_LENGTH) == 0)
      {
#ifdef _WITH_PTHREAD
         int file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int  file_counter = *files_to_send;
#endif
            int  convert_type;
            char *p_convert_id;

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
               if ((convert(file_path, p_file_name, convert_type, file_size)) < 0)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Unable to convert file %s", p_file_name);
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
#ifdef _WITH_PTHREAD
         }

         free(file_name_buffer);
#endif
         NEXT(options);
         continue;
      }

      /* Check if we want to convert WMO to ASCII */
      if (CHECK_STRCMP(options, WMO2ASCII_ID) == 0)
      {
#ifdef _WITH_PTHREAD
         int  file_counter = 0;

         if ((file_counter = get_file_names(file_path, &file_name_buffer,
                                            &p_file_name)) > 0)
         {
#else
            int file_counter = *files_to_send;
#endif
            int recount_files = NO;

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
                              "Removing corrupt file %s", p_file_name);
                  recount_files = YES;
               }
               else
               {
                  *file_size += size;
               }
               p_file_name += MAX_FILENAME_LENGTH;
            }
            if (recount_files == YES)
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


/*+++++++++++++++++++++++++++ recount_files() +++++++++++++++++++++++++++*/
static int
recount_files(char *file_path, off_t *file_size)
{
   int file_counter = 0;
   DIR *dp;

   *file_size = 0;
   if ((dp = opendir(file_path)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
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
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Can't access file %s : %s (%s %d)\n",
                      fullname, strerror(errno), __FILE__, __LINE__);
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
                                "Currently unable to handle directories in job directories. Removing %s.",
                                fullname);
                    (void)rec_rmdir(fullname);
                 }
         }
         errno = 0;
      }
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }

      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not closedir() %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }
   }

   return(file_counter);
}


/*+++++++++++++++++++++++++ delete_all_files() ++++++++++++++++++++++++++*/
static void
delete_all_files(char *file_path)
{
   DIR *dp;

   if ((dp = opendir(file_path)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
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
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Can't access file %s : %s (%s %d)\n",
                      fullname, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (S_ISDIR(stat_buf.st_mode))
            {
               (void)rec_rmdir(fullname);
            }
            else
            {
               if (unlink(fullname) == -1)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to unlink() %s : %s",
                              fullname, strerror(errno));
               }
            }
         }
         errno = 0;
      }
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }

      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not closedir() %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}


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
      (void)rec(sys_log_fd, WARN_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
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
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Can't access file %s : %s (%s %d)\n",
                      fullname, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               if ((file_counter % 10) == 0)
               {
                  int    offset;
                  size_t new_size;

                  /* Calculate new size of file name buffer */
                  new_size = ((file_counter / 10) + 1) * 10 *
                             MAX_FILENAME_LENGTH;

                  /* Increase the space for the file name buffer */
                  offset = p_file_name - file_name_buffer;
                  if ((file_name_buffer = realloc(file_name_buffer,
                                                  new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "Could not realloc() memory : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
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
                                "Currently unable to handle directories in job directories. Removing %s.",
                                fullname);
                    (void)rec_rmdir(fullname);
                 }
         }
         errno = 0;
      }
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }

      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not closedir() %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
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
      (void)rec(sys_log_fd, WARN_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
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
      if ((file_counter % 10) == 0)
      {
         size_t new_size;

         /* Calculate new size of file name buffer */
         new_size = ((file_counter / 10) + 1) * 10 * MAX_FILENAME_LENGTH;

         /* Increase the space for the file name buffer */
         offset = *p_file_name - *file_name_buffer;
         if ((*file_name_buffer = realloc(*file_name_buffer, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not realloc() memory : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
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
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not closedir() %s : %s (%s %d)\n",
                file_path, strerror(errno), __FILE__, __LINE__);
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
            case 'n' : /* Generate a 4 character unique number */
               {
                  int number;

                  next_counter(&number);
                  (void)sprintf(p_name, "%04d", number);
               }
               p_name += 4;
               p_rule++;
               break;

            case 't' : /* Insert actual time */
               p_rule++;
               if (*p_rule == '\0')
               {
                  name[0] = '\0';
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Time option without any parameter for option assemble %s (%s %d)\n",
                            rule, __FILE__, __LINE__);
                  return;
               }
               else
               {
                  int    n;
                  time_t current_time;

                  current_time = time(NULL);
                  switch (*p_rule)
                  {
                     case 'a' : /* Short day eg. Tue */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%a",
                                     gmtime(&current_time));
                        break;

                     case 'A' : /* Long day eg. Tuesday */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%A",
                                     gmtime(&current_time));
                        break;

                     case 'b' : /* Short month eg. Jan */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%b",
                                     gmtime(&current_time));
                        break;

                     case 'B' : /* Long month eg. January */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%B",
                                     gmtime(&current_time));
                        break;

                     case 'd' : /* Day of month (01 - 31) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%d",
                                     gmtime(&current_time));
                        break;

                     case 'j' : /* Day of year (001 - 366) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%j",
                                     gmtime(&current_time));
                        break;

                     case 'y' : /* Year (01 - 99) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%y",
                                     gmtime(&current_time));
                        break;

                     case 'Y' : /* Year eg. 1999 */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%Y",
                                     gmtime(&current_time));
                        break;

                     case 'm' : /* Month (01 - 12) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%m",
                                     gmtime(&current_time));
                        break;

                     case 'H' : /* Hour (00 - 23) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%H",
                                     gmtime(&current_time));
                        break;

                     case 'M' : /* Minute (00 - 59) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%M",
                                     gmtime(&current_time));
                        break;

                     case 'S' : /* Second (00 - 60) */
                        n = strftime(p_name, MAX_FILENAME_LENGTH, "%S",
                                     gmtime(&current_time));
                        break;

                     case 'U' : /* Unix time. */
                        n = sprintf(p_name, "%ld", current_time);
                        break;

                     default  : /* Error in timeformat */
                        name[0] = '\0';
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Unknown parameter %c for timeformat for option assemble %s (%s %d)\n",
                                  *p_rule, rule, __FILE__, __LINE__);
                        return;
                  }
                  p_name += n;
                  p_rule++;
               }

               break;

            default  : /* Unknown */
               name[0] = '\0';
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Unknown format in rule %s for option assemble. (%s %d)\n",
                         rule, __FILE__, __LINE__);
               return;
         }
      }
   } while (*p_rule != '\0');

   *p_name = '\0';

   return;
}
