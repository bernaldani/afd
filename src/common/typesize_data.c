/*
 *  typesize_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_typesize_data - checks if the data types match with the
 **                         curent compiled version
 **   write_typesize_data - writes the size of all data types used
 **                         by the AFD database
 **
 ** SYNOPSIS
 **   int check_typesize_data(void)
 **   int write_typesize_data(void)
 **
 ** DESCRIPTION
 **   The function check_typesize_data() checks if the size of all
 **   data types match the current version. Data types that are
 **   checked are all those used by structure filetransfer_status,
 **   fileretrieve_status, job_id_data, dir_name_buf, passwd_buf,
 **   queue_buf and dir_config_list.
 **
 **   write_typesize_data() stores all the above values in a file.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.10.2011 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* stderr, NULL, fopen(), fclose()      */
#include <string.h>              /* strerror()                           */
#include <stdlib.h>              /* exit(), atoi()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>               /* open()                               */
#ifdef HAVE_MMAP
# include <sys/mman.h>           /* munmap()                             */
#endif
#include <unistd.h>              /* close()                              */
#include <errno.h>

#define MAX_VAR_STR_LENGTH 30
#define CHAR_STR           "char"
#define INT_STR            "int"
#define OFF_T_STR          "off_t"
#define TIME_T_STR         "time_t"
#define SHORT_STR          "short_t"
#ifdef HAVE_LONG_LONG
# define LONG_LONG_STR     "long long"
#endif
#define PID_T_STR          "pid_t"

/* External global variables. */
extern char *p_work_dir;


/*######################## check_typesize_data() ########################*/
int
check_typesize_data(void)
{
   int  fd,
        not_match = 0;
   char typesize_filename[MAX_PATH_LENGTH];

   (void)sprintf(typesize_filename, "%s%s%s",
                 p_work_dir, FIFO_DIR, TYPESIZE_DATA_FILE);
   if ((fd = open(typesize_filename, O_RDONLY)) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s",
                    typesize_filename, strerror(errno));
      }
      not_match = INCORRECT;
   }
   else
   {
      struct stat stat_buf;

      if (fstat(fd, &stat_buf) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() `%s' : %s",
                    typesize_filename, strerror(errno));
         not_match = INCORRECT;
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
                            fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                                MAP_SHARED, typesize_filename,
                                0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap() error : %s", strerror(errno));
               not_match = INCORRECT;
            }
            else
            {
               int  i, j, k,
                    val,
                    vallist[] =
                    {
                       MAX_MSG_NAME_LENGTH,
                       MAX_FILENAME_LENGTH,
                       MAX_HOSTNAME_LENGTH,
                       MAX_REAL_HOSTNAME_LENGTH,
                       MAX_PROXY_NAME_LENGTH,
                       MAX_TOGGLE_STR_LENGTH,
                       ERROR_HISTORY_LENGTH,
                       MAX_NO_PARALLEL_JOBS,
                       MAX_DIR_ALIAS_LENGTH,
                       MAX_RECIPIENT_LENGTH,
                       MAX_WAIT_FOR_LENGTH,
                       MAX_FRA_TIME_ENTRIES,
                       MAX_OPTION_LENGTH,
                       MAX_PATH_LENGTH,
                       MAX_USER_NAME_LENGTH,
                       SIZEOF_CHAR,
                       SIZEOF_INT,
                       SIZEOF_OFF_T,
                       SIZEOF_TIME_T,
                       SIZEOF_SHORT,
#ifdef HAVE_LONG_LONG
                       SIZEOF_LONG_LONG,
#endif
                       SIZEOF_PID_T
                    };
               char *p_start = ptr,
                    val_str[MAX_INT_LENGTH + 1],
                    *varlist[] =
                    {
                       MAX_MSG_NAME_LENGTH_STR,
                       MAX_FILENAME_LENGTH_STR,
                       MAX_HOSTNAME_LENGTH_STR,
                       MAX_REAL_HOSTNAME_LENGTH_STR,
                       MAX_PROXY_NAME_LENGTH_STR,
                       MAX_TOGGLE_STR_LENGTH_STR,
                       ERROR_HISTORY_LENGTH_STR,
                       MAX_NO_PARALLEL_JOBS_STR,
                       MAX_DIR_ALIAS_LENGTH_STR,
                       MAX_RECIPIENT_LENGTH_STR,
                       MAX_WAIT_FOR_LENGTH_STR,
                       MAX_FRA_TIME_ENTRIES_STR,
                       MAX_OPTION_LENGTH_STR,
                       MAX_PATH_LENGTH_STR,
                       MAX_USER_NAME_LENGTH_STR,
                       CHAR_STR,
                       INT_STR,
                       OFF_T_STR,
                       TIME_T_STR,
                       SHORT_STR,
#ifdef HAVE_LONG_LONG
                       LONG_LONG_STR,
#endif
                       PID_T_STR
                    },
                    var_str[MAX_VAR_STR_LENGTH + 1];

               do
               {
                  if (*ptr == '#')
                  {
                     while ((*ptr != '\n') && (*ptr != '\r') &&
                            ((ptr - p_start) < stat_buf.st_size))
                     {
                        ptr++;
                     }
                     while (((*ptr == '\n') || (*ptr == '\r')) &&
                            ((ptr - p_start) < stat_buf.st_size))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     i = 0;
                     while ((*ptr != '|') && (i < MAX_VAR_STR_LENGTH) &&
                            ((ptr - p_start) < stat_buf.st_size))
                     {
                        var_str[i] = *ptr;
                        ptr++; i++;
                     }
                     if (*ptr == '|')
                     {
                        var_str[i] = '\0';
                        for (j = 0; j < (sizeof(varlist) / sizeof(*varlist)); j++)
                        {
                           if (strcmp(var_str, varlist[j]) == 0)
                           {
                              ptr++;
                              k = 0;
                              while ((*ptr != '\n') && (*ptr != '\r') &&
                                     (k < MAX_INT_LENGTH) &&
                                     ((ptr - p_start) < stat_buf.st_size))
                              {
                                 val_str[k] = *ptr;
                                 ptr++; k++;
                              }
                              if ((*ptr == '\n') || (*ptr == '\r'))
                              {
                                 if (k > 0)
                                 {
                                    val_str[k] = '\0';
                                    val = atoi(val_str);
                                    if (val != vallist[j])
                                    {
                                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                  "[%d] %s %d != %d",
                                                  not_match, varlist[j], val,
                                                  vallist[j]);
                                       not_match++;
                                    }
                                 }
                              }
                              else
                              {
                                 while ((*ptr != '\n') && (*ptr != '\r') &&
                                        ((ptr - p_start) < stat_buf.st_size))
                                 {
                                    k++;
                                 }
                              }
                              while (((*ptr == '\n') || (*ptr == '\r')) &&
                                     ((ptr - p_start) < stat_buf.st_size))
                              {
                                 ptr++;
                              }

                              break;
                           }
                        }
                     }
                     else
                     {
                        /* Go to end of line. */
                        while ((*ptr != '\n') && (*ptr != '\r') &&
                               ((ptr - p_start) < stat_buf.st_size))
                        {
                           ptr++;
                        }
                        while (((*ptr == '\n') || (*ptr == '\r')) &&
                               ((ptr - p_start) < stat_buf.st_size))
                        {
                           ptr++;
                        }
                     }
                  }
               } while ((ptr - p_start) < stat_buf.st_size);

               /* Unmap from file. */
#ifdef HAVE_MMAP
               if (munmap(p_start, stat_buf.st_size) == -1)
#else
               if (munmap_emu(p_start) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to munmap() `%s' : %s",
                             typesize_filename, strerror(errno));
               }
            }
         }
         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to close() `%s' : %s",
                       typesize_filename, strerror(errno));
         }
      }
   }

   return(not_match);
}


/*######################## write_typesize_data() ########################*/
int
write_typesize_data(void)
{
   int  fd;
   FILE *fp;
   char typesize_filename[MAX_PATH_LENGTH];

   (void)sprintf(typesize_filename, "%s%s%s",
                 p_work_dir, FIFO_DIR, TYPESIZE_DATA_FILE);
   if ((fd = open(typesize_filename, (O_RDWR | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to fopen() `%s' : %s",
                 typesize_filename, strerror(errno));
      return(INCORRECT);
   }
   if ((fp = fdopen(fd, "w")) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to fdopen() `%s' : %s",
                 typesize_filename, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   (void)fprintf(fp, "# NOTE: Under no circumstances edit this file!!!!\n");
   (void)fprintf(fp, "#       Please use the header files in the source code\n");
   (void)fprintf(fp, "#       tree and then recompile AFD.\n");
   (void)fprintf(fp, "MAX_MSG_NAME_LENGTH|%d\n", MAX_MSG_NAME_LENGTH);
   (void)fprintf(fp, "MAX_FILENAME_LENGTH|%d\n", MAX_FILENAME_LENGTH);
   (void)fprintf(fp, "MAX_HOSTNAME_LENGTH|%d\n", MAX_HOSTNAME_LENGTH);
   (void)fprintf(fp, "MAX_REAL_HOSTNAME_LENGTH|%d\n", MAX_REAL_HOSTNAME_LENGTH);
   (void)fprintf(fp, "MAX_PROXY_NAME_LENGTH|%d\n", MAX_PROXY_NAME_LENGTH);
   (void)fprintf(fp, "MAX_TOGGLE_STR_LENGTH|%d\n", MAX_TOGGLE_STR_LENGTH);
   (void)fprintf(fp, "ERROR_HISTORY_LENGTH|%d\n", ERROR_HISTORY_LENGTH);
   (void)fprintf(fp, "MAX_NO_PARALLEL_JOBS|%d\n", MAX_NO_PARALLEL_JOBS);
   (void)fprintf(fp, "MAX_DIR_ALIAS_LENGTH|%d\n", MAX_DIR_ALIAS_LENGTH);
   (void)fprintf(fp, "MAX_RECIPIENT_LENGTH|%d\n", MAX_RECIPIENT_LENGTH);
   (void)fprintf(fp, "MAX_WAIT_FOR_LENGTH|%d\n", MAX_WAIT_FOR_LENGTH);
   (void)fprintf(fp, "MAX_FRA_TIME_ENTRIES|%d\n", MAX_FRA_TIME_ENTRIES);
   (void)fprintf(fp, "MAX_OPTION_LENGTH|%d\n", MAX_OPTION_LENGTH);
   (void)fprintf(fp, "MAX_PATH_LENGTH|%d\n", MAX_PATH_LENGTH);
   (void)fprintf(fp, "MAX_USER_NAME_LENGTH|%d\n", MAX_USER_NAME_LENGTH);

   (void)fprintf(fp, "%s|%d\n", CHAR_STR, SIZEOF_CHAR);
   (void)fprintf(fp, "%s|%d\n", INT_STR, SIZEOF_INT);
   (void)fprintf(fp, "%s|%d\n", OFF_T_STR, SIZEOF_OFF_T);
   (void)fprintf(fp, "%s|%d\n", TIME_T_STR, SIZEOF_TIME_T);
   (void)fprintf(fp, "%s|%d\n", SHORT_STR, SIZEOF_SHORT);
#ifdef HAVE_LONG_LONG
   (void)fprintf(fp, "%s|%d\n", LONG_LONG_STR, SIZEOF_LONG_LONG);
#endif
   (void)fprintf(fp, "%s|%d\n", PID_T_STR, SIZEOF_PID_T);

   if (fclose(fp) == EOF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to fclose() `%s' : %s",
                 typesize_filename, strerror(errno));
   }

   return(SUCCESS);
}
