/*
 *  archive_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 */
DESCR__E_M3

#include <stdio.h>       /* fprintf(), stderr, remove()                  */
#include <string.h>      /* strcpy(), strcat(), strerror()               */
#include <time.h>        /* time()                                       */
#include <ctype.h>       /* isdigit()                                    */
#include <limits.h>      /* LINK_MAX                                     */
#include <stdlib.h>      /* atoi()                                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>      /* opendir(), readdir(), closedir()             */
#include <fcntl.h>
#include <unistd.h>      /* mkdir()                                      */
#include <errno.h>
#include "fddefs.h"

#define ARCHIVE_FULL -3
#ifdef _ARCHIVE_TEST
#define LINKY_MAX  10
#endif

/* External global variables */
extern int    sys_log_fd;
extern char   *p_work_dir;

/* Local variables */
static time_t archive_start_time = 0L;

/* Local functions */
static int  create_archive_dir(char *, char, time_t, int, char *),
            get_dir_number(char *);


/*########################### archive_file() ############################*/
int
archive_file(char       *file_path,  /* Original path of file to archive.*/
             char       *filename,   /* The file that is to be archived. */
             struct job *p_db)       /* Holds all data of current        */
                                     /* transferring job.                */
{
   time_t diff_time;
   char   oldname[MAX_PATH_LENGTH],
          newname[MAX_PATH_LENGTH];

   diff_time = time(NULL) - archive_start_time;
   if ((p_db->archive_dir[0] == '\0') || (diff_time > ARCHIVE_RESCAN_TIME))
   {
      int         i = 0,
                  dir_number,
                  length;
      char        *ptr;
      struct stat stat_buf;

      /*
       * Create a unique directory where we can store the
       * file(s).
       */
      (void)strcpy(p_db->archive_dir, p_work_dir);
      (void)strcat(p_db->archive_dir, AFD_ARCHIVE_DIR);
      (void)strcat(p_db->archive_dir, "/");
#ifdef _OUTPUT_LOG
      p_db->archive_offset = strlen(p_db->archive_dir);
#endif
      (void)strcat(p_db->archive_dir, p_db->host_alias);
      if ((stat(p_db->archive_dir, &stat_buf) < 0) ||
          (!S_ISDIR(stat_buf.st_mode)))
      {
         if (mkdir(p_db->archive_dir, DIR_MODE) < 0)
         {
            /* Could be that another job is doing just the */
            /* same thing. So check if this is the case.   */
            if (errno != EEXIST)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to create directory %s : %s (%s %d)\n",
                         p_db->archive_dir, strerror(errno),
                         __FILE__, __LINE__);
               p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
               return(INCORRECT);
            }
         }
      }
      ptr = p_db->archive_dir + strlen(p_db->archive_dir);
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
      *ptr = '\0';

      if ((stat(p_db->archive_dir, &stat_buf) < 0) ||
          (!S_ISDIR(stat_buf.st_mode)))
      {
         if (mkdir(p_db->archive_dir, DIR_MODE) < 0)
         {
            /* Could be that another job is doing just the */
            /* same thing. So check if this is the case.   */
            if (errno != EEXIST)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to create directory : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
               return(INCORRECT);
            }
         }
      }

      if ((dir_number = get_dir_number(p_db->archive_dir)) < 0)
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
            (void)rec(sys_log_fd, ERROR_SIGN, "Archive %s is FULL! (%s %d)\n",
                      p_db->archive_dir, __FILE__, __LINE__);
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to get directory number for %s (%s %d)\n",
                      p_db->archive_dir, __FILE__, __LINE__);
         }
         p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
         return(INCORRECT);
      }
      ptr = p_db->archive_dir + strlen(p_db->archive_dir);
      length = sprintf(ptr, "/%d/", dir_number);

      while (create_archive_dir(p_db->archive_dir, p_db->msg_name[0],
                                p_db->archive_time,
                                p_db->job_id, ptr + length) < 0)
      {
         if (errno == EEXIST)
         {
            /* Directory does already exist. */
            break;
         }
         else if (errno == EMLINK) /* Too many links */
              {
                 *ptr = '\0';
                 if ((dir_number = get_dir_number(p_db->archive_dir)) < 0)
                 {
                    if (dir_number == ARCHIVE_FULL)
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Archive %s is FULL! (%s %d)\n",
                                 p_db->archive_dir, __FILE__, __LINE__);
                    }
                    else
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Failed to get directory number for %s (%s %d)\n",
                                 p_db->archive_dir, __FILE__, __LINE__);
                    }
                    p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                    return(INCORRECT);
                 }
                 length = sprintf(ptr, "/%d/", dir_number);
              }
         else if (errno == ENOSPC) /* No space left on device */
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Failed to create unique name. Disk full. (%s %d)\n",
                           p_db->archive_dir, __FILE__, __LINE__);
                 p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                 return(INCORRECT);
              }
              else
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Failed to create a unique name %s : %s (%s %d)\n",
                           p_db->archive_dir, strerror(errno),
                           __FILE__, __LINE__);
                 p_db->archive_dir[0] = FAILED_TO_CREATE_ARCHIVE_DIR;
                 return(INCORRECT);
              }
      }
   }

   /* Move file to archive directory */
   (void)strcpy(newname, p_db->archive_dir);
   (void)strcat(newname, "/");
   (void)strcat(newname, filename);
   (void)strcpy(oldname, file_path);
   (void)strcat(oldname, "/");
   (void)strcat(oldname, filename);

   if (move_file(oldname, newname) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to move file %s to %s. (%s %d)\n",
                oldname, newname, __FILE__, __LINE__);
   }
   else
   {
      /*
       * When both oldname and newname point to the same file
       * move_file() aka rename() will return SUCCESS but will
       * do nothing! Oldname and newname can be the same when
       * resending files from show_olog. So we must make certain
       * the file is really gone. The cheapest way seems to
       * make just another system call :-(((
       */
      (void)remove(oldname);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ get_dir_number() ++++++++++++++++++++++++++*/
static int
get_dir_number(char *directory)
{
   static int    dir_number;
   int           max_dir_number;
   char          *ptr,
                 *p_number;
   struct stat   stat_buf;
   struct dirent *p_dir;
   DIR           *dp;

   if ((dp = opendir(directory)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                directory, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   dir_number = max_dir_number = -1;

   ptr = directory + strlen(directory);
   *ptr++ = '/';

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }

      *ptr = '\0';
      (void)strcpy(ptr, p_dir->d_name);
      if (stat(directory, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN, "Can't access %s : %s (%s %d)\n",
                   directory, strerror(errno), __FILE__, __LINE__);
         continue;
      }

      if (S_ISDIR(stat_buf.st_mode))
      {
         /* Check if it's numeric */
         p_number = p_dir->d_name;
         while ((*p_number != '\0') && (isdigit(*p_number) != 0))
         {
            p_number++;
         }
         if (*p_number == '\0')
         {
            dir_number = atoi(p_dir->d_name);
#ifdef _ARCHIVE_TEST
            if (stat_buf.st_nlink < LINKY_MAX)
#else
            if (stat_buf.st_nlink < LINK_MAX)
#endif
            {
               ptr[-1] = '\0';
               (void)closedir(dp);
               return(dir_number);
            }
            if (max_dir_number < dir_number)
            {
               max_dir_number = dir_number;
            }
         }
      }
      errno = 0;
   }

   if (errno)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to readdir() %s : %s (%s %d)\n",
                directory, strerror(errno), __FILE__, __LINE__);
   }

   dir_number = max_dir_number;

   for (;;)
   {
      dir_number++;
#ifdef _ARCHIVE_TEST
      if (dir_number < (LINKY_MAX - 2))
#else
      if (dir_number < (LINK_MAX - 2))
#endif
      {
         /* Create new numeric directory */
         (void)sprintf(ptr, "%d", dir_number);
         if (mkdir(directory, DIR_MODE) < 0)
         {
            if (errno != EEXIST)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to create directory : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               ptr[-1] = '\0';
               (void)closedir(dp);
               return(INCORRECT);
            }
         }
         else
         {
            break;
         }
      }
      else /* No more space left to archive! */
      {
         ptr[-1] = '\0';
         (void)closedir(dp);
         return(ARCHIVE_FULL);
      }
   } /* for (;;) */

   ptr[-1] = '\0';

   if (closedir(dp) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to closedir() %s : %s (%s %d)\n",
                directory, strerror(errno), __FILE__, __LINE__);
   }

   return(dir_number);
}


/*++++++++++++++++++++++++ create_archive_dir() +++++++++++++++++++++++++*/
static int
create_archive_dir(char   *p_path,
                   char   priority,
                   time_t archive_time,
                   int    job_id,
                   char   *msg_name)
{
   archive_start_time = time(NULL);
   (void)sprintf(msg_name, "%c_%ld_%d", priority,
                 ((archive_start_time + (archive_time * ARCHIVE_UNIT)) /
                  ARCHIVE_STEP_TIME) * ARCHIVE_STEP_TIME,
                 job_id);

   return(mkdir(p_path, DIR_MODE));
}