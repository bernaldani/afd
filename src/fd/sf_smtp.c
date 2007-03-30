/*
 *  sf_smtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_smtp - send data via SMTP
 **
 ** SYNOPSIS
 **   sf_smtp <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **   options
 **       --version        Version
 **       -a <age limit>   The age limit for the files being send.
 **       -A               Disable archiving of files.
 **       -o <retries>     Old/Error message and number of retries.
 **       -r               Resend from archive (job from show_olog).
 **       -s <SMTP server> Server where to send the mails.
 **       -t               Temp toggle.
 **
 ** DESCRIPTION
 **   sf_smtp sends the given files to the defined recipient via SMTP
 **   It does so by using it's own SMTP-client.
 **
 **   In the message file will be the data it needs about the
 **   remote host in the following format:
 **       [destination]
 **       <sheme>://<user>:<password>@<host>:<port>/<url-path>
 **
 **       [options]
 **       <a list of FD options, terminated by a newline>
 **
 **   If the archive flag is set, each file will be archived after it
 **   has been send successful.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1996 H.Kiehl Created
 **   27.01.1997 H.Kiehl Include support for output logging.
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   09.08.1997 H.Kiehl Write some more detailed logging when
 **                      an error has occurred.
 **   29.08.1997 H.Kiehl Support for 'real' host names.
 **   03.06.1998 H.Kiehl Added subject option.
 **   28.07.1998 H.Kiehl Added 'file name is user' option.
 **   27.11.1998 H.Kiehl Added attaching file.
 **   29.03.1999 H.Kiehl Local user name is LOGNAME.
 **   24.08.1999 H.Kiehl Enhanced option "attach file" to support trans-
 **                      renaming.
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   19.02.2002 H.Kiehl Sending a single mail to multiple users.
 **   02.07.2002 H.Kiehl Added from option.
 **   03.07.2002 H.Kiehl Added charset option.
 **   04.07.2002 H.Kiehl Add To header by default.
 **   13.06.2004 H.Kiehl Added transfer rate limit.
 **   04.06.2006 H.Kiehl Added option 'file name is target'.
 **   05.08.2006 H.Kiehl For option 'subject' added possibility to insert
 **                      the filename or part of it.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#include <errno.h>
#include "fddefs.h"
#include "smtpdefs.h"
#include "version.h"

/* Global variables */
int                        exitflag = IS_FAULTY_VAR,
                           files_to_delete, /* NOT USED */
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           fsa_id,
                           fsa_fd = -1,
                           line_length = 0, /* encode_base64()           */
                           *p_no_of_hosts = NULL,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           trans_rename_blocked = NO,
                           amg_flag = NO,
                           timeout_flag;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer;
long                       transfer_timeout;
char                       *p_work_dir = NULL,
                           msg_str[MAX_RET_MSG_LENGTH],
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           *del_file_name_buffer = NULL, /* NOT USED */
                           *file_name_buffer = NULL;
struct filetransfer_status *fsa = NULL;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG                                   
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local functions */
static void                sf_smtp_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              counter_fd = -1,
                    j,
                    fd,
                    status,
                    loops,
                    rest,
                    mail_header_size = 0,
                    files_to_send = 0,
                    files_send,
                    blocksize,
                    set_smtp_server,
                    write_size;
   off_t            no_of_bytes,
                    *p_file_size_buffer;
   clock_t          clktck;
   char             *smtp_buffer = NULL,
                    *p_file_name_buffer,
                    *ptr,
                    host_name[256],
                    local_user[MAX_FILENAME_LENGTH],
                    multipart_boundary[MAX_FILENAME_LENGTH],
                    remote_user[MAX_FILENAME_LENGTH],
                    *buffer,
                    *buffer_ptr,
                    *encode_buffer = NULL,
                    final_filename[MAX_FILENAME_LENGTH],
                    fullname[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    *extra_mail_header_buffer = NULL,
                    *mail_header_buffer = NULL,
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
# ifdef WITHOUT_FIFO_RW_SUPPORT
   int              ol_readfd = -1;
# endif
   unsigned int     *ol_job_number;
   char             *ol_data = NULL,
                    *ol_file_name;
   unsigned short   *ol_archive_name_length,
                    *ol_file_name_length,
                    *ol_unl;
   off_t            *ol_file_size;
   size_t           ol_size,
                    ol_real_size;
   clock_t          end_time = 0,
                    start_time = 0,
                    *ol_transfer_time;
   struct tms       tmsdummy;
#endif

   CHECK_FOR_VERSION(argc, argv);

#ifdef SA_FULLDUMP
   /*
    * When dumping core sure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(sf_smtp_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   files_to_send = init_sf(argc, argv, file_path, SMTP_FLAG);
   p_db = &db;
   if (fsa->trl_per_process > 0)
   {
      if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not get clock ticks per second : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
      if (fsa->trl_per_process < fsa->block_size)
      {
         blocksize = fsa->trl_per_process;

         /*
          * Blocksize must be large enough to accommodate two or three lines
          * since we write stuff like From: etc in one hunk.
          */
         if (blocksize < 256)
         {
            blocksize = 256;
         }
      }
      else
      {
         blocksize = fsa->block_size;
      }
   }
   else
   {
      blocksize = fsa->block_size;
   }

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR))  /* Ignore SIGPIPE! */
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /*
    * The extra buffer is needed to convert LF's to CRLF.
    */
   if ((smtp_buffer = malloc(((blocksize * 2) + 1))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(ALLOC_ERROR);
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
# ifdef WITHOUT_FIFO_RW_SUPPORT
      output_log_ptrs(&ol_fd, &ol_readfd, &ol_job_number, &ol_data, &ol_file_name,
# else
      output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
# endif
                      &ol_file_name_length, &ol_archive_name_length,
                      &ol_file_size, &ol_unl, &ol_size, &ol_transfer_time,
                      db.host_alias, SMTP);
   }
#endif

   timeout_flag = OFF;

   if (db.smtp_server[0] == '\0')
   {
      (void)strcpy(db.smtp_server, SMTP_HOST_NAME);
      set_smtp_server = NO;
   }
   else
   {
      set_smtp_server = YES;
   }

   /* Connect to remote SMTP-server */
   if ((status = smtp_connect(db.smtp_server, db.port)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                "SMTP connection to <%s> at port %d failed (%d).",
                db.smtp_server, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str, "Connected.");
      }
   }

   /* Now send HELO */
   if (gethostname(host_name, 255) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "gethostname() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((status = smtp_helo(host_name)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                "Failed to send HELO to <%s> (%d).", db.smtp_server, status);
      (void)smtp_quit();
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str, "Send HELO.");
      }
   }

   /* Inform FSA that we have finished connecting */
   (void)gsf_check_fsa();
   if (db.fsa_pos != INCORRECT)
   {
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON);
#endif
      fsa->job_status[(int)db.job_no].connect_status = EMAIL_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
      fsa->connections += 1;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON);
#endif
   }

   /* Prepare local and remote user name. */
   if (db.from != NULL)
   {
      (void)strcpy(local_user, db.from);
   }
   else
   {
      if ((ptr = getenv("LOGNAME")) != NULL)
      {
         (void)sprintf(local_user, "%s@%s", ptr, host_name);
      }
      else
      {
         (void)sprintf(local_user, "%s@%s", AFD_USER_NAME, host_name);
      }
   }

   if (set_smtp_server == YES)
   {
      ptr = db.smtp_server;
   }
   else
   {
      ptr = db.hostname;
   }
   if (db.toggle_host == YES)
   {
      if (fsa->host_toggle == HOST_ONE)
      {
         (void)strcpy(ptr, fsa->real_hostname[HOST_TWO - 1]);
      }
      else
      {
         (void)strcpy(ptr, fsa->real_hostname[HOST_ONE - 1]);
      }
   }
   else
   {
      (void)strcpy(ptr, fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
   }
   if (((db.special_flag & FILE_NAME_IS_USER) == 0) &&
       ((db.special_flag & FILE_NAME_IS_TARGET) == 0) &&
       (db.group_list == NULL))
   {
      (void)sprintf(remote_user, "%s@%s", db.user, db.hostname);
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 blocksize + 1, strerror(errno));
      exit(ALLOC_ERROR);
   }
   if (db.special_flag & ATTACH_FILE)
   {
      if ((encode_buffer = malloc(2 * (blocksize + 1))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    2 * (blocksize + 1),  strerror(errno));
         exit(ALLOC_ERROR);
      }

      /*
       * When encoding in base64 is done the blocksize must be
       * divideable by three!!!!
       */
      blocksize = blocksize - (blocksize % 3);
      if (blocksize == 0)
      {
         blocksize = 3;
      }
   }

   /* Read mail header file. */
   multipart_boundary[0] = '\0';
   if (db.special_flag & ADD_MAIL_HEADER)
   {
      int  mail_fd;
      char mail_header_file[MAX_PATH_LENGTH];

      if (db.special_ptr == NULL)
      {
         /*
          * Try read default mail header file for this host.
          */
         (void)sprintf(mail_header_file, "%s%s/%s%s",
                       p_work_dir, ETC_DIR, MAIL_HEADER_IDENTIFIER,
                       fsa->host_alias);
      }
      else
      {
         /*
          * Try read user specified mail header file for this host.
          */
         (void)strcpy(mail_header_file, db.special_ptr);
      }

      if ((mail_fd = open(mail_header_file, O_RDONLY)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to open() mail header file %s : %s",
                    mail_header_file, strerror(errno));
      }
      else
      {
         struct stat stat_buf;

         if (fstat(mail_fd, &stat_buf) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to fstat() mail header file %s : %s",
                       mail_header_file, strerror(errno));
         }
         else
         {
            if (stat_buf.st_size <= 204800)
            {
               if ((mail_header_buffer = malloc(stat_buf.st_size + 1)) == NULL)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to malloc() buffer for mail header file : %s",
                             strerror(errno));
               }
               else
               {
                  if ((extra_mail_header_buffer = malloc(((2 * stat_buf.st_size) + 1))) == NULL)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to malloc() buffer for mail header file : %s",
                                strerror(errno));
                     free(mail_header_buffer);
                     mail_header_buffer = NULL;
                  }
                  else
                  {
                     mail_header_size = stat_buf.st_size;
                     if (read(mail_fd, mail_header_buffer, mail_header_size) != stat_buf.st_size)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to read() mail header file %s : %s",
                                   mail_header_file, strerror(errno));
                        free(mail_header_buffer);
                        mail_header_buffer = NULL;
                     }
                     else
                     {
                        mail_header_buffer[mail_header_size] = '\0';

                        /* If we are attaching a file we have to */
                        /* do a multipart mail.                  */
                        if (db.special_flag & ATTACH_FILE)
                        {
                           (void)sprintf(multipart_boundary, "----%s",
                                         db.msg_name);
                        }
                     }
                  }
               }
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                          "Mail header file %s to large (%ld Bytes). Allowed are 204800 bytes.",
#else
                          "Mail header file %s to large (%lld Bytes). Allowed are 204800 bytes.",
#endif
                          mail_header_file, (pri_off_t)stat_buf.st_size);
            }
         }
         if (close(mail_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   } /* if (db.special_flag & ADD_MAIL_HEADER) */

   if ((db.special_flag & ATTACH_ALL_FILES) &&
       (multipart_boundary[0] == '\0'))
   {
      (void)sprintf(multipart_boundary, "----%s", db.msg_name);
   }

   /* Send all files */
   p_file_name_buffer = file_name_buffer;
   p_file_size_buffer = file_size_buffer;
   for (files_send = 0; files_send < files_to_send; files_send++)
   {
      if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0))
      {
         /* Send local user name */
         if ((status = smtp_user(local_user)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                      "Failed to send local user <%s> (%d).",
                      local_user, status);
            (void)smtp_quit();
            exit(eval_timeout(USER_ERROR));
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                            "Entered local user name <%s>.", local_user);
            }
         }

         if (db.special_flag & FILE_NAME_IS_USER)
         {
            if (db.user_rename_rule[0] != '\0')
            {
               register int k;

               for (k = 0; k < rule[db.user_rule_pos].no_of_rules; k++)
               {
                  if (pmatch(rule[db.user_rule_pos].filter[k],
                             p_file_name_buffer, NULL) == 0)
                  {
                     change_name(p_file_name_buffer,
                                 rule[db.user_rule_pos].filter[k],
                                 rule[db.user_rule_pos].rename_to[k],
                                 db.user, &counter_fd, db.job_id);
                     break;
                  }
               }
            }
            else
            {
               (void)strcpy(db.user, p_file_name_buffer);
            }
            (void)sprintf(remote_user, "%s@%s", db.user, db.hostname);
         }
         else if (db.special_flag & FILE_NAME_IS_TARGET)
              {
                 register int k;

                 if (db.user_rename_rule[0] != '\0')
                 {
                    for (k = 0; k < rule[db.user_rule_pos].no_of_rules; k++)
                    {
                       if (pmatch(rule[db.user_rule_pos].filter[k],
                                  p_file_name_buffer, NULL) == 0)
                       {
                          change_name(p_file_name_buffer,
                                      rule[db.user_rule_pos].filter[k],
                                      rule[db.user_rule_pos].rename_to[k],
                                      remote_user, &counter_fd, db.job_id);
                          break;
                       }
                    }
                 }
                 else
                 {
                    (void)strcpy(remote_user, p_file_name_buffer);
                 }
                 k = 0;
                 ptr = remote_user;
                 while ((*ptr != '@') && (*ptr != '\0'))
                 {
                    db.user[k] = *ptr;
                    k++; ptr++;
                 }
                 if (*ptr == '@')
                 {
                    db.user[k] = '\0';
                 }
                 else
                 {
                    db.user[0] = '\0';
                    trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                              "File name `%s' is not a mail address!",
                              remote_user);
                 }
              }
   
         /* Send remote user name */
         if (db.group_list == NULL)
         {
            if ((status = smtp_rcpt(remote_user)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                         "Failed to send remote user <%s> (%d).",
                         remote_user, status);
               (void)smtp_quit();
               exit(eval_timeout(REMOTE_USER_ERROR));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Remote user <%s> accepted by SMTP-server.",
                               remote_user);
               }
            }
         }
         else
         {
            int k;

            for (k = 0; k < db.no_listed; k++)
            {
               if ((status = smtp_rcpt(db.group_list[k])) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                            "Failed to send remote user <%s> (%d).",
                            db.group_list[k], status);
                  (void)smtp_quit();
                  exit(eval_timeout(REMOTE_USER_ERROR));
               }
               else
               {
                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                  "Remote user <%s> accepted by SMTP-server.",
                                  db.group_list[k]);
                  }
               }
            }
         }

         /* Enter data mode */
         if ((status = smtp_open()) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                      "Failed to set DATA mode (%d).", status);
            (void)smtp_quit();
            exit(eval_timeout(DATA_ERROR));
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                            "Set DATA mode.");
            }
         }
      } /* if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0)) */

      /* Get the the name of the file we want to send next */
      (void)strcpy(final_filename, p_file_name_buffer);
      (void)sprintf(fullname, "%s/%s", file_path, p_file_name_buffer);

      /* Open local file */
#ifdef O_LARGEFILE
      if ((fd = open(fullname, O_RDONLY | O_LARGEFILE)) < 0)
#else
      if ((fd = open(fullname, O_RDONLY)) < 0)
#endif
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "Failed to open() local file `%s' : %s",
                   fullname, strerror(errno));
         (void)smtp_close();
         (void)smtp_quit();
         exit(OPEN_LOCAL_ERROR);
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Open local file `%s'", fullname);
         }
      }

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         start_time = times(&tmsdummy);
      }
#endif

      /* Write status to FSA? */
      (void)gsf_check_fsa();
      if (db.fsa_pos != INCORRECT)
      {
         fsa->job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
         (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                      final_filename);
      }

      /* Read (local) and write (remote) file */
      no_of_bytes = 0;
      loops = *p_file_size_buffer / blocksize;
      rest = *p_file_size_buffer % blocksize;

      if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0))
      {
         size_t length;

         if (db.from != NULL)
         {
            length = sprintf(buffer, "From: %s\n", db.from);
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to write From to SMTP-server.");
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes = length;
         }

         if (db.reply_to != NULL)
         {
            length = sprintf(buffer, "Reply-To: %s\n", db.reply_to);
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to write Reply-To to SMTP-server.");
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         }

         /* Send file name as subject if wanted. */
         if (db.special_flag & MAIL_SUBJECT)
         {
            if (db.filename_pos_subject == -1)
            {
               length = sprintf(buffer, "Subject: %s\r\n", db.subject);
            }
            else
            {
               db.subject[db.filename_pos_subject] = '\0';
               length = sprintf(buffer, "Subject: %s", db.subject);
               if (db.subject_rename_rule[0] == '\0')
               {
                  length += sprintf(&buffer[length], "%s", final_filename);
               }
               else
               {
                  int  k;
                  char new_filename[MAX_FILENAME_LENGTH];

                  new_filename[0] = '\0';
                  for (k = 0; k < rule[db.subject_rule_pos].no_of_rules; k++)
                  {
                     if (pmatch(rule[db.subject_rule_pos].filter[k],
                                final_filename, NULL) == 0)
                     {
                        change_name(final_filename,
                                    rule[db.subject_rule_pos].filter[k],
                                    rule[db.subject_rule_pos].rename_to[k],
                                    new_filename, &counter_fd, db.job_id);
                        break;
                     }
                  }
                  if (new_filename[0] == '\0')
                  {
                     (void)strcpy(new_filename, final_filename);
                  }
                  length += sprintf(&buffer[length], "%s", new_filename);
               }
               if (db.subject[db.filename_pos_subject + 2] != '\0')
               {
                  length += sprintf(&buffer[length], "%s\r\n",
                                    &db.subject[db.filename_pos_subject + 2]);
               }
               else
               {
                  length += sprintf(&buffer[length], "\r\n");
               }
               db.subject[db.filename_pos_subject] = '%';
            }
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to write subject to SMTP-server.");
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         }
         else if (db.special_flag & FILE_NAME_IS_SUBJECT)
              {
                 length = sprintf(buffer, "Subject: %s\r\n", final_filename);
                 if (smtp_write(buffer, NULL, length) < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "Failed to write the filename as subject to SMTP-server.");
                    (void)smtp_quit();
                    exit(eval_timeout(WRITE_REMOTE_ERROR));
                 }
                 no_of_bytes += length;
              }

         if ((db.special_flag & FILE_NAME_IS_USER) == 0)
         {
            if (db.group_list == NULL)
            {
               length = sprintf(buffer, "To: %s\r\n", remote_user);
            }
            else
            {
               length = sprintf(buffer, "To: %s\r\n", p_db->user);
            }
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to write To header to SMTP-server.");
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         }

         /* Send MIME information. */
         if (db.special_flag & ATTACH_FILE)
         {
            if (multipart_boundary[0] != '\0')
            {
               length = sprintf(buffer,
                                "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: MULTIPART/MIXED; BOUNDARY=\"%s\"\r\n",
                                PACKAGE_VERSION, multipart_boundary);
               buffer_ptr = buffer;
            }
            else
            {
               if (db.trans_rename_rule[0] != '\0')
               {
                  int  k;
                  char content_type[MAX_CONTENT_TYPE_LENGTH],
                       new_filename[MAX_FILENAME_LENGTH];

                  new_filename[0] = '\0';
                  for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
                  {
                     if (pmatch(rule[db.trans_rule_pos].filter[k],
                                final_filename, NULL) == 0)
                     {
                        change_name(final_filename,
                                    rule[db.trans_rule_pos].filter[k],
                                    rule[db.trans_rule_pos].rename_to[k],
                                    new_filename, &counter_fd, db.job_id);
                        break;
                     }
                  }
                  if (new_filename[0] == '\0')
                  {
                     (void)strcpy(new_filename, final_filename);
                  }
                  get_content_type(new_filename, content_type);
                  length = sprintf(encode_buffer,
                                   "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                   PACKAGE_VERSION, content_type,
                                   new_filename, new_filename);
               }
               else
               {
                  char content_type[MAX_CONTENT_TYPE_LENGTH];

                  get_content_type(final_filename, content_type);
                  length = sprintf(encode_buffer,
                                   "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                   PACKAGE_VERSION, content_type,
                                   final_filename, final_filename);
               }
               buffer_ptr = encode_buffer;
            }

            if (smtp_write(buffer_ptr, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to write start of multipart boundary to SMTP-server.");
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         } /* if (db.special_flag & ATTACH_FILE) */
         else if (db.charset != NULL)
              {
                 length = sprintf(buffer,
                                  "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: TEXT/plain; charset=%s\r\nContent-Transfer-Encoding: 8BIT\r\n",
                                  PACKAGE_VERSION, db.charset);

                 if (smtp_write(buffer, NULL, length) < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "Failed to write MIME header with charset to SMTP-server.");
                    (void)smtp_quit();
                    exit(eval_timeout(WRITE_REMOTE_ERROR));
                 }
                 no_of_bytes += length;
              }

         /* Write the mail header. */
         if (mail_header_buffer != NULL)
         {
            length = 0;

            if (db.special_flag & ATTACH_FILE)
            {
               /* Write boundary */
               if (db.charset == NULL)
               {
                  length = sprintf(encode_buffer,
                                   "\r\n--%s\r\nContent-Type: TEXT/plain; charset=US-ASCII\r\n\r\n",
                                   multipart_boundary);
               }
               else
               {
                  length = sprintf(encode_buffer,
                                   "\r\n--%s\r\nContent-Type: TEXT/plain; charset=%s\r\nContent-Transfer-Encoding: 8BIT\r\n\r\n",
                                   multipart_boundary, db.charset);
               }

               if (smtp_write(encode_buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to write the Content-Type (TEXT/plain) to SMTP-server.");
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            } /* if (db.special_flag & ATTACH_FILE) */

            /* Now lets write the message. */
            extra_mail_header_buffer[0] = '\n';
            if (db.special_flag & ENCODE_ANSI)
            {
               if (smtp_write_iso8859(mail_header_buffer,
                                      extra_mail_header_buffer,
                                      mail_header_size) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to write the mail header content to SMTP-server.");
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
            }
            else
            {
               if (smtp_write(mail_header_buffer, extra_mail_header_buffer,
                              mail_header_size) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to write the mail header content to SMTP-server.");
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
            }
            no_of_bytes += length;

            if (db.special_flag & ATTACH_FILE)
            {
               /* Write boundary */
               if (db.trans_rename_rule[0] != '\0')
               {
                  int  k;
                  char content_type[MAX_CONTENT_TYPE_LENGTH],
                       new_filename[MAX_FILENAME_LENGTH];

                  new_filename[0] = '\0';
                  for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
                  {
                     if (pmatch(rule[db.trans_rule_pos].filter[k],
                                final_filename, NULL) == 0)
                     {
                        change_name(final_filename,
                                    rule[db.trans_rule_pos].filter[k],
                                    rule[db.trans_rule_pos].rename_to[k],
                                    new_filename, &counter_fd, db.job_id);
                        break;
                     }
                  }
                  if (new_filename[0] == '\0')
                  {
                     (void)strcpy(new_filename, final_filename);
                  }
                  get_content_type(new_filename, content_type);
                  length = sprintf(encode_buffer,
                                   "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                   multipart_boundary, content_type,
                                   new_filename, new_filename);
               }
               else
               {
                  char content_type[MAX_CONTENT_TYPE_LENGTH];

                  get_content_type(final_filename, content_type);
                  length = sprintf(encode_buffer,
                                   "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                   multipart_boundary, content_type,
                                   final_filename, final_filename);
               }

               if (smtp_write(encode_buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to write the Content-Type to SMTP-server.");
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            }
         } /* if (mail_header_buffer != NULL) */

         /*
          * We need a second CRLF to indicate end of header. The stuff
          * that follows is the message body.
          */
         if (smtp_write("\r\n", NULL, 2) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to write second CRLF to indicate end of header.");
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }
      } /* if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0)) */

      if ((db.special_flag & ATTACH_ALL_FILES) &&
          ((mail_header_buffer == NULL) || (files_send != 0)))
      {
         int  length = 0;
         char content_type[MAX_CONTENT_TYPE_LENGTH];

         /* Write boundary */
         if (db.trans_rename_rule[0] != '\0')
         {
            int  k;
            char new_filename[MAX_FILENAME_LENGTH];

            new_filename[0] = '\0';
            for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
            {
               if (pmatch(rule[db.trans_rule_pos].filter[k],
                          final_filename, NULL) == 0)
               {
                  change_name(final_filename,
                              rule[db.trans_rule_pos].filter[k],
                              rule[db.trans_rule_pos].rename_to[k],
                              new_filename, &counter_fd, db.job_id);
                  break;
               }
            }
            if (new_filename[0] == '\0')
            {
               (void)strcpy(new_filename, final_filename);
            }
            get_content_type(new_filename, content_type);
            if (files_send == 0)
            {
               length = sprintf(encode_buffer,
                                "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                multipart_boundary, content_type,
                                new_filename, new_filename);
            }
            else
            {
               length = sprintf(encode_buffer,
                                "\r\n\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                multipart_boundary, content_type,
                                new_filename, new_filename);
            }
         }
         else
         {
            get_content_type(final_filename, content_type);
            if (files_send == 0)
            {
               length = sprintf(encode_buffer,
                                "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                multipart_boundary, content_type,
                                final_filename, final_filename);
            }
            else
            {
               length = sprintf(encode_buffer,
                                "\r\n\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                multipart_boundary, content_type,
                                final_filename, final_filename);
            }
         }

         if (smtp_write(encode_buffer, NULL, length) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to write the Content-Type to SMTP-server.");
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }

         no_of_bytes += length;
      }
      if (smtp_buffer != NULL)
      {
         smtp_buffer[0] = '\n';
      }

      if (fsa->trl_per_process > 0)
      {
         init_limit_transfer_rate();
      }
      for (;;)
      {
         for (j = 0; j < loops; j++)
         {
            if (read(fd, buffer, blocksize) != blocksize)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to read() %s : %s",
                         fullname, strerror(errno));
               (void)smtp_close();
               (void)smtp_quit();
               exit(READ_LOCAL_ERROR);
            }
            if (db.special_flag & ATTACH_FILE)
            {
               write_size = encode_base64((unsigned char *)buffer, blocksize,
                                          (unsigned char *)encode_buffer);
               if (smtp_write(encode_buffer, NULL, write_size) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to write data from the source file to the SMTP-server.");
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
            }
            else
            {
               if (db.special_flag & ENCODE_ANSI)
               {
                  if (smtp_write_iso8859(buffer, smtp_buffer, blocksize) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write data from the source file to the SMTP-server.");
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (smtp_write(buffer, smtp_buffer, blocksize) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write data from the source file to the SMTP-server.");
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               write_size = blocksize;
            }
            if (fsa->trl_per_process > 0)
            {
               limit_transfer_rate(write_size, fsa->trl_per_process, clktck);
            }

            no_of_bytes += write_size;

            (void)gsf_check_fsa();
            if (db.fsa_pos != INCORRECT)
            {
               fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
               fsa->job_status[(int)db.job_no].file_size_done += write_size;
               fsa->job_status[(int)db.job_no].bytes_send += write_size;
            }
         }
         if (rest > 0)
         {
            if (read(fd, buffer, rest) != rest)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to read() rest from %s : %s",
                         fullname, strerror(errno));
               (void)smtp_close();
               (void)smtp_quit();
               exit(READ_LOCAL_ERROR);
            }
            if (db.special_flag & ATTACH_FILE)
            {
               write_size = encode_base64((unsigned char *)buffer, rest,
                                          (unsigned char *)encode_buffer);
               if (smtp_write(encode_buffer, NULL, write_size) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to write the rest data from the source file to the SMTP-server.");
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
            }
            else
            {
               if (db.special_flag & ENCODE_ANSI)
               {
                  if (smtp_write_iso8859(buffer, smtp_buffer, rest) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write the rest data from the source file to the SMTP-server.");
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (smtp_write(buffer, smtp_buffer, rest) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write the rest data from the source file to the SMTP-server.");
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               write_size = rest;
            }
            if (fsa->trl_per_process > 0)
            {
               limit_transfer_rate(write_size, fsa->trl_per_process, clktck);
            }

            no_of_bytes += write_size;

            (void)gsf_check_fsa();
            if (db.fsa_pos != INCORRECT)
            {
               fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
               fsa->job_status[(int)db.job_no].file_size_done += write_size;
               fsa->job_status[(int)db.job_no].bytes_send += write_size;
            }
         }

         /*
          * Since there are always some users sending files to the
          * AFD not in dot notation, lets check here if this is really
          * the EOF.
          * If not lets continue so long until we hopefully have reached
          * the EOF.
          * NOTE: This is NOT a fool proof way. There must be a better
          *       way!
          */
         if (fstat(fd, &stat_buf) == -1)
         {
            (void)rec(transfer_log_fd, DEBUG_SIGN,
                      "Hmmm. Failed to stat() %s : %s (%s %d)\n",
                      fullname, strerror(errno), __FILE__, __LINE__);
            break;
         }
         else
         {
            if (stat_buf.st_size > *p_file_size_buffer)
            {
               loops = (stat_buf.st_size - *p_file_size_buffer) / blocksize;
               rest = (stat_buf.st_size - *p_file_size_buffer) % blocksize;
               *p_file_size_buffer = stat_buf.st_size;

               /*
                * Give a warning in the system log, so some action
                * can be taken against the originator.
                */
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "File %s for host %s was DEFINITELY NOT send in dot notation.",
                          final_filename, fsa->host_dsp_name);
            }
            else
            {
               break;
            }
         }
      } /* for (;;) */

      /* Write boundary end if neccessary. */
      if (((db.special_flag & ATTACH_ALL_FILES) == 0) ||
          (files_send == (files_to_send - 1)))
      {
         if ((db.special_flag & ATTACH_FILE) && (multipart_boundary[0] != '\0'))
         {
            int length;

            /* Write boundary */
            length = sprintf(buffer, "\r\n--%s--\r\n", multipart_boundary);
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to write end of multipart boundary to SMTP-server.");
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }

            no_of_bytes += length;
         } /* if ((db.special_flag & ATTACH_FILE) && (multipart_boundary[0] != '\0')) */
      }

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         end_time = times(&tmsdummy);
      }
#endif

      /* Close local file */
      if (close(fd) == -1)
      {
         (void)rec(transfer_log_fd, WARN_SIGN,
                   "%-*s[%d]: Failed to close() local file %s : %s (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   final_filename, strerror(errno), __FILE__, __LINE__);
         /*
          * Since we usually do not send more then 100 files and
          * sf_smtp() will exit(), there is no point in stopping
          * the transmission.
          */
      }

      if (((db.special_flag & ATTACH_ALL_FILES) == 0) ||
          (files_send == (files_to_send - 1)))
      {
         /* Close remote file */
         if ((status = smtp_close()) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                      "Failed to close data mode (%d).", status);
            (void)smtp_quit();
            exit(eval_timeout(CLOSE_REMOTE_ERROR));
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                            "Closing data mode.");
            }
         }
      }

      /* Tell user via FSA a file has been mailed. */
      (void)gsf_check_fsa();
      if (db.fsa_pos != INCORRECT)
      {
#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
         fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
         fsa->job_status[(int)db.job_no].no_of_files_done = files_send + 1;
         fsa->job_status[(int)db.job_no].file_size_in_use = 0;
         fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

         /* Total file counter */
         fsa->total_file_counter -= 1;
#ifdef _VERIFY_FSA
         if (fsa->total_file_counter < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Total file counter for host %s less then zero. Correcting to %d.",
                       fsa->host_dsp_name,
                       files_to_send - (files_send + 1));
            fsa->total_file_counter = files_to_send - (files_send + 1);
         }
#endif

         /* Total file size */
         fsa->total_file_size -= stat_buf.st_size;
#ifdef _VERIFY_FSA
         if (fsa->total_file_size < 0)
         {
            int   k;
            off_t *tmp_ptr = p_file_size_buffer;

            tmp_ptr++;
            fsa->total_file_size = 0;
            for (k = (files_send + 1); k < files_to_send; k++)
            {
               fsa->total_file_size += *tmp_ptr;
            }
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                      "Total file size for host %s overflowed. Correcting to %ld.",
# else
                      "Total file size for host %s overflowed. Correcting to %lld.",
# endif
                      fsa->host_dsp_name, (pri_off_t)fsa->total_file_size);
         }
         else if ((fsa->total_file_counter == 0) &&
                  (fsa->total_file_size > 0))
              {
                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "fc for host %s is zero but fs is not zero. Correcting.",
                            fsa->host_dsp_name);
                 fsa->total_file_size = 0;
              }
#endif

         fsa->file_counter_done += 1;
         fsa->bytes_send += stat_buf.st_size;
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
      }

#ifdef _WITH_TRANS_EXEC
      if (db.special_flag & TRANS_EXEC)
      {
         trans_exec(file_path, fullname, p_file_name_buffer);
      }
#endif /* _WITH_TRANS_EXEC */

      /* Now archive file if necessary */
      if ((db.archive_time > 0) &&
          (p_db->archive_dir[0] != FAILED_TO_CREATE_ARCHIVE_DIR))
      {
         /*
          * By telling the function archive_file() that this
          * is the first time to archive a file for this job
          * (in struct p_db) it does not always have to check
          * whether the directory has been created or not. And
          * we ensure that we do not create duplicate names
          * when adding ARCHIVE_UNIT * db.archive_time to
          * msg_name.
          */
         if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to archive file `%s'", final_filename);
            }

            /*
             * NOTE: We _MUST_ delete the file we just send,
             *       else the file directory will run full!
             */
            if (unlink(fullname) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file `%s' after sending it successfully : %s",
                          strerror(errno), fullname);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
               ol_file_name[*ol_file_name_length + 1] = '\0';
               (*ol_file_name_length)++;
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
               *ol_unl = db.unl;
               *ol_transfer_time = end_time - start_time;
               *ol_archive_name_length = 0;
               ol_real_size = *ol_file_name_length + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "write() error : %s", strerror(errno));
               }
            }
#endif
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Archived file `%s'", final_filename);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
               ol_file_name[*ol_file_name_length + 1] = '\0';
               (*ol_file_name_length)++;
               (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
               *ol_unl = db.unl;
               *ol_transfer_time = end_time - start_time;
               *ol_archive_name_length = (unsigned short)strlen(&ol_file_name[*ol_file_name_length + 1]);
               ol_real_size = *ol_file_name_length +
                              *ol_archive_name_length + 1 + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "write() error : %s", strerror(errno));
               }
            }
#endif
         }
      }
      else
      {
         /* Delete the file we just have send */
         if (unlink(fullname) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not unlink() local file %s after sending it successfully : %s",
                       strerror(errno), fullname);
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
            (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
            *ol_file_name_length = (unsigned short)strlen(ol_file_name);
            ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
            ol_file_name[*ol_file_name_length + 1] = '\0';
            (*ol_file_name_length)++;
            *ol_file_size = *p_file_size_buffer;
            *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
            *ol_unl = db.unl;
            *ol_transfer_time = end_time - start_time;
            *ol_archive_name_length = 0;
            ol_real_size = *ol_file_name_length + ol_size;
            if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
            }
         }
#endif
      }

      /*
       * After each successful transfer set error
       * counter to zero, so that other jobs can be
       * started.
       */
      if (fsa->error_counter > 0)
      {
         int  fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
              readfd,
#endif
              j;
         char fd_wake_up_fifo[MAX_PATH_LENGTH];

         /* Number of errors that have occurred since last */
         /* successful transfer.                          */
#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
         fsa->error_counter = 0;

         /*
          * Wake up FD!
          */
         (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                       FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
         if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to open() FIFO %s : %s",
                       fd_wake_up_fifo, strerror(errno));
         }
         else
         {
            if (write(fd, "", 1) != 1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to write() to FIFO %s : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (close(readfd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO %s (read) : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
#endif
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO %s : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
         }

         /*
          * Remove the error condition (NOT_WORKING) from all jobs
          * of this host.
          */
         for (j = 0; j < fsa->allowed_transfers; j++)
         {
            if ((j != db.job_no) &&
                (fsa->job_status[j].connect_status == NOT_WORKING))
            {
               fsa->job_status[j].connect_status = DISCONNECT;
            }
         }
         fsa->error_history[0] = 0;
         fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif

         /*
          * Since we have successfully transmitted a file, no need to
          * have the queue stopped anymore.
          */
         if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
         {
            fsa->host_status ^= AUTO_PAUSE_QUEUE_STAT;
            error_action(fsa->host_alias, "stop");
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       "Starting input queue for %s that was stopped by init_afd.",
                       fsa->host_alias);
         }
      }
#ifdef WITH_ERROR_QUEUE
      if (db.special_flag & IN_ERROR_QUEUE)
      {
         remove_from_error_queue(db.job_id, fsa);
      }
#endif

      p_file_name_buffer += MAX_FILENAME_LENGTH;
      p_file_size_buffer++;
   } /* for (files_send = 0; files_send < files_to_send; files_send++) */

   free(buffer);

   /* Logout again */
   if ((status = smtp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                "Failed to disconnect from SMTP-server (%d).", status);

      /*
       * Since all files have been transfered successful it is
       * not necessary to indicate an error in the status display.
       * It's enough when we say in the Transfer log that we failed
       * to log out.
       */
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str, "Logged out.");
      }
   }

   /* Don't need the ASCII buffer */
   free(smtp_buffer);

   /*
    * Remove file directory, but only when all files have
    * been transmitted.
    */
   if (files_to_send == files_send)
   {
      if (rmdir(file_path) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to remove directory `%s' : %s",
                    file_path, strerror(errno));
      }
   }
   else
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "There are still files for `%s'. Will NOT remove this job!",
                 file_path);
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*++++++++++++++++++++++++++++ sf_smtp_exit() +++++++++++++++++++++++++++*/
static void
sf_smtp_exit(void)
{
   int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int  readfd;
#endif
   char sf_fin_fifo[MAX_PATH_LENGTH];

   if ((fsa != NULL) && (db.fsa_pos >= 0))
   {
      if ((fsa->job_status[(int)db.job_no].file_size_done > 0) ||
          (fsa->job_status[(int)db.job_no].no_of_files_done > 0))
      {
         msg_str[0] = '\0';
#if SIZEOF_OFF_T == 4
         trans_log(INFO_SIGN, NULL, 0, NULL, "%lu Bytes mailed in %d file(s).",
#else
         trans_log(INFO_SIGN, NULL, 0, NULL, "%llu Bytes mailed in %d file(s).",
#endif
                   fsa->job_status[(int)db.job_no].file_size_done,
                   fsa->job_status[(int)db.job_no].no_of_files_done);
      }
      reset_fsa((struct job *)&db, exitflag);
   }

   if (file_name_buffer != NULL)
   {
      free(file_name_buffer);
   }
   if (file_size_buffer != NULL)
   {
      free(file_size_buffer);
   }

   (void)strcpy(sf_fin_fifo, p_work_dir);
   (void)strcat(sf_fin_fifo, FIFO_DIR);
   (void)strcat(sf_fin_fifo, SF_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(sf_fin_fifo, &readfd, &fd) == -1)
#else
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", sf_fin_fifo, strerror(errno));
   }
   else
   {
#ifdef _FIFO_DEBUG
      char  cmd[2];

      cmd[0] = ACKN; cmd[1] = '\0';
      show_fifo_data('W', "sf_fin", cmd, 1, __FILE__, __LINE__);
#endif
      /* Tell FD we are finished */
      if (write(fd, &db.my_pid, sizeof(pid_t)) != sizeof(pid_t))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      (void)close(readfd);
#endif
      (void)close(fd);
   }
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   exit(INCORRECT);
}