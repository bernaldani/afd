/*
 *  fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2013 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   fd - creates transfer jobs and manages them
 **
 ** SYNOPSIS
 **   fd [--version] [-w <AFD working directory>]
 **
 ** DESCRIPTION
 **   This program starts at the most MAX_DEFAULT_CONNECTIONS jobs
 **   (parallel), to send files to certain destinations. It always
 **   waits for these process to finish and will generate an
 **   appropriate message when one has finished.
 **   To start a new job it always looks every FD_RESCAN_TIME seconds
 **   in the message directory to see whether a new message has arrived.
 **   This message will then be moved to the transmitting directory
 **   and will then start sf_xxx. If sf_xxx ends successfully it will
 **   remove this message (or archive it) else the fd will move the
 **   message and the appropriate files into the error directories.
 **
 **   The FD communicates with the AFD via the to fifo FD_CMD_FIFO.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.10.1995 H.Kiehl Created
 **   07.01.1997 H.Kiehl Implemented automatic switching when secondary
 **                      host is available.
 **   25.01.1997 H.Kiehl Add support for output logging.
 **   30.01.1997 H.Kiehl Added burst mode.
 **   02.05.1997 H.Kiehl Include support for MAP.
 **   15.01.1998 H.Kiehl Complete redesign to accommodate new messages.
 **   09.07.1998 H.Kiehl Option to delete all jobs from a specific host.
 **   10.08.2000 H.Kiehl Added support to fetch files from a remote host.
 **   14.12.2000 H.Kiehl Decrease priority of a job in queue on certain
 **                      errors of sf_xxx.
 **   16.04.2002 H.Kiehl Improve speed when inserting new jobs in queue.
 **   10.12.2003 H.Kiehl When we fail to fork we forget to reset some
 **                      structures.
 **   26.12.2003 H.Kiehl Added tracing.
 **   24.09.2004 H.Kiehl Added split job counter.
 **   07.03.2006 H.Kiehl Added group transfer limits.
 **   31.03.2008 H.Kiehl Check if qb->pos is still pointing to the correct
 **                      position in mdb.
 **
 */
DESCR__E_M1

/* #define WITH_MEMCHECK */

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strcpy(), strcat(), strerror(),         */
                              /* memset(), memmove()                     */
#include <stdlib.h>           /* getenv(), atexit(), exit()              */
#include <unistd.h>           /* unlink(), mkdir(), fpathconf()          */
#include <signal.h>           /* kill(), signal()                        */
#include <limits.h>           /* LINK_MAX                                */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WITH_MEMCHECK
# include <mcheck.h>
#endif
#include <sys/mman.h>         /* msync(), munmap()                       */
#include <sys/time.h>         /* struct timeval                          */
#ifdef HAVE_SETPRIORITY
# include <sys/resource.h>    /* getpriority(), setpriority()            */
#endif
#include <sys/wait.h>         /* waitpid()                               */
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <errno.h>
#include "fddefs.h"
#include "smtpdefs.h"         /* For SMTP_HOST_NAME.                     */
#include "httpdefs.h"         /* For HTTP_PROXY_NAME.                    */
#include "logdefs.h"
#include "version.h"

/* #define WITH_MULTI_FSA_CHECKS */
/* #define _MACRO_DEBUG */
/* #define _TESTEN_ */

/* Global variables. */
int                        amg_flag = NO,
                           default_age_limit = DEFAULT_AGE_LIMIT,
                           delete_jobs_fd,
                           event_log_fd = STDERR_FILENO,
                           fd_cmd_fd,
#ifdef HAVE_SETPRIORITY
                           add_afd_priority = DEFAULT_ADD_AFD_PRIORITY_DEF,
                           current_priority = 0,
                           max_sched_priority = DEFAULT_MAX_NICE_VALUE,
                           min_sched_priority = DEFAULT_MIN_NICE_VALUE,
#endif
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           delete_jobs_writefd,
                           fd_cmd_writefd,
                           fd_wake_up_writefd,
                           msg_fifo_writefd,
                           read_fin_writefd,
                           retry_writefd,
                           transfer_log_readfd,
                           trl_calc_writefd,
#endif
                           fd_wake_up_fd,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
#ifdef HAVE_HW_CRC32
                           have_hw_crc32,
#endif
                           last_pos_lookup = INCORRECT,
                           loop_counter = 0,
                           max_connections = MAX_DEFAULT_CONNECTIONS,
#ifdef _OUTPUT_LOG
                           max_output_log_files = MAX_OUTPUT_LOG_FILES,
#endif
                           mdb_fd = -1,
                           msg_fifo_fd,
                           *no_msg_queued,
                           *no_msg_cached,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           no_of_retrieves = 0,
                           no_of_trl_groups = 0,
                           no_of_zombie_waitstates = 0,
                           qb_fd = -1,
                           read_fin_fd,
                           remote_file_check_interval = DEFAULT_REMOTE_FILE_CHECK_INTERVAL,
                           remove_error_jobs_not_in_queue = NO,
                           *retrieve_list = NULL,
                           retry_fd,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trl_calc_fd,
                           *zwl;
long                       link_max;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
off_t                      *buf_file_size = NULL;
char                       stop_flag = 0,
                           *p_work_dir,
                           **p_buf_name = NULL,
                           *file_buffer = NULL,
                           *p_file_dir,
                           *p_msg_dir,
                           str_age_limit[MAX_INT_LENGTH],
                           str_fsa_id[MAX_INT_LENGTH],
                           str_remote_file_check_interval[MAX_INT_LENGTH],
                           file_dir[MAX_PATH_LENGTH],
                           msg_dir[MAX_PATH_LENGTH],
                           default_http_proxy[MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH],
                           *default_smtp_from,
                           *default_smtp_reply_to,
                           default_smtp_server[MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH];
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct afd_status          *p_afd_status;
struct connection          *connection = NULL;
struct queue_buf           *qb;
struct msg_cache_buf       *mdb;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int                 no_of_local_interfaces = 0;
static char                **local_interface_names = NULL;
static uid_t               euid, /* Effective user ID. */
                           ruid; /* Real user ID. */
static time_t              now;
static double              max_threshold;

#define START_PROCESS()                                       \
        {                                                     \
           int fsa_pos,                                       \
               kk;                                            \
                                                              \
           /* Try handle any pending jobs. */                 \
           for (kk = 0; ((kk < *no_msg_queued) && (p_afd_status->no_of_transfers < max_connections)); kk++) \
           {                                                  \
              if (qb[kk].pid == PENDING)                      \
              {                                               \
                 if (qb[kk].msg_name[0] != '\0')              \
                 {                                            \
                    fsa_pos = mdb[qb[kk].pos].fsa_pos;        \
                 }                                            \
                 else                                         \
                 {                                            \
                    fsa_pos = fra[qb[kk].pos].fsa_pos;        \
                 }                                            \
                 if ((qb[kk].pid = start_process(fsa_pos, kk, now, NO)) == REMOVED) \
                 {                                            \
                    /*                                        \
                     * The message can be removed because the \
                     * files are queued in another message    \
                     * or have been removed due to age.       \
                     */                                       \
                    remove_msg(kk);                           \
                    if (kk < *no_msg_queued)                  \
                    {                                         \
                       kk--;                                  \
                    }                                         \
                 }                                            \
              }                                               \
           }                                                  \
        }

/* Local function prototypes. */
static void  check_zombie_queue(time_t, int),
             fd_exit(void),
             get_afd_config_value(void),
             get_local_interface_names(void),
             qb_pos_fsa(int, int *),
             qb_pos_pid(pid_t, int *),
             sig_exit(int),
             sig_segv(int),
             sig_bus(int);
static int   check_local_interface_names(char *),
             get_free_connection(void),
             get_free_disp_pos(int),
             zombie_check(struct connection *, time_t, int *, int);
static pid_t make_process(struct connection *, int),
             start_process(int, int, time_t, int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              afd_status_fd,
                    fifo_full_counter = 0,
                    flush_msg_fifo_dump_queue = NO,
                    host_config_counter,
                    i,
                    status,
                    status_done,
                    max_fd,
                    nmsg_bytes_read = 0,
                    nmsg_bytes_buffered;
   unsigned int     *files_to_send,
                    *job_id,
                    last_job_id_lookup,
                    lookup_cache_hits = 0,
                    lookup_cache_misses = 0,
                    *split_job_counter,
                    *unique_number;
   unsigned short   *dir_no;
   long             fd_rescan_time;
   time_t           *creation_time,
                    abnormal_term_check_time,
#ifdef _WITH_INTERRUPT_JOB
                    interrupt_check_time,
#endif
                    remote_file_check_time;
   off_t            *file_size_to_send;
   size_t           fifo_size;
   char             *fifo_buffer,
                    *msg_buffer,
                    *msg_priority,
                    *nmsg_fifo_buffer,
                    *nmsg_read_ptr,
                    *originator,
                    work_dir[MAX_PATH_LENGTH];
   fd_set           rset;
   struct timeval   timeout;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif

#ifdef WITH_MEMCHECK
   mtrace();
#endif
   CHECK_FOR_VERSION(argc, argv);

   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to set back to the real user ID : %s",
                    strerror(errno));
      }
   }

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char *ptr;

      p_work_dir = work_dir;

      /*
       * Lock FD so no other FD can be started!
       */
      if ((ptr = lock_proc(FD_LOCK_ID, NO)) != NULL)
      {
         (void)fprintf(stderr,
                       "Process FD already started by %s : (%s %d)\n",
                       ptr, __FILE__, __LINE__);
	 system_log(ERROR_SIGN, __FILE__, __LINE__,
	            "Process FD already started by %s", ptr);
         exit(INCORRECT);
      }
   }

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, NULL) > 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "The compiled binary does not match stored database.");
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Initialize database with the command : afd -i");
      exit(INCORRECT);
   }

   /* Initialise variables. */
   (void)strcpy(msg_dir, work_dir);
   (void)strcat(msg_dir, AFD_MSG_DIR);
   (void)strcat(msg_dir, "/");
   (void)strcpy(file_dir, work_dir);
   (void)strcat(file_dir, AFD_FILE_DIR);
   (void)strcat(file_dir, OUTGOING_DIR);
   (void)strcat(file_dir, "/");
   p_msg_dir = msg_dir + strlen(msg_dir);
   p_file_dir = file_dir + strlen(file_dir);

#ifdef HAVE_UNSETENV
   /* Unset DISPLAY if exists, otherwise SSH might not work. */
   (void)unsetenv("DISPLAY");
#endif

   init_msg_ptrs(&creation_time,
                 &job_id,
                 &split_job_counter,
                 &files_to_send,
                 &file_size_to_send,
                 &dir_no,
                 &unique_number,
                 &msg_priority,
                 &originator,
                 &msg_buffer);

   /* Open and create all fifos. */
   if (init_fifos_fd() == INCORRECT)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to initialize fifos. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get the fra_id and no of directories of the FRA. */
   if (fra_attach() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      fra[i].queued = 0;
   }
   init_fra_data();

   /* Get the fsa_id and no of host of the FSA. */
   if (fsa_attach(FD) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }
#ifdef HAVE_SNPRINTF
   (void)snprintf(str_fsa_id, MAX_INT_LENGTH, "%d", fsa_id);
#else
   (void)sprintf(str_fsa_id, "%d", fsa_id);
#endif

   /* Attach to the AFD Status Area. */
   if (attach_afd_status(&afd_status_fd, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Failed to map to AFD status area.");
      exit(INCORRECT);
   }

   /* Initialize transfer rate limit data. */
   init_trl_data();

   /* Initialize all connections in case FD crashes. */
   p_afd_status->no_of_transfers = 0;
   for (i = 0; i < no_of_hosts; i++)
   {
      int j;

      fsa[i].active_transfers = 0;
      fsa[i].mc_nack_counter = 0;
      if ((no_of_trl_groups > 0) ||
          (fsa[i].transfer_rate_limit > 0))
      {
         calc_trl_per_process(i);
      }
      else
      {
         fsa[i].trl_per_process = 0;
         fsa[i].mc_ctrl_per_process = 0;
      }
      for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
      {
         fsa[i].job_status[j].no_of_files = 0;
         fsa[i].job_status[j].proc_id = -1;
         fsa[i].job_status[j].connect_status = DISCONNECT;
         fsa[i].job_status[j].file_name_in_use[0] = '\0';
         fsa[i].job_status[j].file_name_in_use[1] = 0;
      }
   }
   host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT);

   /* Initialize local interface names. */
   get_local_interface_names();

#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif

   /* Get value from AFD_CONFIG file. */
   get_afd_config_value();

   /* Attach/create memory area for message data and queue. */
   init_msg_buffer();

#ifdef _LINK_MAX_TEST
   link_max = LINKY_MAX;
#else
# ifdef REDUCED_LINK_MAX
   link_max = REDUCED_LINK_MAX;
# else                          
   if ((link_max = pathconf(work_dir, _PC_LINK_MAX)) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "pathconf() _PC_LINK_MAX error, setting to %d : %s",
                 _POSIX_LINK_MAX, strerror(errno));
      link_max = _POSIX_LINK_MAX;
   }
# endif
#endif

   /*
    * Initialize the queue and remove any queued retrieve job from
    * it. Because at this point we do not have the necessary information
    * to check if this job still does exists. Additionally check if
    * the pos value in qb still points to the correct position in
    * mdb (struct msg_cache_buf).
    */
   for (i = 0; i < *no_msg_queued; i++)
   {
      qb[i].pid = PENDING;
      if (qb[i].msg_name[0] == '\0')
      {
         if ((fra[qb[i].pos].fsa_pos >= 0) &&
             (fra[qb[i].pos].fsa_pos < no_of_hosts))
         {
            ABS_REDUCE(fra[qb[i].pos].fsa_pos);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Unable to reduce jobs_queued for FSA position %d since it is out of range (0 - %d), for queue position %d (i = %d).",
                       fra[qb[i].pos].fsa_pos, no_of_hosts, qb[i].pos, i);
         }
         remove_msg(i);
         i--;
      }
      else
      {
         errno = 0;
         last_job_id_lookup = (unsigned int)strtoul(qb[i].msg_name, NULL, 16);
         if ((errno == 0) && (mdb[qb[i].pos].job_id != last_job_id_lookup))
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Position in mdb for job %x in queue incorrect. Trying to fix this.",
                       last_job_id_lookup);
            if ((qb[i].pos = lookup_job_id(last_job_id_lookup)) == INCORRECT)
            {
               remove_msg(i);
               i--;
            }
         }
      }
   }

   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((i = (int)fpathconf(delete_jobs_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   else
   {
      fifo_size = i;
   }

   /* Allocate a buffer for reading data from FIFO's. */
   if (((fifo_buffer = malloc(fifo_size)) == NULL) ||
       ((nmsg_fifo_buffer = malloc(fifo_size)) == NULL))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s", fifo_size, strerror(errno));
      exit(INCORRECT);
   }

#ifdef WITH_ERROR_QUEUE
   if (attach_error_queue() == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to the error queue!");
   }
#endif

#ifdef SA_FULLDUMP
   /*
    * When dumping core ensure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit. */
   if (atexit(fd_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Could not register exit handler : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Could not set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_HW_CRC32
   have_hw_crc32 = detect_cpu_crc32();
#endif

   /* Find largest file descriptor. */
   max_fd = read_fin_fd;
   if (fd_cmd_fd > max_fd)
   {
      max_fd = fd_cmd_fd;
   }
   if (msg_fifo_fd > max_fd)
   {
      max_fd = msg_fifo_fd;
   }
   if (fd_wake_up_fd > max_fd)
   {
      max_fd = fd_wake_up_fd;
   }
   if (retry_fd > max_fd)
   {
      max_fd = retry_fd;
   }
   if (delete_jobs_fd > max_fd)
   {
      max_fd = delete_jobs_fd;
   }
   if (trl_calc_fd > max_fd)
   {
      max_fd = trl_calc_fd;
   }
   max_fd++;

   /* Allocate memory for connection structure. */
   if (((connection = malloc((max_connections * sizeof(struct connection)))) == NULL) ||
       ((zwl = malloc((max_connections * sizeof(int)))) == NULL))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes or %d bytes] : %s",
                 max_connections * sizeof(struct connection),
                 max_connections * sizeof(int), strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise structure connection. */
   (void)memset(connection, 0, (max_connections * sizeof(struct connection)));

   /* Tell user we are starting the FD. */
   system_log(INFO_SIGN, NULL, 0, "Starting %s (%s)", FD, PACKAGE_VERSION);
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Max. connections              %d",
              max_connections);
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Remote file check interval    %d (sec)",
              remote_file_check_interval);
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: FD rescan interval            %ld (sec)",
              FD_RESCAN_TIME);
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Create target dir by default  %s",
              (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & ENABLE_CREATE_TARGET_DIR) ? "Yes" : "No");
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Number of TRL groups          %d",
              no_of_trl_groups);
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Default HTTP proxy            %s",
              (default_http_proxy[0] == '\0') ? HTTP_PROXY_NAME : default_http_proxy);
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Default SMTP server           %s",
              (default_smtp_server[0] == '\0') ? SMTP_HOST_NAME : default_smtp_server);
   if (default_smtp_from != NULL)
   {
      system_log(DEBUG_SIGN, NULL, 0,
                 "FD configuration: Default SMTP from             %s",
                 default_smtp_from);
   }
   if (default_smtp_reply_to != NULL)
   {
      system_log(DEBUG_SIGN, NULL, 0,
                 "FD configuration: Default SMTP reply to         %s",
                 default_smtp_reply_to);
   }
   abnormal_term_check_time = ((time(&now) / 45) * 45) + 45;
   remote_file_check_time = ((now / remote_file_check_interval) *
                             remote_file_check_interval) +
                            remote_file_check_interval;
#ifdef _WITH_INTERRUPT_JOB
   interrupt_check_time = ((now / PRIORITY_INTERRUPT_CHECK_TIME) *
                          PRIORITY_INTERRUPT_CHECK_TIME) +
                          PRIORITY_INTERRUPT_CHECK_TIME;
#endif
   max_threshold = (double)now * 10000.0 * 20.0;
   FD_ZERO(&rset);

   /* Now watch and start transfer jobs. */
   for (;;)
   {
      /* Initialise descriptor set and timeout. */
      FD_SET(fd_cmd_fd, &rset);
      FD_SET(read_fin_fd, &rset);
      FD_SET(msg_fifo_fd, &rset);
      FD_SET(fd_wake_up_fd, &rset);
      FD_SET(retry_fd, &rset);
      FD_SET(delete_jobs_fd, &rset);
      FD_SET(trl_calc_fd, &rset);
      if (no_of_zombie_waitstates == 0)
      {
         fd_rescan_time = AFD_RESCAN_TIME;
      }
      else
      {
         fd_rescan_time = 1L;
      }
      now = time(NULL);
      if (flush_msg_fifo_dump_queue == NO)
      {
         timeout.tv_usec = 0L;
         timeout.tv_sec = ((now / fd_rescan_time) * fd_rescan_time) +
                          fd_rescan_time - now;
      }
      else
      {
         timeout.tv_usec = 100000L;
         timeout.tv_sec = 0L;
      }

      if (*no_msg_queued > p_afd_status->max_queue_length)
      {
         p_afd_status->max_queue_length = *no_msg_queued;
      }

      /*
       * Always check in 45 second intervals if a process has
       * terminated abnormally, ie where we do not get a
       * message via the READ_FIN_FIFO (eg. someone killed it with
       * a kill -9). Also check if the content of any message has
       * changed since the last check.
       */
      if (now > abnormal_term_check_time)
      {
         if (p_afd_status->no_of_transfers > 0)
         {
            for (i = 0; i < max_connections; i++)
            {
               if (connection[i].pid > 0)
               {
                  int qb_pos;

                  qb_pos_pid(connection[i].pid, &qb_pos);
                  if (qb_pos != -1)
                  {
                     int faulty;

                     if ((faulty = zombie_check(&connection[i], now, &qb_pos,
                                                WNOHANG)) == NO)
                     {
                        remove_msg(qb_pos);
                     }
                     else if ((faulty == YES) || (faulty == NONE))
                          {
                             qb[qb_pos].pid = PENDING;
                             if (qb[qb_pos].msg_name[0] != '\0')
                             {
                                fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                             }
                             else
                             {
                                fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                             }
                          }

                     if ((stop_flag == 0) && (faulty != NEITHER) &&
                         (*no_msg_queued > 0))
                     {
                        START_PROCESS();
                     }
                  } /* if (qb_pos != -1) */
               } /* if (connection[i].pid > 0) */
            } /* for (i = 0; i < max_connections; i++) */
         } /* if (p_afd_status->no_of_transfers > 0) */
         else if (p_afd_status->no_of_transfers == 0)
              {
                 pid_t ret;

                 while ((ret = waitpid(-1, NULL, WNOHANG)) > 0)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                               "GOTCHA! Caught some unknown zombie with pid %d",
#else
                               "GOTCHA! Caught some unknown zombie with pid %lld",
#endif
                               (pri_pid_t)ret);

                    /* Double check if this is not still in the */
                    /* connection structure.                    */
                    for (i = 0; i < max_connections; i++)
                    {
                       if (connection[i].pid == ret)
                       {
                          remove_connection(&connection[i], NEITHER, now);
                          break;
                       }
                    }
                 }
                 if ((ret == -1) && (errno != ECHILD))
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "waitpid() error : %s", strerror(errno));
                 }
              }

         /*
          * Check if the content of any message has changed since
          * we have last checked.
          */
         check_msg_time();

         /*
          * Check jobs_queued counter in the FSA is still correct. If not
          * reset it to the correct value.
          */
         if (*no_msg_queued == 0)
         {
            for (i = 0; i < no_of_hosts; i++)
            {
               if (fsa[i].jobs_queued != 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Jobs queued for %s is %u and not zero. Reset to zero.",
                             fsa[i].host_dsp_name, fsa[i].jobs_queued);
                  fsa[i].jobs_queued = 0;
               }
            }
         }

         check_trl_file();

         abnormal_term_check_time = ((now / 45) * 45) + 45;
         max_threshold = (double)now * 10000.0 * 20.0;
      }

#ifdef _WITH_INTERRUPT_JOB
      if (now > interrupt_check_time)
      {
         if (*no_msg_queued > 0)
         {
            int *pos_list;

            if ((pos_list = malloc(no_of_hosts * sizeof(int))) == NULL)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "malloc() error [%d bytes] : %s",
                          no_of_hosts * sizeof(int), strerror(errno));
            }
            else
            {
               int full_hosts = 0,
                   hosts_done = 0;

               for (i = 0; i < no_of_hosts; i++)
               {
                  if (fsa[i].active_transfers >= fsa[i].allowed_transfers)
                  {
                     pos_list[full_hosts] = i;
                     full_hosts++;
                  }
               }
               if (full_hosts > 0)
               {
                  for (i = 0; ((i < *no_msg_queued) && (full_hosts > hosts_done)); i++)
                  {
                     if (qb[i].msg_name[0] != '\0')
                     {
                        if (qb[i].msg_name[0] > '8')
                        {
                           break;
                        }
                        else
                        {
                           if (qb[i].pid == PENDING)
                           {
                              for (j = 0; j < full_hosts; j++)
                              {
                                 if ((pos_list[j] != -1) &&
                                     (pos_list[j] == connection[qb[i].connect_pos].fsa_pos))
                                 {
                                    int  k,
                                         pos = -1;
                                    char largest_priority = '0';

                                    for (k = 0; k < fsa[pos_list[j]].allowed_transfers; k++)
                                    {
                                       if ((fsa[pos_list[j]].job_status[k].unique_name[0] > largest_priority) &&
                                           ((fsa[pos_list[j]].job_status[k].special_flag & INTERRUPT_JOB) == 0) &&
                                           ((fsa[pos_list[j]].job_status[k].no_of_files - fsa[pos_list[j]].job_status[k].no_of_files_done) > 1))
                                       {
                                          largest_priority = fsa[pos_list[j]].job_status[k].unique_name[0];
                                          pos = k;
                                       }
                                    }
                                    if (pos > -1)
                                    {
                                       if (qb[i].msg_name[0] > largest_priority)
                                       {
                                          fsa[pos_list[j]].job_status[k].special_flag ^= INTERRUPT_JOB;
system_log(DEBUG_SIGN, NULL, 0,
           "Setting INTERRUPT_JOB for host %s in position %d",
           fsa[pos_list[j]].host_dsp_name, k);
                                       }
                                    }
                                    hosts_done++;
                                    pos_list[j] = -1;
                                 }
                              }
                           }
                        }
                     }
                  }
               }
               (void)free(pos_list);
            }
         }
         interrupt_check_time = ((now / PRIORITY_INTERRUPT_CHECK_TIME) *
                                PRIORITY_INTERRUPT_CHECK_TIME) +
                                PRIORITY_INTERRUPT_CHECK_TIME;
      }
#endif /* _WITH_INTERRUPT_JOB */

      /*
       * Check if we must check for files on any remote system.
       */
      if ((p_afd_status->no_of_transfers < max_connections) &&
          (no_of_retrieves > 0))
      {
         if ((*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_RETRIEVE) == 0)
         {
            if (now >= remote_file_check_time)
            {
               for (i = 0; i < no_of_retrieves; i++)
               {
                  if ((fra[retrieve_list[i]].queued == 0) &&
                      ((fra[retrieve_list[i]].dir_flag & DIR_DISABLED) == 0) &&
                      ((fsa[fra[retrieve_list[i]].fsa_pos].special_flag & HOST_DISABLED) == 0) &&
                      ((fsa[fra[retrieve_list[i]].fsa_pos].host_status & STOP_TRANSFER_STAT) == 0) &&
                      ((fra[retrieve_list[i]].no_of_time_entries == 0) ||
                       (fra[retrieve_list[i]].next_check_time <= now)))
                  {
                     int    qb_pos;
                     double msg_number = ((double)fra[retrieve_list[i]].priority - 47.0) *
                                         ((double)now * 10000.0);

                     check_queue_space();
                     if (*no_msg_queued > 0)
                     {
                        if (*no_msg_queued == 1)
                        {
                           if (qb[0].msg_number < msg_number)
                           {
                              qb_pos = 1;
                           }
                           else
                           {
                              size_t move_size;

                              move_size = *no_msg_queued * sizeof(struct queue_buf);
                              (void)memmove(&qb[1], &qb[0], move_size);
                              qb_pos = 0;
                           }
                        }
                        else
                        {
                           if (msg_number < qb[0].msg_number)
                           {
                              size_t move_size;

                              move_size = *no_msg_queued * sizeof(struct queue_buf);
                              (void)memmove(&qb[1], &qb[0], move_size);
                              qb_pos = 0;
                           }
                           else if (msg_number > qb[*no_msg_queued - 1].msg_number)
                                {
                                   qb_pos = *no_msg_queued;
                                }
                                else
                                {
                                   int center,
                                       end = *no_msg_queued - 1,
                                       start = 0;

                                   for (;;)
                                   {
                                      center = (end - start) / 2;
                                      if (center == 0)
                                      {
                                         size_t move_size;

                                         move_size = (*no_msg_queued - (start + 1)) *
                                                     sizeof(struct queue_buf);
                                         (void)memmove(&qb[start + 2], &qb[start + 1],
                                                       move_size);
                                         qb_pos = start + 1;
                                         break;
                                      }
                                      if (msg_number < qb[start + center].msg_number)
                                      {
                                         end = start + center;
                                      }
                                      else
                                      {
                                         start = start + center;
                                      }
                                   }
                                }
                        }
                     }
                     else
                     {
                        qb_pos = 0;
                     }

                     /* Put data in queue. */
#ifdef HAVE_SETPRIORITY
                     qb[qb_pos].msg_name[MAX_MSG_NAME_LENGTH - 1] = fra[retrieve_list[i]].priority - '0';
#endif
                     qb[qb_pos].msg_name[0] = '\0';
                     qb[qb_pos].msg_number = msg_number;
                     qb[qb_pos].creation_time = now;
                     qb[qb_pos].pos = retrieve_list[i];
                     qb[qb_pos].connect_pos = -1;
                     qb[qb_pos].retries = 0;
                     qb[qb_pos].special_flag = 0;
                     qb[qb_pos].files_to_send = 0;
                     qb[qb_pos].file_size_to_send = 0;
                     (*no_msg_queued)++;
                     fsa[fra[retrieve_list[i]].fsa_pos].jobs_queued++;
                     fra[retrieve_list[i]].queued += 1;

                     if ((fsa[fra[retrieve_list[i]].fsa_pos].error_counter == 0) &&
                         (stop_flag == 0))
                     {
                        qb[qb_pos].pid = start_process(fra[retrieve_list[i]].fsa_pos,
                                                       qb_pos, now, NO);
                     }
                     else
                     {
                        qb[qb_pos].pid = PENDING;
                     }
                  }
                  else if (((fra[retrieve_list[i]].dir_flag & DIR_DISABLED) ||
                            (fsa[fra[retrieve_list[i]].fsa_pos].special_flag & HOST_DISABLED) ||
                            (fsa[fra[retrieve_list[i]].fsa_pos].host_status & STOP_TRANSFER_STAT)) &&
                           (fra[retrieve_list[i]].no_of_time_entries > 0) &&
                           (fra[retrieve_list[i]].next_check_time <= now))
                       {
                          fra[retrieve_list[i]].next_check_time = calc_next_time_array(fra[retrieve_list[i]].no_of_time_entries,
                                                                                       &fra[retrieve_list[i]].te[0], now,
                                                                                       __FILE__, __LINE__);
                       }
               }
               remote_file_check_time = ((now / remote_file_check_interval) *
                                         remote_file_check_interval) +
                                        remote_file_check_interval;
            }
         }
         else
         {
            /*
             * We must always recalculate the next check time. If we
             * do not do it and suddenly enable retrieving it will start
             * retrieving immediatly for the old (not updated) times.
             */
            for (i = 0; i < no_of_retrieves; i++)
            {
               if ((fra[retrieve_list[i]].no_of_time_entries > 0) &&
                   (fra[retrieve_list[i]].next_check_time <= now))
               {
                  fra[retrieve_list[i]].next_check_time = calc_next_time_array(fra[retrieve_list[i]].no_of_time_entries,
                                                                               &fra[retrieve_list[i]].te[0], now,
                                                                               __FILE__, __LINE__);
               }
            }
         }
      }

      /* Check if we have to stop and we have */
      /* no more running jobs.                */
      if ((stop_flag > 0) && (p_afd_status->no_of_transfers < 1))
      {
         break;
      }

      /* Check if HOST_CONFIG has been changed. */
      if (host_config_counter != (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT))
      {
         init_trl_data();

         /* Yes, there was a change. Recalculate trl_per_process. */
         for (i = 0; i < no_of_hosts; i++)
         {
            if ((no_of_trl_groups > 0) ||
                (fsa[i].transfer_rate_limit > 0))
            {
               calc_trl_per_process(i);
            }
            else
            {
               fsa[i].trl_per_process = 0;
               fsa[i].mc_ctrl_per_process = 0;
            }
         }
         host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT);
      }

      /* Wait for message x seconds and then continue. */
      status = select(max_fd, &rset, NULL, NULL, &timeout);
      status_done = 0;

      /*
       * MESSAGE FROM COMMAND FIFO ARRIVED
       * =================================
       */
      if ((status > 0) && (FD_ISSET(fd_cmd_fd, &rset)))
      {
         char buffer;

         /* Read the message. */
         if (read(fd_cmd_fd, &buffer, 1) > 0)
         {
            switch (buffer)
            {
               case REREAD_LOC_INTERFACE_FILE :
                  get_local_interface_names();
                  break;

               case FSA_ABOUT_TO_CHANGE :
                  {
                     int         fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     int         readfd;
#endif
                     char        fifo[MAX_PATH_LENGTH];
                     struct stat stat_buf;

#ifdef HAVE_SNPRINTF
                     (void)snprintf(fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
                     (void)sprintf(fifo, "%s%s%s",
#endif
                                   p_work_dir, FIFO_DIR, FD_READY_FIFO);
                     if ((stat(fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
                     {
                        if (make_fifo(fifo) < 0)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Could not create fifo %s.", fifo);
                           exit(INCORRECT);
                        }
                     }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(fifo, O_RDWR)) == -1)
#endif
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Could not open() fifo %s : %s",
                                  fifo, strerror(errno));
                     }
                     else
                     {
                        if (write(fd, "", 1) != 1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Could not write() to fifo %s : %s",
                                      fifo, strerror(errno));
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "close() error : %s", strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "close() error : %s", strerror(errno));
                        }
                     }
                     if (fd_check_fsa() == YES)
                     {
                        if (check_fra_fd() == YES)
                        {
                           init_fra_data();
                        }
                        get_new_positions();
                        init_msg_buffer();
                        last_pos_lookup = INCORRECT;
                     }
                  }
                  break;

               case FLUSH_MSG_FIFO_DUMP_QUEUE :
                  flush_msg_fifo_dump_queue = YES;
                  break;

               case FORCE_REMOTE_DIR_CHECK :
                  remote_file_check_time = 0L;
                  break;

               case CHECK_FSA_ENTRIES :
                  check_fsa_entries();
                  break;

               case SAVE_STOP :
                  /* Here all running transfers are */
                  /* completed and no new jobs will */
                  /* be started.                    */
                  if (stop_flag == SAVE_STOP)
                  {
                     system_log(INFO_SIGN, NULL, 0,
                                "%s is already shutting down. Please be patient.",
                                FD);
                     system_log(INFO_SIGN, NULL, 0,
                                "Maximum shutdown time for %s is %d seconds.",
                                FD, FD_TIMEOUT);
                  }
                  else
                  {
                     system_log(INFO_SIGN, NULL, 0, "FD shutting down ...");
                     stop_flag = SAVE_STOP;
                  }
                  break;

               case STOP :
               case QUICK_STOP :
                  /* All transfers are aborted and  */
                  /* we do a shutdown as quick as   */
                  /* possible by killing all jobs.  */
                  stop_flag = (int)buffer;
                  loop_counter = 0;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Lookup cache: %u hits  %u misses",
                             lookup_cache_hits, lookup_cache_misses);
                  system_log(INFO_SIGN, NULL, 0, "FD shutting down ...");

                  exit(SUCCESS);

               default :
                  /* Most properly we are reading garbage. */
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Reading garbage (%d) on fifo %s.",
                             (int)buffer, FD_CMD_FIFO);
                  break;
            } /* switch (buffer) */
         }
         status_done++;
      }

      /*
       * sf_xxx or gf_xxx PROCESS TERMINATED
       * ===================================
       *
       * Everytime any child terminates it sends its PID via a well
       * known FIFO to this process. If the PID is negative the child
       * asks the parent for more data to process.
       *
       * This communication could be down more effectifly by catching
       * the SIGCHLD signal everytime a child terminates. This was
       * implemented in version 1.2.12-pre4. It just turned out that
       * this was not very portable since the behaviour differed
       * in combination with the select() system call. When using
       * SA_RESTART both Linux and Solaris would cause select() to
       * EINTR, which is very usfull, but FTX and Irix did not
       * do this. Then it could take up to 10 seconds before FD
       * caught its children. For this reason this was dropped. :-(
       */
      if ((flush_msg_fifo_dump_queue == NO) && ((status - status_done) > 0) &&
          (FD_ISSET(read_fin_fd, &rset)))
      {
         int n;

         if ((n = read(read_fin_fd, fifo_buffer, fifo_size)) >= sizeof(pid_t))
         {
            int   bytes_done = 0,
                  qb_pos;
            pid_t pid;
#ifdef _WITH_BURST_2
            int   start_new_process;
#endif

            now = time(NULL);
            do
            {
#ifdef WITH_MULTI_FSA_CHECKS
               if (fd_check_fsa() == YES)
               {
                  if (check_fra_fd() == YES)
                  {
                     init_fra_data();
                  }
                  get_new_positions();
                  init_msg_buffer();
                  last_pos_lookup = INCORRECT;
               }
#endif
               pid = *(pid_t *)&fifo_buffer[bytes_done];
#ifdef _WITH_BURST_2
               if (pid < 0)
               {
                  pid = -pid;
                  qb_pos_pid(pid, &qb_pos);
                  if (qb_pos == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                                "Hmmm, qb_pos is -1! (pid=%d bytes_done=%d n=%d no_msg_queued=%d)",
# else
                                "Hmmm, qb_pos is -1! (pid=%lld bytes_done=%d n=%d no_msg_queued=%d)",
# endif
                                (pri_pid_t)pid, bytes_done, n, *no_msg_queued);
                  }
                  else
                  {
                     if (qb[qb_pos].msg_name[0] != '\0')
                     {
                        /*
                         * Check third byte in unique_name. If this is
                         * NOT set to zero the process sf_xxx has given up
                         * waiting for FD to give it a new job, ie. the
                         * process no longer exists and we need to start
                         * a new one.
                         */
                        if (((fsa[mdb[qb[qb_pos].pos].fsa_pos].protocol_options & DISABLE_BURSTING) == 0) &&
                            (fsa[mdb[qb[qb_pos].pos].fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[2] == 4))
                        {
                           start_new_process = NO;
                        }
                        else
                        {
                           start_new_process = YES;
                        }
                     }
                     else
                     {
                        if ((stop_flag == 0) &&
                            (p_afd_status->no_of_transfers < max_connections)  &&
                            (fsa[fra[qb[qb_pos].pos].fsa_pos].active_transfers < fsa[fra[qb[qb_pos].pos].fsa_pos].allowed_transfers) &&
                            ((fra[qb[qb_pos].pos].dir_flag & DIR_DISABLED) == 0) &&
                            ((fsa[fra[qb[qb_pos].pos].fsa_pos].special_flag & HOST_DISABLED) == 0) &&
                            ((fsa[fra[qb[qb_pos].pos].fsa_pos].host_status & STOP_TRANSFER_STAT) == 0) &&
                            (fsa[fra[qb[qb_pos].pos].fsa_pos].error_counter == 0))
                        {
                           int new_qb_pos = *no_msg_queued;

                           /* Put data in queue. */
                           check_queue_space();
                           qb[new_qb_pos].msg_name[0] = '\0';
                           qb[new_qb_pos].msg_number = (double)now * 10000.0 * 200.0;
                           qb[new_qb_pos].creation_time = now;
                           qb[new_qb_pos].pos = qb[qb_pos].pos;
                           qb[new_qb_pos].connect_pos = -1;
                           qb[new_qb_pos].retries = 0;
                           qb[new_qb_pos].special_flag = HELPER_JOB;
                           qb[new_qb_pos].files_to_send = 0;
                           qb[new_qb_pos].file_size_to_send = 0;
                           (*no_msg_queued)++;
                           fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                           fra[qb[qb_pos].pos].queued += 1;

                           qb[new_qb_pos].pid = start_process(fra[qb[qb_pos].pos].fsa_pos,
                                                              new_qb_pos, now,
                                                              NO);
                           if ((qb[new_qb_pos].pid == PENDING) ||
                               (qb[new_qb_pos].pid == REMOVED))
                           {
                              /* We did not start a process, lets remove */
                              /* this job from the queue again.          */
                              fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued--;
                              fra[qb[qb_pos].pos].queued -= 1;
                              (*no_msg_queued)--;
                           }
                        }
                        start_new_process = NEITHER;
                     }
                  }
               }
               else
               {
                  qb_pos_pid(pid, &qb_pos);
                  start_new_process = YES;
               }
#else
               qb_pos_pid(pid, &qb_pos);
#endif /* _WITH_BURST_2 */
               if (qb_pos != -1)
               {
#ifdef _WITH_BURST_2
                  /*
                   * This process is ready to process more data.
                   * Have a look in the queue if there is another
                   * job pending for this host.
                   */
                  if (start_new_process == NO)
                  {
                     int fsa_pos,
                         gotcha = NO;

                     fsa_pos = mdb[qb[qb_pos].pos].fsa_pos;
                     if (fsa[fsa_pos].jobs_queued > 0)
                     {
                        for (i = 0; i < *no_msg_queued; i++)
                        {
                           if ((qb[i].pid == PENDING) &&
                               (qb[i].msg_name[0] != '\0') &&
                               (mdb[qb[i].pos].fsa_pos == fsa_pos) &&
                               (mdb[qb[i].pos].type == mdb[qb[qb_pos].pos].type) &&
                               (mdb[qb[i].pos].port == mdb[qb[qb_pos].pos].port)
# ifdef WITH_ERROR_QUEUE
                               && (((fsa[fsa_pos].host_status & ERROR_QUEUE_SET) == 0) ||
                                   ((fsa[fsa_pos].host_status & ERROR_QUEUE_SET) &&
                                    (check_error_queue(mdb[qb[i].pos].job_id,
                                                       -1, now,
                                                       fsa[fsa_pos].retry_interval) == NO)))
# endif
                              )
                           {
                              /* Yep, there is another job pending! */
                              gotcha = YES;
                              break;
                           }
                        }
                     }
                     if (gotcha == YES)
                     {
# ifdef _WITH_INTERRUPT_JOB
                        int interrupt;

                        if (fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[3] == 4)
                        {
                           interrupt = YES;
                           if (fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag & INTERRUPT_JOB)
                           {
                              fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag ^= INTERRUPT_JOB;
                           }
                        }
                        else
                        {
                           interrupt = NO;
                        }
# endif
                        if (qb[i].retries > 0)
                        {
                           fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[0] = '\0';
                           fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[1] = 1;
# ifdef HAVE_SNPRINTF
                           (void)snprintf(&fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[2], MAX_FILENAME_LENGTH - 2,
# else
                           (void)sprintf(&fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[2],
# endif
                                         "%u", qb[i].retries);
                        }
                        fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].job_id = mdb[qb[i].pos].job_id;
                        (void)memcpy(fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name,
                                     qb[i].msg_name, MAX_MSG_NAME_LENGTH);
                        (void)memcpy(connection[qb[qb_pos].connect_pos].msg_name,
                                     qb[i].msg_name, MAX_MSG_NAME_LENGTH);
                        qb[i].pid = pid;
                        qb[i].connect_pos = qb[qb_pos].connect_pos;
                        mdb[qb[qb_pos].pos].last_transfer_time = mdb[qb[i].pos].last_transfer_time = now;
# ifdef _WITH_INTERRUPT_JOB
                        if (interrupt == NO)
                        {
# endif
                           ABS_REDUCE(fsa_pos);
                           remove_msg(qb_pos);
# ifdef _WITH_INTERRUPT_JOB
                        }
# endif
                        p_afd_status->burst2_counter++;
                     }
                     else
                     {
                        fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[0] = '\0';
                        fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[1] = 1;
# ifdef _WITH_INTERRUPT_JOB
                        if (fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag & INTERRUPT_JOB)
                        {
                           fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag ^= INTERRUPT_JOB;
                        }
# endif
                        if ((fsa[fsa_pos].transfer_rate_limit > 0) ||
                            (no_of_trl_groups > 0))
                        {
                           calc_trl_per_process(fsa_pos);
                        }
                     }
                     if (pid > 0)
                     {
                        if (kill(pid, SIGUSR1) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                                      "Failed to send SIGUSR1 to %d : %s",
# else
                                      "Failed to send SIGUSR1 to %lld : %s",
# endif
                                      (pri_pid_t)pid, strerror(errno));
                        }
                     }
                     else
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                                   "Hmmm, pid = %d!!!", (pri_pid_t)pid);
# else
                                   "Hmmm, pid = %lld!!!", (pri_pid_t)pid);
# endif
                     }
                  }
                  else
                  {
#endif /* _WITH_BURST_2 */
                     check_zombie_queue(now, qb_pos);
#ifdef _WITH_BURST_2
                  }
#endif
               }
               bytes_done += sizeof(pid_t);
            } while ((n > bytes_done) &&
                     ((n - bytes_done) >= sizeof(pid_t)));
            if ((n - bytes_done) > 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Reading garbage from fifo [%d]",
                          (n - bytes_done));
            }

            if ((stop_flag == 0) && (*no_msg_queued > 0))
            {
               /*
                * If the number of messages queued is very large
                * and most messages belong to a host that is very
                * slow, new messages will only be processed very
                * slowly when we always scan the whole queue.
                */
               if (*no_msg_queued < MAX_QUEUED_BEFORE_CECKED)
               {
                  START_PROCESS();
               }
               else
               {
                  if (loop_counter > ELAPSED_LOOPS_BEFORE_CHECK)
                  {
                     START_PROCESS();
                     loop_counter = 0;
                  }
                  else
                  {
                     loop_counter++;
                  }
               }
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "read() error or reading garbage from fifo %s",
                       SF_FIN_FIFO);
         }
         status_done++;
      } /* sf_xxx or gf_xxx PROCESS TERMINATED */

      /*
       * RETRY
       * =====
       */
      if ((flush_msg_fifo_dump_queue == NO) && ((status - status_done) > 0) &&
          (FD_ISSET(retry_fd, &rset)))
      {
         int fsa_pos;

         if (read(retry_fd, &fsa_pos, sizeof(int)) != sizeof(int))
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Reading garbage from fifo %s", RETRY_FD_FIFO);
         }
         else
         {
            if (stop_flag == 0)
            {
               int qb_pos;

               qb_pos_fsa(fsa_pos, &qb_pos);
               if (qb_pos != -1)
               {
                  if ((qb[qb_pos].pid = start_process(fsa_pos, qb_pos,
                                                      time(NULL),
                                                      YES)) == REMOVED)
                  {
                     remove_msg(qb_pos);
                  }
               }
            }
         }
         status_done++;
      } /* RETRY */
           
      /*
       * NEW MESSAGE ARRIVED
       * ===================
       */
      if (((status - status_done) > 0) && (FD_ISSET(msg_fifo_fd, &rset)))
      {
         if (nmsg_bytes_read == 0)
         {
            nmsg_bytes_buffered = 0;
         }
         else
         {
            (void)memmove(nmsg_fifo_buffer, nmsg_read_ptr,
                          nmsg_bytes_read);
            nmsg_bytes_buffered = nmsg_bytes_read;
         }
         nmsg_read_ptr = nmsg_fifo_buffer;

         if (((nmsg_bytes_read = read(msg_fifo_fd,
                                      &nmsg_fifo_buffer[nmsg_bytes_buffered],
                                      (fifo_size - nmsg_bytes_buffered))) > 0) &&
             ((nmsg_bytes_read + nmsg_bytes_buffered) >= MAX_BIN_MSG_LENGTH))
         {
            int bytes_done = 0,
                pos;

            now = time(NULL);
            nmsg_bytes_read += nmsg_bytes_buffered;
            do
            {
#ifdef WITH_MULTI_FSA_CHECKS
               if (fd_check_fsa() == YES)
               {
                  if (check_fra_fd() == YES)
                  {
                     init_fra_data();
                  }
                  get_new_positions();
                  init_msg_buffer();
                  last_pos_lookup = INCORRECT;
               }
#endif
               (void)memcpy(msg_buffer, &nmsg_fifo_buffer[bytes_done],
                            MAX_BIN_MSG_LENGTH);

               /* Queue the job order. */
               if (*msg_priority != 0)
               {
                  check_queue_space();

                  if (last_pos_lookup == INCORRECT)
                  {
                     last_pos_lookup = pos = lookup_job_id(*job_id);
                     last_job_id_lookup = *job_id;
                  }
                  else if (last_job_id_lookup != *job_id)
                       {
                          lookup_cache_misses++;
                          last_pos_lookup = pos = lookup_job_id(*job_id);
                          last_job_id_lookup = *job_id;
                       }
                       else
                       {
                          pos = last_pos_lookup;
                          lookup_cache_hits++;
                       }

                  if (pos == INCORRECT)
                  {
                     char del_dir[MAX_PATH_LENGTH];

                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not locate job %x", *job_id);
#ifdef HAVE_SNPRINTF
                     (void)snprintf(del_dir, MAX_PATH_LENGTH,
#else
                     (void)sprintf(del_dir,
#endif
#if SIZEOF_TIME_T == 4
                                   "%s%s%s/%x/%x/%lx_%x_%x",
#else
                                   "%s%s%s/%x/%x/%llx_%x_%x",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   *job_id, *dir_no,
                                   (pri_time_t)*creation_time,
                                   *unique_number, *split_job_counter);
#ifdef _DELETE_LOG
                     *dl.input_time = *creation_time;
                     *dl.unique_number = *unique_number;
                     *dl.split_job_counter = *split_job_counter;
                     remove_job_files(del_dir, -1, *job_id, FD,
                                      JID_LOOKUP_FAILURE_DEL, -1);
#else
                     remove_job_files(del_dir, -1, -1);
#endif
                  }
                  else
                  {
                     int    qb_pos;
                     double msg_number = ((double)*msg_priority - 47.0) *
                                         (((double)*creation_time * 10000.0) +
                                          (double)*unique_number +
                                          (double)*split_job_counter);

                     if (*no_msg_queued > 0)
                     {
                        if (*no_msg_queued == 1)
                        {
                           if (qb[0].msg_number < msg_number)
                           {
                              qb_pos = 1;
                           }
                           else
                           {
                              size_t move_size;

                              move_size = *no_msg_queued *
                                          sizeof(struct queue_buf);
                              (void)memmove(&qb[1], &qb[0], move_size);
                              qb_pos = 0;
                           }
                        }
                        else
                        {
                           if (msg_number < qb[0].msg_number)
                           {
                              size_t move_size;

                              move_size = *no_msg_queued *
                                          sizeof(struct queue_buf);
                              (void)memmove(&qb[1], &qb[0], move_size);
                              qb_pos = 0;
                           }
                           else if (msg_number > qb[*no_msg_queued - 1].msg_number)
                                {
                                   qb_pos = *no_msg_queued;
                                }
                                else
                                {
                                   int center,
                                       end = *no_msg_queued - 1,
                                       start = 0;

                                   for (;;)
                                   {
                                      center = (end - start) / 2;
                                      if (center == 0)
                                      {
                                         size_t move_size;

                                         move_size = (*no_msg_queued - (start + 1)) *
                                                     sizeof(struct queue_buf);
                                         (void)memmove(&qb[start + 2],
                                                       &qb[start + 1],
                                                       move_size);
                                         qb_pos = start + 1;
                                         break;
                                      }
                                      if (msg_number < qb[start + center].msg_number)
                                      {
                                         end = start + center;
                                      }
                                      else
                                      {
                                         start = start + center;
                                      }
                                   }
                                }
                        }
                     }
                     else
                     {
                        qb_pos = 0;
                     }

#ifdef HAVE_SETPRIORITY
                     qb[qb_pos].msg_name[MAX_MSG_NAME_LENGTH - 1] = *msg_priority - '0';
                     /* NOTE: We write the priority before in case   */
                     /*       msg_name is really MAX_MSG_NAME_LENGTH */
                     /*       long.                                  */
#endif
#ifdef HAVE_SNPRINTF
                     (void)snprintf(qb[qb_pos].msg_name, MAX_MSG_NAME_LENGTH,
#else
                     (void)sprintf(qb[qb_pos].msg_name,
#endif
#if SIZEOF_TIME_T == 4
                                   "%x/%x/%lx_%x_%x",
#else
                                   "%x/%x/%llx_%x_%x",
#endif
                                   *job_id, *dir_no,
                                   (pri_time_t)*creation_time,
                                   *unique_number, *split_job_counter);
#ifdef _TESTEN_
system_log(INFO_SIGN, __FILE__, __LINE__,
           "%d: job_id=%x  creation_time=%x  unique_number=%x  sjc=%x [pos=%d no_msg_queued=%d]",
           qb_pos, *job_id, *creation_time, *unique_number, *split_job_counter, pos, *no_msg_queued);
system_log(INFO_SIGN, __FILE__, __LINE__, "%s", qb[qb_pos].msg_name);
#endif
                     qb[qb_pos].msg_number = msg_number;
                     qb[qb_pos].pid = PENDING;
                     qb[qb_pos].creation_time = *creation_time;
                     qb[qb_pos].pos = pos;
                     qb[qb_pos].connect_pos = -1;
                     qb[qb_pos].retries = 0;
                     qb[qb_pos].files_to_send = *files_to_send;
                     qb[qb_pos].file_size_to_send = *file_size_to_send;
                     qb[qb_pos].special_flag = 0;
                     if (*originator == SHOW_OLOG_NO)
                     {
                        qb[qb_pos].special_flag |= RESEND_JOB;
                     }
                     (*no_msg_queued)++;
                     fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                  }
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm. Priority data is NULL! Must be reading garbage (creation_time:%ld job_id:%u unique_number:%u priority:%d)!",
                             *creation_time, *job_id, *unique_number,
                             *msg_priority);
               }
               bytes_done += MAX_BIN_MSG_LENGTH;
               nmsg_bytes_read -= MAX_BIN_MSG_LENGTH;
            } while (nmsg_bytes_read >= MAX_BIN_MSG_LENGTH);
            nmsg_read_ptr += bytes_done;

            if (((bytes_done + nmsg_bytes_read) == fifo_size) &&
                (fifo_full_counter < 6))
            {
               fifo_full_counter++;
            }
            else
            {
               fifo_full_counter = 0;
            }

            /*
             * Try to handle other queued files. If we do not do
             * it here, where else?
             */
            if ((fifo_full_counter == 0) && (stop_flag == 0) &&
                (*no_msg_queued > 0))
            {
               /*
                * If the number of messages queued is very large
                * and most messages belong to a host that is very
                * slow, new messages will only be processed very
                * slowly when we always scan the whole queue.
                */
               if (*no_msg_queued < MAX_QUEUED_BEFORE_CECKED)
               {
                  START_PROCESS();
               }
               else
               {
                  if (loop_counter > ELAPSED_LOOPS_BEFORE_CHECK)
                  {
                     START_PROCESS();
                     loop_counter = 0;
                  }
                  else
                  {
                     loop_counter++;
                  }
               }
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Hmmm. Seems like I am reading garbage from the fifo.");
         }
         status_done++;
      } /* NEW MESSAGE ARRIVED */

      /*
       * DELETE FILE(S) FROM QUEUE
       * =========================
       */
      if (((status - status_done) > 0) && (FD_ISSET(delete_jobs_fd, &rset)))
      {
         handle_delete_fifo(delete_jobs_fd, fifo_size, file_dir);
         status_done++;
      }

      /*
       * RECALCULTE TRANSFER RATE LIMIT
       * ==============================
       */
      if (((status - status_done) > 0) && (FD_ISSET(trl_calc_fd, &rset)))
      {
         int n;

         if ((n = read(trl_calc_fd, fifo_buffer, fifo_size)) >= sizeof(int))
         {
            int bytes_done = 0,
                trl_fsa_pos;

            do
            {
               trl_fsa_pos = *(int *)&fifo_buffer[bytes_done];

               /* Ensure that the position is a valid one. */
               if ((trl_fsa_pos > -1) && (trl_fsa_pos < no_of_hosts))
               {
                  calc_trl_per_process(trl_fsa_pos);
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Assuming to read garbage from fifo (trl_fsa_pos=%d no_of_hosts=%d)",
                             trl_fsa_pos, no_of_hosts);
               }
               bytes_done += sizeof(int);
            } while ((n > bytes_done) &&
                     ((n - bytes_done) >= sizeof(int)));
            if ((n - bytes_done) > 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Reading garbage from fifo [%d]",
                          (n - bytes_done));
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "read() error or reading garbage from fifo %s",
                       TRL_CALC_FIFO);
         }
         status_done++;
      }

      /*
       * TIMEOUT or WAKE-UP (Start/Stop Transfer)
       * ==================
       */
      if ((status == 0) || (FD_ISSET(fd_wake_up_fd, &rset)))
      {
         /* Clear wake-up FIFO if necessary. */
         if ((status > 0) && (FD_ISSET(fd_wake_up_fd, &rset)))
         {
            if (read(fd_wake_up_fd, fifo_buffer, fifo_size) < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "read() error : %s", strerror(errno));
            }
         }
         if (no_of_zombie_waitstates > 0)
         {
            check_zombie_queue(now, -1);
         }

         if (stop_flag == 0)
         {
#ifdef _MACRO_DEBUG
            int fsa_pos;

            /* Try handle any pending jobs. */
            for (i = 0; ((i < *no_msg_queued) &&
                         (p_afd_status->no_of_transfers < max_connections)); i++)
            {
               if (qb[i].pid == PENDING)
               {
                  if (qb[i].msg_name[0] != '\0')
                  {
                     fsa_pos = mdb[qb[i].pos].fsa_pos;
                  }
                  else
                  {
                     fsa_pos = fra[qb[i].pos].fsa_pos;
                  }
                  if ((qb[i].pid = start_process(fsa_pos, i, now, NO)) == REMOVED)
                  {
                     /*
                      * The message can be removed because the
                      * files are queued in another message
                      * or have been removed due to age.
                      */
                     remove_msg(i);
                     if (i < *no_msg_queued)
                     {
                        i--;
                     }
                  }
               }
            }
#else
            START_PROCESS();
#endif
         }
         else
         {
            /* Lets not wait too long. It could be that one of the */
            /* sending process is in an infinite loop and we could */
            /* wait forever. So after a certain time kill(!) all   */
            /* jobs and then exit.                                 */
            loop_counter++;
            if ((stop_flag == SAVE_STOP) || (stop_flag == STOP))
            {
               if ((loop_counter * fd_rescan_time) > FD_TIMEOUT)
               {
                  break; /* To get out of for(;;) loop. */
               }
            }
            else
            {
               if ((loop_counter * fd_rescan_time) > FD_QUICK_TIMEOUT)
               {
                  break; /* To get out of for(;;) loop. */
               }
            }
         }
      } /* TIMEOUT or WAKE-UP (Start/Stop Transfer) */

           /*
            * SELECT ERROR
            * ============
            */
      else if (status < 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "Select error : %s", strerror(errno));
              exit(INCORRECT);
           }

      if ((flush_msg_fifo_dump_queue == YES) &&
          ((status == 0) || (FD_ISSET(msg_fifo_fd, &rset) == 0)))
      {
#ifdef WITHOUT_FIFO_RW_SUPPORT
         int  qlr_read_fd;
#endif
         int  qlr_fd;
         char queue_list_ready_fifo[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
         (void)snprintf(queue_list_ready_fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
         (void)sprintf(queue_list_ready_fifo, "%s%s%s",
#endif
                       p_work_dir, FIFO_DIR, QUEUE_LIST_READY_FIFO);

         /* Dump what we have currently in the queue. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(queue_list_ready_fifo, &qlr_read_fd, &qlr_fd) == -1)
#else
         if ((qlr_fd = open(queue_list_ready_fifo, O_RDWR)) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open fifo %s : %s",
                       queue_list_ready_fifo, strerror(errno));
         }
         else
         {
            char buf;

            if (*no_msg_queued == 0)
            {
               buf = QUEUE_LIST_EMPTY;
            }
            else
            {
               buf = QUEUE_LIST_READY;
            }
            if (write(qlr_fd, &buf, 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() to %s : %s",
                          queue_list_ready_fifo, strerror(errno));
            }
            else
            {
#ifdef WITHOUT_FIFO_RW_SUPPORT
               int qld_write_fd;
#endif
               int qld_fd;

               /* Wait for dir_check to respond. */
#ifdef HAVE_SNPRINTF
               (void)snprintf(queue_list_ready_fifo, MAX_PATH_LENGTH, "%s%s%s",
#else
               (void)sprintf(queue_list_ready_fifo, "%s%s%s",
#endif
                             p_work_dir, FIFO_DIR, QUEUE_LIST_DONE_FIFO);

#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(queue_list_ready_fifo, &qld_fd, &qld_write_fd) == -1)
#else
               if ((qld_fd = open(queue_list_ready_fifo, O_RDWR)) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open fifo %s : %s",
                             queue_list_ready_fifo, strerror(errno));
               }
               else
               {
                  int            ql_status;
                  fd_set         ql_rset;
                  struct timeval ql_timeout;

                  FD_ZERO(&ql_rset);
                  FD_SET(qld_fd, &ql_rset);
                  ql_timeout.tv_usec = 0;
                  ql_timeout.tv_sec = QUEUE_LIST_DONE_TIMEOUT;

                  ql_status = select((qld_fd + 1), &ql_rset, NULL, NULL,
                                     &ql_timeout);

                  if ((ql_status > 0) && (FD_ISSET(qld_fd, &ql_rset)))
                  {
                     char buffer[32];

                     if (read(qld_fd, buffer, 32) <= 0)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "read() error : %s", strerror(errno));
                     }
                  }
                  else if (ql_status == 0)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "%s failed to respond.", DIR_CHECK);
                       }
                       else
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "select() error (%d) : %s",
                                     ql_status, strerror(errno));
                       }

#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if ((close(qld_fd) == -1) || (close(qld_write_fd) == -1))
#else
                  if (close(qld_fd) == -1)
#endif
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "close() error : %s", strerror(errno));
                  }
               }
            }

#ifdef WITHOUT_FIFO_RW_SUPPORT
            if ((close(qlr_fd) == -1) || (close(qlr_read_fd) == -1))
#else
            if (close(qlr_fd) == -1)
#endif
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
         }
         check_fsa_entries();

         /* Back to normal mode. */
         flush_msg_fifo_dump_queue = NO;
      }
   } /* for (;;) */

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ start_process() ++++++++++++++++++++++++++*/
static pid_t
start_process(int fsa_pos, int qb_pos, time_t current_time, int retry)
{
   pid_t pid = PENDING;

   if (fsa_pos < 0)
   {
      /*
       * If a retrieve job is removed we have a small window where
       * we try to start something that no longer exists.
       */
      (void)system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmm, trying to start a process at FSA position %d!!!",
                       fsa_pos);

      return(REMOVED);
   }

   if ((qb[qb_pos].msg_name[0] != '\0') &&
       (mdb[qb[qb_pos].pos].age_limit > 0) &&
       ((fsa[fsa_pos].host_status & DO_NOT_DELETE_DATA) == 0) &&
       (current_time > qb[qb_pos].creation_time) &&
       ((current_time - qb[qb_pos].creation_time) > mdb[qb[qb_pos].pos].age_limit))
   {
      char del_dir[MAX_PATH_LENGTH];

#ifdef WITH_ERROR_QUEUE
      if (fsa[fsa_pos].host_status & ERROR_QUEUE_SET)
      {
         remove_from_error_queue(mdb[qb[qb_pos].pos].job_id, &fsa[fsa_pos],
                                 fsa_pos, fsa_fd);
      }
#endif
#ifdef HAVE_SNPRINTF
      (void)snprintf(del_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
      (void)sprintf(del_dir, "%s%s%s/%s",
#endif
                    p_work_dir, AFD_FILE_DIR,
                    OUTGOING_DIR, qb[qb_pos].msg_name);
#ifdef _DELETE_LOG
      extract_cus(qb[qb_pos].msg_name, dl.input_time, dl.split_job_counter,
                  dl.unique_number);
      remove_job_files(del_dir, fsa_pos, mdb[qb[qb_pos].pos].job_id,
                       FD, AGE_OUTPUT, -1);
#else
      remove_job_files(del_dir, fsa_pos, -1);
#endif
      ABS_REDUCE(fsa_pos);
      pid = REMOVED;
   }
   else
   {
#ifdef WITH_ERROR_QUEUE
      int in_error_queue = NEITHER;
#endif

      if ((qb[qb_pos].msg_name[0] == '\0') &&
          (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_RETRIEVE))
      {
         ABS_REDUCE(fsa_pos);

         return(REMOVED);
      }

      if (((fsa[fsa_pos].host_status & STOP_TRANSFER_STAT) == 0) &&
#ifdef WITH_ERROR_QUEUE
          ((retry == YES) ||
           ((fsa[fsa_pos].error_counter == 0) &&
            (((fsa[fsa_pos].host_status & ERROR_QUEUE_SET) == 0) ||
             ((fsa[fsa_pos].host_status & ERROR_QUEUE_SET) &&
              ((in_error_queue = check_error_queue(mdb[qb[qb_pos].pos].job_id,
                                                   -1, current_time,
                                                   fsa[fsa_pos].retry_interval)) == NO)))) ||
           ((fsa[fsa_pos].error_counter > 0) &&
            (fsa[fsa_pos].host_status & ERROR_QUEUE_SET) &&
            ((current_time - (fsa[fsa_pos].last_retry_time + fsa[fsa_pos].retry_interval)) >= 0) &&
            ((in_error_queue == NO) ||
             ((in_error_queue == NEITHER) &&
              (check_error_queue(mdb[qb[qb_pos].pos].job_id, -1, current_time,
                                 fsa[fsa_pos].retry_interval) == NO)))) ||
#else
          ((fsa[fsa_pos].error_counter == 0) || (retry == YES) ||
#endif
           ((fsa[fsa_pos].active_transfers == 0) &&
            ((current_time - (fsa[fsa_pos].last_retry_time + fsa[fsa_pos].retry_interval)) >= 0))))
      {
         /*
          * First lets try and take an existing process,
          * that is waiting for more data to come.
          */
         if ((fsa[fsa_pos].original_toggle_pos == NONE) &&
             ((fsa[fsa_pos].protocol_options & DISABLE_BURSTING) == 0) &&
             (fsa[fsa_pos].keep_connected > 0) &&
             (fsa[fsa_pos].active_transfers > 0) &&
             (fsa[fsa_pos].jobs_queued > 0) &&
             ((((fsa[fsa_pos].special_flag & KEEP_CON_NO_SEND) == 0) &&
               (qb[qb_pos].msg_name[0] != '\0')) ||
              (((fsa[fsa_pos].special_flag & KEEP_CON_NO_FETCH) == 0) &&
               (qb[qb_pos].msg_name[0] == '\0'))) &&
             ((qb[qb_pos].special_flag & HELPER_JOB) == 0))
         {
            int i,
                other_job_wait_pos[MAX_NO_PARALLEL_JOBS],
                other_qb_pos[MAX_NO_PARALLEL_JOBS],
                wait_counter = 0;

            for (i = 0; i < fsa[fsa_pos].allowed_transfers; i++)
            {
               if ((fsa[fsa_pos].job_status[i].proc_id != -1) &&
                   (fsa[fsa_pos].job_status[i].unique_name[2] == 5))
               {
                  int exec_qb_pos;

                  qb_pos_pid(fsa[fsa_pos].job_status[i].proc_id, &exec_qb_pos);
                  if (exec_qb_pos != -1)
                  {
                     if ((qb[qb_pos].msg_name[0] != '\0') &&
                         (qb[exec_qb_pos].msg_name[0] != '\0') &&
                         (mdb[qb[qb_pos].pos].type == mdb[qb[exec_qb_pos].pos].type) &&
                         (mdb[qb[qb_pos].pos].port == mdb[qb[exec_qb_pos].pos].port))
                     {
                        if (qb[qb_pos].retries > 0)
                        {
                           fsa[fsa_pos].job_status[i].file_name_in_use[0] = '\0';
                           fsa[fsa_pos].job_status[i].file_name_in_use[1] = 1;
#ifdef HAVE_SNPRINTF
                           (void)snprintf(&fsa[fsa_pos].job_status[i].file_name_in_use[2], MAX_FILENAME_LENGTH - 2,
#else
                           (void)sprintf(&fsa[fsa_pos].job_status[i].file_name_in_use[2],
#endif
                                         "%u", qb[qb_pos].retries);
                        }
                        fsa[fsa_pos].job_status[i].job_id = mdb[qb[qb_pos].pos].job_id;
                        mdb[qb[qb_pos].pos].last_transfer_time = mdb[qb[exec_qb_pos].pos].last_transfer_time = current_time;
                        (void)memcpy(fsa[fsa_pos].job_status[i].unique_name,
                                     qb[qb_pos].msg_name, MAX_MSG_NAME_LENGTH);
                        (void)memcpy(connection[qb[exec_qb_pos].connect_pos].msg_name,
                                     qb[qb_pos].msg_name, MAX_MSG_NAME_LENGTH);
                        qb[qb_pos].pid = qb[exec_qb_pos].pid;
                        qb[qb_pos].connect_pos = qb[exec_qb_pos].connect_pos;
                        qb[qb_pos].special_flag |= BURST_REQUEUE;
                        connection[qb[exec_qb_pos].connect_pos].job_no = i;
                        if (qb[exec_qb_pos].pid > 0)
                        {
                           if (kill(qb[exec_qb_pos].pid, SIGUSR1) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                         "Failed to send SIGUSR1 to %d : %s",
#else
                                         "Failed to send SIGUSR1 to %lld : %s",
#endif
                                         (pri_pid_t)qb[exec_qb_pos].pid, strerror(errno));
                           }
                           p_afd_status->burst2_counter++;
#ifdef HAVE_SETPRIORITY
                           if (add_afd_priority == YES)
                           {
                              int sched_priority;

                              sched_priority = current_priority + qb[qb_pos].msg_name[MAX_MSG_NAME_LENGTH - 1];
                              if (sched_priority > min_sched_priority)
                              {
                                 sched_priority = min_sched_priority;
                              }
                              else if (sched_priority < max_sched_priority)
                                   {
                                      sched_priority = max_sched_priority;
                                   }
                              if (euid != ruid)
                              {
                                 if (seteuid(euid) == -1)
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               "Failed to set the effective user ID : %s",
                                               strerror(errno));
                                 }
                              }
                              if (setpriority(PRIO_PROCESS, qb[qb_pos].pid,
                                              sched_priority) == -1)
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                                            "Failed to setpriority() to %d of process %d : %s",
# else
                                            "Failed to setpriority() to %d of process %lld : %s",
# endif
                                            sched_priority,
                                            (pri_pid_t)qb[qb_pos].pid,
                                            strerror(errno));
                              }
                              if (euid != ruid)
                              {
                                 if (seteuid(ruid) == -1)
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               "Failed to set back to the real user ID : %s",
                                               strerror(errno));
                                 }
                              }
                           }
#endif /* HAVE_SETPRIORITY */
                        }
                        else
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                      "Hmmm, pid = %d!!!", (pri_pid_t)qb[exec_qb_pos].pid);
#else
                                      "Hmmm, pid = %lld!!!", (pri_pid_t)qb[exec_qb_pos].pid);
#endif
                        }
                        if ((fsa[fsa_pos].transfer_rate_limit > 0) ||
                            (no_of_trl_groups > 0))
                        {
                           calc_trl_per_process(fsa_pos);
                        }
                        ABS_REDUCE(fsa_pos);
                        remove_msg(exec_qb_pos);

                        return(qb[qb_pos].pid);
                     }
                     else
                     {
                        other_job_wait_pos[wait_counter] = i;
                        other_qb_pos[wait_counter] = exec_qb_pos;
                        wait_counter++;
                     }
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                "Unable to locate qb_pos for %d [fsa_pos=%d].",
#else
                                "Unable to locate qb_pos for %lld [fsa_pos=%d].",
#endif
                                (pri_pid_t)fsa[fsa_pos].job_status[i].proc_id,
                                fsa_pos);
                  }
               }
            }
            if ((fsa[fsa_pos].active_transfers == fsa[fsa_pos].allowed_transfers) &&
                (wait_counter > 0))
            {
               for (i = 0; i < wait_counter; i++)
               {
                  if (fsa[fsa_pos].job_status[other_job_wait_pos[i]].unique_name[2] == 5)
                  {
                     if (qb[other_qb_pos[i]].pid > 0)
                     {
                        fsa[fsa_pos].job_status[other_job_wait_pos[i]].unique_name[2] = 6;
                        if (qb[other_qb_pos[i]].msg_name[0] == '\0')
                        {
                           return(PENDING);
                        }
                        else
                        {
                           if (kill(qb[other_qb_pos[i]].pid, SIGUSR1) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                         "Failed to send SIGUSR1 to %d : %s",
#else
                                         "Failed to send SIGUSR1 to %lld : %s",
#endif
                                         (pri_pid_t)qb[other_qb_pos[i]].pid, strerror(errno));
                              fsa[fsa_pos].job_status[other_job_wait_pos[i]].unique_name[2] = 5;
                           }
                           else
                           {
                              return(PENDING);
                           }
                        }
                     }
                     else
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                   "Hmmm, pid = %d!!!", (pri_pid_t)qb[other_qb_pos[i]].pid);
#else
                                   "Hmmm, pid = %lld!!!", (pri_pid_t)qb[other_qb_pos[i]].pid);
#endif
                     }
                  }
               }
            }
         }

         if ((p_afd_status->no_of_transfers < max_connections) &&
             (fsa[fsa_pos].active_transfers < fsa[fsa_pos].allowed_transfers))
         {
            int pos;

            if ((pos = get_free_connection()) == INCORRECT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to get free connection.");
            }
            else
            {
#ifdef _WITH_SERVER_SUPPORT
# ifdef LOCK_DEBUG
               lock_region_w(fsa_fd, (AFD_WORD_OFFSET +
                                      (fsa_pos * sizeof(struct filetransfer_status)) +
                                      LOCK_CON), __FILE__, __LINE__);
# else
               lock_region_w(fsa_fd, (AFD_WORD_OFFSET +
                                      (fsa_pos * sizeof(struct filetransfer_status)) +
                                      LOCK_CON));
# endif
#endif
               if ((connection[pos].job_no = get_free_disp_pos(fsa_pos)) != INCORRECT)
               {
                  if (qb[qb_pos].msg_name[0] == '\0')
                  {
                     connection[pos].fra_pos = qb[qb_pos].pos;
                     connection[pos].protocol = fra[qb[qb_pos].pos].protocol;
                     connection[pos].msg_name[0] = '\0';
                     (void)memcpy(connection[pos].dir_alias,
                                  fra[qb[qb_pos].pos].dir_alias,
                                  MAX_DIR_ALIAS_LENGTH + 1);
                  }
                  else
                  {
                     connection[pos].fra_pos = -1;
                     connection[pos].protocol = mdb[qb[qb_pos].pos].type;
                     (void)memcpy(connection[pos].msg_name, qb[qb_pos].msg_name,
                                  MAX_MSG_NAME_LENGTH);
                     connection[pos].dir_alias[0] = '\0';
                  }
                  if (qb[qb_pos].special_flag & RESEND_JOB)
                  {
                     connection[pos].resend = YES;
                  }
                  else
                  {
                     connection[pos].resend = NO;
                  }
                  connection[pos].temp_toggle = OFF;
                  (void)memcpy(connection[pos].hostname, fsa[fsa_pos].host_alias,
                               MAX_HOSTNAME_LENGTH + 1);
                  connection[pos].host_id = fsa[fsa_pos].host_id;
                  connection[pos].fsa_pos = fsa_pos;
#ifdef WITH_MULTI_FSA_CHECKS
                  if (fd_check_fsa() == YES)
                  {
                     if (check_fra_fd() == YES)
                     {
                        init_fra_data();
                     }

                     /*
                      * We need to set the connection[pos].pid to a
                      * value higher then 0 so the function get_new_positions()
                      * also locates the new connection[pos].fsa_pos. Otherwise
                      * from here on we point to some completely different
                      * host and this can cause havoc when someone uses
                      * edit_hc and changes the alias order.
                      */
                     connection[pos].pid = 1;
                     get_new_positions();
                     connection[pos].pid = 0;
                     init_msg_buffer();
                     fsa_pos = connection[pos].fsa_pos;
                     last_pos_lookup = INCORRECT;
                  }
#endif
                  (void)memcpy(fsa[fsa_pos].job_status[connection[pos].job_no].unique_name,
                               qb[qb_pos].msg_name, MAX_MSG_NAME_LENGTH);
                  if ((fsa[fsa_pos].error_counter == 0) &&
                      (fsa[fsa_pos].auto_toggle == ON) &&
                      (fsa[fsa_pos].original_toggle_pos != NONE) &&
                      (fsa[fsa_pos].max_successful_retries > 0))
                  {
                     if ((fsa[fsa_pos].original_toggle_pos == fsa[fsa_pos].toggle_pos) &&
                         (fsa[fsa_pos].successful_retries > 0))
                     {
                        fsa[fsa_pos].original_toggle_pos = NONE;
                        fsa[fsa_pos].successful_retries = 0;
                     }
                     else if (fsa[fsa_pos].successful_retries >= fsa[fsa_pos].max_successful_retries)
                          {
                             connection[pos].temp_toggle = ON;
                             fsa[fsa_pos].successful_retries = 0;
                          }
                          else
                          {
                             fsa[fsa_pos].successful_retries++;
                          }
                  }

                  /* Create process to distribute file. */
                  if ((connection[pos].pid = make_process(&connection[pos],
                                                          qb_pos)) > 0)
                  {
                     pid = fsa[fsa_pos].job_status[connection[pos].job_no].proc_id = connection[pos].pid;
#ifdef HAVE_SETPRIORITY
                     if (add_afd_priority == YES)
                     {
                        int sched_priority;

                        sched_priority = current_priority + qb[qb_pos].msg_name[MAX_MSG_NAME_LENGTH - 1];
                        if (sched_priority > min_sched_priority)
                        {
                           sched_priority = min_sched_priority;
                        }
                        else if (sched_priority < max_sched_priority)
                             {
                                sched_priority = max_sched_priority;
                             }
                        if (euid != ruid)
                        {
                           if (seteuid(euid) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to set the effective user ID : %s",
                                         strerror(errno));
                           }
                        }
                        if (setpriority(PRIO_PROCESS, pid,
                                        sched_priority) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                                      "Failed to setpriority() to %d of process %d : %s",
# else
                                      "Failed to setpriority() to %d of process %lld : %s",
# endif
                                      sched_priority, (pri_pid_t)pid,
                                      strerror(errno));
                        }
                        if (euid != ruid)
                        {
                           if (seteuid(ruid) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to set back to the real user ID : %s",
                                         strerror(errno));
                           }
                        }
                     }
#endif /* HAVE_SETPRIORITY */
                     fsa[fsa_pos].active_transfers += 1;
                     if ((fsa[fsa_pos].transfer_rate_limit > 0) ||
                         (no_of_trl_groups > 0))
                     {
                        calc_trl_per_process(fsa_pos);
                     }
                     ABS_REDUCE(fsa_pos);
                     qb[qb_pos].connect_pos = pos;
                     p_afd_status->no_of_transfers++;
                  }
                  else
                  {
                     fsa[fsa_pos].job_status[connection[pos].job_no].connect_status = NOT_WORKING;
                     fsa[fsa_pos].job_status[connection[pos].job_no].no_of_files = 0;
                     fsa[fsa_pos].job_status[connection[pos].job_no].no_of_files_done = 0;
                     fsa[fsa_pos].job_status[connection[pos].job_no].file_size = 0;
                     fsa[fsa_pos].job_status[connection[pos].job_no].file_size_done = 0;
                     fsa[fsa_pos].job_status[connection[pos].job_no].file_size_in_use = 0;
                     fsa[fsa_pos].job_status[connection[pos].job_no].file_size_in_use_done = 0;
                     fsa[fsa_pos].job_status[connection[pos].job_no].file_name_in_use[0] = '\0';
                     fsa[fsa_pos].job_status[connection[pos].job_no].file_name_in_use[1] = 0;
#ifdef _WITH_BURST_2
                     fsa[fsa_pos].job_status[connection[pos].job_no].unique_name[0] = '\0';
#endif
                     connection[pos].hostname[0] = '\0';
                     connection[pos].msg_name[0] = '\0';
                     connection[pos].host_id = 0;
                     connection[pos].job_no = -1;
                     connection[pos].fsa_pos = -1;
                     connection[pos].fra_pos = -1;
                     connection[pos].pid = 0;
                  }
               }
#ifdef _WITH_SERVER_SUPPORT
# ifdef LOCK_DEBUG
               unlock_region(fsa_fd, (AFD_WORD_OFFSET +
                                      (fsa_pos * sizeof(struct filetransfer_status)) +
                                      LOCK_CON), __FILE__, __LINE__);
# else
               unlock_region(fsa_fd, (AFD_WORD_OFFSET +
                                      (fsa_pos * sizeof(struct filetransfer_status)) +
                                      LOCK_CON));
# endif
#endif
            }
         }
      }
   }
   return(pid);
}


/*---------------------------- make_process() ---------------------------*/
static pid_t
make_process(struct connection *con, int qb_pos)
{
   int   argcount;
   pid_t pid;
#ifdef HAVE_HW_CRC32
   char  *args[21],
#else
   char  *args[20],
#endif
#ifdef USE_SPRINTF
         str_job_no[MAX_INT_LENGTH],
         str_fsa_pos[MAX_INT_LENGTH],
#else
         str_job_no[4],
         str_fsa_pos[6],
#endif
         str_retries[MAX_INT_LENGTH];

#ifdef USE_SPRINTF
# ifdef HAVE_SNPRINTF
   (void)snprintf(str_job_no, MAX_INT_LENGTH, "%d", con->job_no);
# else
   (void)sprintf(str_job_no, "%d", con->job_no);
# endif
#else
   if (con->job_no < 10)
   {
      str_job_no[0] = con->job_no + '0';
      str_job_no[1] = '\0';
   }
   else if (con->job_no < 100)
        {
           str_job_no[0] = (con->job_no / 10) + '0';
           str_job_no[1] = (con->job_no % 10) + '0';
           str_job_no[2] = '\0';
        }
   else if (con->fsa_pos < 1000)
        {
           str_job_no[0] = (con->job_no / 100) + '0';
           str_job_no[1] = ((con->job_no / 10) % 10) + '0';
           str_job_no[2] = (con->job_no % 10) + '0';
           str_job_no[3] = '\0';
        }
        else
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Insert a '#define USE_SPRINTF' in this program! Or else you are in deep trouble!");
           str_job_no[0] = ((con->job_no / 100) % 10) + '0';
           str_job_no[1] = ((con->job_no / 10) % 10) + '0';
           str_job_no[2] = (con->job_no % 10) + '0';
           str_job_no[3] = '\0';
        }
#endif

#ifdef USE_SPRINTF
# ifdef HAVE_SNPRINTF
   (void)snprintf(str_fsa_pos, MAX_INT_LENGTH, "%d", con->fsa_pos);
# else
   (void)sprintf(str_fsa_pos, "%d", con->fsa_pos);
# endif
#else
   if (con->fsa_pos < 10)
   {
      str_fsa_pos[0] = con->fsa_pos + '0';
      str_fsa_pos[1] = '\0';
   }
   else if (con->fsa_pos < 100)
        {
           str_fsa_pos[0] = (con->fsa_pos / 10) + '0';
           str_fsa_pos[1] = (con->fsa_pos % 10) + '0';
           str_fsa_pos[2] = '\0';
        }
   else if (con->fsa_pos < 1000)
        {
           str_fsa_pos[0] = (con->fsa_pos / 100) + '0';
           str_fsa_pos[1] = ((con->fsa_pos / 10) % 10) + '0';
           str_fsa_pos[2] = (con->fsa_pos % 10) + '0';
           str_fsa_pos[3] = '\0';
        }
   else if (con->fsa_pos < 10000)
        {
           str_fsa_pos[0] = (con->fsa_pos / 1000) + '0';
           str_fsa_pos[1] = ((con->fsa_pos / 100) % 10) + '0';
           str_fsa_pos[2] = ((con->fsa_pos / 10) % 10) + '0';
           str_fsa_pos[3] = (con->fsa_pos % 10) + '0';
           str_fsa_pos[4] = '\0';
        }
   else if (con->fsa_pos < 100000)
        {
           str_fsa_pos[0] = (con->fsa_pos / 10000) + '0';
           str_fsa_pos[1] = ((con->fsa_pos / 1000) % 10) + '0';
           str_fsa_pos[2] = ((con->fsa_pos / 100) % 10) + '0';
           str_fsa_pos[3] = ((con->fsa_pos / 10) % 10) + '0';
           str_fsa_pos[4] = (con->fsa_pos % 10) + '0';
           str_fsa_pos[5] = '\0';
        }
        else
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Insert a '#define USE_SPRINTF' in this program! Or else you are in deep trouble!");
           str_fsa_pos[0] = ((con->fsa_pos / 10000) % 10) + '0';
           str_fsa_pos[1] = ((con->fsa_pos / 1000) % 10) + '0';
           str_fsa_pos[2] = ((con->fsa_pos / 100) % 10) + '0';
           str_fsa_pos[3] = ((con->fsa_pos / 10) % 10) + '0';
           str_fsa_pos[4] = (con->fsa_pos % 10) + '0';
           str_fsa_pos[5] = '\0';
        }
#endif

   if ((fsa[con->fsa_pos].protocol_options & FILE_WHEN_LOCAL_FLAG) &&
       (check_local_interface_names(fsa[con->fsa_pos].real_hostname[(int)(fsa[con->fsa_pos].host_toggle - 1)]) == YES))
   {
      args[0] = SEND_FILE_LOC;
   }
   else
   {
      if (con->protocol == FTP)
      {
         if (con->msg_name[0] == '\0')
         {
            if (fsa[con->fsa_pos].debug > YES)
            {
               args[0] = GET_FILE_FTP_TRACE;
            }
            else
            {
               args[0] = GET_FILE_FTP;
            }
         }
         else
         {
            if (fsa[con->fsa_pos].debug > YES)
            {
               args[0] = SEND_FILE_FTP_TRACE;
            }
            else
            {
               args[0] = SEND_FILE_FTP;
            }
         }
      }
      else if (con->protocol == LOC)
           {
              args[0] = SEND_FILE_LOC;
           }
#ifdef _WITH_SCP_SUPPORT
      else if (con->protocol == SCP)
           {
              if (fsa[con->fsa_pos].debug > YES)
              {
                 args[0] = SEND_FILE_SCP_TRACE;
              }
              else
              {
                 args[0] = SEND_FILE_SCP;
              }
           }
#endif
#ifdef _WITH_WMO_SUPPORT
      else if (con->protocol == WMO)
           {
              if (fsa[con->fsa_pos].debug > YES)
              {
                 args[0] = SEND_FILE_WMO_TRACE;
              }
              else
              {
                 args[0] = SEND_FILE_WMO;
              }
           }
#endif
#ifdef _WITH_MAP_SUPPORT
      else if (con->protocol == MAP)
           {
              args[0] = SEND_FILE_MAP;
           }
#endif
      else if (con->protocol == SFTP)
           {
              if (con->msg_name[0] == '\0')
              {
                 if (fsa[con->fsa_pos].debug > YES)
                 {
                    args[0] = GET_FILE_SFTP_TRACE;
                 }
                 else
                 {
                    args[0] = GET_FILE_SFTP;
                 }
              }
              else
              {
                 if (fsa[con->fsa_pos].debug > YES)
                 {
                    args[0] = SEND_FILE_SFTP_TRACE;
                 }
                 else
                 {
                    args[0] = SEND_FILE_SFTP;
                 }
              }
           }
      else if (con->protocol == HTTP)
           {
              if (con->msg_name[0] == '\0')
              {
                 if (fsa[con->fsa_pos].debug > YES)
                 {
                    args[0] = GET_FILE_HTTP_TRACE;
                 }
                 else
                 {
                    args[0] = GET_FILE_HTTP;
                 }
              }
              else
              {
                 if (fsa[con->fsa_pos].debug > YES)
                 {
                    args[0] = SEND_FILE_HTTP_TRACE;
                 }
                 else
                 {
                    args[0] = SEND_FILE_HTTP;
                 }
              }
           }
      else if (con->protocol == SMTP)
           {
              if (fsa[con->fsa_pos].debug > YES)
              {
                 args[0] = SEND_FILE_SMTP_TRACE;
              }
              else
              {
                 args[0] = SEND_FILE_SMTP;
              }
           }
      else if (con->protocol == EXEC)
           {
              if (con->msg_name[0] == '\0')
              {
                 args[0] = GET_FILE_EXEC;
              }
              else
              {
                 args[0] = SEND_FILE_EXEC;
              }
           }
           else
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         ".....? Unknown protocol (%d)", con->protocol);
              return(INCORRECT);
           }
   }
   args[1] = p_work_dir;
   args[2] = str_job_no;
   args[3] = str_fsa_id;
   args[4] = str_fsa_pos;
   if (con->msg_name[0] == '\0')
   {
      args[5] = fra[con->fra_pos].dir_alias;
   }
   else
   {
      args[5] = con->msg_name;
   }
   argcount = 5;
   if (con->msg_name[0] != '\0')
   {
      if (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_ARCHIVE)
      {
         argcount++;
         args[argcount] = "-A";
      }
      if (con->resend == YES)
      {
         argcount++;
         args[argcount] = "-r";
      }
      if (default_age_limit > 0)
      {
         argcount++;
         args[argcount] = "-a";
         argcount++;
         args[argcount] = str_age_limit;
      }
   }
   else
   {
      if (qb[qb_pos].special_flag & HELPER_JOB)
      {
         argcount++;
         args[argcount] = "-d";
      }
      argcount++;
      args[argcount] = "-i";
      argcount++;
      args[argcount] = str_remote_file_check_interval;
   }
   if (con->temp_toggle == ON)
   {
      argcount++;
      args[argcount] = "-t";
   }
   if (con->protocol == SMTP)
   {
      if (default_smtp_from != NULL)
      {
         argcount++;
         args[argcount] = "-f";
         argcount++;
         args[argcount] = default_smtp_from;
      }
      if (default_smtp_reply_to != NULL)
      {
         argcount++;
         args[argcount] = "-R";
         argcount++;
         args[argcount] = default_smtp_reply_to;
      }
      if (default_smtp_server[0] != '\0')
      {
         argcount++;
         args[argcount] = "-s";
         argcount++;
         args[argcount] = default_smtp_server;
      }
   }
   if (con->protocol == HTTP)
   {
      if (default_http_proxy[0] != '\0')
      {
         argcount++;
         args[argcount] = "-h";
         argcount++;
         args[argcount] = default_http_proxy;
      }
   }
   if (qb[qb_pos].retries > 0)
   {
      argcount++;
      args[argcount] = "-o";
#ifdef HAVE_SNPRINTF
      (void)snprintf(str_retries, MAX_INT_LENGTH, "%u", qb[qb_pos].retries);
#else
      (void)sprintf(str_retries, "%u", qb[qb_pos].retries);
#endif
      argcount++;
      args[argcount] = str_retries;
   }
#ifdef HAVE_HW_CRC32
   if (have_hw_crc32 == YES)
   {
      argcount++;
      args[argcount] = "-c";
   }
#endif
   args[argcount + 1] = NULL;

   switch (pid = fork())
   {
      case -1 :

         /* Could not generate process. */
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s", strerror(errno));
         return(INCORRECT);

      case  0 : /* Child process. */

#ifdef WITH_MEMCHECK
         muntrace();
#endif
         (void)execvp(args[0], args);
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to start process %s : %s",
                    args[0], strerror(errno));
         pid = getpid();
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (write(read_fin_writefd, &pid, sizeof(pid_t)) != sizeof(pid_t))
#else
         if (write(read_fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
         _exit(INCORRECT);

      default :

         /* Parent process. */
         p_afd_status->fd_fork_counter++;
         return(pid);
   }
}


/*+++++++++++++++++++++++++ check_zombie_queue() ++++++++++++++++++++++++*/
static void
check_zombie_queue(time_t now, int qb_pos)
{
   int faulty;

   if (qb_pos != -1)
   {
      if ((faulty = zombie_check(&connection[qb[qb_pos].connect_pos], now,
                                 &qb_pos, WNOHANG)) == NO)
      {
         remove_msg(qb_pos);
      }
      else if ((faulty == YES) || (faulty == NONE))
           {
              qb[qb_pos].pid = PENDING;
              if (qb[qb_pos].msg_name[0] != '\0')
              {
                 fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
              }
              else
              {
                 fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
              }
           }
      else if (faulty == NEITHER)
           {
              if (no_of_zombie_waitstates < max_connections)
              {
                 register int i;
                 int          gotcha = NO;

                 for (i = 0; i < no_of_zombie_waitstates; i++)
                 {
                    if (zwl[i] == qb[qb_pos].connect_pos)
                    {
                       gotcha = YES;
                       break;
                    }
                 }
                 if (gotcha == NO)
                 {
                    zwl[no_of_zombie_waitstates] = qb[qb_pos].connect_pos;
                    no_of_zombie_waitstates++;
                 }
              }
              else
              {
                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Oops, how can this be? no_of_zombie_waitstates is %d, but maximum is %d!",
                            no_of_zombie_waitstates + 1, max_connections);
              }
           }
   }

   if ((no_of_zombie_waitstates > 1) ||
       ((no_of_zombie_waitstates == 1) &&
        ((qb_pos == -1) || (faulty != NEITHER))))
   {
      int i,
          remove_from_zombie_queue,
          tmp_qb_pos;

      for (i = 0; i < no_of_zombie_waitstates; i++)
      {
         if (zwl[i] < max_connections)
         {
            remove_from_zombie_queue = NO;
            qb_pos_pid(connection[zwl[i]].pid, &tmp_qb_pos);
            if (tmp_qb_pos != -1)
            {
               if ((faulty = zombie_check(&connection[zwl[i]], now, &tmp_qb_pos,
                                          WNOHANG)) == NO)
               {
                  remove_msg(tmp_qb_pos);
                  remove_from_zombie_queue = YES;
               }
               else if ((faulty == YES) || (faulty == NONE))
                    {
                       qb[tmp_qb_pos].pid = PENDING;
                       if (qb[tmp_qb_pos].msg_name[0] != '\0')
                       {
                          fsa[mdb[qb[tmp_qb_pos].pos].fsa_pos].jobs_queued++;
                       }
                       else
                       {
                          fsa[fra[qb[tmp_qb_pos].pos].fsa_pos].jobs_queued++;
                       }
                       remove_from_zombie_queue = YES;
                    }
            }
            else
            {
               remove_from_zombie_queue = YES;
            }
            if (remove_from_zombie_queue == YES)
            {
               if (i != (no_of_zombie_waitstates - 1))
               {
                  size_t move_size;

                  move_size = (no_of_zombie_waitstates - (i + 1)) * sizeof(int);
                  (void)memmove(&zwl[i], &zwl[i + 1], move_size);
               }
               no_of_zombie_waitstates--;
               i--;
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Oops, how can this be? Connect position is %d, but maximum is %d!",
                       zwl[i], max_connections);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*
 * Description : Checks if any process is finished (zombie), if this
 *               is the case it is killed with waitpid().
 */
static int
zombie_check(struct connection *p_con,
             time_t            now,
             int               *qb_pos,
             int               options)
{
   if (p_con->pid > 0)
   {
      int           faulty = YES,
                    status;
      pid_t         ret;
#ifdef HAVE_WAIT4
      struct rusage ru;
#endif

      /* Wait for process to terminate. */
#ifdef HAVE_WAIT4
      if ((ret = wait4(p_con->pid, &status, options, &ru)) == p_con->pid)
#else
      if ((ret = waitpid(p_con->pid, &status, options)) == p_con->pid)
#endif
      {
         if (WIFEXITED(status))
         {
            int exit_status;

            qb[*qb_pos].retries++;
            switch (exit_status = WEXITSTATUS(status))
            {
               case STILL_FILES_TO_SEND   :
               case TRANSFER_SUCCESS      : /* Ordinary end of process. */
                  if (((p_con->temp_toggle == ON) &&
                       (fsa[p_con->fsa_pos].original_toggle_pos != fsa[p_con->fsa_pos].host_toggle)) ||
                      (fsa[p_con->fsa_pos].original_toggle_pos == fsa[p_con->fsa_pos].host_toggle))
                  {
                     /*
                      * Do not forget to toggle back to the original
                      * host and deactivate original_toggle_pos!
                      */
                     p_con->temp_toggle = OFF;
                     fsa[p_con->fsa_pos].successful_retries = 0;
                     if (fsa[p_con->fsa_pos].original_toggle_pos != NONE)
                     {
                        fsa[p_con->fsa_pos].host_toggle = fsa[p_con->fsa_pos].original_toggle_pos;
                        fsa[p_con->fsa_pos].original_toggle_pos = NONE;
                        fsa[p_con->fsa_pos].host_dsp_name[(int)fsa[p_con->fsa_pos].toggle_pos] = fsa[p_con->fsa_pos].host_toggle_str[(int)fsa[p_con->fsa_pos].host_toggle];
                        system_log(INFO_SIGN, NULL, 0,
                                   "Switching back to host <%s> after successful transfer.",
                                   fsa[p_con->fsa_pos].host_dsp_name);
                     }
                  }
                  fsa[p_con->fsa_pos].last_connection = now;
                  fsa[p_con->fsa_pos].first_error_time = 0L;
                  if ((exit_status == STILL_FILES_TO_SEND) ||
                      ((qb[*qb_pos].special_flag & BURST_REQUEUE) &&
                       (fsa[p_con->fsa_pos].job_status[p_con->job_no].unique_name[2] > 6)))
                  {
                     faulty = NONE;
                  }
                  else
                  {
                     faulty = NO;
                  }
                  exit_status = TRANSFER_SUCCESS;
                  break;

               case SYNTAX_ERROR          : /* Syntax for sf_xxx/gf_xxx wrong. */
                  if ((remove_error_jobs_not_in_queue == YES) &&
                      (mdb[qb[*qb_pos].pos].in_current_fsa != YES) &&
                      (p_con->msg_name[0] != '\0'))
                  {
                     char del_dir[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
                     (void)snprintf(del_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                     (void)sprintf(del_dir, "%s%s%s/%s",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   p_con->msg_name);
#ifdef _DELETE_LOG
                     extract_cus(p_con->msg_name, dl.input_time,
                                 dl.split_job_counter, dl.unique_number);
                     remove_job_files(del_dir, -1,
                                      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                      FD, DELETE_STALE_ERROR_JOBS, -1);
#else
                     remove_job_files(del_dir, -1, -1);
#endif
                  }
                  else
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

#ifdef WITH_MULTI_FSA_CHECKS
                     if (fd_check_fsa() == YES)
                     {
                        if (check_fra_fd() == YES)
                        {
                           init_fra_data();
                        }
                        get_new_positions();
                        init_msg_buffer();
                        last_pos_lookup = INCORRECT;
                     }
#endif
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].connect_status = NOT_WORKING;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files = 0;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files_done = 0;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size = 0;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_done = 0;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use = 0;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use_done = 0;
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].file_name_in_use[0] = '\0';
                     fsa[p_con->fsa_pos].job_status[p_con->job_no].file_name_in_use[1] = 0;

                     /*
                      * Lets assume that in the case for sf_xxx the message is
                      * moved to the faulty directory. So no further action is
                      * necessary.
                      */
                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Syntax for calling program wrong. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               __FILE__, __LINE__);
                  }
                  break;

               case NO_MESSAGE_FILE : /* The message file has disappeared. */
                                      /* Remove the job, or else we        */
                                      /* will always fall for this one     */
                                      /* again.                            */
                  if (p_con->msg_name[0] != '\0')
                  {
                     char del_dir[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
                     (void)snprintf(del_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                     (void)sprintf(del_dir, "%s%s%s/%s",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   p_con->msg_name);
#ifdef _DELETE_LOG
                     extract_cus(p_con->msg_name, dl.input_time,
                                 dl.split_job_counter, dl.unique_number);
                     remove_job_files(del_dir, -1,
                                      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                      FD, NO_MESSAGE_FILE_DEL, -1);
#else
                     remove_job_files(del_dir, -1, -1);
#endif
                  }
                  break;

               case JID_NUMBER_ERROR      : /* Hmm, failed to determine JID */
                                            /* number, lets assume the      */
                                            /* queue entry is corrupted.    */

                  if ((remove_error_jobs_not_in_queue == YES) &&
                      (mdb[qb[*qb_pos].pos].in_current_fsa != YES) &&
                      (p_con->msg_name[0] != '\0'))
                  {
                     char del_dir[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
                     (void)snprintf(del_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                     (void)sprintf(del_dir, "%s%s%s/%s",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   p_con->msg_name);
#ifdef _DELETE_LOG
                     extract_cus(p_con->msg_name, dl.input_time,
                                 dl.split_job_counter, dl.unique_number);
                     remove_job_files(del_dir, -1,
                                      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                      FD, DELETE_STALE_ERROR_JOBS, -1);
#else
                     remove_job_files(del_dir, -1, -1);
#endif
                  }
                  else
                  {
                     /* Note: We have to trust the queue here to get the correct  */
                     /*       connect position in struct connection. How else do  */
                     /*       we know which values are to be reset in the connect */
                     /*       structure!? :-((((                                  */
                     faulty = NO;
                  }
                  break;

               case OPEN_FILE_DIR_ERROR   : /* File directory does not exist. */
                  /*
                   * Since sf_xxx has already reported this incident, no
                   * need to do it here again.
                   */
                  faulty = NO;
                  break;

               case MAIL_ERROR            : /* Failed to send mail to remote host. */
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Failed to send mail. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               __FILE__, __LINE__);
                  }
                  break;

               case TIMEOUT_ERROR         : /* Timeout arrived. */
               case CONNECTION_RESET_ERROR: /* Connection reset by peer. */
               case CONNECT_ERROR         : /* Failed to connect to remote host. */
               case CONNECTION_REFUSED_ERROR: /* Connection refused. */
#ifdef WITH_SSL
               case AUTH_ERROR            : /* SSL/TLS authentification error. */
#endif
               case TYPE_ERROR            : /* Setting transfer type failed. */
               case DATA_ERROR            : /* Failed to send data command. */
               case READ_LOCAL_ERROR      : /* */
               case WRITE_LOCAL_ERROR     : /* */
               case READ_REMOTE_ERROR     : /* */
               case SIZE_ERROR            : /* */
               case DATE_ERROR            : /* */
               case OPEN_LOCAL_ERROR      : /* */
               case WRITE_LOCK_ERROR      : /* */
               case CHOWN_ERROR           : /* sf_loc function check_create_path. */
#ifdef _WITH_WMO_SUPPORT
               case CHECK_REPLY_ERROR     : /* Did not get a correct reply. */
#endif
               case REMOVE_LOCKFILE_ERROR : /* */
               case QUIT_ERROR            : /* Failed to disconnect. */
               case RENAME_ERROR          : /* Rename file locally. */
               case SELECT_ERROR          : /* Selecting on sf_xxx command fifo. */
#ifdef _WITH_WMO_SUPPORT
               case SIG_PIPE_ERROR        : /* When sf_wmo receives a SIGPIPE. */
#endif
#ifdef _WITH_MAP_SUPPORT
               case MAP_FUNCTION_ERROR    : /* MAP function call has failed. */
#endif
                  if ((remove_error_jobs_not_in_queue == YES) &&
                      (mdb[qb[*qb_pos].pos].in_current_fsa != YES) &&
                      (p_con->msg_name[0] != '\0'))
                  {
                     char del_dir[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
                     (void)snprintf(del_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                     (void)sprintf(del_dir, "%s%s%s/%s",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   p_con->msg_name);
#ifdef _DELETE_LOG
                     extract_cus(p_con->msg_name, dl.input_time,
                                 dl.split_job_counter, dl.unique_number);
                     remove_job_files(del_dir, -1,
                                      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                      FD, DELETE_STALE_ERROR_JOBS, -1);
#else
                     remove_job_files(del_dir, -1, -1);
#endif
                  }
                  else
                  {
                     if (fsa[p_con->fsa_pos].first_error_time == 0L)
                     {
                        fsa[p_con->fsa_pos].first_error_time = now;
                     }
#ifdef WITH_ERROR_QUEUE
                     if ((p_con->msg_name[0] != '\0') &&
                         (fsa[p_con->fsa_pos].host_status & ERROR_QUEUE_SET))
                     {
                        update_time_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                now + fsa[p_con->fsa_pos].retry_interval);
                     }
#endif
                  }
                  break;

               case REMOTE_USER_ERROR     : /* Failed to send mail address. */
               case USER_ERROR            : /* User name wrong. */
               case PASSWORD_ERROR        : /* Password wrong. */
               case CHDIR_ERROR           : /* Change remote directory. */
               case CLOSE_REMOTE_ERROR    : /* Close remote file. */
               case MKDIR_ERROR           : /* */
               case MOVE_ERROR            : /* Move file locally. */
               case STAT_TARGET_ERROR     : /* Failed to access target dir. */
               case WRITE_REMOTE_ERROR    : /* */
               case MOVE_REMOTE_ERROR     : /* */
               case OPEN_REMOTE_ERROR     : /* Failed to open remote file. */
               case LIST_ERROR            : /* Sending the LIST command failed. */
               case EXEC_ERROR            : /* Failed to execute command for */
                                            /* scheme exec.                  */
               case FILE_SIZE_MATCH_ERROR : /* Local and remote file size do */
                                            /* not match.                    */
                  if ((remove_error_jobs_not_in_queue == YES) &&
                      (mdb[qb[*qb_pos].pos].in_current_fsa != YES) &&
                      (p_con->msg_name[0] != '\0'))
                  {
                     char del_dir[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
                     (void)snprintf(del_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                     (void)sprintf(del_dir, "%s%s%s/%s",
#endif
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   p_con->msg_name);
#ifdef _DELETE_LOG
                     extract_cus(p_con->msg_name, dl.input_time,
                                 dl.split_job_counter, dl.unique_number);
                     remove_job_files(del_dir, -1,
                                      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                      FD, DELETE_STALE_ERROR_JOBS, -1);
#else
                     remove_job_files(del_dir, -1, -1);
#endif
                  }
                  else
                  {
                     if (fsa[p_con->fsa_pos].protocol_options & NO_AGEING_JOBS)
                     {
#ifdef WITH_ERROR_QUEUE
                        if ((p_con->msg_name[0] != '\0') &&
                            (fsa[p_con->fsa_pos].host_status & ERROR_QUEUE_SET))
                        {
                           update_time_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                   now + fsa[p_con->fsa_pos].retry_interval);
                        }
#endif
                     }
                     else
                     {
                        if (*qb_pos < *no_msg_queued)
                        {
                           if (qb[*qb_pos].msg_number < max_threshold)
                           {
                              register int i = *qb_pos + 1;

                              /*
                               * Increase the message number, so that this job
                               * will decrease in priority and resort the queue.
                               */
                              if (qb[*qb_pos].retries < RETRY_THRESHOLD)
                              {
#ifdef WITH_ERROR_QUEUE
                                 if (p_con->msg_name[0] != '\0')
                                 {
                                    if (qb[*qb_pos].retries == 1)
                                    {
                                       add_to_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                          fsa, p_con->fsa_pos, fsa_fd,
                                                          exit_status,
                                                          now + fsa[p_con->fsa_pos].retry_interval);
                                    }
                                    else
                                    {
                                       update_time_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                               now + fsa[p_con->fsa_pos].retry_interval);
                                    }
                                 }
#endif
                                 qb[*qb_pos].msg_number += 60000000.0;
                              }
                              else
                              {
#ifdef WITH_ERROR_QUEUE
                                 if (p_con->msg_name[0] != '\0')
                                 {
                                    update_time_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                            now + fsa[p_con->fsa_pos].retry_interval);
                                 }
#endif
                                 qb[*qb_pos].msg_number += ((double)qb[*qb_pos].creation_time * 10000.0 *
                                                           (double)(qb[*qb_pos].retries - RETRY_THRESHOLD - 1));
                              }
                              while ((i < *no_msg_queued) &&
                                     (qb[*qb_pos].msg_number > qb[i].msg_number))
                              {
                                 i++;
                              }
                              if (i > (*qb_pos + 1))
                              {
                                 size_t           move_size;
                                 struct queue_buf tmp_qb;

                                 (void)memcpy(&tmp_qb, &qb[*qb_pos],
                                              sizeof(struct queue_buf));
                                 i--;
                                 move_size = (i - *qb_pos) * sizeof(struct queue_buf);
                                 (void)memmove(&qb[*qb_pos], &qb[*qb_pos + 1], move_size);
                                 (void)memcpy(&qb[i], &tmp_qb,
                                              sizeof(struct queue_buf));
                                 *qb_pos = i;
                              }
                           }
#ifdef WITH_ERROR_QUEUE
                           else
                           {
                              if (qb[*qb_pos].retries < RETRY_THRESHOLD)
                              {
                                 if (p_con->msg_name[0] != '\0')
                                 {
                                    if (qb[*qb_pos].retries == 1)
                                    {
                                       add_to_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                          fsa, p_con->fsa_pos, fsa_fd,
                                                          exit_status,
                                                          now + fsa[p_con->fsa_pos].retry_interval);
                                    }
                                    else
                                    {
                                       update_time_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                               now + fsa[p_con->fsa_pos].retry_interval);
                                    }
                                 }
                              }
                              else
                              {
                                 if (p_con->msg_name[0] != '\0')
                                 {
                                    if (update_time_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                                now + fsa[p_con->fsa_pos].retry_interval) == NEITHER)
                                    {
                                       add_to_error_queue(fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                                                          fsa, p_con->fsa_pos, fsa_fd,
                                                          exit_status,
                                                          now + fsa[p_con->fsa_pos].retry_interval);
                                    }
                                 }
                              }
                           }
#endif
                        }
                     }
                     if (fsa[p_con->fsa_pos].first_error_time == 0L)
                     {
                        fsa[p_con->fsa_pos].first_error_time = now;
                     }
                  }
                  break;

               case STAT_ERROR            : /* */
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     if (fsa[p_con->fsa_pos].first_error_time == 0L)
                     {
                        fsa[p_con->fsa_pos].first_error_time = now;
                     }
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Disconnected. Could not stat() local file. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               __FILE__, __LINE__);
                  }
                  break;

               case LOCK_REGION_ERROR     : /* */
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Disconnected. Failed to lock region. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               __FILE__, __LINE__);
                  }
                  break;

               case UNLOCK_REGION_ERROR   : /* */
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Disconnected. Failed to unlock region. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               __FILE__, __LINE__);
                  }
                  break;

               case GOT_KILLED : /* Process has been killed, most properly */
                                 /* by this process.                       */
                  faulty = NONE;
                  fsa[p_con->fsa_pos].job_status[p_con->job_no].connect_status = DISCONNECT;
                  break;

               case NO_FILES_TO_SEND : /* There are no files to send. Most */
                                       /* properly the files have been     */
                                       /* deleted due to age.              */
                  /*
                   * This is actually a good time to check if there are
                   * at all any files to be send. If NOT and the auto
                   * pause queue flag is set, we might get a deadlock.
                   */
                  if (p_con->fsa_pos != -1)
                  {
                     if ((fsa[p_con->fsa_pos].total_file_counter == 0) &&
                         (fsa[p_con->fsa_pos].total_file_size == 0) &&
                         (fsa[p_con->fsa_pos].host_status & AUTO_PAUSE_QUEUE_STAT))
                     {
                        off_t lock_offset;
                        char  *sign;

                        lock_offset = AFD_WORD_OFFSET +
                                      (p_con->fsa_pos * sizeof(struct filetransfer_status));

                        if (fsa[p_con->fsa_pos].error_counter > 0)
                        {
                           int i;
#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, lock_offset + LOCK_EC,
                                         __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, lock_offset + LOCK_EC);
#endif
                           fsa[p_con->fsa_pos].error_counter = 0;
                           fsa[p_con->fsa_pos].error_history[0] = 0;
                           fsa[p_con->fsa_pos].error_history[1] = 0;
                           for (i = 0; i < fsa[p_con->fsa_pos].allowed_transfers; i++)
                           {
                              if (fsa[p_con->fsa_pos].job_status[i].connect_status == NOT_WORKING)
                              {
                                 fsa[p_con->fsa_pos].job_status[i].connect_status = DISCONNECT;
                              }
                           }
#ifdef LOCK_DEBUG
                           unlock_region(fsa_fd, lock_offset + LOCK_EC,
                                         __FILE__, __LINE__);
#else
                           unlock_region(fsa_fd, lock_offset + LOCK_EC);
#endif
                        }
#ifdef LOCK_DEBUG
                        lock_region_w(fsa_fd, lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                        lock_region_w(fsa_fd, lock_offset + LOCK_HS);
#endif
                        fsa[p_con->fsa_pos].host_status &= ~AUTO_PAUSE_QUEUE_STAT;
                        if (fsa[p_con->fsa_pos].last_connection > fsa[p_con->fsa_pos].first_error_time)
                        {
                           if (fsa[p_con->fsa_pos].host_status & HOST_ERROR_EA_STATIC)
                           {
                              fsa[p_con->fsa_pos].host_status &= ~EVENT_STATUS_STATIC_FLAGS;
                           }
                           else
                           {
                              fsa[p_con->fsa_pos].host_status &= ~EVENT_STATUS_FLAGS;
                           }
                           fsa[p_con->fsa_pos].host_status &= ~PENDING_ERRORS;
                           error_action(fsa[p_con->fsa_pos].host_alias, "stop",
                                        HOST_ERROR_ACTION);
                           event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                                     fsa[p_con->fsa_pos].host_alias);
                        }
#ifdef LOCK_DEBUG
                        unlock_region(fsa_fd, lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                        unlock_region(fsa_fd, lock_offset + LOCK_HS);
#endif
                        if ((fsa[p_con->fsa_pos].host_status & HOST_ERROR_OFFLINE_STATIC) ||
                            (fsa[p_con->fsa_pos].host_status & HOST_ERROR_OFFLINE) ||
                            (fsa[p_con->fsa_pos].host_status & HOST_ERROR_OFFLINE_T))
                        {
                           sign = OFFLINE_SIGN;
                        }
                        else
                        {
                           sign = INFO_SIGN;
                        }
                        system_log(sign, __FILE__, __LINE__,
                                   "Starting input queue for %s that was stopped by init_afd.",
                                    fsa[p_con->fsa_pos].host_alias);
                        event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                                  fsa[p_con->fsa_pos].host_alias);
                     }
                  }
                  remove_connection(p_con, NEITHER, now);
                  return(NO);

               case ALLOC_ERROR           : /* */
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Failed to allocate memory. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               __FILE__, __LINE__);
                  }
                  break;

               default                    : /* Unknown error. */
                  {
                     char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                     (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                     (void)rec(transfer_log_fd, WARN_SIGN,
                               "%-*s[%d]: Disconnected due to an unknown error (%d). (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                               exit_status, __FILE__, __LINE__);
                  }
                  break;
            }
            (void)memmove(&fsa[p_con->fsa_pos].error_history[1],
                          fsa[p_con->fsa_pos].error_history,
                          (ERROR_HISTORY_LENGTH - 1));
            if ((exit_status == GOT_KILLED) ||
                (fsa[p_con->fsa_pos].host_status & HOST_ERROR_OFFLINE) ||
                (fsa[p_con->fsa_pos].host_status & HOST_ERROR_OFFLINE_T) ||
                (fsa[p_con->fsa_pos].host_status & HOST_ERROR_OFFLINE_STATIC))
            {
               /*
                * This will ensure that this host will not be shown in
                * mon_ctrl dialog when pressing on the error_counter or
                * error_host number.
                */
               fsa[p_con->fsa_pos].error_history[0] = 0;
            }
            else
            {
               fsa[p_con->fsa_pos].error_history[0] = (unsigned char)exit_status;
            }

#ifdef HAVE_WAIT4
            p_afd_status->fd_child_utime.tv_usec += ru.ru_utime.tv_usec;
            if (p_afd_status->fd_child_utime.tv_usec > 1000000000L)
            {
               p_afd_status->fd_child_utime.tv_sec++;
               p_afd_status->fd_child_utime.tv_usec -= 1000000000L;
            }
            p_afd_status->fd_child_utime.tv_sec += ru.ru_utime.tv_sec;

            /* System CPU time. */
            p_afd_status->fd_child_stime.tv_usec += ru.ru_stime.tv_usec;
            if (p_afd_status->fd_child_stime.tv_usec > 1000000000L)
            {
               p_afd_status->fd_child_stime.tv_sec++;
               p_afd_status->fd_child_stime.tv_usec -= 1000000000L;
            }
            p_afd_status->fd_child_stime.tv_sec += ru.ru_stime.tv_sec;
#endif

            /*
             * When auto_toggle is active and we have just tried
             * the original host, lets not slow things done by
             * making this appear as an error. The second host
             * might be perfectly okay, lets continue sending
             * files as quickly as possible. So when temp_toggle
             * is ON, it may NEVER be faulty.
             */
            if ((p_con->temp_toggle == ON) && (faulty == YES))
            {
               faulty = NONE;
            }
         }
         else if (WIFSIGNALED(status))
              {
                 int  signum;
                 char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                 /* Abnormal termination. */
#ifdef WITH_MULTI_FSA_CHECKS
                 if (fd_check_fsa() == YES)
                 {
                    if (check_fra_fd() == YES)
                    {
                       init_fra_data();
                    }
                    get_new_positions();
                    init_msg_buffer();
                    last_pos_lookup = INCORRECT;
                 }
#endif
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].connect_status = NOT_WORKING;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files = 0;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files_done = 0;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size = 0;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_done = 0;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use = 0;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use_done = 0;
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].file_name_in_use[0] = '\0';
                 fsa[p_con->fsa_pos].job_status[p_con->job_no].file_name_in_use[1] = 0;

                 (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                 signum = WTERMSIG(status);
                 if (signum == SIGUSR1)
                 {
                    (void)rec(transfer_log_fd, DEBUG_SIGN,
#if SIZEOF_PID_T == 4
                              "%-*s[%d]: Abnormal termination (by signal %d) of transfer job (%d). (%s %d)\n",
#else
                              "%-*s[%d]: Abnormal termination (by signal %lld) of transfer job (%d). (%s %d)\n",
#endif
                              MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                              signum, (pri_pid_t)p_con->pid, __FILE__, __LINE__);
                 }
                 else
                 {
                    (void)rec(transfer_log_fd, WARN_SIGN,
#if SIZEOF_PID_T == 4
                              "%-*s[%d]: Abnormal termination (by signal %d) of transfer job (%d). (%s %d)\n",
#else
                              "%-*s[%d]: Abnormal termination (by signal %d) of transfer job (%lld). (%s %d)\n",
#endif
                              MAX_HOSTNAME_LENGTH, tr_hostname, p_con->job_no,
                              signum, (pri_pid_t)p_con->pid, __FILE__, __LINE__);
                 }
              }
         else if (WIFSTOPPED(status))
              {
                 char tr_hostname[MAX_HOSTNAME_LENGTH + 2];

                 (void)strcpy(tr_hostname, fsa[p_con->fsa_pos].host_dsp_name);
                 (void)rec(transfer_log_fd, WARN_SIGN,
#if SIZEOF_PID_T == 4
                           "%-*s[%d]: Process stopped by signal %d for transfer job (%d). (%s %d)\n",
#else
                           "%-*s[%d]: Process stopped by signal %d for transfer job (%lld). (%s %d)\n",
#endif
                           MAX_HOSTNAME_LENGTH, tr_hostname,
                           p_con->job_no, WSTOPSIG(status),
                           (pri_pid_t)p_con->pid, __FILE__, __LINE__);
              }

         remove_connection(p_con, faulty, now);

         /*
          * Even if we did fail to send a file, lets set the transfer
          * time. Otherwise jobs will get deleted to early together
          * with their current files if no transfer was successful
          * and we did a reread DIR_CONFIG.
          */
         if (qb[*qb_pos].msg_name[0] != '\0')
         {
            mdb[qb[*qb_pos].pos].last_transfer_time = now;
         }
      } /* if (waitpid(p_con->pid, &status, 0) == p_con->pid) */
      else
      {
         if (ret == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "waitpid() error [%d] : %s",
#else
                       "waitpid() error [%lld] : %s",
#endif
                       (pri_pid_t)p_con->pid, strerror(errno));
            if (errno == ECHILD)
            {
               faulty = NONE;
               remove_connection(p_con, NONE, now);
            }
         }
         else
         {
            faulty = NEITHER;
         }
      }

      return(faulty);
   }

   return(NO);
}


/*++++++++++++++++++++++++++++ qb_pos_pid() +++++++++++++++++++++++++++++*/
static void
qb_pos_pid(pid_t pid, int *qb_pos)
{
   register int i;

   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid == pid)
      {
         *qb_pos = i;
         return;
      }
   }
   *qb_pos = -1;

   return;
}


/*++++++++++++++++++++++++++++ qb_pos_fsa() +++++++++++++++++++++++++++++*/
static void
qb_pos_fsa(int fsa_pos, int *qb_pos)
{
   register int i, j;

   *qb_pos = -1;
   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid == PENDING)
      {
         if (qb[i].msg_name[0] != '\0')
         {
            for (j = 0; j < *no_msg_cached; j++)
            {
               if ((fsa_pos == mdb[j].fsa_pos) && (qb[i].pos == j))
               {
                  *qb_pos = i;
                  return;
               }
            }
         }
         else
         {
            if (fsa_pos == fra[qb[i].pos].fsa_pos)
            {
               *qb_pos = i;
               return;
            }
         }
      }
   }
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "No message for %s in queue that is PENDING.",
              fsa[fsa_pos].host_dsp_name);

   return;
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
#else
   (void)sprintf(config_file, "%s%s%s",
#endif
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, __FILE__, __LINE__) != INCORRECT))
   {
#if MAX_RECIPIENT_LENGTH > (MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH)
      char value[MAX_RECIPIENT_LENGTH];
#else
      char value[MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH];
#endif

      if (get_definition(buffer, MAX_CONNECTIONS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_connections = atoi(value);
         if ((max_connections < 1) ||
             (max_connections > MAX_CONFIGURABLE_CONNECTIONS))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "It is only possible to configure a maximum of %d (specified are %d) for %s in AFD_CONFIG. Setting to default: %d",
                       MAX_CONFIGURABLE_CONNECTIONS, max_connections,
                       MAX_CONNECTIONS_DEF, MAX_DEFAULT_CONNECTIONS);
            max_connections = MAX_DEFAULT_CONNECTIONS;
         }
      }
      if (get_definition(buffer, REMOTE_FILE_CHECK_INTERVAL_DEF,
                         str_remote_file_check_interval,
                         MAX_INT_LENGTH) != NULL)
      {
         remote_file_check_interval = atoi(str_remote_file_check_interval);
         if (remote_file_check_interval < 1)
         {
            remote_file_check_interval = DEFAULT_REMOTE_FILE_CHECK_INTERVAL;
#ifdef HAVE_SNPRINTF
            (void)snprintf(str_remote_file_check_interval, MAX_INT_LENGTH, "%d",
#else
            (void)sprintf(str_remote_file_check_interval, "%d",
#endif
                          remote_file_check_interval);
         }
      }
      else
      {
#ifdef HAVE_SNPRINTF
          (void)snprintf(str_remote_file_check_interval, MAX_INT_LENGTH, "%d",
#else
          (void)sprintf(str_remote_file_check_interval, "%d",
#endif
                        remote_file_check_interval);
      }
#ifdef _OUTPUT_LOG
      if (get_definition(buffer, MAX_OUTPUT_LOG_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_output_log_files = atoi(value);
         if ((max_output_log_files < 1) || (max_output_log_files > 599))
         {
            max_output_log_files = MAX_OUTPUT_LOG_FILES;
         }
      }
#endif
      if (get_definition(buffer, DEFAULT_AGE_LIMIT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_age_limit = (unsigned int)atoi(value);
      }
#ifdef HAVE_SNPRINTF
      (void)snprintf(str_age_limit, MAX_INT_LENGTH, "%u", default_age_limit);
#else
      (void)sprintf(str_age_limit, "%u", default_age_limit);
#endif
      if (get_definition(buffer, CREATE_TARGET_DIR_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (((value[0] == 'y') || (value[0] == 'Y')) &&
             ((value[1] == 'e') || (value[1] == 'E')) &&
             ((value[2] == 's') || (value[2] == 'S')) &&
             ((value[3] == '\0') || (value[3] == ' ') || (value[3] == '\t')))
         {
            *(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) |= ENABLE_CREATE_TARGET_DIR;
         }
         else
         {
            if (((value[0] == 'n') || (value[0] == 'N')) &&
                ((value[1] == 'o') || (value[1] == 'O')) &&
                ((value[2] == '\0') || (value[2] == ' ') || (value[2] == '\t')))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Only YES or NO (and not `%s') are possible for %s in AFD_CONFIG. Setting to default: NO",
                          value, CREATE_TARGET_DIR_DEF);
            }
            if (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & ENABLE_CREATE_TARGET_DIR)
            {
               *(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) ^= ENABLE_CREATE_TARGET_DIR;
            }
         }
      }
      else
      {
         if (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & ENABLE_CREATE_TARGET_DIR)
         {
            *(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) ^= ENABLE_CREATE_TARGET_DIR;
         }
      }
      if (get_definition(buffer, DEFAULT_HTTP_PROXY_DEF,
                         value, MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH) != NULL)
      {
         (void)strcpy(default_http_proxy, value);
      }
      else
      {
          default_http_proxy[0] = '\0';
      }
      if (get_definition(buffer, DEFAULT_SMTP_SERVER_DEF,
                         value, MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH) != NULL)
      {
         (void)strcpy(default_smtp_server, value);
      }
      else
      {
          default_smtp_server[0] = '\0';
      }
      if (get_definition(buffer, DEFAULT_SMTP_FROM_DEF,
                         value, MAX_RECIPIENT_LENGTH) != NULL)
      {
         size_t length;

         length = strlen(value) + 1;
         if ((default_smtp_from = malloc(length)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       length, strerror(errno));
            exit(INCORRECT);
         }
         (void)strcpy(default_smtp_from, value);
      }
      else
      {
          default_smtp_from = NULL;
      }
      if (get_definition(buffer, DEFAULT_SMTP_REPLY_TO_DEF,
                         value, MAX_RECIPIENT_LENGTH) != NULL)
      {
         size_t length;

         length = strlen(value) + 1;
         if ((default_smtp_reply_to = malloc(length)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       length, strerror(errno));
            exit(INCORRECT);
         }
         (void)strcpy(default_smtp_reply_to, value);
      }
      else
      {
          default_smtp_reply_to = NULL;
      }
      if (get_definition(buffer, DELETE_STALE_ERROR_JOBS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if ((value[0] == 'Y')  && (value[1] == 'E')  &&
             (value[2] == 'S')  && (value[3] == '\0'))
         {
            remove_error_jobs_not_in_queue = YES;
         }
      }
#ifdef HAVE_SETPRIORITY
      if (get_definition(buffer, FD_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         current_priority = atoi(value);
         if (setpriority(PRIO_PROCESS, 0, current_priority) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to set priority to %d : %s",
                       current_priority, strerror(errno));
            errno = 0;
            if (((current_priority = getpriority(PRIO_PROCESS, 0)) == -1) &&
                (errno != 0))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to getpriority() : %s", strerror(errno));
               current_priority = 0;
            }
         }
      }
      else
      {
         errno = 0;
         if (((current_priority = getpriority(PRIO_PROCESS, 0)) == -1) &&
             (errno != 0))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to getpriority() : %s", strerror(errno));
            current_priority = 0;
         }
      }
      if (euid == 0) /* Only root can increase priority! */
      {
         if (get_definition(buffer, ADD_AFD_PRIORITY_DEF,
                            value, MAX_INT_LENGTH) != NULL)
         {
            if (((value[0] == 'n') || (value[0] == 'N')) &&
                ((value[1] == 'o') || (value[1] == 'O')) &&
                ((value[2] == '\0') || (value[2] == ' ') || (value[2] == '\t')))
            {
               add_afd_priority = NO;
            }
            else if (((value[0] == 'y') || (value[0] == 'Y')) &&
                     ((value[1] == 'e') || (value[1] == 'E')) &&
                     ((value[2] == 's') || (value[2] == 'S')) &&
                     ((value[3] == '\0') || (value[3] == ' ') || (value[3] == '\t')))
                 {
                    add_afd_priority = YES;
                 }
                 else
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Only YES or NO (and not `%s') are possible for %s in AFD_CONFIG. Setting to default: %s",
                               value, ADD_AFD_PRIORITY_DEF,
                               (add_afd_priority == YES) ? "YES" : "NO");
                 }
         }
         if (get_definition(buffer, MAX_NICE_VALUE_DEF,
                            value, MAX_INT_LENGTH) != NULL)
         {
            max_sched_priority = atoi(value);
         }
         if (get_definition(buffer, MIN_NICE_VALUE_DEF,
                            value, MAX_INT_LENGTH) != NULL)
         {
            min_sched_priority = atoi(value);
         }
      }
      else
      {
         add_afd_priority = NO;
      }
#endif
      free(buffer);
   }
   else
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(str_remote_file_check_interval, MAX_INT_LENGTH, "%d",
#else
      (void)sprintf(str_remote_file_check_interval, "%d",
#endif
                    remote_file_check_interval);
      if (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & ENABLE_CREATE_TARGET_DIR)
      {
         *(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) ^= ENABLE_CREATE_TARGET_DIR;
      }
   }

   return;
}


/*+++++++++++++++++++++ get_local_interface_names() +++++++++++++++++++++*/
static void
get_local_interface_names(void)
{
   char          interface_file[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   static time_t interface_file_time = 0;

#ifdef HAVE_SNPRINTF
   (void)snprintf(interface_file, MAX_PATH_LENGTH, "%s%s%s",
#else
   (void)sprintf(interface_file, "%s%s%s",
#endif
                 p_work_dir, ETC_DIR, AFD_LOCAL_INTERFACE_FILE);
   if ((stat(interface_file, &stat_buf) == -1) && (errno != ENOENT))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to stat() `%s' : %s", interface_file, strerror(errno));
   }
   else
   {
      if ((stat_buf.st_mtime > interface_file_time) && (stat_buf.st_size > 0))
      {
         char *buffer = NULL;

         if ((eaccess(interface_file, F_OK) == 0) &&
             (read_file_no_cr(interface_file, &buffer, __FILE__, __LINE__) != INCORRECT))
         {
            int  i;
            char *ptr,
                 *p_end;

            if (local_interface_names != NULL)
            {
               FREE_RT_ARRAY(local_interface_names);
               local_interface_names = NULL;
            }
            no_of_local_interfaces = 0;
            interface_file_time = stat_buf.st_mtime;
            ptr = buffer;
            p_end = buffer + stat_buf.st_size;
            while (ptr < p_end)
            {
               if (*ptr == '#')
               {
                  while ((*ptr != '\n') && (ptr < p_end))
                  {
                     ptr++;
                  }
                  if (*ptr == '\n')
                  {
                     ptr++;
                  }
               }
               else
               {
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     ptr++;
                  }
                  i = 0;
                  while ((*(ptr + i) != '\n') && (i < HOST_NAME_MAX) &&
                         ((ptr + i) < p_end))
                  {
                     i++;
                  }
                  if (i > 0)
                  {
                     if (*(ptr + i) == '\n')
                     {
                        if (no_of_local_interfaces == 0)
                        {
                           RT_ARRAY(local_interface_names, 1,
                                    (HOST_NAME_MAX + 1), char);
                        }
                        else
                        {
                           REALLOC_RT_ARRAY(local_interface_names,
                                            (no_of_local_interfaces + 1),
                                            (HOST_NAME_MAX + 1), char);
                        }
                        i = 0;
                        while ((*(ptr + i) != '\n') && (i < HOST_NAME_MAX) &&
                               ((ptr + i) < p_end))
                        {
                           local_interface_names[no_of_local_interfaces][i] = *(ptr + i);
                           i++;
                        }
                        local_interface_names[no_of_local_interfaces][i] = '\0';
                        ptr += i + 1;
                        no_of_local_interfaces++;
                     }
                     else
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Interface name to long in %s.",
                                   interface_file);
                        ptr += i;
                        while ((*ptr != '\n') && (ptr < p_end))
                        {
                           ptr++;
                        }
                        if (*ptr == '\n')
                        {
                           ptr++;
                        }
                     }
                  }
                  else
                  {
                     if (*ptr == '\n')
                     {
                        ptr++;
                     }
                  }
               }
            }
            free(buffer);
         }
      }
   }

   if (no_of_local_interfaces == 0)
   {
      if (local_interface_names != NULL)
      {
         FREE_RT_ARRAY(local_interface_names);
      }
      RT_ARRAY(local_interface_names, 1, (HOST_NAME_MAX + 1), char);
      (void)gethostname(local_interface_names[0], HOST_NAME_MAX);
      no_of_local_interfaces = 1;
   }

   return;
}


/*++++++++++++++++++++ check_local_interface_names() ++++++++++++++++++++*/
static int
check_local_interface_names(char *hostname)
{
   int i;

   for (i = 0; i < no_of_local_interfaces; i++)
   {
      if (CHECK_STRCMP(hostname, local_interface_names[i]) == 0)
      {
         return(YES);
      }
   }

   return(NO);
}


/*++++++++++++++++++++++++++ get_free_connection() ++++++++++++++++++++++*/
static int
get_free_connection(void)
{
   register int i;

   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].hostname[0] == '\0')
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*+++++++++++++++++++++++++ get_free_disp_pos() +++++++++++++++++++++++++*/
static int
get_free_disp_pos(int pos)
{
   register int i;

   for (i = 0; i < fsa[pos].allowed_transfers; i++)
   {
      if (fsa[pos].job_status[i].proc_id == -1)
      {
         return(i);
      }
   }

   /*
    * This should be impossible. But it could happen that when
    * we reduce the 'active_transfers' the next process is started
    * before we get a change to reset the 'connect_status' variable.
    */
   if ((pos >= 0) && (pos < no_of_hosts))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmm. No display position free for %s [%d].",
                 fsa[pos].host_dsp_name, pos);
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmm. No display position free for FSA position %d.", pos);
   }

   /*
    * This is a good opertunity to check if the process for this
    * host still exist. If not lets simply reset all relevant
    * parameters of the job_status structure.
    */
   for (i = 0; i < fsa[pos].allowed_transfers; i++)
   {
      /*
       * We check greater 0 because doing a kill on pid 0 sends
       * the signal to ALL process in the process group of this
       * process.
       */
      if (fsa[pos].job_status[i].proc_id > 0)
      {
         if (kill(fsa[pos].job_status[i].proc_id, 0) == -1)
         {
            fsa[pos].job_status[i].proc_id = -1;
#ifdef _WITH_BURST_2
            fsa[pos].job_status[i].unique_name[0] = '\0';
            fsa[pos].job_status[i].job_id = NO_ID;
#endif
         }
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++++ fd_exit() ++++++++++++++++++++++++++++++*/
static void
fd_exit(void)
{
   register int i, j;
   int          jobs_killed = 0,
                loop_counter;
   struct stat  stat_buf;

   if ((connection == NULL) || (qb == NULL) || (mdb == NULL))
   {
      /* We allready walked through this function. */
      return;
   }
   now = time(NULL);
   if (stop_flag == 0)
   {
      stop_flag = SAVE_STOP;
   }

   loop_counter = 0;
   do
   {
      /* Kill any job still active with a normal kill (2)! */
      for (i = 0; i < max_connections; i++)
      {
         if (connection[i].pid > 0)
         {
            if (kill(connection[i].pid, SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             "Failed to kill transfer job to %s (%d) : %s",
#else
                             "Failed to kill transfer job to %s (%lld) : %s",
#endif
                             connection[i].hostname,
                             (pri_pid_t)connection[i].pid, strerror(errno));
               }
            }
         }
      }
      (void)sleep(1);
      for (i = 0; i < max_connections; i++)
      {
         if (connection[i].pid > 0)
         {
            int qb_pos;

            qb_pos_pid(connection[i].pid, &qb_pos);
            if (qb_pos != -1)
            {
               int faulty;

               if (((faulty = zombie_check(&connection[i], now, &qb_pos,
                                           WNOHANG)) == YES) ||
                   (faulty == NONE))
               {
                  char        file_dir[MAX_PATH_LENGTH];
                  struct stat stat_buf;

#ifdef HAVE_SNPRINTF
                  (void)snprintf(file_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                  (void)sprintf(file_dir, "%s%s%s/%s",
#endif
                                p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                qb[qb_pos].msg_name);
                  if ((stat(file_dir, &stat_buf) == -1) && (errno == ENOENT))
                  {
                     /* Process was in disconnection phase, so we */
                     /* can remove the message from the queue.    */
                     remove_msg(qb_pos);
                  }
                  else
                  {
                     qb[qb_pos].pid = PENDING;
                     if (qb[qb_pos].msg_name[0] != '\0')
                     {
                        fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                     }
                     else
                     {
                        fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                     }
                  }
               }
               else if (faulty == NO)
                    {
                       remove_msg(qb_pos);
                    }
            }
         } /* if (connection[i].pid > 0) */
      }
      loop_counter++;
   } while ((p_afd_status->no_of_transfers > 0) && (loop_counter < 15));

   /* Kill any job still active with a kill -9! */
   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].pid > 0)
      {
         if (kill(connection[i].pid, SIGKILL) == -1)
         {
            if (errno != ESRCH)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "Failed to kill transfer job to %s (%d) : %s",
#else
                          "Failed to kill transfer job to %s (%lld) : %s",
#endif
                          connection[i].hostname,
                          (pri_pid_t)connection[i].pid, strerror(errno));
            }
         }
         else
         {
            int qb_pos;

            jobs_killed++;
            qb_pos_pid(connection[i].pid, &qb_pos);
            if (qb_pos != -1)
            {
               int faulty;

               if (((faulty = zombie_check(&connection[i], now,
                                          &qb_pos, 0)) == YES) ||
                   (faulty == NONE))
               {
                  char        file_dir[MAX_PATH_LENGTH];
                  struct stat stat_buf;

#ifdef HAVE_SNPRINTF
                  (void)snprintf(file_dir, MAX_PATH_LENGTH, "%s%s%s/%s",
#else
                  (void)sprintf(file_dir, "%s%s%s/%s",
#endif
                                p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                qb[qb_pos].msg_name);
                  if ((stat(file_dir, &stat_buf) == -1) && (errno == ENOENT))
                  {
                     /* Process was in disconnection phase, so we */
                     /* can remove the message from the queue.    */
                     remove_msg(qb_pos);
                  }
                  else
                  {
                     qb[qb_pos].pid = PENDING;
                  }
               }
               else if (faulty == NO)
                    {
                       remove_msg(qb_pos);
                    }
            }
         }
      }
   }
   if (jobs_killed > 0)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Have killed %d jobs the hard way!", jobs_killed);
   }

   /* Unmap message queue buffer. */
   if (fstat(qb_fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "fstat() error : %s", strerror(errno));
   }
   else
   {
      char *ptr = (char *)qb - AFD_WORD_OFFSET;

      if (msync(ptr, stat_buf.st_size, MS_SYNC) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "msync() error : %s", strerror(errno));
      }
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "munmap() error : %s", strerror(errno));
      }
      else
      {
         qb = NULL;
      }
   }
   if (close(qb_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /* Unmap message cache buffer. */
   if (fstat(mdb_fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "fstat() error : %s", strerror(errno));
   }
   else
   {
      char *ptr = (char *)mdb - AFD_WORD_OFFSET;

      if (msync(ptr, stat_buf.st_size, MS_SYNC) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "msync() error : %s", strerror(errno));
      }
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "munmap() error : %s", strerror(errno));
      }
      else
      {
         mdb = NULL;
      }
   }
   if (close(mdb_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /* Free all memory that we allocated. */
   free(connection);
   connection = NULL;

   /* Set number of transfers to zero. */
   p_afd_status->no_of_transfers = 0;
   for (i = 0; i < no_of_hosts; i++)
   {
      fsa[i].active_transfers = 0;
      fsa[i].trl_per_process = 0;
      fsa[i].mc_ctrl_per_process = 0;
      for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
      {
         fsa[i].job_status[j].no_of_files = 0;
         fsa[i].job_status[j].proc_id = -1;
         fsa[i].job_status[j].connect_status = DISCONNECT;
         fsa[i].job_status[j].file_name_in_use[0] = '\0';
         fsa[i].job_status[j].file_name_in_use[1] = 0;
      }
   }
   (void)fsa_detach(YES);
   (void)fra_detach();

   system_log(INFO_SIGN, NULL, 0, "Stopped %s.", FD);

   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   p_afd_status->fd = OFF;
   system_log(FATAL_SIGN,  __FILE__, __LINE__, "Aaarrrggh! Received SIGSEGV.");
   fd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   p_afd_status->fd = OFF;
   system_log(FATAL_SIGN,  __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   fd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
