/*
 *  check_file_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2012 Deutscher Wetterdienst (DWD),
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
 **   check_file_dir - checks AFD file directory for jobs without
 **                    messages
 **
 ** SYNOPSIS
 **   void check_file_dir(time_t now)
 **
 ** DESCRIPTION
 **   The function check_file_dir() checks the AFD file directory for
 **   jobs without messages.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.1998 H.Kiehl Created
 **   16.04.2002 H.Kiehl Improve speed when inserting new jobs in queue.
 **   01.03.2008 H.Kiehl Moved from fd to dir_check.
 **
 */
DESCR__E_M3

#include <string.h>          /* strcmp(), strerror(), strcpy(), strlen() */
#include <stdlib.h>          /* strtoul(), strtol()                      */
#include <time.h>            /* time()                                   */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>        /* S_ISDIR(), stat()                        */
#include <sys/mman.h>        /* munmap()                                 */
#include <dirent.h>          /* opendir(), readdir()                     */
#include <unistd.h>          /* rmdir(), read()                          */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* #define WITH_VERBOSE_LOG */
#define MAX_FILE_DIR_CHECK_TIME 30L

/* External global variables. */
extern int                        no_of_jobs,
                                  *no_of_process;
extern char                       *p_work_dir;
extern struct instant_db          *db;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra,
                                  *p_fra;
extern struct dc_proc_list        *dcpl;
extern struct afd_status          *p_afd_status;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local global variables. */
static int                        no_of_fd_msgs = 0,
                                  no_of_job_ids;
static char                       **fd_msg_list = NULL,
                                  file_dir[MAX_PATH_LENGTH],
                                  *p_file_dir;
static struct job_id_data         *jd = NULL;

/* Local function prototypes. */
static void                       add_message_to_queue(char *, int, off_t,
                                                       unsigned int, int),
                                  check_jobs(void);
static int                        lookup_db_pos(unsigned int),
                                  message_in_queue(char *);


/*########################## check_file_dir() ###########################*/
void
check_file_dir(time_t now)
{
   time_t      diff_time;
   struct stat stat_buf;

#ifdef HAVE_SNPRINTF
   p_file_dir = file_dir + snprintf(file_dir, MAX_PATH_LENGTH, "%s%s%s/",
#else
   p_file_dir = file_dir + sprintf(file_dir, "%s%s%s/",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR);
   if (stat(file_dir, &stat_buf) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to stat() `%s' : %s"), file_dir, strerror(errno));
      return;
   }
   p_afd_status->amg_jobs |= CHECK_FILE_DIR_ACTIVE;

#ifdef WITH_VERBOSE_LOG
   system_log(DEBUG_SIGN, NULL, 0,
              _("%s starting file dir check . . ."), DIR_CHECK);
#endif

   if (read_job_ids(NULL, &no_of_job_ids, &jd) == INCORRECT)
   {
      no_of_job_ids = 0;
   }

   check_jobs();

   p_afd_status->amg_jobs &= ~CHECK_FILE_DIR_ACTIVE;

   diff_time = time(NULL) - now;
   if (diff_time > MAX_FILE_DIR_CHECK_TIME)
   {
      system_log(DEBUG_SIGN, NULL, 0,
#if SIZEOF_TIME_T == 4
                 _("Checking file directory for jobs without messages took %ld seconds!"),
#else
                 _("Checking file directory for jobs without messages took %lld seconds!"),
#endif
                 (pri_time_t)diff_time);
   }

#ifdef WITH_VERBOSE_LOG
   system_log(DEBUG_SIGN, NULL, 0, _("%s file dir check done."), DIR_CHECK);
#endif

   if (jd != NULL)
   {
      free(jd);
      jd = NULL;
   }
   if (fd_msg_list != NULL)
   {
      FREE_RT_ARRAY(fd_msg_list);
      fd_msg_list = NULL;
      no_of_fd_msgs = 0;
   }
   else
   {
      int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  fd_cmd_readfd;
#endif
      char fd_cmd_fifo[MAX_PATH_LENGTH];

      /* Tell FD to perform a check of its FSA entries. */
#ifdef HAVE_SNPRINTF
      (void)snprintf(fd_cmd_fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
      (void)sprintf(fd_cmd_fifo, "%s%s%s",
#endif
                    p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
      if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    fd_cmd_fifo, strerror(errno));
      }
      else
      {
         if (send_cmd(CHECK_FSA_ENTRIES, fd_cmd_fd) != SUCCESS)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       fd_cmd_fifo, strerror(errno));
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if ((close(fd_cmd_fd) == -1) || (close(fd_cmd_readfd) == -1))
#else
         if (close(fd_cmd_fd) == -1)
#endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ check_jobs() +++++++++++++++++++++++++++*/
static void
check_jobs(void)
{
   int           i,
                 jd_pos;
   unsigned int  job_id;
   char          *p_dir_no,
                 *p_job_id,
                 *ptr;
   DIR           *dir_no_dp,
                 *dp,
                 *id_dp;
   struct dirent *dir_no_dirp,
                 *dirp,
                 *id_dirp;
   struct stat   stat_buf;

   /*
    * Check file directory for files that do not have a message.
    */
   if ((dp = opendir(file_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' : %s"), file_dir, strerror(errno));
      return;
   }
   ptr = p_file_dir;
   if (*(ptr - 1) != '/')
   {
      *(ptr++) = '/';
   }

   /* Check each job if it has a corresponding message. */
   errno = 0;
   while ((dirp = readdir(dp)) != NULL)
   {
      if (dirp->d_name[0] != '.')
      {
         (void)strcpy(ptr, dirp->d_name);
         job_id = (unsigned int)strtoul(ptr, (char **)NULL, 16);
         jd_pos = -1;
         for (i = 0; i < no_of_job_ids; i++)
         {
            if (jd[i].job_id == job_id)
            {
               jd_pos = i;
               break;
            }
         }
         if (jd_pos == -1)
         {
            /*
             * This is a old directory no longer in
             * our job list. So lets remove the
             * entire directory.
             */
            if (rec_rmdir(file_dir) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to rec_rmdir() `%s'"), file_dir);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Removed directory `%s' since it is no longer in database."),
                          file_dir);
            }
         }
         else
         {
            int proc_pos;

            proc_pos = -1;
            for (i = 0; i < *no_of_process; i++)
            {
               if (job_id == dcpl[i].job_id)
               {
                  proc_pos = i;
                  break;
               }
            }
            if (proc_pos == -1)
            {
               if (stat(file_dir, &stat_buf) == -1)
               {
                  /*
                   * Be silent when there is no such file or directory, since
                   * it could be that it has been removed by some other process.
                   */
                  if (errno != ENOENT)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Failed to stat() `%s' : %s"),
                                file_dir, strerror(errno));
                  }
               }
               else
               {
                  /* Test if it is a directory. */
                  if (S_ISDIR(stat_buf.st_mode))
                  {
                     if ((id_dp = opendir(file_dir)) == NULL)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to opendir() `%s' : %s"),
                                   file_dir, strerror(errno));
                     }
                     else
                     {
                        p_job_id = ptr + strlen(dirp->d_name);
                        *p_job_id = '/';
                        p_job_id++;
                        errno = 0;
                        while ((id_dirp = readdir(id_dp)) != NULL)
                        {
                           if (id_dirp->d_name[0] != '.')
                           {
                              (void)strcpy(p_job_id, id_dirp->d_name);
                              if (stat(file_dir, &stat_buf) == -1)
                              {
                                 if (errno != ENOENT)
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               _("Failed to stat() `%s' : %s"),
                                               file_dir, strerror(errno));
                                 }
                              }
                              else
                              {
                                 if (S_ISDIR(stat_buf.st_mode))
                                 {
                                    if (stat_buf.st_nlink < MAX_CHECK_FILE_DIRS)
                                    {
                                       if ((dir_no_dp = opendir(file_dir)) == NULL)
                                       {
                                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                                     _("Failed to opendir() `%s' : %s"),
                                                     file_dir, strerror(errno));
                                       }
                                       else
                                       {
                                          p_dir_no = p_job_id + strlen(id_dirp->d_name);
                                          *p_dir_no = '/';
                                          p_dir_no++;
                                          errno = 0;
                                          while ((dir_no_dirp = readdir(dir_no_dp)) != NULL)
                                          {
                                             if (dir_no_dirp->d_name[0] != '.')
                                             {
                                                (void)strcpy(p_dir_no, dir_no_dirp->d_name);
                                                if (stat(file_dir, &stat_buf) == -1)
                                                {
                                                   if (errno != ENOENT)
                                                   {
                                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                 _("Failed to stat() `%s' : %s"),
                                                                 file_dir, strerror(errno));
                                                   }
                                                }
                                                else
                                                {
                                                   if (S_ISDIR(stat_buf.st_mode))
                                                   {
                                                      if (message_in_queue(ptr) == NO)
                                                      {
                                                         char          *p_file;
                                                         DIR           *file_dp;
                                                         struct dirent *file_dirp;

                                                         if ((file_dp = opendir(file_dir)) == NULL)
                                                         {
                                                            if (errno == ENOENT)
                                                            {
                                                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                          _("Failed to opendir() `%s' : %s"),
                                                                          file_dir, strerror(errno));
                                                            }
                                                         }
                                                         else
                                                         {
                                                            int   file_counter = 0;
                                                            off_t size_counter = 0;

                                                            p_file = file_dir + strlen(file_dir);
                                                            *p_file = '/';
                                                            p_file++;
                                                            errno = 0;
                                                            while ((file_dirp = readdir(file_dp)) != NULL)
                                                            {
                                                               if (((file_dirp->d_name[0] == '.') &&
                                                                   (file_dirp->d_name[1] == '\0')) ||
                                                                   ((file_dirp->d_name[0] == '.') &&
                                                                   (file_dirp->d_name[1] == '.') &&
                                                                   (file_dirp->d_name[2] == '\0')))
                                                               {
                                                                  continue;
                                                               }
                                                               (void)strcpy(p_file, file_dirp->d_name);
                                                               if (stat(file_dir, &stat_buf) != -1)
                                                               {
                                                                  file_counter++;
                                                                  size_counter += stat_buf.st_size;
                                                               }
                                                               errno = 0;
                                                            }
                                                            *(p_file - 1) = '\0';
                                                            if ((errno) && (errno != ENOENT))
                                                            {
                                                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                          _("Failed to readdir() `%s' : %s"),
                                                                          file_dir, strerror(errno));
                                                            }
                                                            if (closedir(file_dp) == -1)
                                                            {
                                                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                          _("Failed to closedir() `%s' : %s"),
                                                                          file_dir, strerror(errno));
                                                            }

                                                            if (file_counter > 0)
                                                            {
                                                               /*
                                                                * Message is NOT in queue. Add message to queue.
                                                                */
                                                               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                                                          _("Message `%s' not in queue, adding message (%d files %ld bytes)."),
#else
                                                                          _("Message `%s' not in queue, adding message (%d files %lld bytes)."),
#endif
                                                                          ptr, file_counter,
                                                                          (pri_off_t)size_counter);
                                                               add_message_to_queue(ptr, file_counter, size_counter, job_id, jd_pos);
                                                            }
                                                            else
                                                            {
                                                               /*
                                                                * This is just an empty directory, delete it.
                                                                */
                                                               if (rmdir(file_dir) == -1)
                                                               {
                                                                  if ((errno == ENOTEMPTY) ||
                                                                      (errno == EEXIST))
                                                                  {
                                                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                                _("Failed to rmdir() `%s' because there is still data in it, deleting everything in this directory."),
                                                                                file_dir);
                                                                     (void)rec_rmdir(file_dir);
                                                                  }
                                                                  else
                                                                  {
                                                                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                                _("Failed to rmdir() `%s' : %s"),
                                                                                file_dir, strerror(errno));
                                                                  }
                                                               }
                                                               else
                                                               {
                                                                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                             _("Deleted empty directory `%s'."),
                                                                             file_dir);
                                                               }
                                                            }
                                                         }
                                                      }
                                                   }
                                                }
                                             }
                                             errno = 0;
                                          }

                                          if ((errno) && (errno != ENOENT))
                                          {
                                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                        _("Failed to readdir() `%s' : %s"),
                                                        file_dir, strerror(errno));
                                          }
                                          if (closedir(dir_no_dp) == -1)
                                          {
                                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                        _("Failed to closedir() `%s' : %s"),
                                                        file_dir, strerror(errno));
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                           errno = 0;
                        }

                        if ((errno) && (errno != ENOENT))
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to readdir() `%s' : %s"),
                                      file_dir, strerror(errno));
                        }
                        if (closedir(id_dp) == -1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to closedir() `%s' : %s"),
                                      file_dir, strerror(errno));
                        }
                     }
                  }
               }
            }
         }
      }
      errno = 0;
   } /* while ((dirp = readdir(dp)) != NULL) */

   *ptr = '\0';
   if ((errno) && (errno != ENOENT))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to readdir() `%s' : %s"),
                 file_dir, strerror(errno));
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to closedir() `%s' : %s"),
                 file_dir, strerror(errno));
   }

   return;
}


/*-------------------------- message_in_queue() -------------------------*/
static int
message_in_queue(char *msg_name)
{
   int i;

   /*
    * If we do not already have it, ask FD to send us a current
    * message list.
    */
   if (fd_msg_list == NULL)
   {
      int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  fd_cmd_readfd;
#endif
      char fd_cmd_fifo[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
      (void)snprintf(fd_cmd_fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
      (void)sprintf(fd_cmd_fifo, "%s%s%s",
#endif
                    p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
      if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    fd_cmd_fifo, strerror(errno));
      }
      else
      {
         int  qlr_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
         int  qlr_write_fd;
#endif
         char queue_list_ready_fifo[MAX_PATH_LENGTH];

         if (send_cmd(FLUSH_MSG_FIFO_DUMP_QUEUE, fd_cmd_fd) != SUCCESS)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       fd_cmd_fifo, strerror(errno));
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if ((close(fd_cmd_fd) == -1) || (close(fd_cmd_readfd) == -1))
#else
         if (close(fd_cmd_fd) == -1)
#endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }

#ifdef HAVE_SNPRINTF
         (void)snprintf(queue_list_ready_fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
         (void)sprintf(queue_list_ready_fifo, "%s%s%s",
#endif
                       p_work_dir, FIFO_DIR, QUEUE_LIST_READY_FIFO);

#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(queue_list_ready_fifo, &qlr_fd, &qlr_write_fd) == -1)
#else
         if ((qlr_fd = open(queue_list_ready_fifo, O_RDWR)) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       queue_list_ready_fifo, strerror(errno));
         }
         else
         {
            int            status;
            fd_set         rset;
            struct timeval timeout;

            FD_ZERO(&rset);
            FD_SET(qlr_fd, &rset);
            timeout.tv_usec = 0;
            timeout.tv_sec = QUEUE_LIST_READY_TIMEOUT;

            status = select((qlr_fd + 1), &rset, NULL, NULL, &timeout);

            if (((status > 0) && (FD_ISSET(qlr_fd, &rset))) ||
                (p_afd_status->fd != ON))
            {
               if (p_afd_status->fd == ON)
               {
                  int  qld_fd,
                       ret;
                  char buffer[32];

                  if ((ret = read(qlr_fd, buffer, 32)) > 0)
                  {
                     if (buffer[0] == QUEUE_LIST_READY)
                     {
                        int   msg_queue_fd;
                        off_t msg_queue_size;
                        char  msg_queue_file[MAX_PATH_LENGTH],
                              *ptr;

#ifdef HAVE_SNPRINTF
                        (void)snprintf(msg_queue_file, MAX_PATH_LENGTH, "%s%s%s",
#else
                        (void)sprintf(msg_queue_file, "%s%s%s",
#endif
                                      p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
                        if ((ptr = map_file(msg_queue_file, &msg_queue_fd,
                                            &msg_queue_size, NULL,
                                            O_RDONLY)) == (caddr_t) -1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to map_file() `%s' : %s"),
                                      msg_queue_file, strerror(errno));
                        }
                        else
                        {
                           struct queue_buf *qb;

                           no_of_fd_msgs = *(int *)ptr;
                           ptr += AFD_WORD_OFFSET;
                           qb = (struct queue_buf *)ptr;

                           if (no_of_fd_msgs > 0)
                           {
                              RT_ARRAY(fd_msg_list, no_of_fd_msgs,
                                       MAX_MSG_NAME_LENGTH, char);
                              for (i = 0; i < no_of_fd_msgs; i++)
                              {
                                 (void)strcpy(fd_msg_list[i], qb[i].msg_name);
                              }
                           }

                           if (close(msg_queue_fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         _("Failed to close() `%s' : %s"),
                                         msg_queue_file, strerror(errno));
                           }
                           ptr -= AFD_WORD_OFFSET;
                           if (munmap(ptr, msg_queue_size) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Failed to munmap() from `%s' : %s"),
                                         msg_queue_file, strerror(errno));
                           }
                        }
                     }
                     else if (buffer[0] == QUEUE_LIST_EMPTY)
                          {
                             no_of_fd_msgs = 0;
                          }
                          else
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        _("Reading garbage (%d) from `%s'."),
                                        (int)buffer[0], queue_list_ready_fifo);
                          }
                  }
                  else
                  {
                     if (ret == 0)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("Reading zero!"));
                     }
                     else
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("read() error : %s"), strerror(errno));
                     }
                  }

                  /* Respond to FD so it can continue normal operations. */
#ifdef HAVE_SNPRINTF
                  (void)snprintf(queue_list_ready_fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
                  (void)sprintf(queue_list_ready_fifo, "%s%s%s",
#endif
                                p_work_dir, FIFO_DIR, QUEUE_LIST_DONE_FIFO);

                  if ((qld_fd = open(queue_list_ready_fifo, O_WRONLY)) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                queue_list_ready_fifo, strerror(errno));
                  }
                  else
                  {
                     FD_ZERO(&rset);
                     FD_SET(qld_fd, &rset);
                     timeout.tv_usec = 0;
                     timeout.tv_sec = QUEUE_LIST_READY_TIMEOUT;

                     status = select((qld_fd + 1), NULL, &rset, NULL, &timeout);

                     if ((status > 0) && (FD_ISSET(qld_fd, &rset)))
                     {
                        char buf = QUEUE_LIST_DONE;

                        if (write(qld_fd, &buf, 1) != 1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to write() to `%s' : %s"),
                                      queue_list_ready_fifo, strerror(errno));
                        }
                     }
                     else if (status == 0)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        _("%s failed to respond."), FD);
                          }
                          else
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        _("select() error (%d) : %s"),
                                        status, strerror(errno));
                          }

                     if (close(qld_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   queue_list_ready_fifo, strerror(errno));
                     }
                  }
               }
               else
               {
                  int   msg_queue_fd;
                  off_t msg_queue_size;
                  char  msg_queue_file[MAX_PATH_LENGTH],
                        *ptr;

#ifdef HAVE_SNPRINTF
                  (void)snprintf(msg_queue_file, MAX_PATH_LENGTH, "%s%s%s",
#else
                  (void)sprintf(msg_queue_file, "%s%s%s",
#endif
                                p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
                  if ((ptr = map_file(msg_queue_file, &msg_queue_fd,
                                      &msg_queue_size, NULL,
                                      O_RDONLY)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to map_file() `%s' : %s"),
                                msg_queue_file, strerror(errno));
                  }
                  else
                  {
                     struct queue_buf *qb;

                     no_of_fd_msgs = *(int *)ptr;
                     ptr += AFD_WORD_OFFSET;
                     qb = (struct queue_buf *)ptr;

                     if (no_of_fd_msgs > 0)
                     {
                        RT_ARRAY(fd_msg_list, no_of_fd_msgs,
                                 MAX_MSG_NAME_LENGTH, char);
                        for (i = 0; i < no_of_fd_msgs; i++)
                        {
                           (void)strcpy(fd_msg_list[i], qb[i].msg_name);
                        }
                     }

                     if (close(msg_queue_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   msg_queue_file, strerror(errno));
                     }
                     ptr += AFD_WORD_OFFSET;
                     if (munmap(ptr, msg_queue_size) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to munmap() from `%s' : %s"),
                                   msg_queue_file, strerror(errno));
                     }
                  }
               }
            }
            else if (status == 0)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("%s failed to respond."), FD);
                 }
                 else
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               _("select() error (%d) : %s"),
                               status, strerror(errno));
                 }

#ifdef WITHOUT_FIFO_RW_SUPPORT
            if ((close(qlr_fd) == -1) || (close(qlr_write_fd) == -1))
#else
            if (close(qlr_fd) == -1)
#endif
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
         }
      }
   }

   /* See if any of the messages in list match, ie. FD has currently */
   /* in its queue or is under proccessing.                          */
   for (i = 0; i < no_of_fd_msgs; i++)
   {
      if (strcmp(msg_name, fd_msg_list[i]) == 0)
      {
         return(YES);
      }
   }

   return(NO);
}


/*------------------------ add_message_to_queue() -----------------------*/
static void
add_message_to_queue(char         *dir_name,
                     int          file_counter,
                     off_t        size_counter,
                     unsigned int job_id,
                     int          jd_pos)
{
   unsigned int pos,
                split_job_counter,
                unique_number;
   time_t       creation_time;
   char         missing_file_dir[MAX_PATH_LENGTH],
                msg_name[MAX_MSG_NAME_LENGTH],
                *p_start,
                *ptr;

   /*
    * Retrieve priority, creation time, unique number and job ID
    * from the message name.
    */
   (void)strcpy(msg_name, dir_name);
   ptr = msg_name;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '/')
   {
      return;
   }
   ptr++;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++; /* Ignore directory number. */
   }
   if (*ptr != '/')
   {
      return;
   }
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '_')
   {
      return;
   }
   *ptr = '\0';
   creation_time = (time_t)str2timet(p_start, (char **)NULL, 16);
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '_')
   {
      return;
   }
   *ptr = '\0';
   unique_number = (unsigned int)strtoul(p_start, (char **)NULL, 16);
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '\0')
   {
      return;
   }
   *ptr = '\0';
   split_job_counter = (unsigned int)strtoul(p_start, (char **)NULL, 16);

   if ((pos = lookup_db_pos(job_id)) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not locate job %x"), job_id);
#ifdef HAVE_SNPRINTF
      (void)snprintf(missing_file_dir, MAX_PATH_LENGTH, "%s%s%s/%s", p_work_dir,
#else
      (void)sprintf(missing_file_dir, "%s%s%s/%s", p_work_dir,
#endif
                    AFD_FILE_DIR, OUTGOING_DIR, dir_name);
#ifdef _DELETE_LOG
      *dl.input_time = creation_time;
      *dl.unique_number = unique_number;
      *dl.split_job_counter = split_job_counter;
      remove_job_files(missing_file_dir, -1, job_id,
                       DIR_CHECK, JID_LOOKUP_FAILURE_DEL, -1);
#else
      remove_job_files(missing_file_dir, -1, -1);
#endif
   }
   else
   {
      char *p_unique_name;

#ifdef HAVE_SNPRINTF
      p_unique_name = missing_file_dir + snprintf(missing_file_dir, MAX_PATH_LENGTH,
#else
      p_unique_name = missing_file_dir + sprintf(missing_file_dir,
#endif
                                                 "%s%s%s",
                                                 p_work_dir, AFD_FILE_DIR,
                                                 OUTGOING_DIR);
      p_unique_name++;
#ifdef HAVE_SNPRINTF
      (void)snprintf(p_unique_name,
                     MAX_PATH_LENGTH - (p_unique_name - missing_file_dir),
                     "/%s", dir_name);
#else
      (void)sprintf(p_unique_name, "/%s", dir_name);
#endif
      if (jd_pos != -1)
      {
         p_fra = &fra[db[pos].fra_pos];
         send_message(missing_file_dir, p_unique_name, split_job_counter,
                      unique_number, creation_time, pos, 0, file_counter,
                      size_counter, NO);
      }
   }

   return;
}


/*-------------------------- lookup_db_pos() ---------------------------*/
static int
lookup_db_pos(unsigned int job_id)
{
   register int i;

   for (i = 0; i < no_of_jobs; i++)
   {
      if (db[i].job_id == job_id)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
