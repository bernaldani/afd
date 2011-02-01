/*
 *  convert.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert - converts a file from one format to another
 **
 ** SYNOPSIS
 **   int convert(char *file_path, char *file_name, int type, off_t *file_size)
 **
 ** DESCRIPTION
 **   The function converts the file from one format to another. The
 **   following conversions are currently implemented:
 **
 **     sohetx      - Adds <SOH><CR><CR><LF> to the beginning of the
 **                   file and <CR><CR><LF><ETX> to the end of the file.
 **     wmo         - Just add WMO 8 byte ascii length and 2 bytes type
 **                   indicator. If the message contains <SOH><CR><CR><LF>
 **                   at start and <CR><CR><LF><ETX> at the end, these
 **                   will be removed.
 **     sohetxwmo   - Adds WMO 8 byte ascii length and 2 bytes type
 **                   indicator plus <SOH><CR><CR><LF> at start and
 **                   <CR><CR><LF><ETX> to end if they are not there.
 **     sohetx2wmo1 - converts a file that contains many ascii bulletins
 **                   starting with SOH and ending with ETX to the WMO
 **                   standart (8 byte ascii length and 2 bytes type
 **                   indicators). The SOH and ETX will NOT be copied
 **                   to the new file.
 **     sohetx2wmo0 - same as above only that here the SOH and ETX will be
 **                   copied to the new file.
 **     mrz2wmo     - Converts GRIB, BUFR and BLOK files to files with
 **                   8 byte ascii length and 2 bytes type indicator plus
 **                   <SOH><CR><CR><LF> at start and <CR><CR><LF><ETX> to
 **                   the end, for the individual fields.
 **     unix2dos    - Converts all <LF> to <CR><LF>.
 **     dos2unix    - Converts all <CR><LF> to <LF>.
 **     lf2crcrlf   - Converts all <LF> to <CR><CR><LF>.
 **     crcrlf2lf   - Converts all <CR><CR><LF> to <LF>.
 **
 **   The original file will be overwritten with the new format.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the size of the
 **   newly created file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.09.2003 H.Kiehl Created
 **   10.05.2005 H.Kiehl Don't just check for SOH and ETX, check for
 **                      <SOH><CR><CR><LF> and <CR><CR><LF><ETX>.
 **   20.07.2006 H.Kiehl Added mrz2wmo.
 **   19.11.2008 H.Kiehl Added unix2dos, dos2unix, lf2crcrlf and crcrlf2lf.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* snprintf(), rename()         */
#include <stdlib.h>                      /* strtoul(), malloc(), free()  */
#include <string.h>                      /* strerror()                   */
#include <ctype.h>                       /* isdigit()                    */
#include <unistd.h>                      /* close(), unlink()            */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE     /* Required for BSD          */
# define MAP_FILE 0  /* All others do not need it */
#endif

/* Local function prototypes. */
static int crcrlf2lf(char *, char *, off_t *, off_t *),
           dos2unix(char *, char *, off_t *, off_t *),
           lf2crcrlf(char *, char *, off_t *, off_t *),
           unix2dos(char *, char *, off_t *, off_t *);


/*############################### convert() #############################*/
int
convert(char *file_path, char *file_name, int type, off_t *file_size)
{
   off_t new_length = 0,
         orig_size = 0;
   char  fullname[MAX_PATH_LENGTH],
         new_name[MAX_PATH_LENGTH];

#ifdef HAVE_SNPRINTF
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", file_path, file_name);
   (void)snprintf(new_name, MAX_PATH_LENGTH, "%s.tmpnewname", fullname);
#else
   (void)sprintf(fullname, "%s/%s", file_path, file_name);
   (void)sprintf(new_name, "%s.tmpnewname", fullname);
#endif
   if (type == UNIX2DOS)
   {
      if (unix2dos(fullname, new_name, &new_length, &orig_size) == INCORRECT)
      {
         return(INCORRECT);
      }
   }
   else if (type == DOS2UNIX)
        {
           if (dos2unix(fullname, new_name, &new_length,
                        &orig_size) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
   else if (type == LF2CRCRLF)
        {
           if (lf2crcrlf(fullname, new_name, &new_length,
                         &orig_size) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
   else if (type == CRCRLF2LF)
        {
           if (crcrlf2lf(fullname, new_name, &new_length,
                         &orig_size) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
        else
        {
           int         from_fd,
                       to_fd;
           char        *src_ptr;
           struct stat stat_buf;

           if ((from_fd = open(fullname, O_RDONLY)) < 0)
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                          _("Could not open() `%s' for extracting : %s"),
                          fullname, strerror(errno));
              return(INCORRECT);
           }

           if (fstat(from_fd, &stat_buf) < 0)   /* need size of input file */
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                          _("fstat() error : %s"), strerror(errno));
              (void)close(from_fd);
              return(INCORRECT);
           }

           /*
            * If the size of the file is less then 10 forget it. There can't
            * be a WMO bulletin in it.
            */
           if (stat_buf.st_size < 10)
           {
              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                          _("Got a file for converting that is %ld bytes long!"),
                          stat_buf.st_size);
              (void)close(from_fd);
              return(INCORRECT);
           }

#ifdef HAVE_MMAP
           if ((src_ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                               (MAP_FILE | MAP_SHARED),
                               from_fd, 0)) == (caddr_t) -1)
#else
           if ((src_ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                                   (MAP_FILE | MAP_SHARED),
                                   fullname, 0)) == (caddr_t) -1)
#endif
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                          _("mmap() error : %s"), strerror(errno));
              (void)close(from_fd);
              return(INCORRECT);
           }

           if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                             stat_buf.st_mode)) == -1)
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                          _("Failed to open() %s : %s"),
                          new_name, strerror(errno));
              (void)close(from_fd);
#ifdef HAVE_MMAP
              (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
              (void)munmap_emu((void *)src_ptr);
#endif
              return(INCORRECT);
           }

           switch (type)
           {
              case SOHETX :
                 if ((*src_ptr != 1) && (*(src_ptr + stat_buf.st_size - 1) != 3))
                 {
                    char buffer[4];

                    buffer[0] = 1;
                    buffer[1] = 13;
                    buffer[2] = 13;
                    buffer[3] = 10;
                    if (write(to_fd, buffer, 4) != 4)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to write() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += 4;

                    if (writen(to_fd, src_ptr, stat_buf.st_size,
                               stat_buf.st_blksize) != stat_buf.st_size)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += stat_buf.st_size;

                    buffer[0] = 13;
                    buffer[2] = 10;
                    buffer[3] = 3;
                    if (write(to_fd, buffer, 4) != 4)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to write() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += 4;
                 }
                 break;

              case ONLY_WMO :
                 {
                    int   offset;
                    off_t size;
                    char  length_indicator[10];

                    if ((*src_ptr == 1) &&
                        (*(src_ptr + 1) == 13) &&
                        (*(src_ptr + 2) == 13) &&
                        (*(src_ptr + 3) == 10) &&
                        (*(src_ptr + stat_buf.st_size - 4) == 13) &&
                        (*(src_ptr + stat_buf.st_size - 3) == 13) &&
                        (*(src_ptr + stat_buf.st_size - 2) == 10) &&
                        (*(src_ptr + stat_buf.st_size - 1) == 3))
                    {
                       offset = 4;
                       size = stat_buf.st_size - 8;
                    }
                    else
                    {
                       size = stat_buf.st_size;
                       offset = 0;
                    }
                    (void)sprintf(length_indicator, "%08lu",
                                  (unsigned long)size);
                    length_indicator[8] = '0';
                    length_indicator[9] = '1';
                    if (write(to_fd, length_indicator, 10) != 10)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to write() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += 10;

                    if (writen(to_fd, (src_ptr + offset), size,
                               stat_buf.st_blksize) != size)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += size;
                 }
                 break;

              case SOHETXWMO :
                 {
                    int    additional_length,
                           end_offset,
                           front_offset;
                    size_t length,
                           write_length;
                    char   length_indicator[14];

                    if (*src_ptr != 1)
                    {
                       if ((stat_buf.st_size > 10) &&
                           (isdigit((int)*src_ptr)) &&
                           (isdigit((int)*(src_ptr + 1))) &&
                           (isdigit((int)*(src_ptr + 2))) &&
                           (isdigit((int)*(src_ptr + 3))) &&
                           (isdigit((int)*(src_ptr + 4))) &&
                           (isdigit((int)*(src_ptr + 5))) &&
                           (isdigit((int)*(src_ptr + 6))) &&
                           (isdigit((int)*(src_ptr + 7))) &&
                           (isdigit((int)*(src_ptr + 8))) &&
                           (isdigit((int)*(src_ptr + 9))))
                       {
                          length_indicator[0] = *src_ptr;
                          length_indicator[1] = *(src_ptr + 1);
                          length_indicator[2] = *(src_ptr + 2);
                          length_indicator[3] = *(src_ptr + 3);
                          length_indicator[4] = *(src_ptr + 4);
                          length_indicator[5] = *(src_ptr + 5);
                          length_indicator[6] = *(src_ptr + 6);
                          length_indicator[7] = *(src_ptr + 7);
                          length_indicator[8] = '\0';
                          length = (size_t)strtoul(length_indicator, NULL, 10);
                          if (stat_buf.st_size == (length + 10))
                          {
                             if (*(src_ptr + 10) == 1)
                             {
                                if (*(src_ptr + 11) == 10)
                                {
                                   length_indicator[10] = 1;
                                   length_indicator[11] = 13;
                                   length_indicator[12] = 13;
                                   length_indicator[13] = 10;
                                   length = 14;
                                   front_offset = 12;
                                   additional_length = 4;
                                }
                                else if ((*(src_ptr + 11) == 13) &&
                                         (*(src_ptr + 12) == 10))
                                     {
                                        length_indicator[10] = 1;
                                        length_indicator[11] = 13;
                                        length_indicator[12] = 13;
                                        length_indicator[13] = 10;
                                        length = 14;
                                        front_offset = 13;
                                        additional_length = 4;
                                     }
                                     else
                                     {
                                        length = 10;
                                        front_offset = 10;
                                        additional_length = 0;
                                     }
                             }
                             else
                             {
                                length_indicator[10] = 1;
                                length_indicator[11] = 13;
                                length_indicator[12] = 13;
                                length_indicator[13] = 10;
                                length = 14;
                                front_offset = 10;
                                additional_length = 4;
                             }
                          }
                          else
                          {
                             length_indicator[10] = 1;
                             length_indicator[11] = 13;
                             length_indicator[12] = 13;
                             length_indicator[13] = 10;
                             length = 14;
                             front_offset = 0;
                             additional_length = 4;
                          }
                       }
                       else
                       {
                          length_indicator[10] = 1;
                          length_indicator[11] = 13;
                          length_indicator[12] = 13;
                          length_indicator[13] = 10;
                          length = 14;
                          front_offset = 0;
                          additional_length = 4;
                       }
                    }
                    else
                    {
                       if (*(src_ptr + 1) == 10)
                       {
                          length_indicator[10] = 1;
                          length_indicator[11] = 13;
                          length_indicator[12] = 13;
                          length_indicator[13] = 10;
                          length = 14;
                          front_offset = 2;
                          additional_length = 4;
                       }
                       else if ((*(src_ptr + 1) == 13) &&
                                (*(src_ptr + 2) == 10))
                            {
                               length_indicator[10] = 1;
                               length_indicator[11] = 13;
                               length_indicator[12] = 13;
                               length_indicator[13] = 10;
                               length = 14;
                               front_offset = 3;
                               additional_length = 4;
                            }
                       else if ((*(src_ptr + 1) == 13) &&
                                (*(src_ptr + 2) == 13) &&
                                (*(src_ptr + 3) == 10))
                            {
                               length = 10;
                               front_offset = 0;
                               additional_length = 0;
                            }
                            else
                            {
                               length_indicator[10] = 1;
                               length_indicator[11] = 13;
                               length_indicator[12] = 13;
                               length_indicator[13] = 10;
                               length = 14;
                               front_offset = 1;
                               additional_length = 4;
                            }
                    }
                    if (*(src_ptr + stat_buf.st_size - 1) != 3)
                    {
                       end_offset = 0;
                       additional_length += 4;
                    }
                    else
                    {
                       if (*(src_ptr + stat_buf.st_size - 2) != 10)
                       {
                          end_offset = 1;
                          additional_length += 4;
                       }
                       else if (*(src_ptr + stat_buf.st_size - 3) != 13)
                            {
                               end_offset = 2;
                               additional_length += 4;
                            }
                            else
                            {
                               end_offset = 0;
                            }
                    }
                    (void)sprintf(length_indicator, "%08lu",
                                  (unsigned long)stat_buf.st_size - front_offset - end_offset + additional_length);
                    length_indicator[8] = '0';
                    length_indicator[9] = '0';
                    if (writen(to_fd, length_indicator, length,
                               stat_buf.st_blksize) != length)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += length;

                    write_length = stat_buf.st_size - (front_offset + end_offset);
                    if (writen(to_fd, src_ptr + front_offset, write_length,
                               stat_buf.st_blksize) != write_length)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
#ifdef HAVE_MMAP
                       (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                       (void)munmap_emu((void *)src_ptr);
#endif
                       return(INCORRECT);
                    }
                    new_length += write_length;

                    if ((*(src_ptr + stat_buf.st_size - 1) != 3) ||
                        (*(src_ptr + stat_buf.st_size - 2) != 10) ||
                        (*(src_ptr + stat_buf.st_size - 3) != 13))
                    {
                       length_indicator[10] = 13;
                       length_indicator[11] = 13;
                       length_indicator[12] = 10;
                       length_indicator[13] = 3;
                       if (write(to_fd, &length_indicator[10], 4) != 4)
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to write() to `%s' : %s"),
                                      new_name, strerror(errno));
                          (void)close(from_fd);
                          (void)close(to_fd);
#ifdef HAVE_MMAP
                          (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                          (void)munmap_emu((void *)src_ptr);
#endif
                          return(INCORRECT);
                       }
                       new_length += 4;
                    }
                 }
                 break;

              case SOHETX2WMO0 :
              case SOHETX2WMO1 :
                 {
                    int    add_sohcrcrlf;
                    size_t length;
                    char   length_indicator[14],
                           *ptr,
                           *ptr_start;

                    ptr = src_ptr;
                    do
                    {
                       /* Always try finding the beginning of a message */
                       /* first.                                        */
                       do
                       {
                          while ((*ptr != 1) &&
                                 ((ptr - src_ptr + 1) < stat_buf.st_size))
                          {
                             ptr++;
                          }
                          if (*ptr == 1)
                          {
                             if (((ptr - src_ptr + 3) < stat_buf.st_size) &&
                                 ((*(ptr + 1) == 10) || (*(ptr + 3) == 10) ||
                                  (*(ptr + 2) == 10)))
                             {
                                break;
                             }
                             else
                             {
                                ptr++;
                             }
                          }
                       } while ((ptr - src_ptr + 1) < stat_buf.st_size);

                       /* Now lets cut out the message. */
                       if (*ptr == 1)
                       {
                          if (type == SOHETX2WMO1)
                          {
                             ptr++; /* Away with SOH */
                             add_sohcrcrlf = NO;
                          }
                          else
                          {
                             if ((ptr + 1 - src_ptr + 3) < stat_buf.st_size)
                             {
                                if (*(ptr + 1) == 10)
                                {
                                   ptr += 2;
                                   add_sohcrcrlf = YES;
                                }
                                else if ((*(ptr + 1) == 13) &&
                                         (*(ptr + 2) == 10))
                                     {
                                        ptr += 3;
                                        add_sohcrcrlf = YES;
                                     }
                                else if ((*(ptr + 1) == 13) &&
                                         (*(ptr + 2) == 13) &&
                                         (*(ptr + 3) == 10))
                                     {
                                        add_sohcrcrlf = NO;
                                     }
                                     else
                                     {
                                        ptr++;
                                        add_sohcrcrlf = YES;
                                     }
                             }
                             else
                             {
                                add_sohcrcrlf = NO;
                             }
                          }

                          /* Lets find end of message. */
                          ptr_start = ptr;
                          do
                          {
                             while ((*ptr != 3) &&
                                    ((ptr - src_ptr + 1) < stat_buf.st_size))
                             {
                                ptr++;
                             }
                             if (*ptr == 3)
                             {
                                if (((ptr - ptr_start) >= 1) &&
                                    (*(ptr - 1) == 10))
                                {
                                   break;
                                }
                                else
                                {
                                   ptr++;
                                }
                             }
                          } while ((ptr - src_ptr + 1) < stat_buf.st_size);

                          if (*ptr == 3)
                          {
                             int end_length,
                                 start_length;

                             if (type == SOHETX2WMO1)
                             {
                                length = ptr - ptr_start;
                                length_indicator[9] = '1';
                                start_length = 10;
                                end_length = 0;
                             }
                             else
                             {
                                if (add_sohcrcrlf == YES)
                                {
                                   length = ptr - ptr_start + 1;
                                   length_indicator[10] = 1;
                                   length_indicator[11] = 13;
                                   length_indicator[12] = 13;
                                   length_indicator[13] = 10;
                                   start_length = 14;
                                }
                                else
                                {
                                   length = ptr - ptr_start + 1;
                                   start_length = 10;
                                }
                                length_indicator[9] = '0';

                                if ((*(ptr - 1) == 10) && (*(ptr - 2) == 13) &&
                                    (*(ptr - 3) == 13))
                                {
                                   end_length = 0;
                                }
                                else if ((*(ptr - 1) == 10) && (*(ptr - 2) == 13))
                                     {
                                        end_length = 4;
                                        length -= 3;
                                     }
                                else if (*(ptr - 1) == 10)
                                     {
                                        end_length = 4;
                                        length -= 2;
                                     }
                                     else
                                     {
                                        end_length = 4;
                                        length -= 1;
                                     }
                             }
                             (void)sprintf(length_indicator, "%08lu",
                                           (unsigned long)(length + end_length));
                             length_indicator[8] = '0';
                             if (write(to_fd, length_indicator,
                                       start_length) != start_length)
                             {
                                receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                            _("Failed to write() to `%s' : %s"),
                                            new_name, strerror(errno));
                                (void)close(from_fd);
                                (void)close(to_fd);
                                return(INCORRECT);
                             }
                             if (writen(to_fd, ptr_start, length,
                                        stat_buf.st_blksize) != length)
                             {
                                receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                            _("Failed to writen() to `%s' : %s"),
                                            new_name, strerror(errno));
                                (void)close(from_fd);
                                (void)close(to_fd);
                                return(INCORRECT);
                             }
                             if (end_length > 0)
                             {
                                length_indicator[0] = 13;
                                length_indicator[1] = 13;
                                length_indicator[2] = 10;
                                length_indicator[3] = 3;
                                if (write(to_fd, length_indicator,
                                          end_length) != end_length)
                                {
                                   receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                               _("Failed to write() to `%s' : %s"),
                                               new_name, strerror(errno));
                                   (void)close(from_fd);
                                   (void)close(to_fd);
                                   return(INCORRECT);
                                }
                             }
                             new_length += (start_length + length + end_length);
                          }
                       }
                    } while ((ptr - src_ptr + 1) < stat_buf.st_size);
                 }
                 break;

              case MRZ2WMO :
                 if ((new_length = bin_file_convert(src_ptr, stat_buf.st_size,
                                                    to_fd)) < 0)
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                _("Failed to convert MRZ file `%s' to WMO-format."),
                                file_name);
                    new_length = 0;
                 }
                 break;

              case ISO8859_2ASCII :
                 {
                    char *dst;

                    if ((dst = malloc((stat_buf.st_size * 3))) == NULL)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("malloc() error : %s"),
                                   strerror(errno));
                       (void)close(from_fd);
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    if ((new_length = iso8859_2ascii(src_ptr, dst,
                                                     stat_buf.st_size)) < 0)
                    {
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to convert ISO8859 file `%s' to ASCII."),
                                   file_name);
                       new_length = 0;
                    }
                    else
                    {
                       if (writen(to_fd, dst, new_length,
                                  stat_buf.st_blksize) != new_length)
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to writen() to `%s' : %s"),
                                      new_name, strerror(errno));
                          (void)close(from_fd);
                          (void)close(to_fd);
                          (void)free(dst);
                          return(INCORRECT);
                       }
                    }
                    (void)free(dst);
                 }
                 break;

              default            : /* Impossible! */
                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                             _("Unknown convert type (%d)."), type);
#ifdef HAVE_MMAP
                 (void)munmap((void *)src_ptr, stat_buf.st_size);
#else
                 (void)munmap_emu((void *)src_ptr);
#endif
                 (void)close(from_fd);
                 (void)close(to_fd);
                 return(INCORRECT);
           }

#ifdef HAVE_MMAP
           if (munmap((void *)src_ptr, stat_buf.st_size) == -1)
#else
           if (munmap_emu((void *)src_ptr) == -1)
#endif
           {
              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                          _("Failed to munmap() `%s' : %s"), fullname, strerror(errno));
           }

           if ((close(from_fd) == -1) || (close(to_fd) == -1))
           {
              receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                          _("close() error : `%s'"), strerror(errno));
           }
           orig_size = stat_buf.st_size;
        }

   /* Remove the file that has just been extracted. */
   if (unlink(fullname) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to unlink() `%s' : %s"), fullname, strerror(errno));
   }
   else
   {
      if (rename(new_name, fullname) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to rename() `%s' to `%s' : %s"),
                     new_name, fullname, strerror(errno));
         *file_size += new_length;
      }
      else
      {
         *file_size += (new_length - orig_size);
      }
   }
   if (new_length == 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                  _("No data converted in %s (%ld bytes)."),
#else
                  _("No data converted in %s (%lld bytes)."),
#endif
                  file_name, (pri_off_t)orig_size);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ unix2dos() ++++++++++++++++++++++++++++++*/
static int
unix2dos(char  *source_file,
         char  *dest_file,
         off_t *new_length,
         off_t *orig_size)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int   prev_char = 0,
               tmp_char;
         off_t extra_chars = 0;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char != 13))
            {
               prev_char = 13;
               if (fputc(prev_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
                  extra_chars++;
               }
            }
            if (fputc(tmp_char, wfp) == EOF)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("fputc() error for file `%s' : %s"),
                           dest_file, strerror(errno));
               (void)fclose(rfp);
               (void)fclose(wfp);
               if (unlink(dest_file) == 0)
               {
                  *new_length = 0;
               }

               return(INCORRECT);
            }
            else
            {
               (*new_length)++;
            }
            prev_char = tmp_char;
         }
         *orig_size = *new_length - extra_chars;

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ dos2unix() ++++++++++++++++++++++++++++++*/
static int
dos2unix(char  *source_file,
         char  *dest_file,
         off_t *new_length,
         off_t *orig_size)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int   prev_char = 0,
               tmp_char;
         off_t del_chars = 0;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char == 13))
            {
               if (fputc(tmp_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
                  del_chars++;
               }
            }
            else
            {
               if (prev_char == 13)
               {
                  if (fputc(prev_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
               if (tmp_char != 13)
               {
                  if (fputc(tmp_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
            }
            prev_char = tmp_char;
            *orig_size = *new_length + del_chars;
         }

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ lf2crcrlf() +++++++++++++++++++++++++++++*/
static int
lf2crcrlf(char  *source_file,
          char  *dest_file,
          off_t *new_length,
          off_t *orig_size)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int   prev_char = 0,
               prev_prev_char = 0,
               tmp_char,
               write_char = 13;
         off_t extra_chars = 0;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char != 13) && (prev_prev_char != 13))
            {
               if (fputc(write_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
                  extra_chars++;
               }
               if (fputc(write_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
                  extra_chars++;
               }
            }
            if ((tmp_char == 10) && (prev_char == 13) && (prev_prev_char != 13))
            {
               if (fputc(write_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
                  extra_chars++;
               }
            }
            if (fputc(tmp_char, wfp) == EOF)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("fputc() error for file `%s' : %s"),
                           dest_file, strerror(errno));
               (void)fclose(rfp);
               (void)fclose(wfp);
               if (unlink(dest_file) == 0)
               {
                  *new_length = 0;
               }

               return(INCORRECT);
            }
            else
            {
               (*new_length)++;
            }
            prev_prev_char = prev_char;
            prev_char = tmp_char;
         }
         *orig_size = *new_length - extra_chars;

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ crcrlf2lf() ++++++++++++++++++++++++++++++*/
static int
crcrlf2lf(char  *source_file,
          char  *dest_file,
          off_t *new_length,
          off_t *orig_size)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int   prev_char = 0,
               prev_prev_char = 0,
               tmp_char;
         off_t del_chars = 0;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char == 13) && (prev_prev_char == 13))
            {
               if (fputc(tmp_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
                  del_chars += 2;
               }
            }
            else
            {
               if ((prev_char == 13) && (tmp_char != 13))
               {
                  if (fputc(prev_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
               if ((prev_char == 13) && (prev_prev_char == 13))
               {
                  if (fputc(prev_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
               if (tmp_char != 13)
               {
                  if (fputc(tmp_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
            }
            prev_prev_char = prev_char;
            prev_char = tmp_char;
         }
         *orig_size = *new_length + del_chars;

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}
