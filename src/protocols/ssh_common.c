/*
 *  ssh_common.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   ssh_common - functions that can be used for SSH protocol
 **
 ** SYNOPSIS
 **   int     ssh_exec(char *host, int port, unsigned char ssh_protocol,
 **                    char *user, char *passwd, char *cmd, char *subsystem,
 **                    int *fd, pid_t *child_pid)
 **   int     ssh_login(int data_fd, char *passwd)
 **   size_t  pipe_write(int fd, char *buf, size_t count)
 **   int     get_ssh_reply(int fd, int check_reply)
 **
 ** DESCRIPTION
 **   The idea to split ssh_exec() and ssh_login() into two separate
 **   functions and using unix sockets to make the password handling
 **   better was taken from the GFTP package, see http://gftp.seul.org/.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.01.2006 H.Kiehl Created
 **   23.07.2006 H.Kiehl Added host fingerprint check.
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>       /* memcpy(), strerror()                        */
#include <stdlib.h>       /* malloc(), free()                            */
#include <sys/types.h>    /* fd_set                                      */
#include <setjmp.h>       /* sigsetjmp(), siglongjmp()                   */
#include <signal.h>       /* kill()                                      */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#include <sys/time.h>     /* struct timeval                              */
#include <sys/stat.h>     /* S_ISUID, S_ISGID, etc                       */
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
#  include <sys/mman.h>   /* mmap(), msync(), munmap()                   */
#  include <pwd.h>        /* getpwuid()                                  */
# endif
#endif
#ifdef HAVE_PTY_H
# include <pty.h>
#endif
#ifdef HAVE_LIBUTIL_H
# include <libutil.h>
#endif
#ifdef HAVE_UTIL_H
# include <util.h>
#endif
#include <termios.h>
#include <unistd.h>       /* select(), exit(), write(), read(), close()  */
#include <fcntl.h>        /* open()                                      */
#include <errno.h>
#include "fddefs.h"
#include "ssh_commondefs.h"


/* External global variables. */
extern int             timeout_flag;
extern char            msg_str[];
extern long            transfer_timeout;

/* Local global variables. */
static int             fdm;
static pid_t           ssh_data_pid;
static sigjmp_buf      env_alrm;
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
static struct ssh_data sd;
# endif
#endif

/* Local function prototypes. */
static int             get_passwd_reply(int),
                       ptym_open(char *),
                       ptys_open(char *),
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
                       remove_from_knownhosts(char *),
# endif
#endif
                       tty_raw(int);
#ifdef WITH_TRACE
static size_t          pipe_write_np(int, char *, size_t);
#endif
static void            sig_handler(int);

#define NO_PROMPT 0

/* #define _WITH_DEBUG */


/*############################## ssh_exec() #############################*/
int
ssh_exec(char          *host,
         int           port,
         unsigned char ssh_protocol,
         char          *user,
         char          *passwd,
         char          *cmd,
         char          *subsystem,
         int           *fd,
         pid_t         *child_pid)
{
   int  status;
   char pts_name[MAX_PATH_LENGTH],
        *identityFilePath = NULL, /* To store the identity part. */
        *passwdBeg = NULL,
        *idBeg = NULL,
        *idEnd,
        *ptr;

   /* We want to be generic and allow a user to place the */
   /* tags in any order                                   */

   if (passwd != NULL)
   {
      int len;

      /* Do we have an identity tag "<i>" ? */
      if ((ptr = strstr(passwd, "<i>")))
      {
         idBeg = ptr + 3;
      }

      /* Do we have a passwd tag "<p>" ? */
      if ((ptr = strstr(passwd, "<p>")))
      {
         passwdBeg = ptr + 3;
      }

      /* Locate identity end tag. */
      if (passwdBeg < idBeg)  /* Id is after Pwd. */
      {
         idEnd = idBeg + (strlen(idBeg) - 1);
      }
      else  /* Id ends with Pwd. */
      {
         idEnd = passwdBeg - 4;
      }

      /* Last case, we have no tag. We should have a password alone. */
      if (!passwdBeg && !idBeg)
      {
         passwdBeg = passwd;
      }

      if (idBeg)
      {
         len = idEnd - idBeg + 1;
         if ((identityFilePath = malloc(len + 1)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
         memcpy(identityFilePath, idBeg, len);
         identityFilePath[len] = 0;
      }
   }

   msg_str[0] = '\0';
   if ((fdm = ptym_open(pts_name)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, "ptym_open() error");
      status = INCORRECT;
      ssh_data_pid = 0;
   }
   else
   {
      int sock_fd[2];

      /* Prepare unix socket for parent child communication. */
#ifdef AF_LOCAL
      if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sock_fd) == -1)
#else
      if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fd) == -1)
#endif
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "socketpair() error : %s", strerror(errno));
         status = INCORRECT;
         ssh_data_pid = 0;
      }
      else
      {
         int pipe_fds[2];

         if (pipe(pipe_fds) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "pipe() error : %s", strerror(errno));
            status = INCORRECT;
            ssh_data_pid = 0;
         }
         else
         {
            if ((*child_pid = fork()) == 0)  /* Child process */
            {
               char *args[17],
                    dummy,
                    str_protocol[1 + 3 + 1],
                    str_port[MAX_INT_LENGTH];
               int  argcount,
                    fds;
#ifdef WITH_TRACE
               int  i,
                    length;
               char buffer[MAX_PATH_LENGTH];
#endif

               setsid();
               if ((fds = ptys_open(pts_name)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "ptys_open() error");
                  (void)close(fdm);
                  _exit(INCORRECT);
               }
               (void)close(fdm);
               if (tty_raw(fds) == -1)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                            "tty_raw() failed in child!");
               }

               (void)close(sock_fd[0]);

               dup2(sock_fd[1], STDIN_FILENO);
               dup2(sock_fd[1], STDOUT_FILENO);
               dup2(fds, STDERR_FILENO);

               if (fds > 2)
               {
                  (void)close(fds);
               }

               args[0] = SSH_COMMAND;
               argcount = 1;
               if (ssh_protocol != 0)
               {
                  args[argcount] = str_protocol;
                  argcount++;
                  (void)sprintf(str_protocol, "-%d", (int)ssh_protocol);
               }
               args[argcount] = "-x";
               argcount++;
               args[argcount] = "-oFallBackToRsh no";
               argcount++;
               args[argcount] = "-p";
               argcount++;
               args[argcount] = str_port;
               argcount++;
               (void)sprintf(str_port, "%d", port);
               if (subsystem != NULL)
               {
                  args[argcount] = "-e";
                  argcount++;
                  args[argcount] = "none";
                  argcount++;
               }
               if (identityFilePath != NULL)
               {
                  args[argcount] = "-i";
                  argcount++;
                  args[argcount] = identityFilePath;
                  argcount++;
               }
               if (user != NULL)
               {
                  args[argcount] = "-l";
                  argcount++;
                  args[argcount] = user;
                  argcount++;
               }
               args[argcount] = host;
               argcount++;
               if (subsystem != NULL)
               {
                  args[argcount] = "-s";
                  argcount++;
                  args[argcount] = subsystem;
                  argcount++;
               }
               if (cmd != NULL)
               {
                  args[argcount] = cmd;
                  argcount++;
               }
               args[argcount] = NULL;
               argcount++;

#ifdef WITH_TRACE
               length = 0;
               for (i = 0; i < (argcount - 1); i++)
               {
                  length += sprintf(&buffer[length], "%s ", args[i]);
               }

               trace_log(NULL, 0, C_TRACE, buffer, length, NULL);
#endif
               /* Synchronize with parent. */
               (void)close(pipe_fds[1]);
               if (read(pipe_fds[0], &dummy, 1) != 1)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "read() error : %s", strerror(errno));
               }
               (void)close(pipe_fds[0]);

               (void)execvp(SSH_COMMAND, args);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "execvp() error : %s", strerror(errno));
               _exit(INCORRECT);
            }
            else if (*child_pid > 0) /* Parent process. */
                 {
                    (void)close(sock_fd[1]);

                    /* Synchronize with child. */
                    (void)close(pipe_fds[0]);
                    if (write(pipe_fds[1], "", 1) != 1)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "write() error : %s", strerror(errno));
                    }
                    (void)close(pipe_fds[1]);

                    *fd = sock_fd[0];
                    if (tty_raw(fdm) == -1)
                    {
                       trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                 "tty_raw() failed in parent!");
                    }
                    ssh_data_pid = *child_pid;

                    status = SUCCESS;
                 }
                 else /* Failed to fork(). */
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "fork() error : %s", strerror(errno));
                    ssh_data_pid = 0;
                    status = INCORRECT;
                 }
         }
      }
   }

   if (identityFilePath)
   {
      (void)free(identityFilePath);
   }
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
   (void)strcpy(sd.hostname, host);
   (void)strcpy(sd.user, user);
   sd.port = port;
# endif
#endif
   return(status);
}


/*############################ ssh_login() ##############################*/
int
#ifdef WITH_SSH_FINGERPRINT
ssh_login(int data_fd, char *passwd, char *fingerprint)
#else
ssh_login(int data_fd, char *passwd)
#endif
{
   int            eio_loops = 0,
                  max_fd,
                  status;
   char           *password = NULL, /* To store the password part. */
                  *passwdBeg = NULL,
                  *passwdEnd,
                  *idBeg = NULL,
                  *ptr;
   fd_set         eset,
                  rset;
   struct timeval timeout;

   /* We want to be generic and allow a user to place the */
   /* tags in any order                                   */

   if (passwd != NULL)
   {
      int len;

      /* Do we have an identity tag "<i>" ? */
      if ((ptr = strstr(passwd, "<i>")))
      {
         idBeg = ptr + 3;
      }

      /* Do we have a passwd tag "<p>" ? */
      if ((ptr = strstr(passwd, "<p>")))
      {
         passwdBeg = ptr + 3;
      }

      /* Locate password end tag. */
      if (idBeg < passwdBeg)  /* Pwd is after Id. */
      {
         passwdEnd = passwdBeg + (strlen(passwdBeg) - 1);
      }
      else  /* Pwd ends with Id. */
      {
         passwdEnd = idBeg - 4;
      }

      /* Last case, we have no tag. We should have a password alone. */
      if (!passwdBeg && !idBeg)
      {
         passwdBeg = passwd;
         passwdEnd = passwdBeg + (strlen(passwdBeg) - 1);
      }

      /* Copy what we found. If we don't have a password, nor an           */
      /* identity file, carry on anyways. In this case, ssh will use       */
      /* The ~/.ssh/id_dsa (or rsa) file. It should not have a passphrase! */

      if (passwdBeg)
      {
         len = passwdEnd - passwdBeg + 1;
         if ((password = malloc(len + 2)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
         memcpy(password, passwdBeg, len);
         password[len] = '\n';
         password[len + 1] = 0;
      }
   }

   /* Initialize select variables. */
   max_fd = fdm;
   if (data_fd > max_fd)
   {
      max_fd = data_fd;
   }
   max_fd++;
   FD_ZERO(&rset);
   FD_ZERO(&eset);

   for (;;)
   {
      FD_SET(data_fd, &rset);
      FD_SET(data_fd, &eset);
      FD_SET(fdm, &rset);
      FD_SET(fdm, &eset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      status = select(max_fd, &rset, NULL, &eset, &timeout);
      if (status > 0)
      {
          if (FD_ISSET(data_fd, &eset) || FD_ISSET(fdm, &eset))
          {
             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                       "Unix socket error.");
             status = INCORRECT;
             break;
          }
          else if (FD_ISSET(data_fd, &rset))
               {
                  /* No password required to login. */
                  status = SUCCESS;
                  break;
               }
          else if (FD_ISSET(fdm, &rset))
               {
                  int tmp_errno;

                  if (signal(SIGALRM, sig_handler) == SIG_ERR)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "ssh_login(): Failed to set signal handler : %s",
                               strerror(errno));
                     status = INCORRECT;
                     break;
                  }
                  if (sigsetjmp(env_alrm, 1) != 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "ssh_login(): read() timeout (%ld)",
                               transfer_timeout);
                     timeout_flag = ON;
                     status = INCORRECT;
                     break;
                  }
                  (void)alarm(transfer_timeout);
                  status = read(fdm, msg_str, MAX_RET_MSG_LENGTH);
                  tmp_errno = errno;
                  (void)alarm(0);

                  if (status < 0)
                  {
                     if ((tmp_errno == EIO) && (eio_loops < 10))
                     {
                        if (eio_loops == 0)
                        {
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                     "Hit an Input/Output error, assuming child was not up. Retrying.");
                        }
                        (void)my_usleep(200000L);
                        eio_loops++;
                        continue;
                     }
                     else
                     {
                        if ((tmp_errno == EIO) && (eio_loops > 0))
                        {
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                     "Hit an Input/Output error, even after retrying %d times.",
                                     eio_loops);
                        }
                        if (errno == ECONNRESET)
                        {
                           timeout_flag = CON_RESET;
                        }
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  "read() error : %s", strerror(tmp_errno));
                        status = INCORRECT;
                     }
                  }
                  else
                  {
#ifdef WITH_TRACE
                     trace_log(NULL, 0, R_TRACE, msg_str, status, NULL);
#endif
                     msg_str[status] = '\0';
                     if (status == 0)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  "SSH program closed the socket unexpected.");
                        status = INCORRECT;
                     }
                     else
                     {
                        char *ptr = msg_str;

                        while (*ptr)
                        {
                           if (*ptr == '\n')
                           {
                              *ptr = ' ';
                           }
                           ptr++;
                        }
                        ptr = msg_str;
                        if ((posi(ptr, "assword:") != NULL) ||
                            (!strncmp(msg_str, "Enter passphrase", 16)))
                        {
                           if (password)
                           {
                              size_t length = strlen(password);

#ifdef WITH_TRACE
                              if ((status = pipe_write_np(fdm, password, length)) != length)
#else
                              if ((status = pipe_write(fdm, password, length)) != length)
#endif
                              {
                                 if (errno != 0)
                                 {
                                    msg_str[0] = '\0';
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                              "write() error [%d] : %s",
                                              status, strerror(errno));
                                 }
                              }
                              else
                              {
                                 /* Check if correct password was entered. */
                                 msg_str[0] = '\0';
                                 if ((status = get_passwd_reply(fdm)) > 0)
                                 {
                                    if ((status == 1) && (msg_str[0] == '\n'))
                                    {
                                       status = SUCCESS;
                                    }
                                    else
                                    {
                                       trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                                                 "Failed to enter passwd.");
                                       status = INCORRECT;
                                       msg_str[0] = '\0';
                                    }
                                 }
                              }
                           }
                           else /* if (!password) */
                           {
                              /* It's asking for a password or passphrase and */
                              /* we don't have one. Report error.             */
                              trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                                        "ssh is asking for password (or passphrase) and none is provided. Bailing out!");
                              status = INCORRECT;
                              msg_str[0] = '\0';
                           }
                        }
                        else if ((status == 1) &&
                                 ((msg_str[0] == '\n') || (msg_str[0] == ' ')))
                             {
                                status = SUCCESS;
#ifdef WITH_EFENCE
                                sleep(1);
                                continue;
#endif
                             }
                        /*
                         * It's not asking for a password. Three cases :
                         * 1) We're using a private key (Identity file)
                         * 2) It's asking for something else (prior host key
                         *    exchange or host key mismatch).
                         * 3) It's an unknown failure. Go on, we'll catch by
                         *    later (with a timeout, and no good message. Bad).
                         */
                        else if (posi(ptr, "(yes/no)") != NULL)
                             {
#ifdef WITH_SSH_FINGERPRINT
                                if ((fingerprint[0] == '\0') ||
                                    (posi(ptr, fingerprint) == NULL))
                                {
#endif
                                   if ((status = pipe_write(fdm, "no\n", 3)) != 3)
                                   {
                                      if (errno != 0)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                                   "write() error [%d] : %s",
                                                   status, strerror(errno));
                                      }
                                   }
                                   else
                                   {
                                      msg_str[0] = '\0';
                                      if ((status = get_ssh_reply(fdm, YES)) != SUCCESS)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                                   "Failed to send no to verify ssh connection. [%d]",
                                                   status);
                                      }
                                   }
                                   trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                             "Please connect to this host with the command line SSH utility and answer this question appropriately, or use fingerprints.");
#else
                                             "Please connect to this host with the command line SSH utility and answer this question appropriately.");
#endif
                                   status = INCORRECT;
#ifdef WITH_SSH_FINGERPRINT
                                }
                                else
                                {
                                   if ((status = pipe_write(fdm, "yes\n", 4)) != 4)
                                   {
                                      if (errno != 0)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                                   "write() error [%d] : %s",
                                                   status, strerror(errno));
                                      }
                                      status = INCORRECT;
                                   }
                                   else
                                   {
                                      msg_str[0] = '\0';
                                      if ((status = get_ssh_reply(fdm, YES)) != SUCCESS)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                                   "Failed to send no to verify ssh connection. [%d]",
                                                   status);
                                      }
                                      continue;
                                   }
                                }
#endif
                             }
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
                        else if ((posi(ptr, "DOING SOMETHING NASTY!") != NULL) ||
                                 (posi(ptr, "man-in-the-middle attack") != NULL) ||
                                 (posi(ptr, "known_hosts") != NULL))
                             {
                                if ((fingerprint[0] != '\0') &&
                                    (posi(ptr, fingerprint) != NULL))
                                {
                                   status = remove_from_knownhosts(sd.hostname);
                                }
                                else
                                {
                                   continue;
                                }
                             }
# endif
#endif
#ifdef WITH_EFENCE
                        else if (posi(ptr, "Electric Fence") != NULL)
                             {
                                status = SUCCESS;
                                continue;
                             }
#endif
                        else if (posi(ptr, "Warning: Permanently added") != NULL)
                             {
                                /* Just some info that key has been added. */
                                status = SUCCESS;
                                continue;
                             }
                             else
                             {
                                /* Replace '\n's by spaces for logging. */
                                while (*ptr++)
                                {
                                   if ((*ptr == '\n') || (*ptr == '\r'))
                                   {
                                      *ptr = ' ';
                                   }
                                }
                                trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                                          "Protocol error. ssh is complaining, see next message (%d).",
                                          status);
                                if (status == 1)
                                {
                                   trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                             "msg_str[0] = %d", msg_str[0]);
                                }
                                msg_str[0] = '\0';
                                status = INCORRECT;
                             }
                     }
                  }
                  break;
               }
      }
      else if (status == 0) /* Timeout. */
           {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "SSH program not responding.");
               status = INCORRECT;
               if (ssh_data_pid > 0)
               {
                  if (kill(ssh_data_pid, SIGKILL) == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "ssh_login(): Failed to kill() data ssh process %d : %s",
                               ssh_data_pid, strerror(errno));
                  }
                  else
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                               "ssh_login(): Killing hanging data ssh process.");
                  }
               }
               break;
           }
           else
           {
              if ((errno != EINTR) && (errno != EAGAIN))
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "select() error : %s", strerror(errno));
                 break;
              }
           }
   }

   if (password)
   {
      (void)free(password);
   }
   return(status);
}


/*############################# pipe_write() ############################*/
size_t
pipe_write(int fd, char *buf, size_t count)
{
   if (fd != -1)
   {
      int            status;
      fd_set         wset;
      struct timeval timeout;

      /* Initialise descriptor set. */
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(fd + 1, NULL, &wset, NULL, &timeout);
      if (status == 0)
      {
         /* Timeout has arrived. */
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "There is no reply from pipe, failed to send command %s.",
                   buf);
      }
      else if (FD_ISSET(fd, &wset))
           {
#ifdef _WITH_DEBUG
              char tmp_char;

              tmp_char = buf[count - 1];
              buf[count - 1] = '\0';
              trans_log(INFO_SIGN, NULL, 0, NULL, "== Pipe Writting (%d) ==",
                        count);
              buf[count - 1] = tmp_char;
#endif /* _WITH_DEBUG */
#ifdef WITH_TRACE
              trace_log(NULL, 0, W_TRACE, buf, count, NULL);
#endif
              return(write(fd, buf, count));
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "select() error : %s", strerror(errno));
           }
   }
   errno = 0;
   return(INCORRECT);
}


#ifdef WITH_TRACE
/*########################### pipe_write_np() ###########################*/
static size_t
pipe_write_np(int fd, char *buf, size_t count)
{
   if (fd != -1)
   {
      int            status;
      fd_set         wset;
      struct timeval timeout;

      /* Initialise descriptor set. */
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(fd + 1, NULL, &wset, NULL, &timeout);
      if (status == 0)
      {
         /* Timeout has arrived. */
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "There is no reply from pipe, failed to send command %s.",
                   buf);
      }
      else if (FD_ISSET(fd, &wset))
           {
# ifdef _WITH_DEBUG
              char tmp_char;

              tmp_char = buf[count - 1];
              buf[count - 1] = '\0';
              trans_log(INFO_SIGN, NULL, 0, NULL, "== Pipe Writting (%d) ==",
                        count);
              buf[count - 1] = tmp_char;
# endif /* _WITH_DEBUG */
              trace_log(NULL, 0, W_TRACE, "XXXX", 4, NULL);
              return(write(fd, buf, count));
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "select() error : %s", strerror(errno));
           }
   }
   errno = 0;
   return(INCORRECT);
}
#endif /* WITH_TRACE */


/*########################### get_ssh_reply() ###########################*/
int
get_ssh_reply(int fd, int check_reply)
{
   int            status;
   fd_set         rset;
   struct timeval timeout;

   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      msg_str[0] = '\0';
      timeout_flag = ON;
      status = INCORRECT;
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              if (errno == ECONNRESET)
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "read() error : %s", strerror(errno));
              status = INCORRECT;
           }
           else
           {
#ifdef WITH_TRACE
              trace_log(NULL, 0, R_TRACE, msg_str, status, NULL);
#endif
              msg_str[status] = '\0';
              if (status == 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "Other side closed the pipe.");
                 status = INCORRECT;
              }
              else
              {
                 char *ptr = msg_str;

                 while (*ptr)
                 {
                    if (*ptr == '\n')
                    {
                       *ptr = ' ';
                    }
                    ptr++;
                 }
#ifdef _WITH_DEBUG
                 if (status == 1)
                 {
                    trans_log(INFO_SIGN, NULL, 0, NULL,
                              "== Reading ONE Byte %d ==", (int)msg_str[0]);
                 }
                 else
                 {
                    trans_log(INFO_SIGN, NULL, 0, NULL,
                              "== Reading (%d) ==", status);
                 }
#endif
                 if (check_reply == YES)
                 {
                    if (msg_str[status - 1] == '\n')
                    {
                       msg_str[status - 1] = '\0';
                    }

                    if ((msg_str[0] == 1) || /* This is a ^A */
                        (msg_str[0] == 2))   /* Fatal error. */
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "scp error : %s", msg_str + 1);
                       status = INCORRECT; /* This could be an exit !! */
                    }
                    else
                    {
                       status = SUCCESS;
                    }
                 }
              }
           }
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "select() error : %s", strerror(errno));
           status = INCORRECT;
        }

   return(status);
}


#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
/*###################### remove_from_knownhosts() #######################*/
static int
remove_from_knownhosts(char *hostname)
{
   int           ret;
   struct passwd *pwd;

   if ((pwd = getpwuid(getuid())) != NULL)
   {
      int  fd;
      char fullname[MAX_PATH_LENGTH];

      (void)sprintf(fullname, "%s/.ssh/known_hosts", pwd->pw_dir);

      if ((fd = lock_file(fullname, ON)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open/lock file `%s' : %s",
                    fullname, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         struct stat stat_buf;

         if (fstat(fd, &stat_buf) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to fstat() `%s' : %s",
                       fullname, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            if (stat_buf.st_size == 0)
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                         "`%s' is empty.", fullname);
               ret = INCORRECT;
            }
            else
            {
               char *data;

               if ((data = mmap(NULL, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                                MAP_SHARED, fd, 0)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap() error : %s", strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
                  size_t remove_size;
                  char   *ptr;

                  remove_size = 0;
                  ptr = data;
                  ret = RETRY;
                  while ((remove_size == 0) && (ret != INCORRECT))
                  {
                     if ((ptr = posi(ptr, hostname)) == NULL)
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                  "Failed to locate `%s' in `%s'",
                                  hostname, fullname);
                        ret = INCORRECT;
                     }
                     else
                     {
                        char *p_tmp = ptr;

                        ptr = ptr - strlen(hostname) - 1;
                        if ((ptr == data) || (*(ptr - 1) == '\n') ||
                            (*(ptr - 1) == ','))
                        {
                           char *p_start;

                           /* We might be looking at the IP number that is */
                           /* following the real hostname.                 */
                           if ((ptr != data) && (*(ptr - 1) == ','))
                           {
                              ptr -= 2;
                              while ((ptr > data) && (*ptr != '\n'))
                              {
                                 ptr--;
                              }
                           }
                           p_start = ptr;

                           while ((ptr < (data + stat_buf.st_size)) &&
                                  (*ptr != '\n'))
                           {
                              ptr++;
                           }
                           if (*ptr == '\n')
                           {
                              ptr++;
                              if (ptr < (data + stat_buf.st_size))
                              {
                                 (void)memmove(p_start, ptr,
                                               ((data + stat_buf.st_size) - ptr));
                                 if (msync(data, stat_buf.st_size, MS_SYNC) == -1)
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               "msync() error : %s",
                                               strerror(errno));
                                 }
                              }
                           }
                           remove_size = ptr - p_start;
                        }
                        else
                        {
                           ptr = p_tmp;
                        }
                     }
                  }
                  if (munmap(data, stat_buf.st_size) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "munmap() error : %s", strerror(errno));
                  }
                  if (remove_size > 0)
                  {
                     if (ftruncate(fd, (stat_buf.st_size - remove_size)) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "ftruncate() error : %s", strerror(errno));
                        ret = INCORRECT;
                     }
                  }
                  else
                  {
                     ret = INCORRECT;
                  }
               }
            } /* (stat_buf.st_size > 0) */
         }
         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to close() `%s' : %s",
                       fullname, strerror(errno));
         }
      }
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "getpwuid() error : %s", strerror(errno));
      ret = INCORRECT;
   }

   return(ret);
}
# endif
#endif


/*-------------------------- get_passwd_reply() -------------------------*/
static int
get_passwd_reply(int fd)
{
   int            status;
   fd_set         rset;
   struct timeval timeout;

   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);
  
   if (status == 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Timeout while waiting for password responce.");
      timeout_flag = ON;
      status = INCORRECT;
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "read() error : %s", strerror(errno));
           }
#ifdef WITH_TRACE
           else
           {
              trace_log(NULL, 0, BIN_CMD_R_TRACE, msg_str, status, NULL);
           }
#endif
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "select() error : %s", strerror(errno));
        }

   return(status);
}


/*----------------------------- ptym_open() -----------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens.                                     */
/*-----------------------------------------------------------------------*/
static int
ptym_open(char *pts_name)
{
   int  fd;
#ifdef HAVE_OPENPTY
   int dummy_fd;

   fd = dummy_fd = 0;
   if (openpty(&fd, &dummy_fd, pts_name, NULL, NULL) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "openpty() error : %s", strerror(errno));
      if (fd != 0)
      {
         (void)close(fd);
      }
      if (dummy_fd != 0)
      {
         (void)close(dummy_fd);
      }
   }
   else
   {
      (void)close(dummy_fd);
   }
   return(fd);
#else
# ifdef HAVE__GETPTY
   char *tempstr;

   if ((tempstr = _getpty(&fd, O_RDWR, 0600, 0)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "_getpty() error : %s", strerror(errno));
      return(-1);
   }
   (void)strcpy(pts_name, tempstr);
   return(fd);
# else
#  ifdef HAVE_GETPT
   char *tempstr;

   if ((fd = getpt()) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "getpt() error : %s", strerror(errno));
      return(-1);
   }
   if ((tempstr = ptsname(fd)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ptsname() error : %s", strerror(errno));
      return(-1);
   }
   (void)strcpy(pts_name, tempstr);
   return(fd);
#  else
#   ifdef SYSV
   char *tempstr;

   strcpy(pts_name, "/dev/ptmx");
   if ((fd = open(pts_name, O_RDWR)) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Failed to open() `%s' error : %s", pts_name, strerror(errno));
      return(-1);
   }

   if (grantpt(fd) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "grantpt() error : %s", strerror(errno));
      (void)close(fd);
      return(-1);
   }

   if (unlockpt(fd) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "unlockpt() error : %s", strerror(errno));
      (void)close(fd);
      return(-1);
   }

   if ((tempstr = ptsname(fd)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "tempstr() error : %s", strerror(errno));
      (void)close(fd);
      return(-1);
   }

   (void)strcpy(pts_name, tempstr);
   return(fd);
#   else
   char *pos1, *pos2;

   (void)strcpy(pts_name, "/dev/ptyXY");
   for (pos1 = "pqrstuvwxyzPQRST"; *pos1 != '\0'; pos1++)
   {
      pts_name[8] = *pos1;
      for (pos2 = "0123456789abcdef"; *pos2 != '\0'; pos2++)
      {
         pts_name[9] = *pos2;
         if ((fd = open(pts_name, O_RDWR)) == -1)
         {
            if (errno == ENOENT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to open() `%s' error : %s",
                         pts_name, strerror(errno));
               return (-1);
            }
            else
            {
               continue;
            }
         }
         pts_name[5] = 't';
         return(fd);
      }
   }
   return(-1);
#   endif
#  endif
# endif
#endif
}


/*----------------------------- ptys_open() -----------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens.                                     */
/*-----------------------------------------------------------------------*/
static int
ptys_open(char *pts_name)
{
   int fds;

   if ((fds = open(pts_name, O_RDWR)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Failed to open() `%s' error : %s", pts_name, strerror(errno));
      return(-1);
   }

#if defined(SYSV)
   if (ioctl(fds, I_PUSH, "ptem") < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ioctl() error : %s", strerror(errno));
      (void)close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ldterm") < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ioctl() error : %s", strerror(errno));
      (void)close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ttcompat") < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ioctl() error : %s", strerror(errno));
      (void)close(fds);
      return(-1);
   }
#endif

#if !defined(SYSV) && defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(HAVE__GETPTY)
   if (ioctl(fds, TIOCSCTTY, (char *) 0) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "ioctl() error : %s", strerror(errno));
      (void)close(fds);
      return(-1);
   }
#endif

   return(fds);
}


/*------------------------------ tty_raw() ------------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens. The modification where taken from   */
/* gFTP software package by Brian Masney.                                */
/*-----------------------------------------------------------------------*/
static int
tty_raw(int fd)
{
   struct termios buf;

   if (tcgetattr(fd, &buf) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "tcgetattr() error : %s", strerror(errno));
      return(-1);
   }
   buf.c_iflag |= IGNPAR;
   buf.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON | IGNCR | IXANY | IXOFF | INLCR);
   buf.c_lflag &= ~(ECHO | ICANON | ISIG | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
   buf.c_lflag &= ~(IEXTEN);
#endif
   buf.c_cflag &= ~(CSIZE | PARENB);
   buf.c_cflag |= CS8;
   buf.c_oflag &= ~(OPOST);  /* Output processing off. */
   buf.c_cc[VMIN] = 1;       /* Case B: 1 byte at a time, no timer. */
   buf.c_cc[VTIME] = 0;

   if (tcsetattr(fd, TCSANOW, &buf) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "tcsetattr() error : %s", strerror(errno));
      return(-1);
   }
   return(0);
}


/*+++++++++++++++++++++++++++++ sig_handler() +++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}
