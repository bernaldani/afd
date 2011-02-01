/*
 *  afdd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afdd - TCP command daemon for the AFD
 **
 ** SYNOPSIS
 **   afdd [--version][-w <AFD working directory>]
 **
 ** DESCRIPTION
 **   This is a small tcp command server at port AFD_PORT_NO that
 **   returns information on the AFD. It functions very similar to
 **   the ftpd except that it does not use a data connection to transmit
 **   the information. The control connection is used instead.
 **
 **   The following commands are supported:
 **      HELP [<sp> <command>]      - Shows all commands supported or help
 **                                   on a specific command.
 **      QUIT                       - Terminate service.
 **
 ** RETURN VALUES
 **   Will exit with INCORRECT when some system call failed.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.06.1997 H.Kiehl Created
 **   22.08.1998 H.Kiehl Added some more exit handlers.
 **   26.01.2006 H.Kiehl Made MAX_AFDD_CONNECTIONS configurable in AFD_CONFIG.
 **   23.11.2008 H.Kiehl Added danger_no_of_jobs.
 **   04.01.2011 H.Kiehl Ensure that we do not set a port that is in the
 **                      dynamic port range.
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), getenv(), malloc()              */
#include <ctype.h>            /* isdigit()                               */
#include <time.h>             /* clock_t                                 */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>         /* waitpid()                               */
#include <netinet/in.h>
#include <arpa/inet.h>        /* inet_ntoa()                             */
#include <unistd.h>           /* close(), select(), sysconf(), pathconf()*/
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "afdddefs.h"
#include "version.h"

/* Global variables. */
int               default_log_defs = DEFAULT_AFDD_LOG_DEFS,
                  *ip_log_defs = NULL,
                  log_defs = 0,
                  number_of_trusted_ips,
                  sys_log_fd = STDERR_FILENO;
long              danger_no_of_jobs;
clock_t           clktck;
char              afd_config_file[MAX_PATH_LENGTH],
                  afd_name[MAX_AFD_NAME_LENGTH],
                  hostname[MAX_FULL_USER_ID_LENGTH],
                  *p_work_dir,
                  *p_work_dir_end,
                  **trusted_ip = NULL;
struct afd_status *p_afd_status;
struct logdata    ld[NO_OF_LOGS];
const char        *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int        in_child = NO,
                  max_afdd_connections = MAX_AFDD_CONNECTIONS,
                  new_sockfd,
                  sockfd,
                  no_of_connections;
static pid_t      *pid;

/* Local function prototypes. */
static int        get_free_connection(void);
static void       afdd_exit(void),
                  get_afdd_config_value(char *, int *),
#ifdef LINUX
                  get_ip_local_port_range(int *, int *),
#endif
                  sig_bus(int),
                  sig_exit(int),
                  sig_segv(int),
                  zombie_check(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                length,
                      pos,
                      port,
                      ports_tried = 0,
                      on = 1,
                      status,
                      trusted_ip_pos = -1;
   my_socklen_t       peer_addrlen;
   char               *ptr,
                      port_no[MAX_INT_LENGTH],
                      reply[256],
                      work_dir[MAX_PATH_LENGTH];
   fd_set             rset;
   struct servent     *p_service;
   struct protoent    *p_protocol;
   struct sockaddr_in data,
                      peer_address;
   struct timeval     timeout;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialize variables. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   p_work_dir_end = work_dir + strlen(work_dir);
   no_of_connections = 0;
   (void)strcpy(port_no, DEFAULT_AFD_PORT_NO);
   get_afdd_config_value(port_no, &max_afdd_connections);
   if ((pid = malloc((max_afdd_connections * sizeof(pid_t)))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to malloc() %d bytes : %s"),
                 (max_afdd_connections * sizeof(pid_t)),
                 strerror(errno));
      exit(INCORRECT);
   }
   (void)memset(pid, 0, max_afdd_connections * sizeof(pid_t));
   if ((ptr = getenv("LOGNAME")) != NULL)
   {
      length = strlen(ptr);
      if (length > 0)
      {
         if ((length + 1) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(hostname, ptr);
            (void)strcat(hostname, "@");
            length++;
         }
         else
         {
            memcpy(hostname, ptr, MAX_FULL_USER_ID_LENGTH - 1);
            hostname[MAX_FULL_USER_ID_LENGTH - 1] = '\0';
            length = MAX_FULL_USER_ID_LENGTH;
         }
      }
      else
      {
#if MAX_FULL_USER_ID_LENGTH > 8
         (void)strcpy(hostname, "unknown@");
         length = 8;
#else
         hostname[0] = '\0';
         length = 0;
#endif
      }
   }
   else
   {
#if MAX_FULL_USER_ID_LENGTH > 8
      (void)strcpy(hostname, "unknown@");
      length = 8;
#else
      hostname[0] = '\0';
      length = 0;
#endif
   }
   if (length < MAX_FULL_USER_ID_LENGTH)
   {
      if (gethostname(&hostname[length], MAX_FULL_USER_ID_LENGTH - length) != 0)
      {
         if ((length + 7) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(&hostname[length], "unknown");
         }
      }
   }
   port = atoi(port_no);
   if (get_afd_name(afd_name) == INCORRECT)
   {
      afd_name[0] = '\0';
   }
   peer_addrlen = sizeof(peer_address);

   /* Initialize the log structure. */
   (void)memset(ld, 0, (NO_OF_LOGS * sizeof(struct logdata)));

   /* Do some cleanups when we exit. */
   if (atexit(afdd_exit) != 0)          
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not register exit handler : %s"), strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not set signal handlers : %s"), strerror(errno));
      exit(INCORRECT);
   }

   if ((ptr = lock_proc(AFDD_LOCK_ID, NO)) != NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Process AFDD already started by %s"), ptr);
      (void)fprintf(stderr, _("Process AFDD already started by %s : (%s %d)\n"),
                    ptr, __FILE__, __LINE__);
      _exit(INCORRECT);
   }

   /*
    * Get clock ticks per second, so we can calculate the transfer time.
    */
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not get clock ticks per second : %s (%s %d)\n"),
                 strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Get maximum number of links to determine danger_no_of_jobs.
    */
#ifdef _LINK_MAX_TEST
   danger_no_of_jobs = LINKY_MAX;
#else
# ifdef REDUCED_LINK_MAX
   danger_no_of_jobs = REDUCED_LINK_MAX;
# else
   (void)strcpy(p_work_dir_end, AFD_FILE_DIR);
   if ((danger_no_of_jobs = pathconf(work_dir, _PC_LINK_MAX)) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("pathconf() _PC_LINK_MAX error, setting to %d : %s"),
                 _POSIX_LINK_MAX, strerror(errno));                  
      danger_no_of_jobs = _POSIX_LINK_MAX;
   }
   *p_work_dir_end = '\0';
# endif
#endif
   danger_no_of_jobs = danger_no_of_jobs / 2;

   /* Attach to the AFD Status Area. */
   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to map to AFD status area."));
      exit(INCORRECT);
   }

   (void)memset((struct sockaddr *) &data, 0, sizeof(data));
   data.sin_family = AF_INET;
   data.sin_addr.s_addr = INADDR_ANY;

   /* Get full name to AFD_CONFIG file. */
   (void)sprintf(afd_config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);

   do
   {
      if ((p_service = getservbyname(port_no, "tcp")))
      {
         data.sin_port = htons(ntohs((u_short)p_service->s_port));
      }
      else if ((data.sin_port = htons((u_short)atoi(port_no))) == 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "Failed to copy service to structure.");
              exit(INCORRECT);
           }

      if ((p_protocol = getprotobyname("tcp")) == 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to get protocol tcp : %s"), strerror(errno));
         exit(INCORRECT);
      }

      if ((sockfd = socket(AF_INET, SOCK_STREAM, p_protocol->p_proto)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Could not create socket : %s"), strerror(errno));
         exit(INCORRECT);
      }

      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("setsockopt() error : %s"), strerror(errno));
         (void)close(sockfd);
         exit(INCORRECT);
      }

      if ((status = bind(sockfd, (struct sockaddr *) &data, sizeof(data))) == -1)
      {
         ports_tried++;
         port++;
         (void)sprintf(port_no, "%d", port);
         if ((errno != EADDRINUSE) || (ports_tried > 100))
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("bind() error : %s"), strerror(errno));
            (void)close(sockfd);
            exit(INCORRECT);
         }
         if (close(sockfd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   } while (status == -1);

   system_log(INFO_SIGN, NULL, 0, _("Starting %s at port %d (%s)"),
              AFDD, port, PACKAGE_VERSION);

   if (listen(sockfd, 5) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("listen() error : %s"), strerror(errno));
      (void)close(sockfd);
      exit(INCORRECT);
   }
   FD_ZERO(&rset);

   for (;;)
   {
      /* Initialise descriptor set. */
      FD_SET(sockfd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 5L;

      if (select(sockfd + 1, &rset, NULL, NULL, &timeout) < 0)
      {
         (void)close(sockfd);
         if (errno != EBADF)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("select() error : %s"), strerror(errno));
         }
         exit(INCORRECT);
      }

      if (FD_ISSET(sockfd, &rset))
      {
         char remote_ip_str[MAX_IP_LENGTH];

         if ((new_sockfd = accept(sockfd, (struct sockaddr *)&peer_address,
                                  &peer_addrlen)) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("accept() error : %s"), strerror(errno));
            (void)close(sockfd);
            exit(INCORRECT);
         }
         (void)strcpy(remote_ip_str, inet_ntoa(peer_address.sin_addr));

         if (number_of_trusted_ips > 0)
         {
            int gotcha = NO,
                i;

            for (i = 0; i < number_of_trusted_ips; i++)
            {
               if (pmatch(trusted_ip[i], remote_ip_str, NULL) == 0)
               {
                  gotcha = YES;
                  trusted_ip_pos = i;
                  break;
               }
            }
            if (gotcha == NO)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("AFDD: Illegal access from %s"), remote_ip_str);
               (void)close(new_sockfd);
               continue;
            }
            if (no_of_connections >= max_afdd_connections)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("AFDD: Connection attempt from %s, but denied because max connection (%d) reached."),
                          remote_ip_str, max_afdd_connections);
            }
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("AFDD: Connection from %s"), remote_ip_str);
            }
         }
         else
         {
            trusted_ip_pos = 0;
            if (no_of_connections >= max_afdd_connections)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("AFDD: Connection attempt from %s, but denied because max connection (%d) reached."),
                          remote_ip_str, max_afdd_connections);
            }
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("AFDD: Connection from %s"), remote_ip_str);
            }
         }

         if (no_of_connections >= max_afdd_connections)
         {
            length = sprintf(reply,
                             "421 Service not available. There are currently to many connections (%d).\r\n",
                             no_of_connections);
            (void)write(new_sockfd, reply, length);
            (void)close(new_sockfd);
         }
         else
         {
            if ((pos = get_free_connection()) < 0)
            {
               length = sprintf(reply, "421 Service not available.\r\n");
               (void)write(new_sockfd, reply, length);
               (void)close(new_sockfd);
            }
            else
            {
               switch (pid[pos] = fork())
               {
                  case -1 : /* Could not generate process. */
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("fork() error : %s"), strerror(errno));
                     break;

                  case 0  : /* Child process to serve user. */
                     in_child = YES;
                     (void)close(sockfd);
                     handle_request(new_sockfd, pos, trusted_ip_pos,
                                    remote_ip_str);
                     exit(0);

                  default : /* Parent process. */
                     (void)close(new_sockfd);
                     no_of_connections++;
                     break;
               }
            }
         }
      } /* if (FD_ISSET(sockfd, &rset)) */

      zombie_check();
   } /* for (;;) */
}


/*++++++++++++++++++++++++++ get_free_connection() ++++++++++++++++++++++*/
static int
get_free_connection(void)
{
   int i;

   for (i = 0; i < max_afdd_connections; i++)
   {
      if (pid[i] == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*                              --------------                          */
/* Description : Checks if any process is finished (zombie), if this    */
/*               is the case it is killed with waitpid().               */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
zombie_check(void)
{
   int i,
       status;

   /* Did any of the jobs become zombies? */
   for (i = 0; i < max_afdd_connections; i++)
   {
      if ((pid[i] > 0) &&
          (waitpid(pid[i], &status, WNOHANG) > 0))
      {
         if (WIFEXITED(status))
         {
            pid[i] = 0;
            no_of_connections--;
         }
         else if (WIFSIGNALED(status))
              {
                 /* Abnormal termination. */
                 pid[i] = 0;
                 no_of_connections--;
              }
         else if (WIFSTOPPED(status))
              {
                 /* Child stopped. */;
              }
      }
   }

   return;
}


/*++++++++++++++++++++++++ get_afdd_config_value() ++++++++++++++++++++++*/
static void                                                                
get_afdd_config_value(char *port_no, int *max_afdd_connections)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s", p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer) != INCORRECT))
   {
      char *ptr = buffer,
           tmp_trusted_ip[MAX_IP_LENGTH + 1],
           value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_AFDD_CONNECTIONS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_afdd_connections = atoi(value);
         if (*max_afdd_connections < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Setting to default %d."),
                       *max_afdd_connections, MAX_AFDD_CONNECTIONS_DEF,
                       MAX_AFDD_CONNECTIONS);
            *max_afdd_connections = MAX_AFDD_CONNECTIONS;
         }
      }

      if (get_definition(buffer, AFD_TCP_PORT_DEF,
                         port_no, MAX_INT_LENGTH) != NULL)
      {
         int lower_limit = 49152,
             port,
             upper_limit = 65535;

         port = atoi(port_no);
#ifdef LINUX
         get_ip_local_port_range(&lower_limit, &upper_limit);
#endif
         if ((port >= lower_limit) && (port <= upper_limit))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Invalid port number given %d (lower limit = %d, upper limit = %d) in AFD_CONFIG, setting to default %s."),
                       port, lower_limit, upper_limit, DEFAULT_AFD_PORT_NO);
            (void)strcpy(port_no, DEFAULT_AFD_PORT_NO);
         }
      }

      if (get_definition(buffer, AFD_TCP_LOGS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_log_defs = atoi(value);
      }

      /*
       * Read all IP-numbers that may connect to AFDD. If none is found
       * all IP's may connect.
       */
      do
      {
         if ((ptr = get_definition(ptr, TRUSTED_REMOTE_IP_DEF, tmp_trusted_ip,
                                   MAX_IP_LENGTH)) != NULL)
         {
            int  counter = 0;
            char *check_ptr;

            check_ptr = tmp_trusted_ip;
            while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                    (*check_ptr == '?')) && (counter < 3))
            {
               check_ptr++; counter++;
            }
            if ((counter < 4) && (*check_ptr == '.'))
            {
               check_ptr++;
               counter = 0;
               while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                       (*check_ptr == '?')) && (counter < 3))
               {
                  check_ptr++; counter++;
               }
               if ((counter < 4) && (*check_ptr == '.'))
               {
                  check_ptr++;
                  counter = 0;
                  while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                          (*check_ptr == '?')) && (counter < 3))
                  {
                     check_ptr++; counter++;
                  }
                  if ((counter < 4) && (*check_ptr == '.'))
                  {
                     check_ptr++;
                     counter = 0;
                     while (((isdigit((int)(*check_ptr))) || (*check_ptr == '*') ||
                             (*check_ptr == '?')) && (counter < 4))
                     {
                        check_ptr++; counter++;
                     }
                     if ((counter < 4) && ((*check_ptr == '\n') ||
                         (*check_ptr == '\0')))
                     {
                        number_of_trusted_ips++;
                        if (number_of_trusted_ips == 1)
                        {
                           RT_ARRAY(trusted_ip, number_of_trusted_ips,
                                    MAX_IP_LENGTH, char);
                           if ((ip_log_defs = malloc(sizeof(int))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to malloc() %d bytes : %s"),
                                         sizeof(int), strerror(errno));
                              exit(INCORRECT);
                           }
                           ip_log_defs[0] = default_log_defs;
                        }
                        else
                        {
                           REALLOC_RT_ARRAY(trusted_ip,
                                            number_of_trusted_ips,
                                            MAX_IP_LENGTH,
                                            char);
                           if ((ip_log_defs = realloc(ip_log_defs,
                                                      number_of_trusted_ips * sizeof(int))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to realloc() %d bytes : %s"),
                                         number_of_trusted_ips * sizeof(int),
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                           ip_log_defs[number_of_trusted_ips - 1] = default_log_defs;
                        }
                        (void)strcpy(trusted_ip[number_of_trusted_ips - 1],
                                     tmp_trusted_ip);
                     }

                     /* Check if log definitions have been added for this ip. */
                     if (*ptr == ' ')
                     {
                        while (*ptr == ' ')
                        {
                           ptr++;
                        }
                        counter = 0;
                        check_ptr = ptr;
                        while ((isdigit((int)(*check_ptr))) &&
                               (*check_ptr != '\n') && (*check_ptr != '\0') && 
                               (counter < MAX_INT_LENGTH))
                        {
                           check_ptr++; counter++;
                        }
                        if (check_ptr != ptr)
                        {
                           ip_log_defs[number_of_trusted_ips - 1] = atoi(ptr);
                        }
                     }
                  }
               }
            }
         }
      } while (ptr != NULL);
      free(buffer);

      if (ip_log_defs == NULL)
      {
         if ((ip_log_defs = malloc(sizeof(int))) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to malloc() %d bytes : %s"),
                       sizeof(int), strerror(errno));
            exit(INCORRECT);
         }
         ip_log_defs[0] = default_log_defs;
      }
   }

   return;
}


#ifdef LINUX
# define LOCAL_IP_RANGE_PROC_FILE "/proc/sys/net/ipv4/ip_local_port_range"

/*---------------------- get_ip_local_port_range() ----------------------*/
static void
get_ip_local_port_range(int *lower_limit, int *upper_limit)
{
   int  fd,
        i,
        ret;
   char buffer[MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1],
        *ptr,
        str_number[MAX_INT_LENGTH + 1];

   if ((fd = open(LOCAL_IP_RANGE_PROC_FILE, O_RDONLY)) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s",
                 LOCAL_IP_RANGE_PROC_FILE, strerror(errno));
      return;
   }
   if ((ret = read(fd, buffer, MAX_INT_LENGTH + 1 + MAX_INT_LENGTH)) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to read() %s : %s",
                 LOCAL_IP_RANGE_PROC_FILE, strerror(errno));
      return;
   }
   buffer[ret] = '\0';
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to close() %s : %s",
                 LOCAL_IP_RANGE_PROC_FILE, strerror(errno));
   }

   ptr = buffer;
   while ((*ptr == ' ') || (*ptr == '\t'))
   {
      ptr++;
   }
   i = 0;
   while ((isdigit(*ptr)) && (i < MAX_INT_LENGTH) && (*ptr != '\0'))
   {
      str_number[i] = *ptr;
      i++; ptr++;
   }
   if ((i > 0) && (i < MAX_INT_LENGTH))
   {
      str_number[i] = '\0';
      *lower_limit = atoi(str_number);
   }
   else
   {
      return;
   }

   while ((*ptr == ' ') || (*ptr == '\t'))
   {
      ptr++;
   }
   i = 0;
   while ((isdigit(*ptr)) && (i < MAX_INT_LENGTH) && (*ptr != '\0'))
   {
      str_number[i] = *ptr;
      i++; ptr++;
   }
   if ((i > 0) && (i < MAX_INT_LENGTH))
   {
      str_number[i] = '\0';
      *upper_limit = atoi(str_number);
   }
   else
   {
      return;
   }

   return;
}
#endif


/*++++++++++++++++++++++++++++++ afdd_exit() ++++++++++++++++++++++++++++*/
static void
afdd_exit(void)
{
   if (in_child == NO)
   {
      int i;

      /* Kill all child process. */
      for (i = 0; i < max_afdd_connections; i++)
      {
         if (pid[i] > 0)
         {
            if (kill(pid[i], SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             _("Failed to kill() %d : %s"),
#else
                             _("Failed to kill() %lld : %s"),
#endif
                             (pri_pid_t)pid[i], strerror(errno));
               }
            }
         }
      }

      system_log(INFO_SIGN, NULL, 0, _("Stopped %s."), AFDD);
   }

   (void)close(sockfd);
   (void)close(new_sockfd);
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              _("Aaarrrggh! Received SIGSEGV."));
   afdd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, _("Uuurrrggh! Received SIGBUS."));
   afdd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*+++++++++++++++++++++++++++++++ sig_exit() ++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
