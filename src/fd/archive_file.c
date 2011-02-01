/*
 *  archive_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   archive_file - archives one file
 **
 ** SYNOPSIS
 **   int archive_file(char       *file_path,
 **                    char       *filename,
 **                    struct job *p_db)
 **
 ** DESCRIPTION
 **   This function archives the file given 'file' into a directory
 **   of the following structure:
 **
 **    /p_work_dir/ARCHIVE_DIR/hostname/user/[dir number]/[time + archive time]
 **
 **   The process archive_watch will traverse through these directories
 **   and remove old archives.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it archived file 'filename'. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.01.1996 H.Kiehl Created
 **   22.01.1997 H.Kiehl Adapted to new messages and job ID's.
 **   25.04.1998 H.Kiehl Solve problem with / in mail user name.
 **   20.03.1999 H.Kiehl Removed unique number from archive name and
 **                      introduced ARCHIVE_STEP_TIME to reduce the
 **                      number of archive directories.
 **   30.10.2002 H.Kiehl Only do the unlink() when it is a resend job.
 **   28.03.2003 H.Kiehl If there is no username insert 'none'
 **                      otherwise archive_watch will NOT remove
 **                      any files from here.
 **   23.01.2005 H.Kiehl Write numbers in hexadecimal.
 **   30.01.2009 H.Kiehl Catch case when filename is too long to add a
 **                      unique part. Otherwise we end up in an endless
 **                      loop.
 **
 */
DESCR__E_M3

#include <stdio.h>       /* fprintf(), stderr                            */
#include <string.h>      /* strcpy(), strcat(), strerror()               */
#include <time.h>        /* time()                                       */
#include <ctype.h>       /* isdigit()                                    */
#include <limits.h>      /* LINK_MAX                                     */
#include <stdlib.h>      /* atoi()                                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>      /* opendir(), readdir(), closedir()             */
#include <fcntl.h>
#include <unistd.h>      /* mkdir(), pathconf(), unlink()                */
#include <errno.h>
#include "fddefs.h"

#define ARCHIVE_FULL -3
#ifdef _ARCHIVE_TEST
# define LINKY_MAX  10
#endif

/* External global variables. */
extern char   *p_work_dir;

/* Local variables. */
static time_t archive_start_time = 0L;
static long   link_max = 0L;

/* Local function prototypes. */
static int    create_archive_dir(char *, time_t, time_t, unsigned int, char *),
              get_archive_dir_number(char *);


/*########################### archive_file() ############################*/
int
archive_file(char       *file_path,  /* Original path of file to archive.*/
             char       *filename,   /* The file that is to be archived. */
             struct job *p_db)       /* Holds all data of current        */
                                     /* transferring job.                */
{
   time_t diff_time,
          now;
   char   newname[MAX_PATH_LENGTH],
          oldname[MAX_PATH_LENGTH],
          *ptr,
          *tmp_ptr;

   now = time(NULL);
   diff_time = now - archive_start_time;
   if ((p_db->archive_dir[p_db->archive_offset] == '\0') ||
       (diff_time > ARCHIVE_STEP_TIME))
   {
      int i = 0,
          dir_number,
          length;

      /*
       * Create a unique directory where we can store the
       * file(s).
       */
      if (archive_start_time == 0L)
      {
         (void)strcpy(p_db->archive_dir, p_work_dir);
         (void)strcat(p_db->archive_dir, AFD_ARCHIVE_DIR);
         p_db->archive_offset = strlen(p_db->archive_dir);
         p_db->archive_dir[p_db->archive_offset] = '/';
         p_db->archive_offset++;
      }
      (void)strcpy(&p_db->archive_dir[p_db->archive_offset], p_db->host_alias);

      /*
       * Lets make an educated guest: Most the time both host_alias,
       * user name and directory number zero do already exist.
       */
      tmp_ptr = ptr = p_db->archive_dir + p_db->archive_offset +
                      strlen(p_db->host_alias);
      *ptr = '/';
      ptr++;
      while (p_db->user[i] != '\0')
      {
         if (p_db->user[i] != '/')
         {
            *ptr = p_db->user[i];
         }
         ptr++; i++;
      }
      if (i == 0)
      {
         *ptr = 'n';
         *(ptr + 1) = 'o';
         *(ptr + 2) = 'n';
         *(ptr + 3) = 'e';
         ptr += 4;
      }
      *ptr = '/';
      *(ptr + 1) = '0';
      *(ptr + 2) = '/';
      *(ptr + 3) = '\0';
      length = 3;
      if (access(p_db->archive_dir, F_OK) != 0)
      {
         *tmp_ptr = '\0';
         if (access(p_db->archive_dir, F_OK) != 0)
         {
            if (mkdir(p_db->archive_dir, DIR_MODE) < 0)
            {
               /* Could be that another job is doing just the */
               /* same thing. So check if this is the case.   */
               if (errno != EEXIST)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to create directory `%s' : %s",
                             p_db->archive_dir, strerror(errno));
                  p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                  return(INCORRECT);
               }
            }
         }
         *tmp_ptr = '/';
         *ptr = '\0';

         if (access(p_db->archive_dir, F_OK) != 0)
         {
            if (mkdir(p_db->archive_dir, DIR_MODE) < 0)
            {
               /* Could be that another job is doing just the */
               /* same thing. So check if this is the case.   */
               if (errno != EEXIST)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to create directory `%s´ : %s",
                             p_db->archive_dir, strerror(errno));
                  p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                  return(INCORRECT);
               }
            }
         }

         if ((dir_number = get_archive_dir_number(p_db->archive_dir)) < 0)
         {
            if (dir_number == ARCHIVE_FULL)
            {
               ptr = p_db->archive_dir + strlen(p_db->archive_dir);
               while ((*ptr != '/') && (ptr != p_db->archive_dir))
               {
                  ptr--;
               }
               if (*ptr == '/')
               {
                  *ptr = '\0';
               }
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Archive `%s' is FULL!", p_db->archive_dir);
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to get directory number for `%s'",
                          p_db->archive_dir);
            }
            p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
            return(INCORRECT);
         }
         length = sprintf(ptr, "/%x/", dir_number);
      }

      while (create_archive_dir(p_db->archive_dir, p_db->archive_time, now,
                                p_db->job_id, ptr + length) != 0)
      {
         if (errno == EEXIST)
         {
            /* Directory does already exist. */
            break;
         }
         else if (errno == EMLINK) /* Too many links. */
              {
                 *ptr = '\0';
                 if ((dir_number = get_archive_dir_number(p_db->archive_dir)) < 0)
                 {
                    if (dir_number == ARCHIVE_FULL)
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Archive `%s' is FULL!", p_db->archive_dir);
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to get directory number for `%s'",
                                  p_db->archive_dir);
                    }
                    p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                    return(INCORRECT);
                 }
                 length = sprintf(ptr, "/%x/", dir_number);
              }
         else if (errno == ENOSPC) /* No space left on device. */
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to create unique name. Disk full.");
                 p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                 return(INCORRECT);
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to create a unique name `%s' : %s",
                            p_db->archive_dir, strerror(errno));
                 p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                 return(INCORRECT);
              }
      }
   }

   /* Move file to archive directory. */
   (void)strcpy(newname, p_db->archive_dir);
   ptr = newname + strlen(newname);
   *ptr = '/';
   ptr++;
#ifdef DO_NOT_ARCHIVE_UNIQUE_PART
   (void)strcpy(ptr, filename);
#else
   tmp_ptr = p_db->msg_name;
   while ((*tmp_ptr != '/') && (*tmp_ptr != '\0'))
   {
      tmp_ptr++;
   }
   if (*tmp_ptr == '/')
   {
      tmp_ptr++;
      while ((*tmp_ptr != '/') && (*tmp_ptr != '\0'))
      {
         tmp_ptr++;
      }
      if (*tmp_ptr == '/')
      {
         tmp_ptr++;
         while (*tmp_ptr != '\0')
         {
            *ptr = *tmp_ptr;
            ptr++; tmp_ptr++;
         }
         if (*(ptr - 1) != '/')
         {
            *ptr = '_';
            ptr++;
         }
         (void)strcpy(ptr, filename);
      }
      else
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmm, `%s' this does not look like a message.",
                    p_db->msg_name);
# if SIZEOF_TIME_T == 4
         (void)sprintf(ptr, "%lx_%x_%x_%s",
# else
         (void)sprintf(ptr, "%llx_%x_%x_%s",
# endif
                       (pri_time_t)p_db->creation_time, p_db->unique_number,
                       p_db->split_job_counter, filename);
      }
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmm, `%s' this does not look like a message.",
                 p_db->msg_name);
# if SIZEOF_TIME_T == 4
      (void)sprintf(ptr, "%lx_%x_%x_%s",
# else
      (void)sprintf(ptr, "%llx_%x_%x_%s",
# endif
                    (pri_time_t)p_db->creation_time, p_db->unique_number,
                    p_db->split_job_counter, filename);
   }
#endif
   (void)strcpy(oldname, file_path);
   (void)strcat(oldname, "/");
   (void)strcat(oldname, filename);

   if (move_file(oldname, newname) < 0)
   {
#ifndef DO_NOT_ARCHIVE_UNIQUE_PART
      if (errno == ENAMETOOLONG)
      {
         /*
          * When name is to long the cause can only be due to our
          * adding a unique part to the name (newname). So in this
          * case lets not archive file. We however need to inform
          * the user about this.
          */
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Failed to archive %s because name is to long to add a unique part.",
                   filename);
         (void)unlink(oldname);
      }
      else
      {
#endif
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "move_file() error : %s", strerror(errno));
#ifndef DO_NOT_ARCHIVE_UNIQUE_PART
      }
#endif
   }
   else
   {
      if (p_db->resend == YES)
      {
         /*
          * When both oldname and newname point to the same file
          * move_file() aka rename() will return SUCCESS but will
          * do nothing! Oldname and newname can be the same when
          * resending files from show_olog. So we must make certain
          * the file is really gone. The cheapest way seems to
          * make just another system call :-(((
          */
         (void)unlink(oldname);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++ get_archive_dir_number() ++++++++++++++++++++++*/
static int
get_archive_dir_number(char *directory)
{
   int         i;
   size_t      length;
   char        fulldir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   if (link_max == 0L)
   {
#ifdef _ARCHIVE_TEST
      link_max = LINKY_MAX;
#else
      if ((link_max = pathconf(directory, _PC_LINK_MAX)) == -1)
      {
# ifdef REDUCED_LINK_MAX
         link_max = REDUCED_LINK_MAX;
# else
         link_max = _POSIX_LINK_MAX;
# endif
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "pathconf() error for _PC_LINK_MAX : %s", strerror(errno));
      }
#endif
   }
   length = strlen(directory);
   (void)memcpy(fulldir, directory, length);
   if (fulldir[length - 1] != '/')
   {
      fulldir[length] = '/';
      length++;
   }

   for (i = 0; i < link_max; i++)
   {
      (void)sprintf(&fulldir[length], "%x", i);
      if (stat(fulldir, &stat_buf) == -1)
      {
         if (errno == ENOENT)
         {
            if ((stat(directory, &stat_buf) == -1) && (errno != ENOENT))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to stat() `%s' : %s",
                          directory, strerror(errno));
               return(INCORRECT);
            }
            if (errno == ENOENT)
            {
               if (mkdir(directory, DIR_MODE) < 0)
               {
                  if (errno != EEXIST)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to mkdir() `%s' : %s",
                                directory, strerror(errno));
                     return(INCORRECT);
                  }
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmm, created directory `%s'", directory);
               }
            }
            if (mkdir(fulldir, DIR_MODE) < 0)
            {
               if (errno != EEXIST)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to mkdir() `%s' : %s",
                             fulldir, strerror(errno));
                  return(INCORRECT);
               }
            }
            return(i);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to stat() `%s' : %s", fulldir, strerror(errno));
            return(INCORRECT);
         }
      }
      else if (stat_buf.st_nlink < link_max)
           {
              return(i);
           }
   }

   return(ARCHIVE_FULL);
}


/*++++++++++++++++++++++++ create_archive_dir() +++++++++++++++++++++++++*/
static int
create_archive_dir(char         *p_path,
                   time_t       archive_time,
                   time_t       now,
                   unsigned int job_id,
                   char         *msg_name)
{
   archive_start_time = now;
#if SIZEOF_TIME_T == 4
   (void)sprintf(msg_name, "%lx_%x",
#else
   (void)sprintf(msg_name, "%llx_%x",
#endif
                 (pri_time_t)(((archive_start_time + archive_time) /
                  ARCHIVE_STEP_TIME) * ARCHIVE_STEP_TIME),
                 job_id);

   return(mkdir(p_path, DIR_MODE));
}
