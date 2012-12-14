/*
 *  get_remote_file_names_http.c - Part of AFD, an automatic file distribution
 *                                 program.
 *  Copyright (c) 2006 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names_http - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_http(off_t *file_size_to_retrieve,
 **                                  int   *more_files_in_list)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.08.2006 H.Kiehl Created
 **   02.12.2009 H.Kiehl Added support for NOAA type HTML file listing.
 **   15.03.2011 H.Kiehl Added HTML list type listing.
 **
 */
DESCR__E_M3

/* #define DUMP_DIR_LIST_TO_DISK */

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <time.h>                  /* time(), mktime()                   */ 
#include <ctype.h>                 /* isdigit()                          */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef DUMP_DIR_LIST_TO_DISK
# include <fcntl.h>
#endif
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "httpdefs.h"
#include "fddefs.h"


#define STORE_HTML_STRING(html_str, max_str_length)                \
        {                                                          \
           int i = 0;                                              \
                                                                   \
           while ((*ptr != '<') && (*ptr != '\n') && (*ptr != '\r') &&\
                  (*ptr != '\0') && (i < ((max_str_length) - 1)))  \
           {                                                       \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*(ptr + 1))                            \
                    {                                              \
                       case 'a': (html_str)[i++] = 228;            \
                                 break;                            \
                       case 'A': (html_str)[i++] = 196;            \
                                 break;                            \
                       case 'o': (html_str)[i++] = 246;            \
                                 break;                            \
                       case 'O': (html_str)[i++] = 214;            \
                                 break;                            \
                       case 'u': (html_str)[i++] = 252;            \
                                 break;                            \
                       case 'U': (html_str)[i++] = 220;            \
                                 break;                            \
                       case 's': (html_str)[i++] = 223;            \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else                                              \
                 {                                                 \
                    while ((*ptr != ';') && (*ptr != '<') &&       \
                           (*ptr != '\n') && (*ptr != '\r') &&     \
                           (*ptr != '\0'))                         \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    if (*ptr != ';')                               \
                    {                                              \
                       break;                                      \
                    }                                              \
                 }                                                 \
              }                                                    \
              (html_str)[i] = *ptr;                                \
              i++; ptr++;                                          \
           }                                                       \
           (html_str)[i] = '\0';                                   \
        }
#define STORE_HTML_DATE()                                          \
        {                                                          \
           int i = 0,                                              \
               space_counter = 0;                                  \
                                                                   \
           while ((*ptr != '<') && (*ptr != '\n') && (*ptr != '\r') &&\
                  (*ptr != '\0') && (i < (MAX_FILENAME_LENGTH - 1)))\
           {                                                       \
              if (*ptr == ' ')                                     \
              {                                                    \
                 if (space_counter == 1)                           \
                 {                                                 \
                    while (*ptr == ' ')                            \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    break;                                         \
                 }                                                 \
                 space_counter++;                                  \
              }                                                    \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*(ptr + 1))                            \
                    {                                              \
                       case 'a': date_str[i++] = 228;              \
                                 break;                            \
                       case 'A': date_str[i++] = 196;              \
                                 break;                            \
                       case 'o': date_str[i++] = 246;              \
                                 break;                            \
                       case 'O': date_str[i++] = 214;              \
                                 break;                            \
                       case 'u': date_str[i++] = 252;              \
                                 break;                            \
                       case 'U': date_str[i++] = 220;              \
                                 break;                            \
                       case 's': date_str[i++] = 223;              \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else                                              \
                 {                                                 \
                    while ((*ptr != ';') && (*ptr != '<') &&       \
                           (*ptr != '\n') && (*ptr != '\r') &&     \
                           (*ptr != '\0'))                         \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    if (*ptr != ';')                               \
                    {                                              \
                       break;                                      \
                    }                                              \
                 }                                                 \
              }                                                    \
              date_str[i] = *ptr;                                  \
              i++; ptr++;                                          \
           }                                                       \
           date_str[i] = '\0';                                     \
        }

/* External global variables. */
extern int                        exitflag,
                                  *no_of_listed_files,
                                  rl_fd,
                                  timeout_flag;
extern char                       msg_str[],
                                  *p_work_dir;
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static int                        nfg;           /* Number of file mask. */
static time_t                     current_time;
static struct file_mask           *fml = NULL;

/* Local function prototypes. */
static int                        check_list(char *, time_t, off_t, off_t,
                                             int *, off_t *, int *),
                                  check_name(char *),
                                  eval_html_dir_list(char *, int *, off_t *,
                                                     int *);
static off_t                      convert_size(char *, off_t *);
#ifdef WITH_ATOM_FEED_SUPPORT
static time_t                     extract_feed_date(char *);
#endif


/*#################### get_remote_file_names_http() #####################*/
int
get_remote_file_names_http(off_t *file_size_to_retrieve,
                           int   *more_files_in_list)
{
   int files_to_retrieve = 0,
       i;

   *file_size_to_retrieve = 0;
   if ((*more_files_in_list == YES) ||
       (db.special_flag & DISTRIBUTED_HELPER_JOB) ||
       ((db.special_flag & OLD_ERROR_JOB) && (db.retries < 30)))
   {
      if (rl_fd == -1)
      {
         if (attach_ls_data() == INCORRECT)
         {
            http_quit();
            exit(INCORRECT);
         }
      }
      *more_files_in_list = NO;
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if ((rl[i].retrieved == NO) && (rl[i].assigned == 0))
         {
            if ((files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size))
            {
               /* Lock this file in list. */
#ifdef LOCK_DEBUG
               if (lock_region(rl_fd, (off_t)i, __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
               if (lock_region(rl_fd, (off_t)i) == LOCK_IS_NOT_SET)
#endif
               {
                  if ((rl[i].file_mtime == -1) || (rl[i].size == -1))
                  {
                     int status;

                     if ((status = http_head(db.hostname, db.target_dir,
                                             rl[i].file_name, &rl[i].size,
                                             &rl[i].file_mtime)) == SUCCESS)
                     {
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                        "Date for %s is %ld, size = %ld bytes.",
# else
                                        "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                        "Date for %s is %ld, size = %lld bytes.",
# else
                                        "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                        rl[i].file_name,
                                        (pri_time_t)rl[i].file_mtime, 
                                        (pri_off_t)rl[i].size);
                        }
                     }
                     else
                     {
                        trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                  __FILE__, __LINE__, NULL, msg_str,
                                  "Failed to get date and size of data %s (%d).",
                                  rl[i].file_name, status);
                        if (timeout_flag != OFF)
                        {
                           http_quit();
                           exit(DATE_ERROR);
                        }
                     }
                  }
                  if (rl[i].file_mtime == -1)
                  {
                     rl[i].got_date = NO;
                  }
                  else
                  {
                     rl[i].got_date = YES;
                  }

                  if ((fra[db.fra_pos].ignore_size == 0) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                       (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra[db.fra_pos].ignore_size > rl[i].size)))
                  {
                     if ((rl[i].got_date == NO) ||
                         (fra[db.fra_pos].ignore_file_time == 0))
                     {
                        files_to_retrieve++;
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve += rl[i].size;
                        }
                        rl[i].assigned = (unsigned char)db.job_no + 1;
                     }
                     else
                     {
                        time_t diff_time;

                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                             (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra[db.fra_pos].ignore_file_time > diff_time)))
                        {
                           files_to_retrieve++;
                           if (rl[i].size > 0)
                           {
                              *file_size_to_retrieve += rl[i].size;
                           }
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                     }
                  }
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)i, __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)i);
#endif
               }
            }
            else
            {
               *more_files_in_list = YES;
               break;
            }
         }
      }
   }
   else
   {
      int       j,
                status;
      time_t    now;
      struct tm *p_tm;

      /* Get all file masks for this directory. */
      if ((j = read_file_mask(fra[db.fra_pos].dir_alias, &nfg, &fml)) != SUCCESS)
      {
         if (j == LOCKFILE_NOT_THERE)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to set lock in file masks for %s, because the file is not there.",
                       fra[db.fra_pos].dir_alias);
         }
         else if (j == LOCK_IS_SET)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to get the file masks for %s, because lock is already set.",
                            fra[db.fra_pos].dir_alias);
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to get the file masks for %s. (%d)",
                            fra[db.fra_pos].dir_alias, j);
              }
         free(fml);
         http_quit();
         exit(INCORRECT);
      }

      if (fra[db.fra_pos].ignore_file_time != 0)
      {
         /* Note: FTP returns GMT so we need to convert this to GMT! */
         current_time = time(NULL);
         now = current_time;
         p_tm = gmtime(&current_time);
         current_time = mktime(p_tm);
      }
      else
      {
         now = 0;
      }

      /*
       * First determine if user wants to try and get a filename
       * listing. This can be done by setting the diretory option
       * 'do not get dir list' in DIR_CONFIG.
       */
      if ((fra[db.fra_pos].dir_flag & DONT_GET_DIR_LIST) == 0)
      {
         off_t bytes_buffered = 0,
               content_length = 0;
         char  *listbuffer = NULL;

         if (((status = http_get(db.hostname, db.target_dir, "",
                                 &content_length, 0)) != SUCCESS) &&
             (status != CHUNKED))
         {
            if (!((timeout_flag == ON) || (timeout_flag == CON_RESET) ||
                  (timeout_flag == CON_REFUSED)))
            {
               size_t new_size,
                      old_size;

               if (attach_ls_data() == INCORRECT)
               {
                  http_quit();
                  exit(INCORRECT);
               }
               new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
               old_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                           RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
               *no_of_listed_files = 0;

               if (old_size != new_size)
               {
                  char *ptr;

                  ptr = (char *)rl - AFD_WORD_OFFSET;
                  if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "mmap_resize() error : %s", strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
                  no_of_listed_files = (int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  rl = (struct retrieve_list *)ptr;
                  if (*no_of_listed_files < 0)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
                     *no_of_listed_files = 0;
                  }
               }
            }
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      "Failed to open remote directory %s (%d).",
                      db.target_dir, status);
            http_quit();
            exit(eval_timeout(OPEN_REMOTE_ERROR));
         }
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Opened HTTP connection for directory %s.",
                         db.target_dir);
         }

         if (status == SUCCESS)
         {
            int read_length;

            if (content_length > MAX_HTTP_DIR_BUFFER)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                          "Directory buffer length is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                          "Directory buffer length is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                          MAX_HTTP_DIR_BUFFER, (pri_off_t)content_length);
               http_quit();
               exit(ALLOC_ERROR);
            } else if (content_length == 0)
                   {
                      content_length = MAX_HTTP_DIR_BUFFER;
                   }

            if ((listbuffer = malloc(content_length + 1)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                          "Failed to malloc() %ld bytes : %s",
#else
                          "Failed to malloc() %lld bytes : %s",
#endif
                          (pri_off_t)(content_length + 1), strerror(errno));
               http_quit();
               exit(ALLOC_ERROR);
            }
            do
            {
               if ((content_length - (bytes_buffered + fsa->block_size)) >= 0)
               {
                  read_length = fsa->block_size;
               }
               else
               {
                  read_length = content_length - bytes_buffered;
               }
               if ((status = http_read(&listbuffer[bytes_buffered],
                                       read_length)) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to read from remote directory listing for %s",
                            db.target_dir);
                  free(listbuffer);
                  http_quit();
                  exit(eval_timeout(READ_REMOTE_ERROR));
               }
               else if (status > 0)
                    {
                       bytes_buffered += status;
                       if (bytes_buffered >= MAX_HTTP_DIR_BUFFER)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                     "Maximum directory buffer length (%ld bytes) reached.",
#else
                                     "Maximum directory buffer length (%lld bytes) reached.",
#endif
                                     (pri_off_t)content_length);
                          status = 0;
                       }
                    }
            } while (status != 0);
         }
         else /* status == CHUNKED */
         {
            int  chunksize;
            char *chunkbuffer = NULL;

            chunksize = fsa->block_size + 4;
            if ((chunkbuffer = malloc(chunksize)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          chunksize, strerror(errno));
               http_quit();
               exit(ALLOC_ERROR);
            }
            do
            {
               if ((status = http_chunk_read(&chunkbuffer,
                                             &chunksize)) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to read from remote directory listing for %s",
                            db.target_dir);
                  free(chunkbuffer);
                  http_quit();
                  exit(eval_timeout(READ_REMOTE_ERROR));
               }
               else if (status > 0)
                    {
                       if (listbuffer == NULL)
                       {
                          if ((listbuffer = malloc(status)) == NULL)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to malloc() %d bytes : %s",
                                        status, strerror(errno));
                             free(chunkbuffer);
                             http_quit();
                             exit(ALLOC_ERROR);
                          }
                       }
                       else
                       {
                          if (bytes_buffered > MAX_HTTP_DIR_BUFFER)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                        "Directory length buffer is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                                        "Directory length buffer is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                                        MAX_HTTP_DIR_BUFFER,
                                        (pri_off_t)content_length);
                             http_quit();
                             free(listbuffer);
                             free(chunkbuffer);
                             exit(ALLOC_ERROR);
                          }
                          if ((listbuffer = realloc(listbuffer,
                                                    bytes_buffered + status)) == NULL)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                        "Failed to realloc() %ld bytes : %s",
#else
                                        "Failed to realloc() %lld bytes : %s",
#endif
                                        (pri_off_t)(bytes_buffered + status),
                                        strerror(errno));
                             free(chunkbuffer);
                             http_quit();
                             exit(ALLOC_ERROR);
                          }
                       }
                       (void)memcpy(&listbuffer[bytes_buffered], chunkbuffer,
                                    status);
                       bytes_buffered += status;
                    }
            } while (status != HTTP_LAST_CHUNK);

            if ((listbuffer = realloc(listbuffer, bytes_buffered + 1)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                          "Failed to realloc() %ld bytes : %s",
#else
                          "Failed to realloc() %lld bytes : %s",
#endif
                          (pri_off_t)(bytes_buffered + 1), strerror(errno));
               free(chunkbuffer);
               http_quit();
               exit(ALLOC_ERROR);
            }

            free(chunkbuffer);
         }

         if (bytes_buffered > 0)
         {
#ifdef DUMP_DIR_LIST_TO_DISK
            int fd;

            if ((fd = open("http_list.dump", (O_WRONLY | O_CREAT | O_TRUNC),
                           FILE_MODE)) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to open() `http_list.dump' : %s",
                          strerror(errno));
            }
            else
            {
               if (write(fd, listbuffer, bytes_buffered) != bytes_buffered)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to write() to `http_list.dump' : %s",
                             strerror(errno));
               }
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() `http_list.dump' : %s",
                             strerror(errno));
               }
            }
#else
            listbuffer[bytes_buffered] = '\0';
#endif
            if (eval_html_dir_list(listbuffer, &files_to_retrieve,
                                   file_size_to_retrieve,
                                   more_files_in_list) == INCORRECT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to evaluate HTML directory listing.");
            }
         }
         free(listbuffer);
      }
      else /* Just copy the file mask list. */
      {
         char *p_mask,
              tmp_mask[MAX_FILENAME_LENGTH];

         if (now == 0)
         {
            now = time(NULL);
         }

         for (i = 0; i < nfg; i++)
         {
            p_mask = fml[i].file_list;
            for (j = 0; j < fml[i].fc; j++)
            {
               /*
                * We cannot just take the mask as is. We must check if we
                * need to expand the mask and then use the expansion.
                */
               expand_filter(p_mask, tmp_mask, now);
               if (check_list(tmp_mask, -1, 0, -1, &files_to_retrieve,
                              file_size_to_retrieve, more_files_in_list) == 0)
               {
                  files_to_retrieve++;
               }
               NEXT(p_mask);
            }
         }
      }

      /* Free file mask list. */
      for (i = 0; i < nfg; i++)
      {
         free(fml[i].file_list);
      }
      free(fml);

      /*
       * Remove all files from the remote_list structure that are not
       * in the current directory listing.
       */
      if ((files_to_retrieve > 0) && (fra[db.fra_pos].stupid_mode != YES) &&
          (fra[db.fra_pos].remove == NO))
      {
         int    files_removed = 0,
                i;
         size_t move_size;

         for (i = 0; i < (*no_of_listed_files - files_removed); i++)
         {
            if (rl[i].in_list == NO)
            {
               int j = i;

               while ((rl[j].in_list == NO) &&
                      (j < (*no_of_listed_files - files_removed)))
               {
                  j++;
               }
               if (j != (*no_of_listed_files - files_removed))
               {
                  move_size = (*no_of_listed_files - files_removed - j) *
                              sizeof(struct retrieve_list);
                  (void)memmove(&rl[i], &rl[j], move_size);
               }
               files_removed += (j - i);
            }
         }

         if (files_removed > 0)
         {
            int    current_no_of_listed_files = *no_of_listed_files;
            size_t new_size,
                   old_size;

            *no_of_listed_files -= files_removed;
            if (*no_of_listed_files < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
               *no_of_listed_files = 0;
            }
            if (*no_of_listed_files == 0)
            {
               new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
            }
            else
            {
               new_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                           RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
            }
            old_size = (((current_no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                        RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;

            if (old_size != new_size)
            {
               char *ptr;

               ptr = (char *)rl - AFD_WORD_OFFSET;
               if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap_resize() error : %s", strerror(errno));
                  http_quit();
                  exit(INCORRECT);
               }
               no_of_listed_files = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               rl = (struct retrieve_list *)ptr;
               if (*no_of_listed_files < 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
                  *no_of_listed_files = 0;
               }
            }
         }
      }
   }

   return(files_to_retrieve);
}


/*++++++++++++++++++++++++ eval_html_dir_list() +++++++++++++++++++++++++*/
static int
eval_html_dir_list(char  *html_buffer,
                   int   *files_to_retrieve,
                   off_t *file_size_to_retrieve,
                   int   *more_files_in_list)
{
   char *ptr;

#ifdef WITH_ATOM_FEED_SUPPORT
   /*
    * First test if we have an atom or rss feed. If that is not the case
    * lets test for a directory listing.
    */
   if ((html_buffer[0] == '<') && (html_buffer[1] == '?') &&
       (html_buffer[2] == 'x') && (html_buffer[3] == 'm') &&
       (html_buffer[4] == 'l') && (html_buffer[5] == ' '))
   {
      char *tmp_ptr;

      /* For now we just implement atom feeds. */
      if ((ptr = lposi(&html_buffer[6], "<entry>", 7)) == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Unknown feed type. Terminating.");
         return(INCORRECT);
      }
      tmp_ptr = ptr;
      if ((ptr = lposi(&html_buffer[6], "<updated>", 7)) == NULL)
      {
      }
      if (ptr < tmp_ptr)
      {
         feed_time = extract_feed_date(ptr);
      }
   }
   else
   {
#endif
      if ((ptr = lposi(html_buffer, "<h1>", 4)) == NULL)
      {
         if ((ptr = lposi(html_buffer, "<PRE>", 5)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
            return(INCORRECT);
         }
         else
         {
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            if ((*ptr == '<') && (*(ptr + 1) == 'H') && (*(ptr + 2) == 'R'))
            {
               time_t file_mtime;
               off_t  exact_size,
                      file_size;
               char   date_str[MAX_FILENAME_LENGTH],
                      file_name[MAX_FILENAME_LENGTH],
                      size_str[MAX_FILENAME_LENGTH];

               /* Ignore HR line. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               /* Ignore the two directory lines. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               while (*ptr == '<')
               {
                  while (*ptr == '<')
                  {
                     ptr++;
                     while ((*ptr != '>') && (*ptr != '\n') &&
                            (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == '>')
                     {
                        ptr++;
                        while (*ptr == ' ')
                        {
                           ptr++;
                        }
                     }
                  }

                  if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     /* Store file name. */
                     STORE_HTML_STRING(file_name, MAX_FILENAME_LENGTH);

                     if (check_name(file_name) == YES)
                     {
                        if (*ptr == '<')
                        {
                           while (*ptr == '<')
                           {
                              ptr++;
                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                                 while (*ptr == ' ')
                                 {
                                    ptr++;
                                 }
                              }
                           }
                        }
                        if ((*ptr != '\n') && (*ptr != '\r') &&
                            (*ptr != '\0'))
                        {
                           while (*ptr == ' ')
                           {
                              ptr++;
                           }

                           /* Store date string. */
                           STORE_HTML_DATE();
                           file_mtime = datestr2unixtime(date_str);

                           if (*ptr == '<')
                           {
                              while (*ptr == '<')
                              {
                                 ptr++;
                                 while ((*ptr != '>') && (*ptr != '\n') &&
                                        (*ptr != '\r') && (*ptr != '\0'))
                                 {
                                    ptr++;
                                 }
                                 if (*ptr == '>')
                                 {
                                    ptr++;
                                    while (*ptr == ' ')
                                    {
                                       ptr++;
                                    }
                                 }
                              }
                           }
                           if ((*ptr != '\n') && (*ptr != '\r') &&
                               (*ptr != '\0'))
                           {
                              /* Store size string. */
                              STORE_HTML_STRING(size_str,
                                                MAX_FILENAME_LENGTH);
                              exact_size = convert_size(size_str,
                                                        &file_size);
                           }
                           else
                           {
                              exact_size = -1;
                              file_size = -1;
                           }
                        }
                        else
                        {
                           file_mtime = -1;
                           exact_size = -1;
                           file_size = -1;
                        }
                     }
                     else
                     {
                        file_name[0] = '\0';
                     }
                  }
                  else
                  {
                     file_name[0] = '\0';
                     file_mtime = -1;
                     exact_size = -1;
                     file_size = -1;
                     break;
                  }

                  if (file_name[0] != '\0')
                  {
                     if (check_list(file_name, file_mtime, exact_size,
                                    file_size, files_to_retrieve,
                                    file_size_to_retrieve,
                                    more_files_in_list) == 0)
                     {
                        (*files_to_retrieve)++;
                     }
                  }

                  /* Go to end of line. */
                  while ((*ptr != '\n') && (*ptr != '\r') &&
                         (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
               return(INCORRECT);
            }
         }
      }
      else
      {
         time_t file_mtime;
         off_t  exact_size,
                file_size;
         char   date_str[MAX_FILENAME_LENGTH],
                file_name[MAX_FILENAME_LENGTH],
                size_str[MAX_FILENAME_LENGTH];

         while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
         {
            ptr++;
         }
         while ((*ptr == '\n') || (*ptr == '\r'))
         {
            ptr++;
         }
         if (*ptr == '<')
         {
            /* Table type listing. */
            if ((*(ptr + 1) == 't') && (*(ptr + 6) == '>'))
            {
               /* Ignore the two heading lines. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               if ((*ptr == '<') && (*(ptr + 1) == 't') &&
                   (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                   (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                   (*(ptr + 6) == 'd'))
               {
                  /* Read line by line. */
                  do
                  {
                     ptr += 6;
                     while ((*ptr != '>') &&
                            (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == '>')
                     {
                        ptr++;
                        while (*ptr == '<')
                        {
                           ptr++;
                           while ((*ptr != '>') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '>')
                           {
                              ptr++;
                           }
                        }
                        if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                        {
                           /* Store file name. */
                           STORE_HTML_STRING(file_name, MAX_FILENAME_LENGTH);

                           if (check_name(file_name) == YES)
                           {
                              while (*ptr == '<')
                              {
                                 ptr++;
                                 while ((*ptr != '>') && (*ptr != '\n') &&
                                        (*ptr != '\r') && (*ptr != '\0'))
                                 {
                                    ptr++;
                                 }
                                 if (*ptr == '>')
                                 {
                                    ptr++;
                                 }
                              }
                              if ((*ptr != '\n') && (*ptr != '\r') &&
                                  (*ptr != '\0'))
                              {
                                 while (*ptr == ' ')
                                 {
                                    ptr++;
                                 }

                                 /* Store date string. */
                                 STORE_HTML_STRING(date_str,
                                                   MAX_FILENAME_LENGTH);
                                 file_mtime = datestr2unixtime(date_str);

                                 while (*ptr == '<')
                                 {
                                    ptr++;
                                    while ((*ptr != '>') && (*ptr != '\n') &&
                                           (*ptr != '\r') && (*ptr != '\0'))
                                    {
                                       ptr++;
                                    }
                                    if (*ptr == '>')
                                    {
                                       ptr++;
                                    }
                                 }
                                 if ((*ptr != '\n') && (*ptr != '\r') &&
                                     (*ptr != '\0'))
                                 {
                                    /* Store size string. */
                                    STORE_HTML_STRING(size_str,
                                                      MAX_FILENAME_LENGTH);
                                    exact_size = convert_size(size_str,
                                                              &file_size);
                                 }
                                 else
                                 {
                                    exact_size = -1;
                                    file_size = -1;
                                 }
                              }
                              else
                              {
                                 file_mtime = -1;
                                 exact_size = -1;
                                 file_size = -1;
                              }
                           }
                           else
                           {
                              file_name[0] = '\0';
                           }
                        }
                        else
                        {
                           file_name[0] = '\0';
                           file_mtime = -1;
                           exact_size = -1;
                           file_size = -1;
                        }
                     }

                     if (file_name[0] != '\0')
                     {
                        if (check_list(file_name, file_mtime, exact_size,
                                       file_size, files_to_retrieve,
                                       file_size_to_retrieve,
                                       more_files_in_list) == 0)
                        {
                           (*files_to_retrieve)++;
                        }
                     }

                     /* Go to end of line. */
                     while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     while ((*ptr == '\n') || (*ptr == '\r'))
                     {
                        ptr++;
                     }
                  } while ((*ptr == '<') && (*(ptr + 1) == 't') &&
                           (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                           (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                           (*(ptr + 6) == 'd'));
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                  return(INCORRECT);
               }
            }
                 /* Pre type listing. */
            else if ((*(ptr + 1) == 'p') && (*(ptr + 4) == '>'))
                 {
                    /* Ignore heading line. */
                    while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }

                    while (*ptr == '<')
                    {
                       while (*ptr == '<')
                       {
                          ptr++;
                          while ((*ptr != '>') && (*ptr != '\n') &&
                                 (*ptr != '\r') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                          if (*ptr == '>')
                          {
                             ptr++;
                             while (*ptr == ' ')
                             {
                                ptr++;
                             }
                          }
                       }

                       if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                       {
                          /* Store file name. */
                          STORE_HTML_STRING(file_name, MAX_FILENAME_LENGTH);

                          if (check_name(file_name) == YES)
                          {
                             if (*ptr == '<')
                             {
                                while (*ptr == '<')
                                {
                                   ptr++;
                                   while ((*ptr != '>') && (*ptr != '\n') &&
                                          (*ptr != '\r') && (*ptr != '\0'))
                                   {
                                      ptr++;
                                   }
                                   if (*ptr == '>')
                                   {
                                      ptr++;
                                      while (*ptr == ' ')
                                      {
                                         ptr++;
                                      }
                                   }
                                }
                             }
                             if ((*ptr != '\n') && (*ptr != '\r') &&
                                 (*ptr != '\0'))
                             {
                                while (*ptr == ' ')
                                {
                                   ptr++;
                                }

                                /* Store date string. */
                                STORE_HTML_DATE();
                                file_mtime = datestr2unixtime(date_str);

                                if (*ptr == '<')
                                {
                                   while (*ptr == '<')
                                   {
                                      ptr++;
                                      while ((*ptr != '>') && (*ptr != '\n') &&
                                             (*ptr != '\r') && (*ptr != '\0'))
                                      {
                                         ptr++;
                                      }
                                      if (*ptr == '>')
                                      {
                                         ptr++;
                                         while (*ptr == ' ')
                                         {
                                            ptr++;
                                         }
                                      }
                                   }
                                }
                                if ((*ptr != '\n') && (*ptr != '\r') &&
                                    (*ptr != '\0'))
                                {
                                   /* Store size string. */
                                   STORE_HTML_STRING(size_str,
                                                     MAX_FILENAME_LENGTH);
                                   exact_size = convert_size(size_str,
                                                             &file_size);
                                }
                                else
                                {
                                   exact_size = -1;
                                   file_size = -1;
                                }
                             }
                             else
                             {
                                file_mtime = -1;
                                exact_size = -1;
                                file_size = -1;
                             }
                          }
                          else
                          {
                             file_name[0] = '\0';
                          }
                       }
                       else
                       {
                          file_name[0] = '\0';
                          file_mtime = -1;
                          exact_size = -1;
                          file_size = -1;
                          break;
                       }

                       if (file_name[0] != '\0')
                       {
                          if (check_list(file_name, file_mtime, exact_size,
                                         file_size, files_to_retrieve,
                                         file_size_to_retrieve,
                                         more_files_in_list) == 0)
                          {
                             (*files_to_retrieve)++;
                          }
                       }

                       /* Go to end of line. */
                       while ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       while ((*ptr == '\n') || (*ptr == '\r'))
                       {
                          ptr++;
                       }
                    }
                 }
                 /* List type listing. */
            else if ((*(ptr + 1) == 'u') && (*(ptr + 3) == '>'))
                 {
                    /* Ignore first line. */
                    while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }

                    while (*ptr == '<')
                    {
                       while (*ptr == '<')
                       {
                          ptr++;
                          while ((*ptr != '>') && (*ptr != '\n') &&
                                 (*ptr != '\r') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                          if (*ptr == '>')
                          {
                             ptr++;
                             while (*ptr == ' ')
                             {
                                ptr++;
                             }
                          }
                       }

                       if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                       {
                          /* Store file name. */
                          STORE_HTML_STRING(file_name, MAX_FILENAME_LENGTH);

                          if (check_name(file_name) == YES)
                          {
                             file_mtime = -1;
                             exact_size = -1;
                             file_size = -1;
                          }
                          else
                          {
                             file_name[0] = '\0';
                          }
                       }
                       else
                       {
                          file_name[0] = '\0';
                          file_mtime = -1;
                          exact_size = -1;
                          file_size = -1;
                          break;
                       }

                       if (file_name[0] != '\0')
                       {
                          if (check_list(file_name, file_mtime, exact_size,
                                         file_size, files_to_retrieve,
                                         file_size_to_retrieve,
                                         more_files_in_list) == 0)
                          {
                             (*files_to_retrieve)++;
                          }
                       }

                       /* Go to end of line. */
                       while ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       while ((*ptr == '\n') || (*ptr == '\r'))
                       {
                          ptr++;
                       }
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                    return(INCORRECT);
                 }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
            return(INCORRECT);
         }
      }
#ifdef WITH_ATOM_FEED_SUPPORT
   }
#endif

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char   *file,
           time_t file_mtime,
           off_t  exact_size,
           off_t  file_size,
           int    *files_to_retrieve,
           off_t  *file_size_to_retrieve,
           int    *more_files_in_list)
{
   int i;

   if (rl_fd == -1)
   {
      if (attach_ls_data() == INCORRECT)
      {
         http_quit();
         exit(INCORRECT);
      }
   }

   if (strlen(file) >= (MAX_FILENAME_LENGTH - 1))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "Remote file name `%s' is to long, it may only be %d bytes long.",
                file, (MAX_FILENAME_LENGTH - 1));
      return(1);
   }

   if ((fra[db.fra_pos].stupid_mode == YES) ||
       (fra[db.fra_pos].remove == YES))
   {
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            rl[i].in_list = YES;
            if ((rl[i].retrieved == NO) && (rl[i].assigned == 0) &&
                (((db.special_flag & OLD_ERROR_JOB) == 0) ||
#ifdef LOCK_DEBUG
                   (lock_region(rl_fd, (off_t)i, __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
                   (lock_region(rl_fd, (off_t)i) == LOCK_IS_NOT_SET)
#endif
                  ))
            {
               int ret;

               if ((file_mtime == -1) &&
                   (fra[db.fra_pos].ignore_file_time != 0))
               {
                  int status;

                  if (((fra[db.fra_pos].dir_flag & DONT_GET_DIR_LIST) == 0) &&
                      ((status = http_head(db.hostname, db.target_dir, file,
                                           &file_size, &file_mtime)) == SUCCESS))
                  {
                     exact_size = 1;
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %ld bytes.",
# else
                                     "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %lld bytes.",
# else
                                     "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                     file, (pri_time_t)file_mtime,
                                     (pri_off_t)file_size);
                     }
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               "Failed to get date and size of file %s (%d).",
                               file, status);
                     if (timeout_flag != OFF)
                     {
                        http_quit();
                        exit(DATE_ERROR);
                     }
                  }
               }
               rl[*no_of_listed_files].size = file_size;
               rl[*no_of_listed_files].file_mtime = file_mtime;
               if (file_mtime == -1)
               {
                  rl[*no_of_listed_files].got_date = NO;
               }
               else
               {
                  rl[*no_of_listed_files].got_date = YES;
               }

               if ((fra[db.fra_pos].ignore_size == 0) ||
                   ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                    (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                   ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                    (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                   ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                    (fra[db.fra_pos].ignore_size > rl[i].size)))
               {
                  if (fra[db.fra_pos].ignore_file_time == 0)
                  {
                     if (rl[i].size > 0)
                     {
                        *file_size_to_retrieve += rl[i].size;
                     }
                     if ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                         (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size))
                     {
                        rl[i].assigned = (unsigned char)db.job_no + 1;
                     }
                     else
                     {
                        rl[i].assigned = 0;
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve -= rl[i].size;
                        }
                        *more_files_in_list = YES;
                     }
                     ret = 0;
                  }
                  else
                  {
                     time_t diff_time;

                     diff_time = current_time - rl[i].file_mtime;
                     if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                          (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                         ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                          (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                         ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                          (fra[db.fra_pos].ignore_file_time > diff_time)))
                     {
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve += rl[i].size;
                        }
                        if ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                            (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size))
                        {
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           if (rl[i].size > 0)
                           {
                              *file_size_to_retrieve -= rl[i].size;
                           }
                           *more_files_in_list = YES;
                        }
                        ret = 0;
                     }
                     else
                     {
                        ret = 1;
                     }
                  }
               }
               else
               {
                  ret = 1;
               }
               if (db.special_flag & OLD_ERROR_JOB)
               {
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)i, __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)i);
#endif
               }
               return(ret);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < *no_of_listed_files; i++) */
   }
   else
   {
      /* Check if this file is in the list. */
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            rl[i].in_list = YES;
            if ((fra[db.fra_pos].stupid_mode == GET_ONCE_ONLY) &&
                (rl[i].retrieved == YES))
            {
               return(1);
            }

            if (((db.special_flag & OLD_ERROR_JOB) == 0) ||
#ifdef LOCK_DEBUG
                (lock_region(rl_fd, (off_t)i, __FILE__, __LINE__) == LOCK_IS_NOT_SET))
#else
                (lock_region(rl_fd, (off_t)i) == LOCK_IS_NOT_SET))
#endif
            {
               int status;

               /* Try to get remote date and size. */
               if (((fra[db.fra_pos].dir_flag & DONT_GET_DIR_LIST) == 0) &&
                   ((file_mtime == -1) || (file_size == -1) || (exact_size != 1)))
               {
                  if ((status = http_head(db.hostname, db.target_dir, file,
                                          &file_size, &file_mtime)) == SUCCESS)
                  {
                     exact_size = 1;
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %ld bytes.",
# else
                                     "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %lld bytes.",
# else
                                     "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                     file, (pri_time_t)file_mtime, 
                                     (pri_off_t)file_size);
                     }
                  }
                  else
                  {
                     trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                               __FILE__, __LINE__, NULL, msg_str,
                               "Failed to get date and size of file %s (%d).",
                               file, status);
                     if (timeout_flag != OFF)
                     {
                        http_quit();
                        exit(DATE_ERROR);
                     }
                  }
               }
               if (file_mtime == -1)
               {
                  rl[i].got_date = NO;
                  rl[i].retrieved = NO;
                  rl[i].assigned = 0;
                  rl[i].file_mtime = file_mtime;
               }
               else
               {
                  rl[i].got_date = YES;
                  if (rl[i].file_mtime != file_mtime)
                  {
                     rl[i].file_mtime = file_mtime;
                     rl[i].retrieved = NO;
                     rl[i].assigned = 0;
                  }
               }
               if (file_size == -1)
               {
                  rl[i].size = file_size;
                  rl[i].retrieved = NO;
                  rl[i].assigned = 0;
               }
               else
               {
                  if (rl[i].size != file_size)
                  {
                     rl[i].size = file_size;
                     rl[i].retrieved = NO;
                     rl[i].assigned = 0;
                  }
               }

               if (rl[i].retrieved == NO)
               {
                  if ((fra[db.fra_pos].ignore_size == 0) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                       (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra[db.fra_pos].ignore_size > rl[i].size)))
                  {
                     if ((rl[i].got_date == NO) ||
                         (fra[db.fra_pos].ignore_file_time == 0))
                     {
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve += rl[i].size;
                        }
                        if ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                            (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size))
                        {
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           if (rl[i].size > 0)
                           {
                              *file_size_to_retrieve -= rl[i].size;
                           }
                           *more_files_in_list = YES;
                        }
                        status = 0;
                     }
                     else
                     {
                        time_t diff_time;

                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                             (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra[db.fra_pos].ignore_file_time > diff_time)))
                        {
                           if (rl[i].size > 0)
                           {
                              *file_size_to_retrieve += rl[i].size;
                           }
                           if ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                               (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size))
                           {
                              rl[i].assigned = (unsigned char)db.job_no + 1;
                           }
                           else
                           {
                              rl[i].assigned = 0;
                              if (rl[i].size > 0)
                              {
                                 *file_size_to_retrieve -= rl[i].size;
                              }
                              *more_files_in_list = YES;
                           }
                           status = 0;
                        }
                        else
                        {
                           status = 1;
                        }
                     }
                  }
                  else
                  {
                     status = 1;
                  }
               }
               else
               {
                  status = 1;
               }
               if (db.special_flag & OLD_ERROR_JOB)
               {
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)i, __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)i);
#endif
               }
               return(status);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < *no_of_listed_files; i++) */
   }

   /* Add this file to the list. */
   if ((*no_of_listed_files != 0) &&
       ((*no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                         RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                         AFD_WORD_OFFSET;

      ptr = (char *)rl - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         http_quit();
         exit(INCORRECT);
      }
      no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      rl = (struct retrieve_list *)ptr;
      if (*no_of_listed_files < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
         *no_of_listed_files = 0;
      }
   }
   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   rl[*no_of_listed_files].retrieved = NO;
   rl[*no_of_listed_files].in_list = YES;

   if (((fra[db.fra_pos].dir_flag & DONT_GET_DIR_LIST) == 0) &&
       ((file_mtime == -1) || (file_size == -1) || (exact_size != 1)))
   {
      int status;

      if ((status = http_head(db.hostname, db.target_dir, file,
                              &file_size, &file_mtime)) == SUCCESS)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                         "Date for %s is %ld, size = %ld bytes.",
# else
                         "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                         "Date for %s is %ld, size = %lld bytes.",
# else
                         "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                         file, file_mtime, file_size);
         }
      }
      else
      {
         trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                   __FILE__, __LINE__, NULL, msg_str,
                   "Failed to get date and size of file %s (%d).",
                   file, status);
         if (timeout_flag != OFF)
         {
            http_quit();
            exit(DATE_ERROR);
         }
      }
   }
   rl[*no_of_listed_files].file_mtime = file_mtime;
   rl[*no_of_listed_files].size = file_size;
   if (file_mtime == -1)
   {
      rl[*no_of_listed_files].got_date = NO;
   }
   else
   {
      rl[*no_of_listed_files].got_date = YES;
   }

   if ((fra[db.fra_pos].ignore_size == 0) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
        (fra[db.fra_pos].ignore_size == rl[*no_of_listed_files].size)) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
        (fra[db.fra_pos].ignore_size < rl[*no_of_listed_files].size)) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
        (fra[db.fra_pos].ignore_size > rl[*no_of_listed_files].size)))
   {
      if ((rl[*no_of_listed_files].got_date == NO) ||
          (fra[db.fra_pos].ignore_file_time == 0))
      {
         if (file_size > 0)
         {
            *file_size_to_retrieve += file_size;
         }
         (*no_of_listed_files)++;
      }
      else
      {
         time_t diff_time;

         diff_time = current_time - rl[*no_of_listed_files].file_mtime;
         if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
              (fra[db.fra_pos].ignore_file_time == diff_time)) ||
             ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
              (fra[db.fra_pos].ignore_file_time < diff_time)) ||
             ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
              (fra[db.fra_pos].ignore_file_time > diff_time)))
         {
            if (file_size > 0)
            {
               *file_size_to_retrieve += file_size;
            }
            (*no_of_listed_files)++;
         }
         else
         {
            return(1);
         }
      }
      if ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
          (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size))
      {
         rl[(*no_of_listed_files) - 1].assigned = (unsigned char)db.job_no + 1;
      }
      else
      {
         rl[(*no_of_listed_files) - 1].assigned = 0;
         if (rl[(*no_of_listed_files) - 1].size > 0)
         {
            *file_size_to_retrieve -= rl[(*no_of_listed_files) - 1].size;
         }
         *more_files_in_list = YES;
      }
      return(0);
   }
   else
   {
      return(1);
   }
}


#ifdef WITH_ATOM_FEED_SUPPORT
/*+++++++++++++++++++++++++ extract_feed_date() +++++++++++++++++++++++++*/
static time_t
extract_feed_date(char *time_str)
{
   if ((isdigit(time_str[0])) && (isdigit(time_str[1])) &&
       (isdigit(time_str[2])) && (isdigit(time_str[3])))
   {
      char      tmp_str[5];
      struct tm bd_time;

      /* Evaluate year. */
      tmp_str[0] = time_str[0];
      tmp_str[1] = time_str[1];
      tmp_str[2] = time_str[2];
      tmp_str[3] = time_str[3];
      tmp_str[4] = '\0';
      bd_time.tm_year = atoi(tmp_str) - 1900;

      if ((time_str[4] == '-') && (isdigit(time_str[5])) &&
          (isdigit(time_str[6])))
      {
         /* Get month. */
         tmp_str[0] = time_str[5];
         tmp_str[1] = time_str[6];
         tmp_str[2] = '\0';
         bd_time.tm_month = atoi(tmp_str) - 1;

         if ((time_str[7] == '-') && (isdigit(time_str[8])) &&
             (isdigit(time_str[9])))
         {
            /* Get day of month. */
            tmp_str[0] = time_str[8];
            tmp_str[1] = time_str[9];
            bd_time.tm_day = atoi(tmp_str);

            if ((time_str[10] == 'T') && (isdigit(time_str[11])) &&
                (isdigit(time_str[12])))
            {
               /* Get hour. */
               tmp_str[0] = time_str[11];
               tmp_str[1] = time_str[12];
               bd_time.tm_hour = atoi(tmp_str);

               if ((time_str[13] == ':') && (isdigit(time_str[14])) &&
                   (isdigit(time_str[15])))
               {
                  /* Get minute. */
                  tmp_str[0] = time_str[14];
                  tmp_str[1] = time_str[15];
                  bd_time.tm_min = atoi(tmp_str);

                  if ((time_str[16] == ':') && (isdigit(time_str[17])) &&
                      (isdigit(time_str[18])))
                  {
                     int pos = 19,
                         timezone_offset;

                     /* Get seconds. */
                     tmp_str[0] = time_str[17];
                     tmp_str[1] = time_str[18];
                     bd_time.tm_sec = atoi(tmp_str);

                     /* We only do full seconds, so ignore fractional part. */
                     if (time_str[pos] == '.')
                     {
                        pos++;
                        while (isdgit(time_str[pos]))
                        {
                           pos++;
                        }
                     }

                     if (((time_str[pos] == '+') || (time_str[pos] == '-')) &&
                         (isdigit(time_str[pos + 1])) &&
                         (isdigit(time_str[pos + 2])) &&
                         (time_str[pos + 3] == ':') &&
                         (isdigit(time_str[pos + 4])) &&
                         (isdigit(time_str[pos + 5])))
                     {
                        tmp_str[0] = time_str[pos + 1];
                        tmp_str[1] = time_str[pos + 2];
                        timezone_offset = atoi(tmp_str) * 3600;

                        tmp_str[0] = time_str[pos + 4];
                        tmp_str[1] = time_str[pos + 5];
                        timezone_offset += (atoi(tmp_str) * 60);
                        if (time_str[pos] == '-')
                        {
                           timezone_offset = -timezone_offset;
                        }
                     }
                     else if (time_str[pos] == 'Z')
                          {
                             timezone_offset = 0;
                          }
                     bd_time.tm_isdst = 0;

                     return(mktime(&bd_time) + timezone_offset);
                  }
               }
            }
         }
      }
   }

   return(0);
}
#endif


/*---------------------------- check_name() -----------------------------*/
static int
check_name(char *file_name)
{
   int  gotcha = NO;
   char *p_mask;

   if ((file_name[0] != '.') ||
       (fra[db.fra_pos].dir_flag & ACCEPT_DOT_FILES))
   {
      int i,
          j,
          status;

      for (i = 0; i < nfg; i++)
      {
         p_mask = fml[i].file_list;
         for (j = 0; j < fml[i].fc; j++)
         {
            if ((status = pmatch(p_mask, file_name, NULL)) == 0)
            {
               gotcha = YES;
               break;
            }
            else if (status == 1)
                 {
                    /* This file is definitly NOT wanted! */
                    /* Lets skip the rest of this group.  */
                    break;
                 }
            NEXT(p_mask);
         }
         if (gotcha == YES)
         {
            break;
         }
      }
   }

   return(gotcha);
}


/*------------------------- convert_size() ------------------------------*/
static off_t
convert_size(char *size_str, off_t *size)
{
   off_t exact_size;
   char  *ptr,
         *ptr_start;

   ptr = size_str;
   while (*ptr == ' ')
   {
      ptr++;
   }
   ptr_start = ptr;

   while (isdigit((int)*ptr))
   {
      ptr++;
   }
   if (*ptr == '.')
   {
      ptr++;
      while (isdigit((int)*ptr))
      {
         ptr++;
      }
   }
   if (ptr != ptr_start)
   {
      switch (*ptr)
      {
         case 'K': /* Kilobytes. */
            exact_size = KILOBYTE;
            break;
         case 'M': /* Megabytes. */
            exact_size = MEGABYTE;
            break;
         case 'G': /* Gigabytes. */
            exact_size = GIGABYTE;
            break;
         case 'T': /* Terabytes. */
            exact_size = TERABYTE;
            break;
         case 'P': /* Petabytes. */
            exact_size = PETABYTE;
            break;
         case 'E': /* Exabytes. */
            exact_size = EXABYTE;
            break;
         default :
            exact_size = 1;
            break;
      }
      *size = (off_t)(strtod(ptr_start, (char **)NULL) * exact_size);
   }
   else
   {
      *size = -1;
      exact_size = -1;
   }

   return(exact_size);
}
