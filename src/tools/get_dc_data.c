/*
 *  get_dc_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dc_data - gets all data out of the DIR_CONFIG for a host
 **
 ** SYNOPSIS
 **   get_dc_data [-h <host alias>] [-d <dir alias>] [-D <dir id>]
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
 **   27.02.1999 H.Kiehl Created
 **   26.04.1999 H.Kiehl Added real hostname to output.
 **   01.01.2004 H.Kiehl Get file mask entries from file mask database.
 **                      Insert password if wanted.
 **   14.08.2005 H.Kiehl Reconstruct DIR_CONFIG by printing everything
 **                      to stdout.
 **   06.01.2006 H.Kiehl When printing data for a specific host, print
 **                      out information about directory options.
 **   28.02.2008 H.Kiehl Added -D option.
 **   13.09.2012 H.Kiehl Added --only_list_target_dirs option.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* free()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>         /* mmap(), munmap()                        */
#include <errno.h>
#include "version.h"
#include "permission.h"
#include "amgdefs.h"

/* Global variables. */
unsigned int                  *current_jid_list;
int                           fra_fd = -1,
                              fra_id,
                              fsa_fd = -1,
                              fsa_id,
                              no_of_current_jobs,
                              no_of_dirs,
                              no_of_hosts,
                              sys_log_fd,
                              view_passwd = NO;
#ifdef HAVE_MMAP
off_t                         fra_size,
                              fsa_size;
#endif
char                          *p_work_dir;
struct filetransfer_status    *fsa;
struct fileretrieve_status    *fra;
const char                    *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int                    no_of_dc_ids,
                              no_of_dirs_in_dnb,
                              no_of_file_mask_ids,
                              no_of_gotchas,
                              no_of_job_ids,
                              no_of_passwd,
                              only_list_target_dirs = NO;
static unsigned int           *gl; /* Gotcha List. */
static char                   *fmd = NULL,
                              *fmd_end;
static struct job_id_data     *jd = NULL;
static struct dir_config_list *dcl = NULL;
static struct dir_name_buf    *dnb = NULL;
static struct passwd_buf      *pwb = NULL;

/* Local function prototypes. */
static void                   check_dir_options(int),
                              get_dc_data(char *, char *, unsigned int),
                              show_data(struct job_id_data *, char *, char *,
                                        int, struct passwd_buf *, int),
                              show_dir_data(int, int),
                              show_target_dir_only(char *),
                              usage(FILE *, char *);
static int                    same_directory(int *, unsigned int),
                              same_file_filter(int *, unsigned int,
                                               unsigned int),
                              same_options(int *, unsigned int, unsigned int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   unsigned int dir_id;
   int          ret;
   char         dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
                fake_user[MAX_FULL_USER_ID_LENGTH],
                host_name[MAX_HOSTNAME_LENGTH + 1],
                *perm_buffer,
                str_dir_id[MAX_INT_HEX_LENGTH + 1],
                work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);
   p_work_dir = work_dir;

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    _("Failed to get working directory of AFD. (%s %d)\n"),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS)
   {
      usage(stdout, argv[0]);
      exit(0);
   }

   if (get_arg(&argc, argv, "-h", host_name, MAX_HOSTNAME_LENGTH) != SUCCESS)
   {
      if (get_arg(&argc, argv, "-d", dir_alias, MAX_DIR_ALIAS_LENGTH) != SUCCESS)
      {
         if (get_arg(&argc, argv, "-D", str_dir_id, MAX_INT_HEX_LENGTH) != SUCCESS)
         {
            dir_id = 0;
            dir_alias[0] = '\0';
            if (argc == 2)
            {
               if (my_strncpy(host_name, argv[1], MAX_HOSTNAME_LENGTH + 1) == -1)
               {
                  usage(stderr, argv[0]);
                  if (strlen(argv[1]) >= MAX_HOSTNAME_LENGTH)
                  {
                     (void)fprintf(stderr,
                                   _("Given host_alias `%s' is to long (> %d)\n"),
                                   argv[1], MAX_HOSTNAME_LENGTH);
                  }
                  exit(INCORRECT);
               }
            }
            else
            {
               host_name[0] = '\0';
            }
         }
         else
         {
            dir_id = (unsigned int)strtoul(str_dir_id, NULL, 16);
            dir_alias[0] = '\0';
            host_name[0] = '\0';
         }
      }
      else
      {
         dir_id = 0;
         host_name[0] = '\0';
      }
   }
   else
   {
      if (get_arg(&argc, argv, "--only_list_target_dirs", NULL, 0) == SUCCESS)
      {
         only_list_target_dirs = YES;
      }
   }

   /* Check if user may view the password. */
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          _("Failed to access `%s', unable to determine users permissions.\n"),
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS :
         /* Lets evaluate the permissions and see what */
         /* the user may do.                           */
         if ((perm_buffer[0] == 'a') &&
             (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') ||
             (perm_buffer[3] == ' ') ||
             (perm_buffer[3] == '\t')))
         {
            view_passwd = YES;
            free(perm_buffer);
            break;
         }
         else
         {
            if (lposi(perm_buffer, VIEW_DIR_CONFIG_PERM,
                      VIEW_DIR_CONFIG_PERM_LENGTH) == NULL)
            {
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
               exit(INCORRECT);
            }
            if (lposi(perm_buffer, VIEW_PASSWD_PERM,
                      VIEW_PASSWD_PERM_LENGTH) != NULL)
            {
               view_passwd = YES;
            }
         }
         free(perm_buffer);
         break;

      case INCORRECT:
         break;

      default :
         (void)fprintf(stderr, _("Impossible!! Remove the programmer!\n"));
         exit(INCORRECT);
   }

   if ((ret = fsa_attach_passive(NO)) < 0)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("This program is not able to attach to the FSA due to incorrect version!\n"));
      }
      else
      {
         (void)fprintf(stderr, _("Failed to attach to FSA!\n"));
      }
      exit(INCORRECT);
   }
   get_dc_data(host_name, dir_alias, dir_id);
   (void)fsa_detach(NO);

   exit(SUCCESS);
}


/*############################ get_dc_data() ############################*/
static void
get_dc_data(char *host_name, char *dir_alias, unsigned int dir_id)
{
   int         dcl_fd,
               dnb_fd,
               fmd_fd,
               i,
               j,
               jd_fd,
               position,
               pwb_fd;
   size_t      dcl_size = 0,
               dnb_size = 0,
               fmd_size = 0,
               jid_size = 0,
               pwb_size = 0;
   char        file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* First check if the host is in the FSA. */
   if ((host_name[0] != '\0') &&
       ((position = get_host_position(fsa, host_name, no_of_hosts)) == INCORRECT))
   {
      (void)fprintf(stderr, _("Host alias %s is not in FSA. (%s %d)\n"),
                    host_name, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   current_jid_list = NULL;
   no_of_current_jobs = 0;

   (void)get_current_jid_list();

   /* Map to JID database. */
   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if ((jd_fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr,
                 _("Failed to open() `%s' : %s (%s %d)\n"),
                 file, strerror(errno), __FILE__, __LINE__);
      free(current_jid_list);
      return;
   }
   if (fstat(jd_fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      (void)close(jd_fd);
      free(current_jid_list);
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      jid_size = stat_buf.st_size;
      if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, _("Failed to mmap() to %s : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
         free(current_jid_list);
         return;
      }
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
      {
         (void)fprintf(stderr,
                       _("Incorrect JID version (data=%d current=%d)!\n"),
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
         (void)close(jd_fd);
         free(current_jid_list);
         return;
      }
      no_of_job_ids = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
   }
   else
   {
      (void)fprintf(stderr, _("Job ID database file is empty. (%s %d)\n"),
                    __FILE__, __LINE__);
      (void)close(jd_fd);
      free(current_jid_list);
      return;
   }
   if (close(jd_fd) == -1)
   {
      (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
   }

   /* Map to directory name buffer. */
   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((dnb_fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      free(current_jid_list);
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (fstat(dnb_fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      (void)close(dnb_fd);
      free(current_jid_list);
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      dnb_size = stat_buf.st_size;
      if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, _("Failed to mmap() to `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         free(current_jid_list);
         if (jid_size > 0)
         {
            if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
            {
               (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
            }
         }
         return;
      }
      no_of_dirs_in_dnb = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   else
   {
      (void)fprintf(stderr, _("Job ID database file is empty. (%s %d)\n"),
                    __FILE__, __LINE__);
      (void)close(dnb_fd);
      free(current_jid_list);
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (close(dnb_fd) == -1)
   {
      (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
   }

   /* Map to file mask database. */
   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, FILE_MASK_FILE);
   if ((fmd_fd = open(file, O_RDONLY)) != -1)
   {
      if (fstat(fmd_fd, &stat_buf) != -1)
      {
         if (stat_buf.st_size > 0)
         {
            char *ptr;

            if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                            MAP_SHARED, fmd_fd, 0)) != (caddr_t) -1)
            {
               fmd_size = stat_buf.st_size;
               fmd_end = ptr + stat_buf.st_size;
               no_of_file_mask_ids = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               fmd = ptr;
            }
            else
            {
               (void)fprintf(stderr,
                             _("Failed to mmap() to `%s' : %s (%s %d)\n"),
                             file, strerror(errno),
                             __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          _("File mask database file is empty. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
      }
      else
      {
         (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      if (close(fmd_fd) == -1)
      {
         (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }

   /* Map to password buffer. */
   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, PWB_DATA_FILE);
   if ((pwb_fd = open(file, O_RDONLY)) != -1)
   {
      if (fstat(pwb_fd, &stat_buf) != -1)
      {
         if (stat_buf.st_size > 0)
         {
            char *ptr;

            if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                            MAP_SHARED, dnb_fd, 0)) != (caddr_t) -1)
            {
               pwb_size = stat_buf.st_size;
               no_of_passwd = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               pwb = (struct passwd_buf *)ptr;
            }
            else
            {
               (void)fprintf(stderr,
                             _("Failed to mmap() to `%s' : %s (%s %d)\n"),
                             file, strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          _("Password database file is empty. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
      }
      else
      {
         (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      if (close(pwb_fd) == -1)
      {
         (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }

   /* Map to DIR_CONFIG name database. */
   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, DC_LIST_FILE);
   if ((dcl_fd = open(file, O_RDONLY)) != -1)
   {
      if (fstat(dcl_fd, &stat_buf) != -1)
      {
         if (stat_buf.st_size > 0)
         {
            char *ptr;

            if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                            MAP_SHARED, dcl_fd, 0)) != (caddr_t) -1)
            {
               dcl_size = stat_buf.st_size;
               no_of_dc_ids = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               dcl = (struct dir_config_list *)ptr;
            }
            else
            {
               (void)fprintf(stderr,
                             _("Failed to mmap() to `%s' : %s (%s %d)\n"),
                             file, strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          _("File mask database file is empty. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
      }
      else
      {
         (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      if (close(dcl_fd) == -1)
      {
         (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Go through current job list and search the JID structure for
    * the host we are looking for.
    */
   if ((i = fra_attach_passive()) != SUCCESS)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("This program is not able to attach to the FRA due to incorrect version!\n"));
      }
      else
      {
         (void)fprintf(stderr, _("Failed to attach to FRA!\n"));
      }
      exit(INCORRECT);
   }
   if (host_name[0] == '\0')
   {
      if ((dir_alias[0] == '\0') && (dir_id == 0))
      {
         for (i = 0; i < no_of_dirs_in_dnb; i++)
         {
            show_dir_data(i, -1);
         }
      }
      else
      {
         if (dir_id == 0)
         {
            for (i = 0; i < no_of_dirs; i++)
            {
               if (strcmp(dir_alias, fra[i].dir_alias) == 0)
               {
                  for (j = 0; j < no_of_dirs_in_dnb; j++)
                  {
                     if (dnb[j].dir_id == fra[i].dir_id)
                     {
                        show_dir_data(j, i);
                        break;
                     }
                  }
                  break;
               }
            }
         }
         else
         {
            for (i = 0; i < no_of_dirs; i++)
            {
               if (fra[i].dir_id == dir_id)
               {
                  for (j = 0; j < no_of_dirs_in_dnb; j++)
                  {
                     if (dnb[j].dir_id == dir_id)
                     {
                        show_dir_data(j, i);
                        break;
                     }
                  }
                  break;
               }
            }
         }
      }
   }
   else
   {
      /*
       * Lets check both cases, a hostname can be used for retrieving files
       * and sending files. Always show both.
       */
      if ((fsa[position].protocol & RETRIEVE_FLAG) &&
          (only_list_target_dirs == NO))
      {
         for (i = 0; i < no_of_dirs; i++)
         {
            if (strcmp(fra[i].host_alias, host_name) == 0)
            {
               for (j = 0; j < no_of_dirs_in_dnb; j++)
               {
                  if (dnb[j].dir_id == fra[i].dir_id)
                  {
                     show_dir_data(j, i);
                     break;
                  }
               }
            }
         }
      }
      if (fsa[position].protocol & SEND_FLAG)
      {
         for (i = 0; i < no_of_current_jobs; i++)
         {
            for (j = 0; j < no_of_job_ids; j++)
            {
               if (jd[j].job_id == current_jid_list[i])
               {
                  if (strcmp(jd[j].host_alias, host_name) == 0)
                  {
                     if (only_list_target_dirs == YES)
                     {
                        show_target_dir_only(jd[j].recipient);
                     }
                     else
                     {
                        show_data(&jd[j], dnb[jd[j].dir_id_pos].dir_name,
                                  fmd, no_of_passwd, pwb, position);
                     }
                  }
                  break;
               }
            }
         }
      }
   }
   (void)fra_detach();

   /* Free all memory that was allocated or mapped. */
   free(current_jid_list);
   if (dcl_size > 0)
   {
      if (munmap(((char *)dcl - AFD_WORD_OFFSET), dcl_size) < 0)
      {
         (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   if (pwb_size > 0)
   {
      if (munmap(((char *)pwb - AFD_WORD_OFFSET), pwb_size) < 0)
      {
         (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   if (fmd_size > 0)
   {
      if (munmap((fmd - AFD_WORD_OFFSET), fmd_size) < 0)
      {
         (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   if (dnb_size > 0)
   {
      if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) < 0)
      {
         (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   if (jid_size > 0)
   {
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
      {
         (void)fprintf(stderr, _("munmap() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ show_data() ++++++++++++++++++++++++++++++*/
static void
show_data(struct job_id_data *p_jd,
          char               *dir_name,
          char               *fmd,
          int                no_of_passwd,
          struct passwd_buf  *pwb,
          int                position)
{
   int                fml_offset,
                      fra_pos = -1,
                      i,
                      mask_offset;
   char               value[MAX_PATH_LENGTH];
   struct dir_options d_o;

   if (no_of_dc_ids > 1)
   {
      for (i = 0; i < no_of_dc_ids; i++)
      {
         if (dcl[i].dc_id == p_jd->dir_config_id)
         {
            (void)fprintf(stdout, "DIR_CONFIG    : %s\n",
                          dcl[i].dir_config_file);
            break;
         }
      }
   }

   (void)fprintf(stdout, "%s%s\n", VIEW_DC_DIR_IDENTIFIER, dir_name);

   get_dir_options(p_jd->dir_id, &d_o);
   if (d_o.url[0] != '\0')
   {
      (void)strcpy(value, d_o.url);
      url_insert_password(value, (view_passwd == YES) ? NULL : "XXXXX");
      (void)fprintf(stdout, "DIR-URL       : %s\n", value);
   }

   for (i = 0; i < no_of_dirs; i++)
   {
      if (fra[i].dir_id == p_jd->dir_id)
      {
         fra_pos = i;
         break;
      }
   }
   if (fra_pos == -1)
   {
      (void)fprintf(stderr, "Failed to locate `%s' in FRA!\n", dir_name);
   }
   else
   {
      (void)fprintf(stdout, "Dir-alias     : %s\n", fra[fra_pos].dir_alias);
   }

   /* If necessary add directory options. */
   if (d_o.no_of_dir_options > 0)
   {
      (void)fprintf(stdout, "DIR-options   : %s\n", d_o.aoptions[0]);
      for (i = 1; i < d_o.no_of_dir_options; i++)
      {
         (void)fprintf(stdout, "                %s\n", d_o.aoptions[i]);
      }
   }

   if (fmd != NULL)
   {
      char *p_file = NULL,
           *ptr;

      /* Show file filters. */
      ptr = fmd;
      fml_offset = sizeof(int) + sizeof(int);
      mask_offset = fml_offset + sizeof(int) + sizeof(unsigned int) +
                    sizeof(unsigned char);
      for (i = 0; i < no_of_file_mask_ids; i++)
      {
         if (*(unsigned int *)(ptr + fml_offset +
                               sizeof(int)) == p_jd->file_mask_id)
         {
            p_file = ptr + mask_offset;
            break;
         }
         ptr += (mask_offset + *(int *)(ptr + fml_offset) + sizeof(char) +
                 *(ptr + mask_offset - 1));
         if (ptr > fmd_end)
         {
            (void)fprintf(stdout,
                          "Filter        : Unable to locate, database corrupt.\n");
            break;
         }
      }
      if (p_file != NULL)
      {
         (void)fprintf(stdout, "Filter        : %s\n", p_file);
         NEXT(p_file);
         for (i = 1; i < *(int *)ptr; i++)
         {
            (void)fprintf(stdout, "                %s\n", p_file);
            NEXT(p_file);
         }
      }
   }

   /* Print recipient. */
   (void)strcpy(value, p_jd->recipient);
   url_insert_password(value, (view_passwd == YES) ? NULL : "XXXXX");
   (void)fprintf(stdout, "Recipient     : %s\n", value);
   (void)fprintf(stdout, "Real hostname : %s\n",
                 fsa[position].real_hostname[0]);
   if (fsa[position].real_hostname[1][0] != '\0')
   {
      (void)fprintf(stdout, "                %s\n",
                    fsa[position].real_hostname[1]);
   }

   /* Show all AMG options. */
   if (p_jd->no_of_loptions > 0)
   {
      char *ptr = p_jd->loptions;

      (void)fprintf(stdout, "AMG-options   : %s\n", ptr);
      NEXT(ptr);
      for (i = 1; i < p_jd->no_of_loptions; i++)
      {
         (void)fprintf(stdout, "                %s\n", ptr);
         NEXT(ptr);
      }
   }

   /* Show all FD options. */
   if (p_jd->no_of_soptions > 0)
   {
      int  counter = 0;
      char *ptr = p_jd->soptions;

      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         value[counter] = *ptr;
         ptr++; counter++;
      }
      value[counter] = '\0';
      ptr++;
      (void)fprintf(stdout, "FD-options    : %s\n", value);
      for (i = 1; i < p_jd->no_of_soptions; i++)
      {
         counter = 0;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            value[counter] = *ptr;
            ptr++; counter++;
         }
         value[counter] = '\0';
         (void)fprintf(stdout, "                %s\n", value);
         if (*ptr == '\0')
         {
            break;
         }
         ptr++;
      }
   }

   /* Priority. */
   (void)fprintf(stdout, "Priority      : %c\n", p_jd->priority);

   /* Job ID. */
   (void)fprintf(stdout, "Job-ID        : %x\n\n", p_jd->job_id);

   return;
}


/*++++++++++++++++++++++ show_target_dir_only() +++++++++++++++++++++++++*/
static void
show_target_dir_only(char *recipient)
{
   unsigned int scheme;
   char         directory[MAX_RECIPIENT_LENGTH + 1],
                user[MAX_USER_NAME_LENGTH + 1];

   if (url_evaluate(recipient, &scheme, user, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                    NULL, NULL,
#endif
                    NULL, NO, NULL, NULL, directory, NULL, NULL,
                    NULL, NULL, NULL) == 0)
   {
      if ((scheme & FTP_FLAG) || (scheme & LOC_FLAG) || (scheme & HTTP_FLAG) ||
          (scheme & SFTP_FLAG) || (scheme & SCP_FLAG))
      {
         if ((directory[0] == '/') || (user[0] == '\0'))
         {
            (void)fprintf(stdout, "%s\n", directory);
         }
         else
         {
            (void)fprintf(stdout, "~%s/%s\n", user, directory);
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++ show_dir_data() ++++++++++++++++++++++++++++*/
static void
show_dir_data(int dir_pos, int fra_pos)
{
   int  fml_offset,
        i, j,
        job_pos = -1,
        mask_offset;
   char value[MAX_PATH_LENGTH];

   for (i = 0; i < no_of_job_ids; i++)
   {
      if (jd[i].dir_id_pos == dir_pos)
      {
         for (j = 0; j < no_of_current_jobs; j++)
         {
            if (jd[i].job_id == current_jid_list[j])
            {
               job_pos = i;
               i = no_of_job_ids;
               break;
            }
         }
      }
   }
   if (job_pos == -1)
   {
      /* This directory is no longer in the current FSA. */
      return;
   }
   if (fra_pos == -1)
   {
      for (i = 0; i < no_of_dirs; i++)
      {
         if (fra[i].dir_id == dnb[dir_pos].dir_id)
         {
            fra_pos = i;
            break;
         }
      }
      if (fra_pos == -1)
      {
         (void)fprintf(stderr, _("Failed to locate `%s' in FRA!\n"),
                       dnb[dir_pos].orig_dir_name);
         exit(INCORRECT);
      }
   }

   if ((gl = malloc((no_of_job_ids * sizeof(unsigned int)))) == NULL)
   {
      (void)fprintf(stderr, _("malloc() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_gotchas = 0;

   /* Directory entry. */
   (void)strcpy(value, dnb[dir_pos].orig_dir_name);
   url_insert_password(value, (view_passwd == YES) ? NULL : "XXXXX");
   if (fra[fra_pos].in_dc_flag & DIR_ALIAS_IDC)
   {
      (void)fprintf(stdout, "%s %s\n%s\n\n",
                    DIR_IDENTIFIER, fra[fra_pos].dir_alias, value);
   }
   else
   {
      (void)fprintf(stdout, "%s\n%s\n\n", DIR_IDENTIFIER, value);
   }

   /* If necessary add directory options. */
   check_dir_options(fra_pos);

   do
   {
      /* Add file entry. */
      if (fmd != NULL)
      {
         char *p_file = NULL,
              *ptr;

         /* Show file filters. */
         ptr = fmd;
         fml_offset = sizeof(int) + sizeof(int);
         mask_offset = fml_offset + sizeof(int) + sizeof(unsigned int) +
                       sizeof(unsigned char);
         for (i = 0; i < no_of_file_mask_ids; i++)
         {
            if (*(unsigned int *)(ptr + fml_offset +
                                  sizeof(int)) == jd[job_pos].file_mask_id)
            {
               p_file = ptr + mask_offset;
               break;
            }
            ptr += (mask_offset + *(int *)(ptr + fml_offset) + sizeof(char) +
                    *(ptr + mask_offset - 1));
            if (ptr > fmd_end)
            {
               (void)fprintf(stdout,
                             "   %s\n   * # Filter database broken, assuming this filter!!!\n",
                             FILE_IDENTIFIER);
               break;
            }
         }
         if (p_file != NULL)
         {
            (void)fprintf(stdout, "   %s\n   %s\n", FILE_IDENTIFIER, p_file);
            NEXT(p_file);
            for (i = 1; i < *(int *)ptr; i++)
            {
               (void)fprintf(stdout, "   %s\n", p_file);
               NEXT(p_file);
            }
         }
      }

      do
      {
         (void)fprintf(stdout, "\n      %s\n\n         %s\n",
                       DESTINATION_IDENTIFIER, RECIPIENT_IDENTIFIER);

         do
         {
            (void)strcpy(value, jd[job_pos].recipient);
            url_insert_password(value, (view_passwd == YES) ? NULL : "XXXXX");
            (void)fprintf(stdout, "         %s\n", value);
         } while (same_options(&job_pos, jd[job_pos].dir_id, jd[job_pos].file_mask_id));

         /* Show all options. */
         (void)fprintf(stdout, "\n         %s\n         %s %c\n",
                       OPTION_IDENTIFIER, PRIORITY_ID, jd[job_pos].priority);

         /* Show all AMG options. */
         if (jd[job_pos].no_of_loptions > 0)
         {
            char *ptr = jd[job_pos].loptions;

            for (i = 0; i < jd[job_pos].no_of_loptions; i++)
            {
               (void)fprintf(stdout, "         %s\n", ptr);
               NEXT(ptr);
            }
         }

         /* Show all FD options. */
         if (jd[job_pos].no_of_soptions > 0)
         {
            int  counter = 0;
            char *ptr = jd[job_pos].soptions;

            for (i = 0; i < jd[job_pos].no_of_soptions; i++)
            {
               counter = 0;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  value[counter] = *ptr;
                  ptr++; counter++;
               }
               value[counter] = '\0';
               (void)fprintf(stdout, "         %s\n", value);
               if (*ptr == '\0')
               {
                  break;
               }
               ptr++;
            }
         }
         (void)fprintf(stdout, "\n");
      } while (same_file_filter(&job_pos, jd[job_pos].file_mask_id, jd[job_pos].dir_id));
   } while (same_directory(&job_pos, jd[job_pos].dir_id));

   free(gl);

   return;
}


/*++++++++++++++++++++++++++++ same_directory() +++++++++++++++++++++++++*/
static int
same_directory(int *jd_pos, unsigned int dir_id)
{
   int i,
       in_gotcha_list,
       j;

   for (i = 0; i < no_of_job_ids; i++)
   {
      in_gotcha_list = NO;
      for (j = 0; j < no_of_gotchas; j++)
      {
         if (gl[j] == jd[i].job_id)
         {
            in_gotcha_list = YES;
            break;
         }
      }
      if (in_gotcha_list == NO)
      {
         if (jd[i].dir_id == dir_id)
         {
            for (j = 0; j < no_of_current_jobs; j++)
            {
               if (jd[i].job_id == current_jid_list[j])
               {
                  *jd_pos = i;
                  return(1);
               }
            }
         }
      }
   }
   return(0);
}


/*++++++++++++++++++++++++++ same_file_filter() +++++++++++++++++++++++++*/
static int
same_file_filter(int *jd_pos, unsigned int file_mask_id, unsigned int dir_id)
{
   int i,
       in_gotcha_list,
       j;

   for (i = (*jd_pos + 1); i < no_of_job_ids; i++)
   {
      in_gotcha_list = NO;
      for (j = 0; j < no_of_gotchas; j++)
      {
         if (gl[j] == jd[i].job_id)
         {
            in_gotcha_list = YES;
            break;
         }
      }
      if (in_gotcha_list == NO)
      {
         if ((jd[i].dir_id == dir_id) && (jd[i].file_mask_id == file_mask_id))
         {
            for (j = 0; j < no_of_current_jobs; j++)
            {
               if (jd[i].job_id == current_jid_list[j])
               {
                  *jd_pos = i;
                  return(1);
               }
            }
         }
      }
   }
   return(0);
}


/*+++++++++++++++++++++++++++++ same_options() ++++++++++++++++++++++++++*/
static int
same_options(int *jd_pos, unsigned int dir_id, unsigned int file_mask_id)
{
   int current_jd_pos,
       i, j;

   gl[no_of_gotchas] = jd[*jd_pos].job_id;
   no_of_gotchas++;
   current_jd_pos = *jd_pos;
   for (i = (*jd_pos + 1); i < no_of_job_ids; i++)
   {
      if ((jd[i].dir_id == dir_id) &&
          (jd[i].file_mask_id == file_mask_id) &&
          (jd[i].priority == jd[current_jd_pos].priority) &&
          (jd[i].no_of_loptions == jd[current_jd_pos].no_of_loptions) &&
          (jd[i].no_of_soptions == jd[current_jd_pos].no_of_soptions))
      {
         if (jd[i].no_of_soptions > 0)
         {
            if (CHECK_STRCMP(jd[i].soptions, jd[current_jd_pos].soptions) != 0)
            {
               continue;
            }
         }
         if (jd[i].no_of_loptions > 0)
         {
            register int gotcha = NO,
                         k;
            char         *p_loptions_db = jd[current_jd_pos].loptions,
                         *p_loptions_jd = jd[i].loptions;

            for (k = 0; k < jd[i].no_of_loptions; k++)
            {
               if (CHECK_STRCMP(p_loptions_jd, p_loptions_db) != 0)
               {
                  gotcha = YES;
                  break;
               }
               NEXT(p_loptions_db);
               NEXT(p_loptions_jd);
            }
            if (gotcha == YES)
            {
               continue;
            }
         }
         for (j = 0; j < no_of_current_jobs; j++)
         {
            if (jd[i].job_id == current_jid_list[j])
            {
               *jd_pos = i;
               return(1);
            }
         }
      }
   }
   return(0);
}


/*+++++++++++++++++++++++++ check_dir_options() +++++++++++++++++++++++++*/
static void
check_dir_options(int fra_pos)
{
   struct dir_options d_o;

   get_dir_options(fra[fra_pos].dir_id, &d_o);
   if (d_o.no_of_dir_options > 0)
   {
      int i;

      (void)fprintf(stdout, "   %s\n", DIR_OPTION_IDENTIFIER);
      for (i = 0; i < d_o.no_of_dir_options; i++)
      {
         (void)fprintf(stdout, "   %s\n", d_o.aoptions[i]);
      }
      (void)fprintf(stdout, "\n");
   }

   return;
}


/*++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(FILE *stream, char *progname)
{
   (void)fprintf(stream,
                 _("Usage: %s [-d <dir alias>] [-h <host alias> [--only_list_target_dirs]] [-D <dir hex id>]\n"),
                 progname);
   return;
}
