/*
 *  afddefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Deutscher Wetterdienst (DWD),
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

#ifndef __afddefs_h
#define __afddefs_h

/* Indicators for start and end of module description for man pages */
#define DESCR__S_M1             /* Start for User Command Man Page. */
#define DESCR__E_M1             /* End for User Command Man Page.   */
#define DESCR__S_M3             /* Start for Subroutines Man Page.  */
#define DESCR__E_M3             /* End for Subroutines Man Page.    */

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "ports.h"
#endif
#include "afdsetup.h"
#if MAX_DIR_ALIAS_LENGTH < 10
# define MAX_DIR_ALIAS_LENGTH 10
#endif
#include <stdio.h>
#ifdef STDC_HEADERS
# include <string.h>
#else               
# ifndef HAVE_MEMCPY         
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
extern int  sys_nerr;

# define strerror(e) sys_errlist[((unsigned)(e) < sys_nerr) ? (e) : 0]
#endif

#if SIZEOF_OFF_T == 4
typedef unsigned long       u_off_t;
typedef long                pri_off_t;
#else
typedef unsigned long long  u_off_t;
typedef long long           pri_off_t;
#endif
#if SIZEOF_TIME_T == 4
typedef long                pri_time_t;
#else
typedef long long           pri_time_t;
#endif
#if SIZEOF_INO_T == 4
typedef long                pri_ino_t;
#else
typedef long long           pri_ino_t;
#endif
#if SIZEOF_PID_T == 4
typedef int                 pri_pid_t;
#else
typedef long long           pri_pid_t;
#endif
#if SIZEOF_NLINK_T > 4
typedef long long           pri_nlink_t;
#else
typedef int                 pri_nlink_t;
#endif
#if SIZEOF_SIZE_T == 4
typedef int                 pri_size_t;
#else
typedef long long           pri_size_t;
#endif
#if SIZEOF_SSIZE_T == 4
typedef int                 pri_ssize_t;
#else
typedef long long           pri_ssize_t;
#endif
#ifdef HAVE_LONG_LONG
typedef unsigned long long  u_long_64;
#else
typedef unsigned long       u_long_64;
#endif

/* Define the language to use */
/* #define GERMAN */
#define ENGLISH

#ifdef _LINK_MAX_TEST
# define LINKY_MAX                 4
#endif

#ifdef _CYGWIN
# ifndef LINK_MAX
#  define LINK_MAX                 1000
# endif
#endif
#ifdef _SCO
# ifndef LINK_MAX
#  define LINK_MAX                 1000 /* SCO does not define it. */
# endif
#endif /* _SCO */
#ifdef LINUX
# define REDUCED_LINK_MAX          8192 /* Reduced for testing.    */
#endif /* LINUX */

#ifndef HAVE_MMAP
/* Dummy definitions for systems that do not have mmap() */
#define PROT_READ                  1
#define PROT_WRITE                 1
#define MAP_SHARED                 1
#endif

/* Define the names of the program's */
#ifndef HAVE_MMAP
# define MAPPER                    "mapper"
#endif
#define AFD                        "init_afd"
#define AMG                        "amg"
#define FD                         "fd"
#define SEND_FILE_FTP              "sf_ftp"
#define SEND_FILE_FTP_TRACE        "sf_ftp_trace"
#define GET_FILE_FTP               "gf_ftp"
#define GET_FILE_FTP_TRACE         "gf_ftp_trace"
#define SEND_FILE_SMTP             "sf_smtp"
#define SEND_FILE_SMTP_TRACE       "sf_smtp_trace"
#define GET_FILE_SMTP              "gf_smtp"
#define SEND_FILE_HTTP             "sf_http"
#define SEND_FILE_HTTP_TRACE       "sf_http_trace"
#define GET_FILE_HTTP              "gf_http"
#define GET_FILE_HTTP_TRACE        "gf_http_trace"
#define SEND_FILE_LOC              "sf_loc"
#ifdef _WITH_SCP_SUPPORT
# define SEND_FILE_SCP             "sf_scp"
# define SEND_FILE_SCP_TRACE       "sf_scp_trace"
# define GET_FILE_SCP              "gf_scp"
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
# define SEND_FILE_WMO             "sf_wmo"
# define SEND_FILE_WMO_TRACE       "sf_wmo_trace"
#endif
#ifdef _WITH_MAP_SUPPORT
# define SEND_FILE_MAP             "sf_map"
#endif
#define SEND_FILE_SFTP             "sf_sftp"
#define SEND_FILE_SFTP_TRACE       "sf_sftp_trace"
#define GET_FILE_SFTP              "gf_sftp"
#define GET_FILE_SFTP_TRACE        "gf_sftp_trace"
#define SLOG                       "system_log"
#define RLOG                       "receive_log"
#define TLOG                       "transfer_log"
#define TDBLOG                     "trans_db_log"
#define MON_SYS_LOG                "mon_sys_log"
#define MONITOR_LOG                "monitor_log"
#define SHOW_ILOG                  "show_ilog"
#define SHOW_OLOG                  "show_olog"
#define SHOW_RLOG                  "show_dlog"
#define SHOW_QUEUE                 "show_queue"
#define SHOW_TRANS                 "show_trans"
#define XSEND_FILE                 "xsend_file"
#ifdef _INPUT_LOG
# define INPUT_LOG_PROCESS         "input_log"
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_LOG_PROCESS        "output_log"
#endif
#ifdef _DELETE_LOG
# define DELETE_LOG_PROCESS        "delete_log"
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_LOG_PROCESS    "production_log"
#endif
#define ARCHIVE_WATCH              "archive_watch"
#define SHOW_LOG                   "show_log"
#define SHOW_CMD                   "show_cmd"
#define AFD_STAT                   "afd_stat"
#define AFD_INFO                   "afd_info"
#define EDIT_HC                    "edit_hc"
#define AFD_LOAD                   "afd_load"
#define AFD_CTRL                   "afd_ctrl"
#define AFDD                       "afdd"
#ifdef _WITH_SERVER_SUPPORT
# define WMOD                      "wmod"
#endif
#define AFD_MON                    "afd_mon"
#define MON_PROC                   "mon"
#define LOG_MON                    "log_mon"
#define MON_CTRL                   "mon_ctrl"
#define MON_INFO                   "mon_info"
#define AFD_CMD                    "afdcmd"
#define VIEW_DC                    "view_dc"
#define GET_DC_DATA                "get_dc_data"
#define DIR_CTRL                   "dir_ctrl"
#define DIR_INFO                   "dir_info"
#define DIR_CHECK                  "dir_check"
#define MAX_PROCNAME_LENGTH        14
#define AFTP                       "aftp"
#define ASMTP                      "asmtp"
#ifdef WITH_AUTO_CONFIG
# define AFD_AUTO_CONFIG           "afd_auto_config"
#endif
#define AFD_USER_NAME              "afd"

#ifdef _DELETE_LOG
/* Reasons for deleting files. */
# define AGE_OUTPUT                0
# define AGE_INPUT                 1
# define USER_DEL                  2
# define EXEC_FAILED_DEL           3
# define OTHER_OUTPUT_DEL          4
# ifdef WITH_DUP_CHECK
#  define DUP_INPUT                5
#  define DUP_OUTPUT               6
# endif
# define DEL_UNKNOWN_FILE          7
# define OTHER_INPUT_DEL           8
#endif

#ifdef _WITH_AFW2WMO
# define WMO_MESSAGE               2
#endif /* _WITH_AFW2WMO */

/* Exit status of afd program that starts the AFD. */
#define AFD_IS_ACTIVE              5
#define AFD_IS_NOT_ACTIVE          10
#define NO_DIR_CONFIG              -2

/* Definitions of lock ID's */
#define EDIT_HC_LOCK_ID            0    /* Edit host configuration      */
#define EDIT_DC_LOCK_ID            1    /* Edit directory configuration */
#define AMG_LOCK_ID                2    /* AMG                          */
#define FD_LOCK_ID                 3    /* FD                           */
#define AW_LOCK_ID                 4    /* Archive watch                */
#define AS_LOCK_ID                 5    /* AFD statistics               */
#define AFDD_LOCK_ID               6    /* AFD TCP Daemon               */
#define NO_OF_LOCK_PROC            7

/* Definitions for options needed both for AMG and FD. */
#ifdef _WITH_TRANS_EXEC
# define TRANS_EXEC_ID             "pexec"
# define TRANS_EXEC_ID_LENGTH      (sizeof(TRANS_EXEC_ID) - 1)
#endif

/* Commands that can be send to DB_UPDATE_FIFO of the AMG */
#define HOST_CONFIG_UPDATE         4
#define DIR_CONFIG_UPDATE          5
#define REREAD_HOST_CONFIG         6
#define REREAD_DIR_CONFIG          7

#define WORK_DIR_ID                "-w"

#ifdef FTX
# define WAIT_LOOPS 600 /* 60 seconds */
#else
# define WAIT_LOOPS 300 /* 30 seconds */
#endif

/* Definitions when AFD file directory is running full. */
#define STOP_AMG_THRESHOLD         20
#define START_AMG_THRESHOLD        100

/* Bit map flag to to enable/disable certain features in the AFD (FSA). */
#define DISABLE_RETRIEVE           1
#define DISABLE_ARCHIVE            2
#define ENABLE_CREATE_TARGET_DIR   4

/* Bit map flag to to enable/disable certain features in the AFD (FRA). */
#define DISABLE_DIR_WARN_TIME      1

/* The number of directories that are always in the AFD file directory: */
/*         ".", "..", "outgoing", "pool", "time", "incoming"            */
#define DIRS_IN_FILE_DIR           6

#define HOST_DISABLED              32
#define HOST_IN_DIR_CONFIG         64  /* Host in DIR_CONFIG file (bit 7)*/

/* Process numbers that are started by AFD */
#define AMG_NO                     0
#define FD_NO                      1
#define SLOG_NO                    2
#define RLOG_NO                    3
#define TLOG_NO                    4
#define TDBLOG_NO                  5
#define AW_NO                      6
#define STAT_NO                    7
#define DC_NO                      8
#define AFDD_NO                    9
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define PL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define DL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define DL_NO                     11
# define PL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define PL_NO                     10
# define NO_OF_PROCESS             11
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define DL_NO                     10
# define NO_OF_PROCESS             11
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define DL_NO                     10
# define PL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define NO_OF_PROCESS             11
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define PL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define DL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define DL_NO                     11
# define PL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define IL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define IL_NO                     11
# define PL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define IL_NO                     11
# define DL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define IL_NO                     11
# define DL_NO                     12
# define PL_NO                     13
# define NO_OF_PROCESS             14
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define OL_NO                     10
# define NO_OF_PROCESS             11
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define OL_NO                     10
# define PL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define OL_NO                     10
# define DL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define OL_NO                     10
# define DL_NO                     11
# define PL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define OL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define OL_NO                     11
# define PL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define OL_NO                     11
# define DL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define OL_NO                     11
# define DL_NO                     12
# define PL_NO                     13
# define NO_OF_PROCESS             14
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define OL_NO                     11
# define NO_OF_PROCESS             12
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define OL_NO                     11
# define PL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define OL_NO                     11
# define DL_NO                     12
# define NO_OF_PROCESS             13
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define IL_NO                     10
# define OL_NO                     11
# define DL_NO                     12
# define PL_NO                     13
# define NO_OF_PROCESS             14
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define IL_NO                     11
# define OL_NO                     12
# define DL_NO                     13
# define PL_NO                     14
# define NO_OF_PROCESS             15
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && defined (HAVE_MMAP)
# define NO_OF_PROCESS             10
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_PRODUCTION_LOG) && !defined (HAVE_MMAP)
# define MAPPER_NO                 10
# define NO_OF_PROCESS             11
#endif
#define SHOW_OLOG_NO               30

#define NA                         -1
#define NO                         0
#define YES                        1
#define NEITHER                    2
#define BOTH                       3
#define INCORRECT                  -1
#define SUCCESS                    0
#define STALE                      -1
#define CON_RESET                  2
#define ON                         1
#define OFF                        0
#define ALL                        0
#define ONE                        1
#define PAUSED                     2
#define PAUSED_REMOTE              2
#define DONE                       3
#define NORMAL                     4
#define NONE                       5
#define NO_ACCESS                  10
#define STAT_ERROR                 17
#define CREATED_DIR                20
#define MKDIR_ERROR                26
#define CHOWN_ERROR                27
#define ALLOC_ERROR                34
#define LOCK_IS_SET                -2
#define LOCKFILE_NOT_THERE         -3
#define LOCK_IS_NOT_SET            11
#define AUTO_SIZE_DETECT           -2
#define FILE_IS_DIR                -2      /* Used by remove_dir().      */
#define GET_ONCE_ONLY              2

#define NO_PRIORITY                -1         /* So it knows it does not */
                                              /* need to create the name */
                                              /* with priority, which is */
                                              /* used by the function    */
                                              /* check_files().          */
#define INCORRECT_VERSION          -2
#define EQUAL_SIGN                 1
#define LESS_THEN_SIGN             2
#define GREATER_THEN_SIGN          3

/* Size definitions. */
#define KILOFILE                   1000
#define MEGAFILE                   1000000
#define GIGAFILE                   1000000000
#define TERAFILE                   1000000000000LL
#define PETAFILE                   1000000000000000LL
#define EXAFILE                    1000000000000000000LL
#define KILOBYTE                   1024
#define MEGABYTE                   1048576
#define GIGABYTE                   1073741824
#define TERABYTE                   1099511627776LL
#define PETABYTE                   1125899906842624LL
#define EXABYTE                    1152921504606846976LL
#define F_KILOBYTE                 1024.0
#define F_MEGABYTE                 1048576.0
#define F_GIGABYTE                 1073741824.0
#define F_TERABYTE                 1099511627776.0
#define F_PETABYTE                 1125899906842624.0
#define F_EXABYTE                  1152921504606846976.0
/* Next comes ZETTABYTE and YOTTABYTE, but these are to large for 2^64, */
/* ie. for 64-bit systems. Wonder what will be the size of the next     */
/* systems.                                                             */

/* Definitions for ignore options in struct fileretrieve_status. */
#define ISIZE_EQUAL                1
#define ISIZE_LESS_THEN            2
#define ISIZE_GREATER_THEN         4
#define ISIZE_OFF_MASK             7
#define IFTIME_EQUAL               8
#define IFTIME_LESS_THEN           16
#define IFTIME_GREATER_THEN        32
#define IFTIME_OFF_MASK            56

#define INFO_SIGN                  "<I>"
#define CONFIG_SIGN                "<C>"   /* If changed see:            */
                                           /*           config_log.c     */
                                           /*           mconfig_log.c    */
#define WARN_SIGN                  "<W>"
#define ERROR_SIGN                 "<E>"
#define FATAL_SIGN                 "<F>"   /* donated by Paul Merken.    */
#define DEBUG_SIGN                 "<D>"
#define TRACE_SIGN                 "<T>"
#define DUMMY_SIGN                 "<#>"   /* To always show some general*/
                                           /* information, eg month.     */
#define SEPARATOR                  "-->"

/* Separator to separate elements in log files. */
#define SEPARATOR_CHAR             '|'

/* Definitions of different exit status */
#define NOT_RUNNING                -1
#define UNKNOWN_STATE              -2
#define STOPPED                    -3
#define DIED                       -4

/* Definitions for different addresses for one host */
#define HOST_ONE                   1
#define HOST_TWO                   2
#define DEFAULT_TOGGLE_HOST        HOST_ONE
#define HOST_TWO_FLAG              64 /* For 'Host status' in HOST_CONFIG. */
#define AUTO_TOGGLE_OPEN           '{'
#define AUTO_TOGGLE_CLOSE          '}'
#define STATIC_TOGGLE_OPEN         '['
#define STATIC_TOGGLE_CLOSE        ']'

/* Definitions of the protocol's and extensions */
#define FTP                        0
#define FTP_FLAG                   1
#define LOC                        1
#define LOC_FLAG                   2
#define LOCAL_ID                   "local"
#define SMTP                       2
#define SMTP_FLAG                  4
#ifdef _WITH_MAP_SUPPORT
# define MAP                       3
# define MAP_FLAG                  8
#endif
#ifdef _WITH_SCP_SUPPORT
# define SCP                       4
# define SCP_FLAG                  16
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
# define WMO                       5
# define WMO_FLAG                  32
#endif
#define HTTP                       6
#define HTTP_FLAG                  64
#ifdef WITH_SSL
# define SSL_FLAG                  536870912
# define FTPS                      7
# define HTTPS                     8
# define SMTPS                     9
#endif
#define SFTP                       10
#define SFTP_FLAG                  128
#define GET_FTP_FLAG               32768
#define GET_HTTP_FLAG              65536
#define GET_SFTP_FLAG              131072
#define SEND_FLAG                  1073741824
#define RETRIEVE_FLAG              2147483648U

/* Definitions for protocol_options in FSA. */
#define FTP_PASSIVE_MODE           1
#define SET_IDLE_TIME              2
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE            4
#endif
#define FTP_FAST_MOVE              8
#define FTP_FAST_CD                16
#define FTP_IGNORE_BIN             32
#define FTP_EXTENDED_MODE          64
#ifdef _WITH_BURST_2
# define DISABLE_BURSTING          128
#endif
#define FTP_ALLOW_DATA_REDIRECT    256

#define FTP_SHEME                  "ftp"
#define FTP_SHEME_LENGTH           (sizeof(FTP_SHEME) - 1)
#ifdef WITH_SSL
# define FTPS_SHEME                "ftps"
# define FTPS_SHEME_LENGTH         (sizeof(FTPS_SHEME) - 1)
#endif
#define LOC_SHEME                  "file"
#define LOC_SHEME_LENGTH           (sizeof(LOC_SHEME) - 1)
#ifdef _WITH_SCP_SUPPORT
# define SCP_SHEME                 "scp"
# define SCP_SHEME_LENGTH          (sizeof(SCP_SHEME) - 1)
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
# define WMO_SHEME                 "wmo"
# define WMO_SHEME_LENGTH          (sizeof(WMO_SHEME) - 1)
#endif
#ifdef _WITH_MAP_SUPPORT
# define MAP_SHEME                 "map"
# define MAP_SHEME_LENGTH          (sizeof(MAP_SHEME) - 1)
#endif
#define SMTP_SHEME                 "mailto"
#define SMTP_SHEME_LENGTH          (sizeof(SMTP_SHEME) - 1)
#ifdef WITH_SSL
# define SMTPS_SHEME               "mailtos"
# define SMTPS_SHEME_LENGTH        (sizeof(SMTPS_SHEME) - 1)
#endif
#define HTTP_SHEME                 "http"
#define HTTP_SHEME_LENGTH          (sizeof(HTTP_SHEME) - 1)
#ifdef WITH_SSL
# define HTTPS_SHEME               "https"
# define HTTPS_SHEME_LENGTH        (sizeof(HTTPS_SHEME) - 1)
#endif
#define SFTP_SHEME                 "sftp"
#define SFTP_SHEME_LENGTH          (sizeof(SFTP_SHEME) - 1)


/* Definitions for [dir options] */
#define DEL_UNKNOWN_FILES_ID             "delete unknown files"
#define DEL_UNKNOWN_FILES_ID_LENGTH      (sizeof(DEL_UNKNOWN_FILES_ID) - 1)
#define DEL_QUEUED_FILES_ID              "delete queued files"
#define DEL_QUEUED_FILES_ID_LENGTH       (sizeof(DEL_QUEUED_FILES_ID) - 1)
#define DEL_OLD_LOCKED_FILES_ID          "delete old locked files"
#define DEL_OLD_LOCKED_FILES_ID_LENGTH   (sizeof(DEL_OLD_LOCKED_FILES_ID) - 1)
#ifdef WITH_INOTIFY
# define INOTIFY_FLAG_ID                 "inotify"
# define INOTIFY_FLAG_ID_LENGTH          (sizeof(INOTIFY_FLAG_ID) - 1)
#endif
#define OLD_FILE_TIME_ID                 "old file time"
#define OLD_FILE_TIME_ID_LENGTH          (sizeof(OLD_FILE_TIME_ID) - 1)
#define DONT_REP_UNKNOWN_FILES_ID        "do not report unknown files"
#define DONT_REP_UNKNOWN_FILES_ID_LENGTH (sizeof(DONT_REP_UNKNOWN_FILES_ID) - 1)
#define END_CHARACTER_ID                 "end character"
#define END_CHARACTER_ID_LENGTH          (sizeof(END_CHARACTER_ID) - 1)
#define TIME_ID                          "time"
#define TIME_ID_LENGTH                   (sizeof(TIME_ID) - 1)
#define MAX_PROCESS_ID                   "max process"
#define MAX_PROCESS_ID_LENGTH            (sizeof(MAX_PROCESS_ID) - 1)
#define DO_NOT_REMOVE_ID                 "do not remove"
#define DO_NOT_REMOVE_ID_LENGTH          (sizeof(DO_NOT_REMOVE_ID) - 1)
#define STORE_RETRIEVE_LIST_ID           "store retrieve list"
#define STORE_RETRIEVE_LIST_ID_LENGTH    (sizeof(STORE_RETRIEVE_LIST_ID) - 1)
#define STORE_REMOTE_LIST                "store remote list"
#define STORE_REMOTE_LIST_LENGTH         (sizeof(STORE_REMOTE_LIST) - 1)
#define DONT_DEL_UNKNOWN_FILES_ID        "do not delete unknown files"
#define DONT_DEL_UNKNOWN_FILES_ID_LENGTH (sizeof(DONT_DEL_UNKNOWN_FILES_ID) - 1)
#define REP_UNKNOWN_FILES_ID             "report unknown files"
#define REP_UNKNOWN_FILES_ID_LENGTH      (sizeof(REP_UNKNOWN_FILES_ID) - 1)
#define FORCE_REREAD_ID                  "force reread"
#define FORCE_REREAD_ID_LENGTH           (sizeof(FORCE_REREAD_ID) - 1)
#define IMPORTANT_DIR_ID                 "important dir"
#define IMPORTANT_DIR_ID_LENGTH          (sizeof(IMPORTANT_DIR_ID) - 1)
#define IGNORE_SIZE_ID                   "ignore size"
#define IGNORE_SIZE_ID_LENGTH            (sizeof(IGNORE_SIZE_ID) - 1)
#define IGNORE_FILE_TIME_ID              "ignore file time"
#define IGNORE_FILE_TIME_ID_LENGTH       (sizeof(IGNORE_FILE_TIME_ID) - 1)
#define MAX_FILES_ID                     "max files"
#define MAX_FILES_ID_LENGTH              (sizeof(MAX_FILES_ID) - 1)
#define MAX_SIZE_ID                      "max size"
#define MAX_SIZE_ID_LENGTH               (sizeof(MAX_SIZE_ID) - 1)
#define WAIT_FOR_FILENAME_ID             "wait for"
#define WAIT_FOR_FILENAME_ID_LENGTH      (sizeof(WAIT_FOR_FILENAME_ID) - 1)
#define ACCUMULATE_ID                    "accumulate"
#define ACCUMULATE_ID_LENGTH             (sizeof(ACCUMULATE_ID) - 1)
#define ACCUMULATE_SIZE_ID               "accumulate size"
#define ACCUMULATE_SIZE_ID_LENGTH        (sizeof(ACCUMULATE_SIZE_ID) - 1)
#ifdef WITH_DUP_CHECK
# define DUPCHECK_ID                     "dupcheck"
# define DUPCHECK_ID_LENGTH              (sizeof(DUPCHECK_ID) - 1)
#endif
#define ACCEPT_DOT_FILES_ID              "accept dot files"
#define ACCEPT_DOT_FILES_ID_LENGTH       (sizeof(ACCEPT_DOT_FILES_ID) - 1)
#define DO_NOT_GET_DIR_LIST_ID           "do not get dir list"
#define DO_NOT_GET_DIR_LIST_ID_LENGTH    (sizeof(DO_NOT_GET_DIR_LIST_ID) - 1)
#define DIR_WARN_TIME_ID                 "warn time"
#define DIR_WARN_TIME_ID_LENGTH          (sizeof(DIR_WARN_TIME_ID) - 1)
#define KEEP_CONNECTED_ID                "keep connected"
#define KEEP_CONNECTED_ID_LENGTH         (sizeof(KEEP_CONNECTED_ID) - 1)
#define UNKNOWN_FILES                    1
#define QUEUED_FILES                     2
#define OLD_LOCKED_FILES                 4

/* Definitions for [options] */
#define AGE_LIMIT_ID                     "age-limit"
#define AGE_LIMIT_ID_LENGTH              (sizeof(AGE_LIMIT_ID) - 1)

/* Default definitions */
#define AFD_CONFIG_FILE                  "/AFD_CONFIG"
#define DEFAULT_DIR_CONFIG_FILE          "/DIR_CONFIG"
#define DEFAULT_HOST_CONFIG_FILE         "/HOST_CONFIG"
#define RENAME_RULE_FILE                 "/rename.rule"
#define AFD_USER_FILE                    "/afd.users"
#define GROUP_FILE                       "/group.list"
#define DEFAULT_FIFO_SIZE                4096
#define DEFAULT_BUFFER_SIZE              1024
#define DEFAULT_MAX_ERRORS               10
#define DEFAULT_SUCCESSFUL_RETRIES       10
#define DEFAULT_FILE_SIZE_OFFSET         -1
#define DEFAULT_TRANSFER_TIMEOUT         120L
#define DEFAULT_NO_OF_NO_BURSTS          0
#define DEFAULT_EXEC_TIMEOUT             0L
#ifdef WITH_DUP_CHECK
# define DEFAULT_DUPCHECK_TIMEOUT        3600L
#endif
#define DEFAULT_OLD_FILE_TIME            24      /* Hours.               */
#define DEFAULT_DIR_WARN_TIME            0L      /* Seconds (0 = unset)  */
#define DEFAULT_KEEP_CONNECTED_TIME      0       /* Seconds (0 = unset)  */
#define DEFAULT_CREATE_SOURCE_DIR_DEF    YES
#ifdef WITH_INOTIFY
# define DEFAULT_INOTIFY_FLAG            0
#endif

/* Definitions to be read from the AFD_CONFIG file. */
#define AFD_TCP_PORT_DEF                 "AFD_TCP_PORT"
#define AFD_TCP_LOGS_DEF                 "AFD_TCP_LOGS"
#define DEFAULT_PRINTER_CMD_DEF          "DEFAULT_PRINTER_CMD"
#define DEFAULT_PRINTER_NAME_DEF         "DEFAULT_PRINTER_NAME"
#define DEFAULT_AGE_LIMIT_DEF            "DEFAULT_AGE_LIMIT"
#define MAX_CONNECTIONS_DEF              "MAX_CONNECTIONS"
#define MAX_COPIED_FILES_DEF             "MAX_COPIED_FILES"
#define MAX_COPIED_FILE_SIZE_DEF         "MAX_COPIED_FILE_SIZE"
#define ONE_DIR_COPY_TIMEOUT_DEF         "ONE_DIR_COPY_TIMEOUT"
#define FULL_SCAN_TIMEOUT_DEF            "FULL_SCAN_TIMEOUT"
#define REMOTE_FILE_CHECK_INTERVAL_DEF   "REMOTE_FILE_CHECK_INTERVAL"
#ifdef WITH_INOTIFY
# define DEFAULT_INOTIFY_FLAG_DEF        "DEFAULT_INOTIFY_FLAG"
#endif
#ifndef _WITH_PTHREAD
# define DIR_CHECK_TIMEOUT_DEF           "DIR_CHECK_TIMEOUT"
#endif
#define TRUSTED_REMOTE_IP_DEF            "TRUSTED_REMOTE_IP"
#define PING_CMD_DEF                     "PING_CMD"
#define TRACEROUTE_CMD_DEF               "TRACEROUTE_CMD"
#define DIR_CONFIG_NAME_DEF              "DIR_CONFIG_NAME"
#define FAKE_USER_DEF                    "FAKE_USER"
#define CREATE_SOURCE_DIR_DEF            "CREATE_SOURCE_DIR"
#define CREATE_TARGET_DIR_DEF            "CREATE_TARGET_DIR"
#define EXEC_TIMEOUT_DEF                 "EXEC_TIMEOUT"
#define DEFAULT_OLD_FILE_TIME_DEF        "DEFAULT_OLD_FILE_TIME"
#define DEFAULT_DELETE_FILES_FLAG_DEF    "DEFAULT_DELETE_FILES_FLAG"
#define DEFAULT_SMTP_SERVER_DEF          "DEFAULT_SMTP_SERVER"
#define DEFAULT_SMTP_FROM_DEF            "DEFAULT_SMTP_FROM"
#define REMOVE_UNUSED_HOSTS_DEF          "REMOVE_UNUSED_HOSTS"
#define DELETE_STALE_ERROR_JOBS_DEF      "DELETE_STALE_ERROR_JOBS"
#define DEFAULT_DIR_WARN_TIME_DEF        "DEFAULT_DIR_WARN_TIME"

/* Heading identifiers for the DIR_CONFIG file and messages. */
#define DIR_IDENTIFIER                   "[directory]"
#define DIR_IDENTIFIER_LENGTH            (sizeof(DIR_IDENTIFIER) - 1)
#define DIR_OPTION_IDENTIFIER            "[dir options]"
#define DIR_OPTION_IDENTIFIER_LENGTH     (sizeof(DIR_OPTION_IDENTIFIER) - 1)
#define FILE_IDENTIFIER                  "[files]"
#define FILE_IDENTIFIER_LENGTH           (sizeof(FILE_IDENTIFIER) - 1)
#define DESTINATION_IDENTIFIER           "[destination]"
#define DESTINATION_IDENTIFIER_LENGTH    (sizeof(DESTINATION_IDENTIFIER) - 1)
#define RECIPIENT_IDENTIFIER             "[recipient]"
#define RECIPIENT_IDENTIFIER_LENGTH      (sizeof(RECIPIENT_IDENTIFIER) - 1)
#define OPTION_IDENTIFIER                "[options]"
#define OPTION_IDENTIFIER_LENGTH         (sizeof(OPTION_IDENTIFIER) - 1)

#define VIEW_DC_DIR_IDENTIFIER           "Directory     : "
#define VIEW_DC_DIR_IDENTIFIER_LENGTH    (sizeof(VIEW_DC_DIR_IDENTIFIER) - 1)

/* Definitions for AFDD Logs. */
/* NOTE: Bits 1 - 2 are defined in afd_mon/mondefs.h */
#define AFDD_SYSTEM_LOG                  16
#define AFDD_RECEIVE_LOG                 32
#define AFDD_TRANSFER_LOG                64
#define AFDD_TRANSFER_DEBUG_LOG          128
#define AFDD_INPUT_LOG                   256
#define AFDD_PRODUCTION_LOG              512
#define AFDD_OUTPUT_LOG                  1024
#define AFDD_DELETE_LOG                  2048
#define AFDD_JOB_DATA                    4096
#define AFDD_COMPRESSION_1               8192
/* NOTE: If new flags are added check afd_mon/mondefs.h first! */

/* Group identifier for mails. */
#define MAIL_GROUP_IDENTIFIER      '$'

/* Definitions of maximum values */
#define MAX_SHUTDOWN_TIME          60     /* When the AMG gets the order  */
                                          /* shutdown, this the time it   */
                                          /* waits for its children to    */
                                          /* return before they get       */
                                          /* eliminated.                  */
#define MAX_REAL_HOSTNAME_LENGTH   40     /* How long the real host name  */
                                          /* or its IP number may be.     */
#define MAX_PROXY_NAME_LENGTH      80     /* The maximum length of the    */
                                          /* remote proxy name.           */
#define MAX_ADD_FNL                35     /* Maximum additional file name */
                                          /* length:                      */
                                          /* <creation_time>_<unique_no>_<split_job_counter>_ */
                                          /* 16 + 1 + 8 + 1 + 8 + 1        */
#define MAX_MSG_NAME_LENGTH        (MAX_ADD_FNL + 19) /* Maximum length   */
                                          /* of message name.             */
                                          /* <job_id>/<counter>/<creation_time>_<unique_no>_<split_job_counter>_ */
                                          /* 8 + 1 + 8 + 1 + 16 + 1 + 8 + 1 + 8 + 1 + 1 */
#define MAX_INT_LENGTH             11     /* When storing integer values  */
                                          /* as string this is the no.    */
                                          /* characters needed to store   */
                                          /* the largest integer value.   */
#if SIZEOF_LONG == 4
# define MAX_LONG_LENGTH           11
#else
# define MAX_LONG_LENGTH           21
#endif
#define MAX_LONG_LONG_LENGTH       21
#if SIZEOF_OFF_T == 4
# define MAX_OFF_T_LENGTH          11
#else
# define MAX_OFF_T_LENGTH          20
#endif
#define MAX_TOGGLE_STR_LENGTH      5
#define MAX_USER_NAME_LENGTH       80     /* Maximum length of the user   */
                                          /* name and password.           */
#define MAX_FULL_USER_ID_LENGTH    80     /* Max length for user name and */
                                          /* display.                     */
#define MAX_COPIED_FILES           100    /* The maximum number of files  */
                                          /* that the AMG may collect     */
                                          /* before it creates a message  */
                                          /* for the FD.                  */
#define MAX_COPIED_FILE_SIZE       102400 /* Same as above only that this */
                                          /* limits the total size copied */
                                          /* in Kilobytes.                */
#define MAX_COPIED_FILE_SIZE_UNIT  1024   /* Unit in Kilobytes for above  */
                                          /* value.                       */
#define MAX_MSG_PER_SEC            9999   /* The maximum number of        */
                                          /* messages that may be         */
                                          /* generated in one second.     */
#define MAX_PRODUCTION_BUFFER_LENGTH 8192 /* Buffer size to hold new file-*/
                                          /* names after a rename, exec,  */
                                          /* etc.                         */
#define MAX_NO_PARALLEL_JOBS       5      /* Maximum number of parallel   */
                                          /* jobs per host alias.         */
#define MAX_FILENAME_LENGTH        256    /* Maximum length of a filename.*/
#define MAX_ERROR_STR_LENGTH       34     /* Maximum length of the FD     */
                                          /* error strings. (see fddefs.h)*/
#define MAX_IP_LENGTH              16     /* Maximum length of an IP      */
                                          /* number as string.            */

/* The length of message we send via fifo from AMG to FD. */
#define MAX_BIN_MSG_LENGTH (sizeof(time_t) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned short) + sizeof(unsigned short) + sizeof(char) + sizeof(char))

/* Miscellaneous definitions */
#define LOG_SIGN_POSITION          13     /* Position in log file where   */
                                          /* type of log entry can be     */
                                          /* determined (I, W, E, F, D).  */
#define LOG_FIFO_SIZE              5      /* The number of letters        */
                                          /* defining the type of log.    */
                                          /* These are displayed in the   */
                                          /* button line.                 */
#define ERROR_HISTORY_LENGTH       5      /* The last five error types    */
                                          /* during transmission.         */
                                          /* NOTE: This value MUST be at  */
                                          /*       least 2!               */
#define ARCHIVE_UNIT               86400  /* Seconds => 1 day             */
#define WD_ENV_NAME                "AFD_WORK_DIR"   /* The working dir-   */
                                                    /* ectory environment */

/* Different host status */
#define STOP_TRANSFER_STAT         1
#define PAUSE_QUEUE_STAT           2
#define AUTO_PAUSE_QUEUE_STAT      4
#define DANGER_PAUSE_QUEUE_STAT    8
/* NOTE: This is not used.         16 */
#define HOST_CONFIG_HOST_DISABLED  32
/* NOTE: HOST_TWO_FLAG             64 */
#ifdef WITH_ERROR_QUEUE
#define ERROR_QUEUE_SET            128
#endif

#define HOST_NOT_IN_DIR_CONFIG     4

/* Position of each colour in global array */
/*############################ LightBlue1 ###############################*/
#define DEFAULT_BG                 0  /* Background                      */
#define HTTP_ACTIVE                0
#define NORMAL_MODE                0
/*############################## White ##################################*/
#define WHITE                      1
#define DISCONNECT                 1  /* Successful completion of        */
                                      /* operation and disconnected.     */
#define DISABLED                   1
#define NO_INFORMATION             1
/*########################### lightskyblue2 #############################*/
#define CHAR_BACKGROUND            2  /* Background color for characters.*/
#define DISCONNECTED               2  /* AFD_MON not connected.          */
#define CLOSING_CONNECTION         2  /* Closing an active connection.   */
/*############################ SaddleBrown ##############################*/
#define PAUSE_QUEUE                3
#ifdef _WITH_SCP_SUPPORT
# define SCP_ACTIVE                3
#endif
/*############################## brown3 #################################*/
#define AUTO_PAUSE_QUEUE           4
#ifdef _WITH_SCP_SUPPORT
# define SCP_BURST_TRANSFER_ACTIVE 4
#endif
#define SFTP_RETRIEVE_ACTIVE       4
/*############################### Blue ##################################*/
#define CONNECTING                 5  /* Open connection to remote host, */
                                      /* sending user and password,      */
                                      /* setting transfer type and       */
                                      /* changing directory.             */
#define LOC_BURST_TRANSFER_ACTIVE  5
/*############################## gray37 #################################*/
#define LOCKED_INVERSE             6
#define HTTP_RETRIEVE_ACTIVE       6
/*############################### gold ##################################*/
#define TR_BAR                     7  /* Colour for transfer rate bar.   */
#define DEBUG_MODE                 7
#ifdef _WITH_WMO_SUPPORT
# define WMO_ACTIVE                7
#endif
/*########################### NavajoWhite1 ##############################*/
#define LABEL_BG                   8  /* Background for label.           */
#ifdef _WITH_MAP_SUPPORT
# define MAP_ACTIVE                8
#endif
#define SFTP_ACTIVE                8
/*############################ SteelBlue1 ###############################*/
#define BUTTON_BACKGROUND          9  /* Background for button line in   */
                                      /* afd_ctrl dialog.                */
#define LOC_ACTIVE                 9
/*############################### pink ##################################*/
#define EMAIL_ACTIVE               10
/*############################## green ##################################*/
#define FTP_BURST2_TRANSFER_ACTIVE 11
/*############################## green3 #################################*/
#define CONNECTION_ESTABLISHED     12 /* AFD_MON                         */
#define NORMAL_STATUS              12
#define INFO_ID                    12
#define FTP_RETRIEVE_ACTIVE        12 /* When gf_ftp retrieves files.    */
/*############################# SeaGreen ################################*/
#define CONFIG_ID                  13
#define TRANSFER_ACTIVE            13 /* Creating remote lockfile and    */
                                      /* transferring files.             */
#define FTP_ACTIVE                 13
#define DIRECTORY_ACTIVE           13
/*############################ DarkOrange ###############################*/
#define STOP_TRANSFER              14 /* Transfer to this host is        */
                                      /* stopped.                        */
#ifdef WITH_ERROR_QUEUE
# define JOBS_IN_ERROR_QUEUE       14
#endif
#define WARNING_ID                 14
#define TRACE_MODE                 14
#ifdef _WITH_TRANS_EXEC
# define POST_EXEC                 14
#endif
/*############################## tomato #################################*/
#define NOT_WORKING                15
/*################################ Red ##################################*/
#define NOT_WORKING2               16
#define FULL_TRACE_MODE            16
#define ERROR_ID                   16
#define CONNECTION_DEFUNCT         16 /* AFD_MON, connection not         */
                                      /* working.                        */
/*############################### Black #################################*/
#define BLACK                      17
#define FG                         17 /* Foreground                      */
#define FAULTY_ID                  17
/*########################## BlanchedAlmond #############################*/
#define SFTP_BURST_TRANSFER_ACTIVE 18
/*############################## yellow #################################*/
#ifdef _WITH_WMO_SUPPORT
#define WMO_BURST_TRANSFER_ACTIVE  19
# define COLOR_POOL_SIZE           20
#else
# define COLOR_POOL_SIZE           19
#endif

/* History types. */
#define RECEIVE_HISTORY            0
#define SYSTEM_HISTORY             1
#define TRANSFER_HISTORY           2
#define NO_OF_LOG_HISTORY          3

/* Directory definitions. */
#define AFD_MSG_DIR                "/messages"
#define AFD_FILE_DIR               "/files"
#define AFD_TMP_DIR                "/pool"
#define AFD_TIME_DIR               "/time"
#define AFD_ARCHIVE_DIR            "/archive"
#define FIFO_DIR                   "/fifodir"
#define LOG_DIR                    "/log"
#define RLOG_DIR                   "/rlog"  /* Only used for afd_mon. */
#define ETC_DIR                    "/etc"
#define ERROR_ACTION_DIR           "/error_action"
#define INCOMING_DIR               "/incoming"
#define OUTGOING_DIR               "/outgoing"
#define OUTGOING_DIR_LENGTH        (sizeof(OUTGOING_DIR) - 1)
#ifdef WITH_DUP_CHECK
# define STORE_DIR                 "/store"
# define CRC_DIR                   "/crc"
#endif
#define FILE_MASK_DIR              "/file_mask"
#define LS_DATA_DIR                "/ls_data"

/*-----------------------------------------------------------------------*/
/* If definitions are added or removed, update init_afd/afd.c!           */
/*-----------------------------------------------------------------------*/
/* Data file definitions. */
#define FSA_ID_FILE                "/fsa.id"
#define FSA_STAT_FILE              "/fsa_status"
#define FSA_STAT_FILE_ALL          "/fsa_status.*"
#define FRA_ID_FILE                "/fra.id"
#define FRA_STAT_FILE              "/fra_status"
#define FRA_STAT_FILE_ALL          "/fra_status.*"
#define STATUS_SHMID_FILE          "/afd.status"
#define BLOCK_FILE                 "/NO_AUTO_RESTART"
#define AMG_COUNTER_FILE           "/amg_counter"
#define COUNTER_FILE               "/any_counter"
#define MESSAGE_BUF_FILE           "/tmp_msg_buffer"
#define MSG_CACHE_FILE             "/fd_msg_cache"
#define MSG_QUEUE_FILE             "/fd_msg_queue"
#ifdef WITH_ERROR_QUEUE
#define ERROR_QUEUE_FILE           "/error_queue"
#endif
#define FILE_MASK_FILE             "/file_masks"
#define DC_LIST_FILE               "/dc_name_data"
#define DIR_NAME_FILE              "/directory_names"
#define JOB_ID_DATA_FILE           "/job_id_data"
#define PWB_DATA_FILE              "/pwb_data"
#define CURRENT_MSG_LIST_FILE      "/current_job_id_list"
#define AMG_DATA_FILE              "/amg_data"
#define AMG_DATA_FILE_TMP          "/amg_data.tmp"
#define ALTERNATE_FILE             "/alternate."
#define ALTERNATE_FILE_ALL         "/alternate.*"
#define LOCK_PROC_FILE             "/LOCK_FILE"
#define AFD_ACTIVE_FILE            "/AFD_ACTIVE"
#define WINDOW_ID_FILE             "/window_ids"

/* Definitions of fifo names. */
#define SYSTEM_LOG_FIFO            "/system_log.fifo"
#define RECEIVE_LOG_FIFO           "/receive_log.fifo"
#define TRANSFER_LOG_FIFO          "/transfer_log.fifo"
#define TRANS_DEBUG_LOG_FIFO       "/trans_db_log.fifo"
#define MON_LOG_FIFO               "/monitor_log.fifo"
#define AFD_CMD_FIFO               "/afd_cmd.fifo"
#define AFD_RESP_FIFO              "/afd_resp.fifo"
#define AMG_CMD_FIFO               "/amg_cmd.fifo"
#define DB_UPDATE_FIFO             "/db_update.fifo"
#define FD_CMD_FIFO                "/fd_cmd.fifo"
#define AW_CMD_FIFO                "/aw_cmd.fifo"
#define IP_FIN_FIFO                "/ip_fin.fifo"
#define SF_FIN_FIFO                "/sf_fin.fifo"
#define RETRY_FD_FIFO              "/retry_fd.fifo"
#define FD_DELETE_FIFO             "/fd_delete.fifo"
#define FD_WAKE_UP_FIFO            "/fd_wake_up.fifo"
#define PROBE_ONLY_FIFO            "/probe_only.fifo"
#ifdef _INPUT_LOG
# define INPUT_LOG_FIFO            "/input_log.fifo"
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_LOG_FIFO           "/output_log.fifo"
#endif
#ifdef _DELETE_LOG
# define DELETE_LOG_FIFO           "/delete_log.fifo"
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_LOG_FIFO       "/production_log.fifo"
#endif
#define RETRY_MON_FIFO             "/retry_mon.fifo."
#define DEL_TIME_JOB_FIFO          "/del_time_job.fifo"
#define FD_READY_FIFO              "/fd_ready.fifo"
#define MSG_FIFO                   "/msg.fifo"
#define AFDD_LOG_FIFO              "/afdd_log.fifo"
/*-----------------------------------------------------------------------*/

/* Definitions for the AFD name */
#define AFD_NAME                   "afd.name"
#define MAX_AFD_NAME_LENGTH        30

#define MSG_CACHE_BUF_SIZE         10000

/* Definitions for the different actions over fifos */
#define HALT                       0
#define STOP                       1
#define START                      2
#define SAVE_STOP                  3
#define QUICK_STOP                 4
#define ACKN                       5
#define NEW_DATA                   6    /* Used by AMG-DB-Editor */
#define START_AMG                  7
#define START_FD                   8
#define STOP_AMG                   9
#define STOP_FD                    10
#define AMG_READY                  11
#define PROC_TERM                  13
#define DEBUG                      14
#define RETRY                      15
#define QUEUE                      16
#define TRANSFER                   17
#define IS_ALIVE                   18
#define SHUTDOWN                   19
#define FSA_ABOUT_TO_CHANGE        20
#define CHECK_FILE_DIR             21 /* Check for jobs without message. */
#define DISABLE_MON                22
#define ENABLE_MON                 23
#define TRACE                      24
#define FULL_TRACE                 25
#define SR_EXEC_STAT               26 /* Show + reset exec stat in dir_check. */
#define SWITCH_MON                 27
#define FORCE_REMOTE_DIR_CHECK     28
#define GOT_LC                     29 /* Got log capabilities. */

#define DELETE_ALL_JOBS_FROM_HOST  1
#define DELETE_MESSAGE             2
#define DELETE_SINGLE_FILE         3
#define DELETE_RETRIEVE            4
#define DELETE_RETRIEVES_FROM_DIR  5

/* Definitions for directory flags. */
#define MAX_COPIED                 1
#define FILES_IN_QUEUE             2
#define ADD_TIME_ENTRY             4
#define LINK_NO_EXEC               8
#define DIR_DISABLED               16
#define ACCEPT_DOT_FILES           32
#define DONT_GET_DIR_LIST          64
#define DIR_ERROR_SET              128
#define WARN_TIME_REACHED          256
#ifdef WITH_INOTIFY
# define INOTIFY_RENAME            512
# define INOTIFY_CLOSE             1024
/*
 * Note: The following inotify flag are for the user interface
 *       AFD_CONFIG and DIR_CONFIG.
 */
# define INOTIFY_RENAME_FLAG       1
# define INOTIFY_CLOSE_FLAG        2
#endif

#ifdef WITH_DUP_CHECK
/* Definitions for duplicate check. */
# define DC_FILENAME_ONLY          1
# define DC_FILENAME_ONLY_BIT      1
# define DC_FILE_CONTENT           2
# define DC_FILE_CONTENT_BIT       2
# define DC_FILE_CONT_NAME         4
# define DC_FILE_CONT_NAME_BIT     3
# define DC_NAME_NO_SUFFIX         8
# define DC_NAME_NO_SUFFIX_BIT     4
# define DC_CRC32                  32768
# define DC_CRC32_BIT              16
# define DC_DELETE                 8388608
# define DC_DELETE_BIT             24
# define DC_STORE                  16777216
# define DC_STORE_BIT              25
# define DC_WARN                   33554432
# define DC_WARN_BIT               26
# define DC_DELETE_WARN_BIT        33
# define DC_STORE_WARN_BIT         34
#endif

/* Bitmap definitions for in_dc_flag in struct fileretrieve_status. */
#define DIR_ALIAS_IDC              1
#define UNKNOWN_FILES_IDC          2
#define QUEUED_FILES_IDC           4
#define OLD_LOCKED_FILES_IDC       8
#define REPUKW_FILES_IDC           16
#define DONT_REPUKW_FILES_IDC      32
#define MAX_CP_FILES_IDC           64
#define MAX_CP_FILE_SIZE_IDC       128
#define WARN_TIME_IDC              256
#define KEEP_CONNECTED_IDC         512
#ifdef WITH_INOTIFY
# define INOTIFY_FLAG_IDC          1024
#endif

/* In process AFD we have various stop flags */
#define STARTUP_ID                 -1
#define NONE_ID                    0
#define ALL_ID                     1
#define AMG_ID                     2
#define FD_ID                      3

#define NO_ID                      0

/* The following definitions are used for the function */
/* write_fsa(), so it knows where to write the info.   */
#define ERROR_COUNTER              1
#define TOTAL_FILE_SIZE            3
#define TRANSFER_RATE              9
#define NO_OF_FILES                11
#define CONNECT_STATUS             20

#define NO_BURST_COUNT_MASK        0x1f /* The mask to read the number   */
                                        /* of transfers that may not     */
                                        /* burst.                        */

#define AFDD_SHUTDOWN_MESSAGE      "500 AFDD shutdown."

/* Definitions for the different lock positions in the FSA. */
#define LOCK_FIU                   3   /* File name in use (job_status) */
/* NOTE: We must keep a gap behind LOCK_FIU since we add the job_no to it! */
#define LOCK_TFC                   20  /* Total file counter */
#define LOCK_EC                    21  /* Error counter */
#define LOCK_CON                   22  /* Connections */
#define LOCK_EXEC                  23  /* Lock for exec and pexec option, */
                                       /* this is ALSO used in FRA.       */

/*-----------------------------------------------------------------------*
 * Word offset for memory mapped structures of the AFD. Best is to leave
 * this value as it is. If you do change it you must remove all existing
 * memory mapped files from the fifo_dir, before starting the AFD with the
 * new value.
 * For the FSA these bytes are used to store information about the hole
 * AFD with the following meaning (assuming SIZEOF_INT is 4):
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of hosts served by the AFD.
 *           |               | If this FSA in no longer in use there
 *           |               | will be a -1 here.
 *    -------+---------------+---------------------------------------
 *       5   | unsigned char | Counter that is increased each time
 *           |               | there was a change in the HOST_CONFIG.
 *    -------+---------------+---------------------------------------
 *       6   | unsigned char | Flag to enable or disable the
 *           |               | following features:
 *           |               | Bit| Meaning
 *           |               | ---+-----------------------
 *           |               |  1 | DISABLE_RETRIEVE
 *           |               |  2 | DISABLE_ARCHIVE
 *           |               |  3 | ENABLE_CREATE_TARGET_DIR
 *    -------+---------------+---------------------------------------
 *       7   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *    9 - 12 | int           | Pagesize of this system.
 *    -------+---------------+---------------------------------------
 *   13 - 16 | int           | Not used.
 *
 * This is also used for the FRA with the following meaning:
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of directories that are
 *           |               | monitored by AFD. If this FRA in no
 *           |               | longer in use there will be a -1 here.
 *    -------+---------------+---------------------------------------
 *       5   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       6   | unsigned char | Flag to enable or disable the
 *           |               | following features:
 *           |               | Bit| Meaning
 *           |               | ---+-----------------------
 *           |               |  1 | DISABLE_DIR_WARN_TIME
 *    -------+---------------+---------------------------------------
 *       7   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *    9 - 16 |               | Not used.
 *-----------------------------------------------------------------------*/
#define AFD_WORD_OFFSET               (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define AFD_FEATURE_FLAG_OFFSET_START 5  /* From start */
#define AFD_FEATURE_FLAG_OFFSET_END   11 /* From end   */

/* Structure that holds status of the file transfer for each host */
#define CURRENT_FSA_VERSION 2
struct status
       {
          pid_t         proc_id;                /* Process ID of trans-  */
                                                /* fering job.           */
#ifdef _WITH_BURST_2
          char          unique_name[MAX_MSG_NAME_LENGTH];
          unsigned int  job_id;                 /* Since each host can   */
                                                /* have different type   */
                                                /* of jobs (other user,  */
                                                /* different directory,  */
                                                /* other options, etc),  */
                                                /* each of these is      */
                                                /* identified by this    */
                                                /* number.               */
#endif
          char          connect_status;         /* The status of what    */
                                                /* sf_xxx() is doing.    */
          int           no_of_files;            /* Total number of all   */
                                                /* files when job        */
                                                /* started.              */
          int           no_of_files_done;       /* Number of files done  */
                                                /* since the job has been*/
                                                /* started.              */
          off_t         file_size;              /* Total size of all     */
                                                /* files when we started */
                                                /* this job.             */
          u_off_t       file_size_done;         /* The total number of   */
                                                /* bytes we have send so */
                                                /* far.                  */
          u_off_t       bytes_send;             /* Overall number of     */
                                                /* bytes send so far for */
                                                /* this job.             */
          char          file_name_in_use[MAX_FILENAME_LENGTH];
                                                /* The name of the file  */
                                                /* that is in transfer.  */
#ifdef _WITH_BURST_2
                                                /* NOTE: We misuse this  */
                                                /* field in case of a    */
                                                /* burst to specify the  */
                                                /* number of retries.    */
#endif
          off_t         file_size_in_use;       /* Total size of current */
                                                /* file.                 */
          off_t         file_size_in_use_done;  /* Number of bytes send  */
                                                /* for current file.     */
       };

struct filetransfer_status
       {
          char           host_alias[MAX_HOSTNAME_LENGTH + 1];
                                            /* Here the alias hostname is*/
                                            /* stored. When a secondary  */
                                            /* host can be specified,    */
                                            /* only that part is stored  */
                                            /* up to the position of the */
                                            /* toggling character eg:    */
                                            /* mrz_mfa + mrz_mfb =>      */
                                            /*       mrz_mf              */
          char           real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                            /* This is the real hostname */
                                            /* where the data should be  */
                                            /* send to.                  */
/* FIXME: Since we use host_dsp_name in several cases for logging and    */
/*        when we use toggling and host_alias is already                 */
/*        MAX_HOSTNAME_LENGTH we place the toggling char where the '\0'  */
/*        should be. So increase the length by one, when updating FSA!   */
          char           host_dsp_name[MAX_HOSTNAME_LENGTH + 1];
                                            /* This is the hostname that */
                                            /* is being displayed by     */
                                            /* afd_ctrl. It's the same   */
                                            /* as stored in host_alias   */
                                            /* plus the toggling         */
                                            /* character.                */
          char           proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          char           host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char           toggle_pos;        /* The position of the       */
                                            /* toggling character in the */
                                            /* host name.                */
          char           original_toggle_pos;/* The original position    */
                                            /* before it was toggled     */
                                            /* automatically.            */
          char           auto_toggle;       /* When ON and an error      */
                                            /* occurs it switches auto-  */
                                            /* matically to the other    */
                                            /* host.                     */
          signed char    file_size_offset;  /* When doing an ls on the   */
                                            /* remote site, this is the  */
                                            /* position where to find    */
                                            /* the size of the file. If  */
                                            /* it is less than 0, it     */
                                            /* means that we do not want */
                                            /* to append files that have */
                                            /* been partly send.         */
          int            successful_retries;/* Number of current         */
                                            /* successful retries.       */
          int            max_successful_retries; /* Retries before       */
                                            /* switching hosts.          */
          unsigned char  special_flag;      /* Special flag with the     */
                                            /* following meaning:        */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*| 8    | Error job under  |*/
                                            /*|      | process.         |*/
                                            /*| 7    | Host is in       |*/
                                            /*|      | DIR_CONFIG file. |*/
                                            /*| 6    | Host disabled.   |*/
                                            /*| 1 - 5| Number of jobs   |*/
                                            /*|      | that may NOT     |*/
                                            /*|      | burst.           |*/
                                            /*+------+------------------+*/
          unsigned int   protocol;          /* Transfer protocol that    */
                                            /* is being used:            */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*| 32   | RETRIEVE         |*/
                                            /*| 31   | SEND             |*/
                                            /*| 30   | SSL              |*/
                                            /*| 19-30| Not used.        |*/
                                            /*| 18   | GET_SFTP         |*/
                                            /*| 17   | GET_HTTP  [SSL]  |*/
                                            /*| 16   | GET_FTP   [SSL]  |*/
                                            /*| 9-15 | Not used.        |*/
                                            /*| 8    | SFTP             |*/
                                            /*| 7    | HTTP      [SSL]  |*/
                                            /*| 6    | WMO              |*/
                                            /*| 5    | SCP              |*/
                                            /*| 4    | MAP              |*/
                                            /*| 3    | SMTP             |*/
                                            /*| 2    | LOC              |*/
                                            /*| 1    | FTP       [SSL]  |*/
                                            /*+------+------------------+*/
          unsigned int   protocol_options;  /* Special options for the   */
                                            /* protocols:                */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*| 9-32 | Not used.        |*/
                                            /*| 8    | DISABLE_BURSTING |*/
                                            /*| 7    | FTP_EXTENDED_MODE|*/
                                            /*| 6    | FTP_IGNORE_BIN   |*/
                                            /*| 5    | FTP_FAST_CD      |*/
                                            /*| 4    | FTP_FAST_MOVE    |*/
                                            /*| 3    | STAT_KEEPALIVE   |*/
                                            /*| 2    | SET_IDLE_TIME    |*/
                                            /*| 1    | FTP_PASSIVE_MODE |*/
                                            /*+------+------------------+*/
          unsigned int   socksnd_bufsize;   /* Socket buffer size for    */
                                            /* sending data. 0 is default*/
                                            /* which is the socket buffer*/
                                            /* is left to system default.*/
          unsigned int   sockrcv_bufsize;   /* Socket buffer size for    */
                                            /* receiving data.           */
          unsigned int   keep_connected;    /* Keep connection open for  */
                                            /* the given number of       */
                                            /* seconds, after all files  */
                                            /* have been transmitted.    */
#ifdef WITH_DUP_CHECK                                                      
          unsigned int   dup_check_flag;    /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|27-32 | Not used.        |*/
                                            /*|   26 | DC_WARN          |*/
                                            /*|   25 | DC_STORE         |*/
                                            /*|   24 | DC_DELETE        |*/
                                            /*|17-23 | Not used.        |*/
                                            /*|   16 | DC_CRC32         |*/
                                            /*| 5-15 | Not used.        |*/
                                            /*|    4 | DC_NAME_NO_SUFFIX|*/
                                            /*|    3 | DC_FILE_CONT_NAME|*/
                                            /*|    2 | DC_FILE_CONTENT  |*/
                                            /*|    1 | DC_FILENAME_ONLY |*/
                                            /*+------+------------------+*/
#endif
          unsigned int   host_id;           /* CRC-32 checksum of        */
                                            /* host_alias above.         */
          char           debug;             /* When this flag is set all */
                                            /* transfer information is   */
                                            /* logged.                   */
          char           host_toggle;       /* If their is more then one */
                                            /* host you can toggle       */
                                            /* between these two         */
                                            /* addresses by toggling     */
                                            /* this switch.              */
          unsigned int   host_status;       /* What is the status for    */
                                            /* this host?                */
                                            /*+----+--------------------------+*/
                                            /*| Bit|      Meaning             |*/
                                            /*+----+--------------------------+*/
                                            /*|9-32| Not used.                |*/
                                            /*|   8| ERROR_QUEUE_SET          |*/
                                            /*|   7| HOST_TWO_FLAG            |*/
                                            /*|   6| HOST_CONFIG_HOST_DISABLED|*/
                                            /*|   5| Not used.                |*/
                                            /*|   4| DANGER_PAUSE_QUEUE_STAT  |*/
                                            /*|   3| AUTO_PAUSE_QUEUE_STAT    |*/
                                            /*|   2| PAUSE_QUEUE_STAT         |*/
                                            /*|   1| STOP_TRANSFER_STAT       |*/
                                            /*+----+--------------------------+*/
          int            error_counter;     /* Errors that have so far   */
                                            /* occurred. With the next   */
                                            /* successful transfer this  */
                                            /* will be set to 0.         */
          unsigned int   total_errors;      /* No. of errors so far.     */
          int            max_errors;        /* The maximum errors that   */
                                            /* may occur before we ring  */
                                            /* the alarm bells ;-).      */
          unsigned char  error_history[ERROR_HISTORY_LENGTH];
          int            retry_interval;    /* After an error has        */
                                            /* occurred, when should we  */
                                            /* retry?                    */
          int            block_size;        /* Block size at which the   */
                                            /* files get transfered.     */
          int            ttl;               /* Time-to-live for outgoing */
                                            /* multicasts.               */
#ifdef WITH_DUP_CHECK                                                      
          time_t         dup_check_timeout; /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          time_t         last_retry_time;   /* When was the last time we */
                                            /* tried to send a file for  */
                                            /* this host?                */
          time_t         last_connection;   /* Time of last successfull  */
                                            /* transfer.                 */
          time_t         first_error_time;  /* The first time when a     */
                                            /* transmission error        */
                                            /* condition started.        */
          int            total_file_counter;/* The overall number of     */
                                            /* files still to be send.   */
          off_t          total_file_size;   /* The overall number of     */
                                            /* bytes still to be send.   */
          unsigned int   jobs_queued;       /* The number of jobs queued */
                                            /* by the FD.                */
          unsigned int   file_counter_done; /* No. of files done so far. */
          u_off_t        bytes_send;        /* No. of bytes send so far. */
          unsigned int   connections;       /* No. of connections.       */
          unsigned int   mc_nack_counter;   /* Multicast Negative        */
                                            /* Acknowledge Counter.      */
                                            /* NOTE: Unused!             */
          int            active_transfers;  /* No. of jobs transferring  */
                                            /* data.                     */
          int            allowed_transfers; /* Maximum no. of parallel   */
                                            /* transfers for this host.  */
          long           transfer_timeout;  /* When to timeout the       */
                                            /* transmitting job.         */
          off_t          transfer_rate_limit; /* The maximum bytes that  */
                                            /* may be transfered per     */
                                            /* second.                   */
          off_t          trl_per_process;   /* Transfer rate limit per   */
                                            /* active process.           */
          off_t          mc_ct_rate_limit;  /* Multicast current transfer*/
                                            /* rate limit.               */
                                            /* NOTE: Unused!             */
          off_t          mc_ctrl_per_process; /* Multicast current       */
                                            /* transfer rate limit per   */
                                            /* process.                  */
                                            /* NOTE: Unused!             */
          struct status  job_status[MAX_NO_PARALLEL_JOBS];
       };

/* Structure that holds all hosts. */
#define HOST_BUF_SIZE 100
struct host_list
       {
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          fullname[MAX_FILENAME_LENGTH];
                                              /* This is needed when we   */
                                              /* have hostname with []    */
                                              /* syntax.                  */
          char          real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          char          host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char          proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          int           allowed_transfers;
          int           max_errors;
          int           retry_interval;
          int           ttl;
          int           transfer_blksize;
          int           transfer_rate_limit;
          int           successful_retries; /* NOTE: Corresponds to      */
                                            /* max_successful_retries in */
                                            /* FSA.                      */
          unsigned int  protocol_options;   /* Mostly used for FTP, to   */
                                            /* indicate for example:     */
                                            /* active-, passive-mode,    */
                                            /* send IDLE command, etc.   */
          unsigned int   socksnd_bufsize;   /* Socket buffer size for    */
                                            /* sending data. 0 is default*/
                                            /* which is the socket buffer*/
                                            /* is left to system default.*/
          unsigned int   sockrcv_bufsize;   /* Socket buffer size for    */
                                            /* receiving data.           */
          unsigned int   keep_connected;    /* Keep connection open for  */
                                            /* the given number of       */
                                            /* seconds, after all files  */
                                            /* have been transmitted.    */
#ifdef WITH_DUP_CHECK                                                      
          unsigned int   dup_check_flag;    /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|27-32 | Not used.        |*/
                                            /*|   26 | DC_WARN          |*/
                                            /*|   25 | DC_STORE         |*/
                                            /*|   24 | DC_DELETE        |*/
                                            /*|17-23 | Not used.        |*/
                                            /*|   16 | DC_CRC32         |*/
                                            /*| 5-15 | Not used.        |*/
                                            /*|    4 | DC_NAME_NO_SUFFIX|*/
                                            /*|    3 | DC_FILE_CONT_NAME|*/
                                            /*|    2 | DC_FILE_CONTENT  |*/
                                            /*|    1 | DC_FILENAME_ONLY |*/
                                            /*+------+------------------+*/
#endif
          unsigned int  protocol;
          long          transfer_timeout;
#ifdef WITH_DUP_CHECK                                                      
          time_t        dup_check_timeout;  /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          signed char   file_size_offset;
          unsigned char number_of_no_bursts;
          unsigned char host_status;
          signed char   in_dir_config;
       };

/* Structure to hold all possible bits for a time entry. */
struct bd_time_entry
       {
#ifdef _WORKING_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };

/* Structure holding all neccessary data for retrieving files */
#define CURRENT_FRA_VERSION 4
#define MAX_WAIT_FOR_LENGTH 64
struct fileretrieve_status
       {
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
                                            /* Here the alias hostname is*/
                                            /* stored. When a secondary  */
                                            /* host can be specified,    */
                                            /* only that part is stored  */
                                            /* up to the position of the */
                                            /* toggling character eg:    */
                                            /* mrz_mfa + mrz_mfb =>      */
                                            /*       mrz_mf              */
          char          url[MAX_RECIPIENT_LENGTH];
          char          wait_for_filename[MAX_WAIT_FOR_LENGTH]; /* Wait  */
                                            /* for the given file name|  */
                                            /* pattern before we take    */
                                            /* files from this directory.*/
          struct bd_time_entry te;          /* Time entry, when files    */
                                            /* are to be searched for.   */
          struct bd_time_entry ate;         /* Additional time entry.    */
          unsigned char dir_status;         /* Status of this directory. */
          unsigned char remove;             /* Should the files be       */
                                            /* removed when they are     */
                                            /* being retrieved.          */
          unsigned char stupid_mode;        /* If set to YES it will NOT */
                                            /* collect information about */
                                            /* files that where found in */
                                            /* directory. So that when   */
                                            /* remove is not set we will */
                                            /* not always collect the    */
                                            /* same files. This ensures  */
                                            /* that files are collected  */
                                            /* only once.                */
                                            /* If this is set to         */
                                            /* GET_ONCE_ONLY it will get */
                                            /* the file once only,       */
                                            /* regardless if the file is */
                                            /* changed. If set to NO the */
                                            /* it will try to fetch it   */
                                            /* again when it changes.    */
          unsigned char delete_files_flag;  /* UNKNOWN_FILES: All unknown*/
                                            /* files will be deleted.    */
                                            /* QUEUED_FILES: Queues will */
                                            /* also be checked for old   */
                                            /* files.                    */
                                            /* OLD_LOCKED_FILES: Old     */
                                            /* locked files are to be    */
                                            /* deleted.                  */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|  4-8 | Not used.        |*/
                                            /*|    3 | OLD_LOCKED_FILES |*/
                                            /*|    2 | QUEUED_FILES     |*/
                                            /*|    1 | UNKNOWN_FILES    |*/
                                            /*+------+------------------+*/
          unsigned char report_unknown_files;
          unsigned char important_dir;      /* Directory is important.   */
                                            /* In times where all        */
                                            /* directories contain lots  */
                                            /* files or the filesystem   */
                                            /* is very slow, this        */
                                            /* directory will get more   */
                                            /* attention.                */
          unsigned char time_option;        /* Flag to indicate if the   */
                                            /* time option is used.      */
          char          force_reread;       /* Always read the directory.*/
                                            /* Don't check the directory */
                                            /* time.                     */
          char          queued;             /* Used by FD, so it knows   */
                                            /* if the job is in the      */
                                            /* queue or not.             */
          char          priority;           /* Priority of this          */
                                            /* directory.                */
          unsigned int  protocol;           /* Transfer protocol that    */
                                            /* is being used.            */
          unsigned int  files_received;     /* No. of files received so  */
                                            /* far.                      */
          unsigned int  dir_flag;           /* Flag for this directory   */
                                            /* informing about the       */
                                            /* following status:         */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|12-32 | Not used.        |*/
                                            /*|   11 | INOTIFY_CLOSE    |*/
                                            /*|   10 | INOTIFY_RENAME   |*/
                                            /*|    9 | WARN_TIME_REACHED|*/
                                            /*|    8 | DIR_ERROR_SET    |*/
                                            /*|    7 | DONT_GET_DIR_LIST|*/
                                            /*|    6 | ACCEPT_DOT_FILES |*/
                                            /*|    5 | DIR_DISABLED     |*/
                                            /*|    4 | LINK_NO_EXEC     |*/
                                            /*|    3 | ADD_TIME_ENTRY   |*/
                                            /*|    2 | FILES_IN_QUEUE   |*/
                                            /*|    1 | MAX_COPIED       |*/
                                            /*+------+------------------+*/
          unsigned int  in_dc_flag;         /* Flag to indicate which of */
                                            /* the options have been     */
                                            /* stored in DIR_CONFIG. This*/
                                            /* is usefull for restoring  */
                                            /* the DIR_CONFIG from       */
                                            /* scratch. The following    */
                                            /* flags are possible:       */
                                            /*+---+---------------------+*/
                                            /*|Bit|        Meaning      |*/
                                            /*+---+---------------------+*/
                                            /*| 8 |MAX_CP_FILE_SIZE_IDC |*/
                                            /*| 7 |MAX_CP_FILES_IDC     |*/
                                            /*| 6 |DONT_REPUKW_FILES_IDC|*/
                                            /*| 5 |REPUKW_FILES_IDC     |*/
                                            /*| 4 |OLD_LOCKED_FILES_IDC |*/
                                            /*| 3 |QUEUED_FILES_IDC     |*/
                                            /*| 2 |UNKNOWN_FILES_IDC    |*/
                                            /*| 1 |DIR_ALIAS_IDC        |*/
                                            /*+---+---------------------+*/
          unsigned int  files_in_dir;       /* The number of files       */
                                            /* currently in this         */
                                            /* directory.                */
          unsigned int  files_queued;       /* The number of files in    */
                                            /* the queue.                */
          unsigned int  accumulate;         /* How many files need to    */
                                            /* accumulate before start   */
                                            /* sending from this dir.    */
          unsigned int  max_copied_files;   /* Maximum number of files   */
                                            /* that we copy in one go.   */
          unsigned int  ignore_file_time;   /* Ignore files which are    */
                                            /* older, equal or newer     */
                                            /* the given time in sec.    */
          unsigned int  gt_lt_sign;         /* The sign for the following*/
                                            /* variables:                */
                                            /*     ignore_size           */
                                            /*     ignore_file_time      */
                                            /* These are bit masked      */
                                            /* for each variable.        */
                                            /*+---+---------------------+*/
                                            /*|Bit|       Meaning       |*/
                                            /*+---+---------------------+*/
                                            /*| 1 | ISIZE_EQUAL         |*/
                                            /*| 2 | ISIZE_LESS_THEN     |*/
                                            /*| 3 | ISIZE_GREATER_THEN  |*/
                                            /*+---+---------------------+*/
                                            /*| 4 | IFTIME_EQUAL        |*/
                                            /*| 5 | IFTIME_LESS_THEN    |*/
                                            /*| 6 | IFTIME_GREATER_THEN |*/
                                            /*+---+---------------------+*/
                                            /*| * | Rest not used.      |*/
                                            /*+---+---------------------+*/
          unsigned int  keep_connected;     /* After all files have been */
                                            /* retrieved, the time to    */
                                            /* stay connected.           */
#ifdef WITH_DUP_CHECK                                                      
          unsigned int  dup_check_flag;     /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|27-32 | Not used.        |*/
                                            /*|   26 | DC_WARN          |*/
                                            /*|   25 | DC_STORE         |*/
                                            /*|   24 | DC_DELETE        |*/
                                            /*|17-23 | Not used.        |*/
                                            /*|   16 | DC_CRC32         |*/
                                            /*| 4-15 | Not used.        |*/
                                            /*|    3 | DC_FILE_CONT_NAME|*/
                                            /*|    2 | DC_FILE_CONTENT  |*/
                                            /*|    1 | DC_FILENAME_ONLY |*/
                                            /*+------+------------------+*/
#endif
          u_off_t       bytes_received;     /* No. of bytes received so  */
                                            /* far.                      */
          off_t         bytes_in_dir;       /* No. of bytes in this      */
                                            /* directory.                */
          off_t         bytes_in_queue;     /* No. of bytes in queue(s). */
          off_t         accumulate_size;    /* How many Bytes need to    */
                                            /* accumulate before we take */
                                            /* files from this dir.      */
          off_t         ignore_size;        /* Ignore any files less     */
                                            /* then, equal or greater    */
                                            /* then the given size.      */
          off_t         max_copied_file_size; /* The maximum number of   */
                                            /* bytes that we copy in one */
                                            /* go.                       */
#ifdef WITH_DUP_CHECK                                                      
          time_t        dup_check_timeout;  /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          time_t        last_retrieval;     /* Time of last retrieval.   */
          time_t        next_check_time;    /* Next time to check for    */
                                            /* files in directory.       */
          time_t        warn_time;          /* Time when to warn that the*/
                                            /* directory has not received*/
                                            /* any data.                 */
          int           unknown_file_time;  /* After how many hours can  */
                                            /* a unknown file be deleted.*/
          int           queued_file_time;   /* After how many hours can  */
                                            /* a queued file be deleted. */
          int           locked_file_time;   /* After how many hours can  */
                                            /* a locked file be deleted. */
          int           end_character;      /* Only pick up files where  */
                                            /* the last charachter (of   */
                                            /* the content) contains     */
                                            /* this character. A -1      */
                                            /* means not to check the    */
                                            /* last character.           */
          unsigned int  dir_id;             /* Unique number to identify */
                                            /* directory faster and      */
                                            /* easier.                   */
          int           fsa_pos;            /* Position of this host in  */
                                            /* FSA, to get the data that */
                                            /* are in the HOST_CONFIG.   */
          int           no_of_process;      /* The number of process     */
                                            /* that currently process    */
                                            /* data for this directory.  */
          int           max_process;        /* The maximum number of     */
                                            /* process that may be       */
                                            /* forked for this directory.*/
          int           max_errors;         /* Max errors before we ring */
                                            /* the alarm bells.          */
          unsigned int  error_counter;      /* The number of errors when */
                                            /* trying to access this     */
                                            /* directory. Will be set to */
                                            /* zero after each succesfull*/
                                            /* access.                   */
       };

/* Bit map flag for AMG and FD communication. */
#define DIR_CHECK_ACTIVE     1
#define REREADING_DIR_CONFIG 2
#define FD_WAITING           4
#define DIR_CHECK_MSG_QUEUED 32
#define WRITTING_JID_STRUCT  64
#define FD_DIR_CHECK_ACTIVE  128

/* Definitions for the different lock positions in the FSA. */
#define LOCK_FD_DIR_CHECK_ACTIVE 1

/* Structure that holds status of all process */
struct afd_status
       {
          signed char   amg;             /* Automatic Message Generator, */
                                         /* can have the following       */
                                         /* values:                      */
                                         /*  -3 - Process has been       */
                                         /*       stopped normally.      */
                                         /*   0 - Not running.           */
                                         /*   1 - Process is running.    */
                                         /*  19 - Shutting down.         */
          unsigned char amg_jobs;        /* Bitmap to show if jobs of    */
                                         /* the AMG (dir_check(),        */
                                         /* time_job(), ...) are active: */
                                         /*+------+---------------------+*/
                                         /*|Bit(s)|      Meaning        |*/
                                         /*+------+---------------------+*/
                                         /*| 1    | dir_check() active  |*/
                                         /*| 2    | Rereading DIR_CONFIG|*/
                                         /*| 3    | FD waiting for AMG  |*/
                                         /*|      | to finish DIR_CONFIG|*/
                                         /*| 4 - 5| Not used.           |*/
                                         /*| 6    | dir_check() has msg |*/
                                         /*|      | queued.             |*/
                                         /*| 7    | AMG writting to     |*/
                                         /*|      | JID structure.      |*/
                                         /*| 8    | FD searching dirs.  |*/
                                         /*+------+---------------------+*/
          signed char   fd;              /* File Distributor             */
          signed char   sys_log;         /* System Log                   */
          signed char   receive_log;     /* Receive Log                  */
          signed char   trans_log;       /* Transfer Log                 */
          signed char   trans_db_log;    /* Transfer Debug Log           */
          signed char   archive_watch;
          signed char   afd_stat;        /* Statistic program            */
          signed char   afdd;
#ifndef HAVE_MMAP
          signed char   mapper;
#endif
#ifdef _INPUT_LOG
          signed char   input_log;
#endif
#ifdef _OUTPUT_LOG
          signed char   output_log;
#endif
#ifdef _DELETE_LOG
          signed char   delete_log;
#endif
#ifdef _PRODUCTION_LOG
          signed char   production_log;
#endif
          unsigned int  sys_log_ec;         /* System log entry counter. */
          char          sys_log_fifo[LOG_FIFO_SIZE + 1];
          char          sys_log_history[MAX_LOG_HISTORY];
          unsigned int  receive_log_ec;     /* Receive log entry counter.*/
          char          receive_log_fifo[LOG_FIFO_SIZE + 1];
          char          receive_log_history[MAX_LOG_HISTORY];
          unsigned int  trans_log_ec;       /* Transfer log entry        */
                                            /* counter.                  */
          char          trans_log_fifo[LOG_FIFO_SIZE + 1];
          char          trans_log_history[MAX_LOG_HISTORY];
          int           no_of_transfers;    /* The number of active      */
                                            /* transfers system wide.    */
          int           no_of_retrieves;    /* The number of process     */
                                            /* that may collect files.   */
          nlink_t       jobs_in_queue;      /* The number of jobs still  */
                                            /* to be done by the FD.     */
          time_t        start_time;         /* Time when AFD was started.*/
                                            /* This value is used for    */
                                            /* eval_database() so it     */
                                            /* when it has started the   */
                                            /* first time.               */
          unsigned int  fd_fork_counter;    /* Number of forks() by FD.  */
          unsigned int  amg_fork_counter;   /* Number of forks() by AMG. */
          unsigned int  burst2_counter;     /* Number of burst2 done by  */
                                            /* FD.                       */
          unsigned int  max_queue_length;   /* Max. FD queue length.     */
          unsigned int  dir_scans;          /* Number of directory scans.*/
       };

/* Structure that holds all relevant information of */
/* all process that have been started by the AFD.   */
struct proc_table
       {
          pid_t       pid;
          signed char *status;
          char        proc_name[MAX_PROCNAME_LENGTH];
       };

/* Definitions for renaming */
#define READ_RULES_INTERVAL        30          /* in seconds             */
#define MAX_RULE_HEADER_LENGTH     50
struct rule
       {
          int  no_of_rules;
          char header[MAX_RULE_HEADER_LENGTH + 1];
          char **filter;
          char **rename_to;
       };

/* Definition for structure that holds all data for one job ID */
#define CURRENT_JID_VERSION 1
struct job_id_data
       {
          unsigned int job_id;          /* CRC-32 checksum of the job.   */
          unsigned int dir_id;          /* CRC-32 checksum of the dir.   */
          unsigned int file_mask_id;    /* CRC-32 checksum of file masks.*/
          unsigned int dir_config_id;   /* CRC-32 checksum of DIR_CONFIG.*/
          int          dir_id_pos;      /* Position of the directory     */
                                        /* name int the structure        */
                                        /* dir_name_buf.                 */
          char         priority;
          int          no_of_loptions;
          char         loptions[MAX_OPTION_LENGTH];
          int          no_of_soptions;
          char         soptions[MAX_OPTION_LENGTH]; /* NOTE: The last    */
                                        /* character is used to change   */
                                        /* CRC value in the very unusal  */
                                        /* case when there is one CRC    */
                                        /* for two or more jobs.         */
          char         recipient[MAX_RECIPIENT_LENGTH];
          char         host_alias[MAX_HOSTNAME_LENGTH + 1];
       };
#define CURRENT_DNB_VERSION 1
struct dir_name_buf
       {
          char         dir_name[MAX_PATH_LENGTH];/* Full directory name. */
          char         orig_dir_name[MAX_PATH_LENGTH]; /* Directory name */
                                            /* as it is in DIR_CONFIG.   */
          unsigned int dir_id;              /* Unique number to identify */
                                            /* directory faster and      */
                                            /* easier.                   */
       };
#define CURRENT_PWB_VERSION 0
#define PWB_STEP_SIZE       20
struct passwd_buf
       {
          char          uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];
          unsigned char passwd[MAX_USER_NAME_LENGTH];
          signed char   dup_check;
       };

/* The file mask structure is not a structure but a collection of */
/* ints, unsigned ints and chars. See amg/lookup_file_mask_id.c   */
/* for more details.                                              */
#define CURRENT_FMD_VERSION 0

/* Definition for structure that holds data for different DIR_CONFIG's. */
#define CURRENT_DCID_VERSION 0
struct dir_config_list
       {
          unsigned int dc_id;
          char         dir_config_file[MAX_PATH_LENGTH];
       };

struct delete_log
       {
          int           fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
          int           readfd;
#endif
          unsigned int  *job_number;
          char          *data;
          char          *file_name;
          unsigned char *file_name_length;
          off_t         *file_size;
          char          *host_name;
          size_t        size;
       };

#ifdef WITH_DUP_CHECK
/* Definition for structure holding CRC values for duplicate checks. */
/*
# define CRC_STEP_SIZE       1024
# define DUPCHECK_CHECK_TIME 600
*/
# define CRC_STEP_SIZE       2
# define DUPCHECK_CHECK_TIME 30
struct crc_buf
       {
          unsigned int crc;
          unsigned int flag;
          time_t       timeout;
       };
#endif

struct dir_options
       {
          int  no_of_dir_options;
          char aoptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
          char dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char url[MAX_PATH_LENGTH];
       };

/* Structure holding all filenames that are/have been retrieved. */
#define CURRENT_RL_VERSION      0
#define RETRIEVE_LIST_STEP_SIZE 10
struct retrieve_list
       {
          char   file_name[MAX_FILENAME_LENGTH];
          char   got_date;
          char   retrieved;              /* Has the file already been      */
                                         /* retrieved?                     */
          char   in_list;                /* Used to remove any files from  */
                                         /* the list that are no longer at */
                                         /* the remote host.               */
          off_t  size;                   /* Size of the file.              */
          time_t file_mtime;             /* Modification time of file.     */
       };

/* For compatibility reasons we must still know the retrieve_list from */
/* 1.2.x so we can convert the old list.                               */
#define OLD_MAX_FTP_DATE_LENGTH 15
struct old_retrieve_list
       {
          char  file_name[MAX_FILENAME_LENGTH];
          char  date[OLD_MAX_FTP_DATE_LENGTH];
          char  retrieved;
          char  in_list;
          off_t size;
       };
struct old_int_retrieve_list
       {
          char file_name[MAX_FILENAME_LENGTH];
          char date[OLD_MAX_FTP_DATE_LENGTH];
          char retrieved;
          char in_list;
          int  size;
       };

/* Runtime array */
#define RT_ARRAY(name, rows, columns, type)                              \
        {                                                                \
           int macro_row_counter;                                        \
                                                                         \
           if ((name = (type **)calloc((rows), sizeof(type *))) == NULL) \
           {                                                             \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                 \
                         "calloc() error : %s", strerror(errno));        \
              exit(INCORRECT);                                           \
           }                                                             \
                                                                         \
           if (((name)[0] = (type *)calloc(((rows) * (columns)),         \
                                           sizeof(type))) == NULL)       \
           {                                                             \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                 \
                         "calloc() error : %s", strerror(errno));        \
              exit(INCORRECT);                                           \
           }                                                             \
                                                                         \
           for (macro_row_counter = 1; macro_row_counter < (rows); macro_row_counter++) \
              (name)[macro_row_counter] = ((name)[0] + (macro_row_counter * (columns)));\
        }
#define FREE_RT_ARRAY(name) \
        {                   \
           free((name)[0]); \
           free((name));    \
        }
#define REALLOC_RT_ARRAY(name, rows, columns, type)                         \
        {                                                                   \
           int  macro_row_counter;                                          \
           char *macro_ptr = (name)[0];                                     \
                                                                            \
           if (((name) = (type **)realloc((name), (rows) * sizeof(type *))) == NULL) \
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "realloc() error : %s", strerror(errno));          \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           if (((name)[0] = (type *)realloc(macro_ptr,                      \
                             (((rows) * (columns)) * sizeof(type)))) == NULL) \
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "realloc() error : %s", strerror(errno));          \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           for (macro_row_counter = 1; macro_row_counter < (rows); macro_row_counter++) \
              (name)[macro_row_counter] = ((name)[0] + (macro_row_counter * (columns)));\
        }

/* Runtime pointer array */
#define RT_P_ARRAY(name, rows, columns, type)                               \
        {                                                                   \
           register int macro_i;                                            \
                                                                            \
           if (((name) = (type ***)malloc(rows * sizeof(type **))) == NULL) \
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "malloc() error : %s", strerror(errno));           \
              exit(INCORRECT);                                              \
           }                                                                \
           if (((name)[0] = (type **)malloc(rows * columns * sizeof(type *))) == NULL) \
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "malloc() error : %s", strerror(errno));           \
              exit(INCORRECT);                                              \
           }                                                                \
           for (macro_i = 0; macro_i < rows; macro_i++)                     \
           {                                                                \
              (name)[macro_i] = (name)[0] + (macro_i * columns);            \
           }                                                                \
        }
#define FREE_RT_P_ARRAY(name)   \
        {                       \
           free((name));        \
           free((name)[0]);     \
        }

/* Macro that does do a strncpy without filling up rest with binary zeros. */
#define STRNCPY(dest, src, n)                          \
        {                                              \
           register int macro_i;                       \
                                                       \
           for (macro_i = 0; macro_i < (n); macro_i++) \
           {                                           \
              (dest)[macro_i] = (src)[macro_i];        \
              if ((src)[macro_i] == '\0')              \
              {                                        \
                 break;                                \
              }                                        \
           }                                           \
        }

/* Macro that positions pointer just after binary zero. */
#define NEXT(ptr)                 \
        {                         \
           while (*(ptr) != '\0') \
           {                      \
              (ptr)++;            \
           }                      \
           (ptr)++;               \
        }

#ifdef LOCK_DEBUG
#define ABS_REDUCE_QUEUE(fra_pos, files, bytes)              \
        {                                                    \
           unsigned int tmp_files;                           \
                                                             \
           lock_region_w(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra, __FILE__, __LINE__);\
           tmp_files = fra[(fra_pos)].files_queued;          \
           fra[(fra_pos)].files_queued -= (files);           \
           if (fra[(fra_pos)].files_queued > tmp_files)      \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Files queued overflowed (%u - %u) for FRA pos %d.", \
                         tmp_files, (files), (fra_pos));     \
              fra[(fra_pos)].files_queued = 0;               \
           }                                                 \
           if ((fra[(fra_pos)].files_queued == 0) &&         \
               (fra[(fra_pos)].dir_flag & FILES_IN_QUEUE))   \
           {                                                 \
              fra[(fra_pos)].dir_flag ^= FILES_IN_QUEUE;     \
           }                                                 \
           fra[(fra_pos)].bytes_in_queue -= (bytes);         \
           if (fra[(fra_pos)].bytes_in_queue < 0)            \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Bytes queued overflowed for FRA pos %d.",\
                         (fra_pos));                         \
              fra[(fra_pos)].bytes_in_queue = 0;             \
           }                                                 \
           unlock_region(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra, __FILE__, __LINE__);\
        }
#else
#define ABS_REDUCE_QUEUE(fra_pos, files, bytes)              \
        {                                                    \
           unsigned int tmp_files;                           \
                                                             \
           lock_region_w(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra);\
           tmp_files = fra[(fra_pos)].files_queued;          \
           fra[(fra_pos)].files_queued -= (files);           \
           if (fra[(fra_pos)].files_queued > tmp_files)      \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Files queued overflowed (%u - %u) for FRA pos %d.", \
                         tmp_files, (files), (fra_pos));     \
              fra[(fra_pos)].files_queued = 0;               \
           }                                                 \
           if ((fra[(fra_pos)].files_queued == 0) &&         \
               (fra[(fra_pos)].dir_flag & FILES_IN_QUEUE))   \
           {                                                 \
              fra[(fra_pos)].dir_flag ^= FILES_IN_QUEUE;     \
           }                                                 \
           fra[(fra_pos)].bytes_in_queue -= (bytes);         \
           if (fra[(fra_pos)].bytes_in_queue < 0)            \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Bytes queued overflowed for FRA pos %d.",\
                         (fra_pos));                         \
              fra[(fra_pos)].bytes_in_queue = 0;             \
           }                                                 \
           unlock_region(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra);\
        }
#endif
#define SET_DIR_STATUS(flag, status)            \
        {                                       \
           if ((flag) & DIR_DISABLED)           \
           {                                    \
              (status) = DISABLED;              \
           }                                    \
           else if ((flag) & DIR_ERROR_SET)     \
                {                               \
                   (status) = NOT_WORKING2;     \
                }                               \
           else if ((flag) & WARN_TIME_REACHED) \
                {                               \
                   (status) = WARNING_ID;       \
                }                               \
                else                            \
                {                               \
                   (status) = NORMAL_STATUS;    \
                }                               \
        }

/* Macro to check if we can avoid a strcmp. */
#define CHECK_STRCMP(a, b)  (*(a) != *(b) ? (int)((unsigned char) *(a) - (unsigned char) *(b)) : strcmp((a), (b)))

/* Function prototypes */
extern char         *get_definition(char *, char *, char *, int),
                    *get_error_str(int),
#ifdef WITH_DUP_CHECK
                    *eval_dupcheck_options(char *, time_t *, unsigned int *),
#endif
                    *lock_proc(int, int),
                    *posi(char *, char *);
extern unsigned int get_checksum(char *, int),
                    get_str_checksum(char *);
extern int          assemble(char *, char *, int, char *, int, int *, off_t *),
                    attach_afd_status(int *),
#ifdef WITH_ERROR_QUEUE
                    attach_error_queue(void),
                    check_error_queue(unsigned int, int),
                    detach_error_queue(void),
                    print_error_queue(FILE *),
                    remove_from_error_queue(unsigned int,
                                            struct filetransfer_status *),
#endif
                    afw2wmo(char *, int *, char **, char *),
#ifdef _PRODUCTION_LOG
                    bin_file_chopper(char *, int *, off_t *, char,
                                     time_t, unsigned short,
                                     unsigned int, char *, char *),
#else
                    bin_file_chopper(char *, int *, off_t *, char),
#endif
                    bin_file_convert(char *, off_t, int),
                    bittest(unsigned char *, int),
                    check_afd_heartbeat(long, int),
                    check_create_path(char *, mode_t, char **, int),
                    check_dir(char *, int),
                    check_fra(int),
                    check_fsa(int),
                    check_lock(char *, char),
                    check_msa(void),
                    check_msg_name(char *),
                    coe_open(char *, int, ...),
                    convert_grib2wmo(char *, off_t *, char *),
                    copy_file(char *, char *, struct stat *),
                    create_message(unsigned int, char *, char *),
                    create_name(char *, signed char, time_t, unsigned int,
                                unsigned int *, int *, char *, int),
                    create_remote_dir(char *, char *),
                    detach_afd_status(void),
#ifndef HAVE_EACCESS
                    eaccess(char *, int),
#endif
                    eval_host_config(int *, char *, struct host_list **, int),
                    eval_timeout(int),
                    exec_cmd(char *, char **, int, char *, int, char *,
                             time_t, int),
                    extract(char *, char *,
#ifdef _PRODUCTION_LOG
                            time_t, unsigned short, unsigned int, char *,
#endif
                            int, int, int *, off_t *),
                    fra_attach(void),
                    fra_attach_passive(void),
                    fra_detach(void),
                    fsa_attach(void),
                    fsa_attach_passive(void),
                    fsa_detach(int),
                    get_afd_name(char *),
                    get_afd_path(int *, char **, char *),
                    get_arg(int *, char **, char *, char *, int),
                    get_arg_array(int *, char **, char *, char ***, int *),
                    get_dir_number(char *, unsigned int, long *),
                    get_dir_position(struct fileretrieve_status *, char *, int),
                    get_file_checksum(int, char *, int, int, unsigned int *),
                    get_host_position(struct filetransfer_status *,
                                      char *, int),
                    get_hostname(char *, char *),
                    get_mon_path(int *, char **, char *),
                    get_permissions(char **, char *),
                    get_pw(char *, char *),
                    get_rule(char *, int),
#ifdef WITH_DUP_CHECK
                    isdup(char *, unsigned int, time_t, int, int),
#endif
                    is_msgname(char *),
                    lock_file(char *, int),
#ifdef LOCK_DEBUG
                    lock_region(int, off_t, char *, int),
#else
                    lock_region(int, off_t),
#endif
                    make_fifo(char *),
                    move_file(char *, char *),
                    msa_attach(void),
                    msa_detach(void),
                    my_strncpy(char *, char *, size_t),
                    next_counter(int, int *),
                    open_counter_file(char *),
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    open_fifo_rw(char *, int *, int *),
#endif
                    pmatch(char *, char *, time_t *),
                    rec(int, char *, char *, ...),
                    rec_rmdir(char *),
#ifdef WITH_UNLINK_DELAY
                    remove_dir(char *, int),
#else
                    remove_dir(char *),
#endif
                    remove_files(char *, char *),
                    send_cmd(char, int),
                    wmo2ascii(char *, char *, off_t *);
extern off_t        dwdtiff2gts(char *, char *),
                    gts2tiff(char *, char *),
                    read_file(char *, char **),
                    tiff2gts(char *, char *);
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG)
extern pid_t        start_log(char *);
#endif
extern ssize_t      readn(int, void *, int);
extern time_t       calc_next_time(struct bd_time_entry *, time_t),
                    calc_next_time_array(int, struct bd_time_entry *, time_t),
                    datestr2unixtime(char *),
                    write_host_config(int, char *, struct host_list *);
#ifdef WITH_ERROR_QUEUE
extern void         add_to_error_queue(unsigned int,
                                       struct filetransfer_status *),
                    *attach_buf(char *, int *, size_t, char *, mode_t, int),
#else
extern void         *attach_buf(char *, int *, size_t, char *, mode_t, int),
#endif
                    bitset(unsigned char *, int),
                    change_alias_order(char **, int),
                    change_name(char *, char *, char *, char *,
                                int *, unsigned int),
                    check_fake_user(int *, char **, char *, char *),
                    check_permissions(void),
                    clr_fl(int, int),
                    count_files(char *, unsigned int *, off_t *),
                    daemon_init(char *),
                    delete_log_ptrs(struct delete_log *),
                    error_action(char *, char *),
                    get_dir_options(unsigned int, struct dir_options *),
                    get_log_number(int *, int, char *, int, char *),
                    get_max_log_number(int *, char *, int),
                    get_rename_rules(char *, int),
                    get_user(char *, char *),
                    inform_fd_about_fsa_change(void),
                    init_fifos_afd(void),
#ifdef LOCK_DEBUG
                    lock_region_w(int, off_t, char *, int),
#else
                    lock_region_w(int, off_t),
#endif
                    *map_file(char *, int *),
                    *mmap_resize(int, void *, size_t),
                    my_usleep(unsigned long),
#ifdef _PRODUCTION_LOG
                    production_log(time_t, unsigned short, unsigned int,
                                   char *, ...),
#endif
                    reshuffel_log_files(int, char *, char *, int, int),
#ifdef LOCK_DEBUG
                    rlock_region(int, off_t, char *, int),
#else
                    rlock_region(int, off_t),
#endif
                    t_hostname(char *, char *),
#ifdef WITH_SETUID_PROGS
                    set_afd_euid(char *),
#endif
                    set_fl(int, int),
                    shutdown_afd(char *),
                    system_log(char *, char *, int, char *, ...),
#ifdef LOCK_DEBUG
                    unlock_region(int, off_t, char *, int),
#else
                    unlock_region(int, off_t),
#endif
                    unmap_data(int, void **),
                    wmoheader_from_grib(char *, char *, char *);
#ifdef _FIFO_DEBUG
extern void         show_fifo_data(char, char *, char *, int, char *, int);
#endif
#ifndef HAVE_MMAP
extern caddr_t      mmap_emu(caddr_t, size_t, int, int, char *, off_t);
extern int          msync_emu(char *),
                    munmap_emu(char *);
#endif

#endif /* __afddefs_h */
