/*
 *  get_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2002 Deutscher Wetterdienst (DWD),
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
 **   get_hostname - extracts hostname from recipient string
 **
 ** SYNOPSIS
 **   int get_hostname(char *recipient, char *real_hostname)
 **
 ** DESCRIPTION
 **   This function returns the hostname from the recipient string
 **   'recipient'. This string has the following format:
 **
 **        <sheme>://<user>:<password>@<host>:<port>/<url-path>
 **
 ** RETURN VALUES
 **   Returns INCORRECT if it is not able to extract the hostname.
 **   Otherwise it will return SUCCESS and the hostname is stored in
 **   real_hostname.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.02.1996 H.Kiehl Created
 **   15.12.1996 H.Kiehl Switched to URL format.
 **   15.12.2001 H.Kiehl When server= is set take this as hostname.
 **   19.02.2002 H.Kiehl Handle group names.
 **
 */
DESCR__E_M3

#include <stdio.h>


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ get_hostname() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
get_hostname(char *recipient, char *real_hostname)
{
   int  i = 0;
   char *ptr;

   ptr = recipient;
   real_hostname[0] = '\0';
   while ((*ptr != '\0') && (*ptr != ':'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      ptr++;
   }
   if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/') &&
       (*(ptr + 3) == MAIL_GROUP_IDENTIFIER))
   {
      ptr += 4;
      while ((*ptr != '\0') && (*ptr != '@') &&
             (*ptr != '/') && /* url-path */
             (*ptr != ':') && /* port number */
             (*ptr != ';') && /* server= */
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         real_hostname[i] = *ptr;
         i++; ptr++;
      }
   }

   while ((*ptr != '\0') && (*ptr != '@') && (*ptr != ';'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      ptr++;
   }

   if (*ptr == '@')
   {
      /* Store the real_hostname */
      ptr++;
      i = 0;
      while ((*ptr != '\0') &&
             (*ptr != '/') && /* url-path */
             (*ptr != ':') && /* port number */
             (*ptr != ';') && /* server= */
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         real_hostname[i] = *ptr;
         i++; ptr++;
      }
   }

   /*
    * NOTE: For mail we take the hostname from the "server=" if
    *       it does exist.
    */
   if ((i == MAX_REAL_HOSTNAME_LENGTH) && (*ptr != '/'))
   {
      while ((*ptr != '\0') && (*ptr != '/') &&
             (*ptr != ':') && (*ptr != ';'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
   }
   if (*ptr == ':')
   {
      while ((*ptr != '\0') && (*ptr != '@'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
   }
   if ((*ptr == ';') && (*(ptr + 1) == 's') && (*(ptr + 2) == 'e') &&
       (*(ptr + 3) == 'r') && (*(ptr + 4) == 'v') &&
       (*(ptr + 5) == 'e') && (*(ptr + 6) == 'r') &&
       (*(ptr + 7) == '='))
   {
      /* Store the real_hostname. */
      ptr += 8;
      i = 0;
      while ((*ptr != '\0') && (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         real_hostname[i] = *ptr;
         i++; ptr++;
      }
   }
   if ((i > 0) && (i < MAX_REAL_HOSTNAME_LENGTH))
   {
      real_hostname[i] = '\0';
      return(SUCCESS);
   }

   return(INCORRECT);
}