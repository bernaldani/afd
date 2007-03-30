/*
 *  afdcfg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2007 Deutscher Wetterdienst (DWD),
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
 **   afdcfg - configure AFD
 **
 ** SYNOPSIS
 **   afdcfg [-w <working directory>] [-u[ <user>]] option
 **                  -a      enable archive
 **                  -A      disable archive
 **                  -c      enable create target dir
 **                  -C      disable create target dir
 **                  -d      enable directory warn time
 **                  -D      disable directory warn time
 **                  -r      enable retrieving of files
 **                  -R      disable retrieving of files
 **                  -s      status of the above flags
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.10.2000 H.Kiehl Created
 **   04.03.2004 H.Kiehl Option to set flag to enable/disable creating
 **                      target directories.
 **   07.02.2007 H.Kiehl Added option to enable/disable directory warn
 **                      time.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <fcntl.h>                       /* open()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "permission.h"
#include "version.h"

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_dirs = 0,
                           no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes */
static void                usage(char *);

#define ENABLE_ARCHIVE_SEL            1
#define DISABLE_ARCHIVE_SEL           2
#define ENABLE_RETRIEVE_SEL           3
#define DISABLE_RETRIEVE_SEL          4
#define ENABLE_DIR_WARN_TIME_SEL      5
#define DISABLE_DIR_WARN_TIME_SEL     6
#define ENABLE_CREATE_TARGET_DIR_SEL  7
#define DISABLE_CREATE_TARGET_DIR_SEL 8
#define STATUS_SEL                    9


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afdcfg() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  action;
   char *perm_buffer,
        *ptr_fra,
        *ptr_fsa,
        fake_user[MAX_FULL_USER_ID_LENGTH],
        user[MAX_FULL_USER_ID_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc != 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   if (get_arg(&argc, argv, "-a", NULL, 0) != SUCCESS)
   {
      if (get_arg(&argc, argv, "-A", NULL, 0) != SUCCESS)
      {
         if (get_arg(&argc, argv, "-c", NULL, 0) != SUCCESS)
         {
            if (get_arg(&argc, argv, "-C", NULL, 0) != SUCCESS)
            {
               if (get_arg(&argc, argv, "-d", NULL, 0) != SUCCESS)
               {
                  if (get_arg(&argc, argv, "-D", NULL, 0) != SUCCESS)
                  {
                     if (get_arg(&argc, argv, "-r", NULL, 0) != SUCCESS)
                     {
                        if (get_arg(&argc, argv, "-R", NULL, 0) != SUCCESS)
                        {
                           if (get_arg(&argc, argv, "-s", NULL, 0) != SUCCESS)
                           {
                              usage(argv[0]);
                              exit(INCORRECT);
                           }
                           else
                           {
                              action = STATUS_SEL;
                           }
                        }
                        else
                        {
                           action = DISABLE_RETRIEVE_SEL;
                        }
                     }
                     else
                     {
                        action = ENABLE_RETRIEVE_SEL;
                     }
                  }
                  else
                  {
                     action = DISABLE_DIR_WARN_TIME_SEL;
                  }
               }
               else
               {
                  action = ENABLE_DIR_WARN_TIME_SEL;
               }
            }
            else
            {
               action = DISABLE_CREATE_TARGET_DIR_SEL;
            }
         }
         else
         {
            action = ENABLE_CREATE_TARGET_DIR_SEL;
         }
      }
      else
      {
         action = DISABLE_ARCHIVE_SEL;
      }
   }
   else /* Enable archive */
   {
      action = ENABLE_ARCHIVE_SEL;
   }
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   get_user(user, fake_user);

   /*
    * Ensure that the user may use this program.
    */
   switch (get_permissions(&perm_buffer, fake_user))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          "Failed to access `%s', unable to determine users permissions.\n",
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         {
            int permission = NO;

            if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
                (perm_buffer[2] == 'l') &&
                ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
                 (perm_buffer[3] == '\t')))
            {
               permission = YES;
            }
            else
            {
               if (posi(perm_buffer, AFD_CFG_PERM) != NULL)
               {
                  permission = YES;
               }
            }
            free(perm_buffer);
            if (permission != YES)
            {
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
               exit(INCORRECT);
            }
         }
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         break;

      default       :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if ((action == ENABLE_ARCHIVE_SEL) || (action == DISABLE_ARCHIVE_SEL) ||
       (action == ENABLE_CREATE_TARGET_DIR_SEL) ||
       (action == DISABLE_CREATE_TARGET_DIR_SEL) ||
       (action == ENABLE_RETRIEVE_SEL) || (action == DISABLE_RETRIEVE_SEL) ||
       (action == STATUS_SEL))
   {
      if (fsa_attach() < 0)
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      ptr_fsa = (char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
   }

   if ((action == ENABLE_DIR_WARN_TIME_SEL) ||
       (action == DISABLE_DIR_WARN_TIME_SEL) ||
       (action == STATUS_SEL))
   {
      if (fra_attach() < 0)
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to FRA. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      ptr_fra = (char *)fra - AFD_FEATURE_FLAG_OFFSET_END;
   }

   switch (action)
   {
      case ENABLE_ARCHIVE_SEL :
         if (*ptr_fsa & DISABLE_ARCHIVE)
         {
            *ptr_fsa ^= DISABLE_ARCHIVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Archiving enabled by %s", user);
         }
         else
         {
            (void)fprintf(stdout, "Archiving is already enabled.\n");
         }
         break;
         
      case DISABLE_ARCHIVE_SEL :
         if (*ptr_fsa & DISABLE_ARCHIVE)
         {
            (void)fprintf(stdout, "Archiving is already disabled.\n");
         }
         else
         {
            *ptr_fsa |= DISABLE_ARCHIVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Archiving disabled by %s", user);
         }
         break;

      case ENABLE_CREATE_TARGET_DIR_SEL :
         if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
         {
            (void)fprintf(stdout, "Create target dir already enabled.\n");
         }
         else
         {
            *ptr_fsa |= ENABLE_CREATE_TARGET_DIR;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Create target dir by default enabled by %s", user);
         }
         break;

      case DISABLE_CREATE_TARGET_DIR_SEL :
         if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
         {
            *ptr_fsa ^= ENABLE_CREATE_TARGET_DIR;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Create target dir by default disabled by %s", user);
         }
         else
         {
            (void)fprintf(stdout, "Create target dir already disabled.\n");
         }
         break;

      case ENABLE_DIR_WARN_TIME_SEL :
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            *ptr_fra ^= DISABLE_DIR_WARN_TIME;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Directory warn time enabled by %s", user);
         }
         else
         {
            (void)fprintf(stdout, "Directory warn time already enabled.\n");
         }
         break;

      case DISABLE_DIR_WARN_TIME_SEL :
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            (void)fprintf(stdout, "Directory warn time is already disabled.\n");
         }
         else
         {
            int i;

            *ptr_fra |= DISABLE_DIR_WARN_TIME;
            for (i = 0; i < no_of_dirs; i++)
            {
               if (fra[i].dir_flag & WARN_TIME_REACHED)
               {
                  fra[i].dir_flag ^= WARN_TIME_REACHED;
                  SET_DIR_STATUS(fra[i].dir_flag, fra[i].dir_status);
               }
            }
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Directory warn time is disabled by %s", user);
         }
         break;

      case ENABLE_RETRIEVE_SEL :
         if (*ptr_fsa & DISABLE_RETRIEVE)
         {
            *ptr_fsa ^= DISABLE_RETRIEVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Retrieving enabled by %s", user);
         }
         else
         {
            (void)fprintf(stdout, "Retrieving is already enabled.\n");
         }
         break;
         
      case DISABLE_RETRIEVE_SEL :
         if (*ptr_fsa & DISABLE_RETRIEVE)
         {
            (void)fprintf(stdout, "Retrieving is already disabled.\n");
         }
         else
         {
            *ptr_fsa |= DISABLE_RETRIEVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Retrieving disabled by %s", user);
         }
         break;
         
      case STATUS_SEL :
         if (*ptr_fsa & DISABLE_ARCHIVE)
         {
            (void)fprintf(stdout, "Archiving        : Disabled\n");
         }
         else
         {
            (void)fprintf(stdout, "Archiving        : Enabled\n");
         }
         if (*ptr_fsa & DISABLE_RETRIEVE)
         {
            (void)fprintf(stdout, "Retrieving       : Disabled\n");
         }
         else
         {
            (void)fprintf(stdout, "Retrieving       : Enabled\n");
         }
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            (void)fprintf(stdout, "Dir warn time    : Disabled\n");
         }
         else
         {
            (void)fprintf(stdout, "Dir warn time    : Enabled\n");
         }
         if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
         {
            (void)fprintf(stdout, "Create target dir: Enabled\n");
         }
         else
         {
            (void)fprintf(stdout, "Create target dir: Disabled\n");
         }
         break;
         
      default :
         (void)fprintf(stderr, "Impossible! (%s %d)\n", __FILE__, __LINE__);
         exit(INCORRECT);
   }

   if ((action == ENABLE_ARCHIVE_SEL) || (action == DISABLE_ARCHIVE_SEL) ||
       (action == ENABLE_CREATE_TARGET_DIR_SEL) ||
       (action == DISABLE_CREATE_TARGET_DIR_SEL) ||
       (action == ENABLE_RETRIEVE_SEL) || (action == DISABLE_RETRIEVE_SEL) ||
       (action == STATUS_SEL))
   {
      (void)fsa_detach(YES);
   }
   if ((action == ENABLE_DIR_WARN_TIME_SEL) ||
       (action == DISABLE_DIR_WARN_TIME_SEL) ||
       (action == STATUS_SEL))
   {
      (void)fra_detach();
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] [-u [<user>]] options\n",
                 progname);
   (void)fprintf(stderr, "          -a      enable archive\n");
   (void)fprintf(stderr, "          -A      disable archive\n");
   (void)fprintf(stderr, "          -c      enable create target dir\n");
   (void)fprintf(stderr, "          -C      disable create target dir\n");
   (void)fprintf(stderr, "          -d      enable dir warn time\n");
   (void)fprintf(stderr, "          -D      disable dir warn time\n");
   (void)fprintf(stderr, "          -r      enable retrieving of files\n");
   (void)fprintf(stderr, "          -R      disable retrieving of files\n");
   (void)fprintf(stderr, "          -s      status of the above flags\n");
   return;
}