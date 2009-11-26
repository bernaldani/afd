/*
 *  handle_setup_file.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1997 - 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_setup_file - reads or writes the initial setup file
 **
 ** SYNOPSIS
 **   void read_setup(char *file_name,
 **                   char *profile,
 **                   int  *hostname_display_length,
 **                   int  *filename_display_length,
 **                   int  *his_log_set,
 **                   char **hosts,
 **                   int  max_hostname_length)
 **   void write_setup(int  hostname_display_length,
 **                    int  filename_display_length,
 **                    int  his_log_set,
 **                    char **hosts,
 **                    int  max_no_hosts,
 **                    int  max_hostname_length)
 **
 ** DESCRIPTION
 **   read_setup() looks in the home directory for the file
 **   file_name. If it finds it, it reads the values of the
 **   font, number of rows and the line style and sets them
 **   as default.
 **
 **   write_setup() writes the above values back to this file.
 **
 **   Since both function lock the setup file, there is no
 **   problem when two users with the same home directory read
 **   or write that file.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.06.1997 H.Kiehl Created
 **   10.09.2000 H.Kiehl Added history log for mon_ctrl dialog.
 **   25.12.2001 H.Kiehl Ignore display and screen number for file name.
 **   27.03.2004 H.Kiehl Enable user to load a certain profile.
 **   26.04.2008 H.Kiehl Added parameter for hostname display length.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcat(), strcmp()     */
#include <stdlib.h>                  /* getenv()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                  /* ftruncate(), geteuid(), getuid() */
#include <fcntl.h>
#include <errno.h>
#include "ui_common_defs.h"

/* External global variables. */
extern int  no_of_rows_set,
            no_of_short_lines;
extern char font_name[],
            line_style,
            user[];

/* Local global variables. */
static char setup_file[MAX_PATH_LENGTH] = { 0 };


/*############################## read_setup() ###########################*/
void
read_setup(char *file_name,
           char *profile,
           int  *hostname_display_length,
           int  *filename_display_length,
           int  *his_log_set,
           char **hosts,
           int  max_hostname_length)
{
   int         fd;
   uid_t       euid, /* Effective user ID. */
               ruid; /* Real user ID. */
   char        *ptr,
               *buffer = NULL;
   struct stat stat_buf;

   if (setup_file[0] == '\0')
   {
      if ((ptr = getenv("HOME")) != NULL)
      {
         char *wptr;

         (void)strcpy(setup_file, ptr);
         wptr = setup_file + strlen(setup_file);
         *wptr = '/';
         *(wptr + 1) = '.';
         wptr += 2;
         (void)strcpy(wptr, file_name);
         wptr += strlen(file_name);
         *wptr = '.';
         *(wptr + 1) = 's';
         *(wptr + 2) = 'e';
         *(wptr + 3) = 't';
         *(wptr + 4) = 'u';
         *(wptr + 5) = 'p';
         *(wptr + 6) = '.';
         wptr += 7;
         if (profile != NULL)
         {
            (void)strcpy(wptr, profile);
         }
         else
         {
            char hostname[MAX_AFD_NAME_LENGTH];

            ptr = user;
            while ((*ptr != '@') && (*ptr != '\0'))
            {
               *wptr = *ptr;
               wptr++; ptr++;
            }
            if ((*ptr == '@') && (*(ptr + 1) != '\0') && (*(ptr + 1) != ':'))
            {
               *wptr = '@';
               wptr++; ptr++;
               while ((*ptr != ':') && (*ptr != '\0'))
               {
                  *wptr = *ptr;
                  wptr++; ptr++;
               }
            }
            if (get_afd_name(hostname) != INCORRECT)
            {
               *wptr = '.';
               (void)strcpy(wptr + 1, hostname);
            }
         }
      }
      else
      {
         return;
      }
   }

   if (stat(setup_file, &stat_buf) == -1)
   {
      if (errno != ENOENT)
      {
         setup_file[0] = '\0';
      }
      return;
   }

   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }
   fd = lock_file(setup_file, ON);
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }
   if (fd < 0)
   {
      setup_file[0] = '\0';
      return;
   }

   if ((buffer = malloc(stat_buf.st_size)) == NULL)
   {
      (void)close(fd);
      return;
   }
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      free(buffer);
      (void)close(fd);
      return;
   }
   (void)close(fd); /* This will release the lock as well. */

   /* Get the default font. */
   if ((ptr = posi(buffer, FONT_ID)) != NULL)
   {
      int i = 0;

      ptr--;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         font_name[i] = *ptr;
         i++; ptr++;
      }
      font_name[i] = '\0';
   }

   /* Get the number of rows. */
   if ((ptr = posi(buffer, ROW_ID)) != NULL)
   {
      int  i = 0;
      char tmp_buffer[MAX_INT_LENGTH + 1];

      ptr--;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         tmp_buffer[i] = *ptr;
         i++; ptr++;
      }
      tmp_buffer[i] = '\0';
      no_of_rows_set = atoi(tmp_buffer);
   }

   /* Get the line style. */
   if ((ptr = posi(buffer, STYLE_ID)) != NULL)
   {
      int  i = 0;
      char tmp_buffer[MAX_INT_LENGTH + 1];

      ptr--;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         tmp_buffer[i] = *ptr;
         i++; ptr++;
      }
      tmp_buffer[i] = '\0';
      line_style = atoi(tmp_buffer);
      if (strcmp(file_name, AFD_CTRL) == 0)
      {
         if (line_style <= CHARACTERS_AND_BARS)
         {
            if (line_style == BARS_ONLY)
            {
               line_style = SHOW_LEDS | SHOW_JOBS | SHOW_BARS;
            }
            else if (line_style == CHARACTERS_ONLY)
                 {
                    line_style = SHOW_LEDS | SHOW_JOBS | SHOW_CHARACTERS;
                 }
                 else
                 {
                    line_style = SHOW_LEDS | SHOW_JOBS | SHOW_CHARACTERS | SHOW_BARS;
                 }
         }
         else if (line_style > (SHOW_LEDS | SHOW_JOBS | SHOW_CHARACTERS | SHOW_BARS))
              {
                 line_style = SHOW_LEDS | SHOW_JOBS | SHOW_CHARACTERS | SHOW_BARS;
              }
      }
      else
      {
         if ((line_style != CHARACTERS_AND_BARS) &&
             (line_style != CHARACTERS_ONLY) &&
             (line_style != BARS_ONLY))
         {
            line_style = CHARACTERS_AND_BARS;
         }
      }
   }

   /* Get the hostname display length. */
   if (hostname_display_length != NULL)
   {
      if ((ptr = posi(buffer, HOSTNAME_DISPLAY_LENGTH_ID)) != NULL)
      {
         int  i = 0;
         char tmp_buffer[MAX_INT_LENGTH + 1];

         ptr--;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            tmp_buffer[i] = *ptr;
            i++; ptr++;
         }
         tmp_buffer[i] = '\0';
         *hostname_display_length = atoi(tmp_buffer);
         if (*hostname_display_length > MAX_HOSTNAME_LENGTH)
         {
            *hostname_display_length = MAX_HOSTNAME_LENGTH;
         }
      }
      else
      {
         *hostname_display_length = DEFAULT_HOSTNAME_DISPLAY_LENGTH;
      }
   }

   /* Get the filename display length. */
   if (filename_display_length != NULL)
   {
      if ((ptr = posi(buffer, FILENAME_DISPLAY_LENGTH_ID)) != NULL)
      {
         int  i = 0;
         char tmp_buffer[MAX_INT_LENGTH + 1];

         ptr--;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            tmp_buffer[i] = *ptr;
            i++; ptr++;
         }
         tmp_buffer[i] = '\0';
         *filename_display_length = atoi(tmp_buffer);
         if (*filename_display_length > MAX_FILENAME_LENGTH)
         {
            *filename_display_length = MAX_FILENAME_LENGTH;
         }
      }
      else
      {
         *filename_display_length = DEFAULT_FILENAME_DISPLAY_LENGTH;
      }
   }

   /* Get the number of history log entries. */
   if (his_log_set != NULL)
   {
      if ((ptr = posi(buffer, NO_OF_HISTORY_LENGTH_ID)) != NULL)
      {
         int  i = 0;
         char tmp_buffer[MAX_INT_LENGTH + 1];

         ptr--;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\n') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            tmp_buffer[i] = *ptr;
            i++; ptr++;
         }
         tmp_buffer[i] = '\0';
         *his_log_set = atoi(tmp_buffer);
         if (*his_log_set > MAX_LOG_HISTORY)
         {
            *his_log_set = MAX_LOG_HISTORY;
         }
      }
      else
      {
         *his_log_set = DEFAULT_NO_OF_HISTORY_LOGS;
      }
   }

   /* Get the list of unimportant host's/AFD's. */
   if (hosts != NULL)
   {
      int i, j, gotcha;

      ptr = buffer;
      while ((ptr = posi(ptr, UNIMPORTANT_ID)) != NULL)
      {
         i = 0;
         gotcha = NO;
         ptr--;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((*ptr != '\n') && (*ptr != '\0') && (i < max_hostname_length))
         {
            hosts[no_of_short_lines][i] = *ptr;
            i++; ptr++;
         }
         hosts[no_of_short_lines][i] = '\0';
         if (i > 0)
         {
            for (j = 0; j < (no_of_short_lines - 1); j++)
            {
               if (strcmp(hosts[no_of_short_lines], hosts[j]) == 0)
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               no_of_short_lines++;
            }
         }
      }
   }

   if (buffer != NULL)
   {
      free(buffer);
   }

   return;
}


/*############################# write_setup() ###########################*/
void
write_setup(int  hostname_display_length,
            int  filename_display_length,
            int  his_log_set,
            char **hosts,
            int  max_no_hosts,
            int  max_hostname_length)
{
   int         buf_length,
               fd = -1,
               length;
   uid_t       euid, /* Effective user ID. */
               ruid; /* Real user ID. */
   char        *buffer;
   struct stat stat_buf;
   
   if (setup_file[0] == '\0')
   {
      /*
       * Since we have failed to find the users home directory,
       * there is no need to continue.
       */
      return;
   }

   if (stat(setup_file, &stat_buf) == -1)
   {
      if (errno == ENOENT)
      {
         euid = geteuid();
         ruid = getuid();
         if (euid != ruid)
         {
            if (seteuid(ruid) == -1)
            {
               (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                             ruid, strerror(errno));
            }
         }
         fd = open(setup_file, (O_WRONLY | O_CREAT | O_TRUNC),
                   (S_IRUSR | S_IWUSR));
         if (euid != ruid)
         {
            if (seteuid(euid) == -1)
            {
               (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                             euid, strerror(errno));
            }
         }
         if (fd < 0)
         {
            (void)xrec(ERROR_DIALOG,
                       "Failed to open() setup file %s : %s (%s %d)",
                       setup_file, strerror(errno), __FILE__, __LINE__);
            return;
         }
      }
   }

   buf_length = strlen(FONT_ID) +  1 + strlen(font_name) + 1 +
                strlen(ROW_ID) + 1 + MAX_INT_LENGTH + 1 +
                strlen(STYLE_ID) + 1 + MAX_INT_LENGTH + 1;
   if (hostname_display_length != -1)
   {
      buf_length += strlen(HOSTNAME_DISPLAY_LENGTH_ID) + 1 + MAX_INT_LENGTH + 1;
   }
   if (filename_display_length != -1)
   {
      buf_length += strlen(FILENAME_DISPLAY_LENGTH_ID) + 1 + MAX_INT_LENGTH + 1;
   }
   if (his_log_set != -1)
   {
      buf_length += strlen(NO_OF_HISTORY_LENGTH_ID) + 1 + MAX_INT_LENGTH + 1;
   }
   if (hosts != NULL)
   {
      buf_length += ((strlen(UNIMPORTANT_ID) + 1 + max_hostname_length + 1) *
                     max_no_hosts);
   }
   if ((buffer = malloc(buf_length)) == NULL)
   {
      (void)xrec(ERROR_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }
   if (fd == -1)
   {
      fd = lock_file(setup_file, ON);
      if (euid != ruid)
      {
         if (seteuid(euid) == -1)
         {
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                          euid, strerror(errno));
         }
      }
      if (fd < 0)
      {
         return;
      }
      if (ftruncate(fd, 0) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to truncate file %s : %s (%s %d)\n",
                    setup_file, strerror(errno), __FILE__, __LINE__);
      }
      length = sprintf(buffer, "%s %s\n%s %d\n%s %d\n",
                       FONT_ID, font_name, ROW_ID, no_of_rows_set,
                       STYLE_ID, line_style);
      if (hostname_display_length != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           HOSTNAME_DISPLAY_LENGTH_ID, hostname_display_length);
      }
      if (filename_display_length != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           FILENAME_DISPLAY_LENGTH_ID, filename_display_length);
      }
      if (his_log_set != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           NO_OF_HISTORY_LENGTH_ID, his_log_set);
      }
      if (hosts != NULL)
      {
         int i;

         for (i = 0; i < max_no_hosts; i++)
         {
            length += sprintf(&buffer[length], "%s %s\n",
                              UNIMPORTANT_ID, hosts[i]);
         }
      }
      if (write(fd, buffer, length) != length)
      {
         (void)xrec(ERROR_DIALOG,
                    "Failed to write to setup file %s : %s (%s %d)",
                    setup_file, strerror(errno), __FILE__, __LINE__);
      }
      (void)close(fd);
   }
   else
   {
      int lock_fd;

      lock_fd = lock_file(setup_file, ON);
      if (euid != ruid)
      {
         if (seteuid(euid) == -1)
         {
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                          euid, strerror(errno));
         }
      }
      if (lock_fd < 0)
      {
         (void)close(fd);
         return;
      }
      if (ftruncate(fd, 0) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to truncate file %s : %s (%s %d)\n",
                    setup_file, strerror(errno), __FILE__, __LINE__);
      }
      length = sprintf(buffer, "%s %s\n%s %d\n%s %d\n",
                       FONT_ID, font_name, ROW_ID, no_of_rows_set,
                       STYLE_ID, line_style);
      if (hostname_display_length != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           HOSTNAME_DISPLAY_LENGTH_ID, hostname_display_length);
      }
      if (filename_display_length != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           FILENAME_DISPLAY_LENGTH_ID, filename_display_length);
      }
      if (his_log_set != -1)
      {
         length += sprintf(&buffer[length], "%s %d\n",
                           NO_OF_HISTORY_LENGTH_ID, his_log_set);
      }
      if (hosts != NULL)
      {
         int i;

         for (i = 0; i < max_no_hosts; i++)
         {
            length += sprintf(&buffer[length], "%s %s\n",
                              UNIMPORTANT_ID, hosts[i]);
         }
      }
      if (write(fd, buffer, length) != length)
      {
         (void)xrec(ERROR_DIALOG,
                    "Failed to write to setup file %s : %s (%s %d)",
                    setup_file, strerror(errno), __FILE__, __LINE__);
      }
      (void)close(fd);
      (void)close(lock_fd);
   }
   free(buffer);

   return;
}
