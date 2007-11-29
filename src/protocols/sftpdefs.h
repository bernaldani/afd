/*
 *  sftpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005, 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __sftpdefs_h
#define __sftpdefs_h

#define MAX_SFTP_REPLY_BUFFER               10
#define MAX_PENDING_WRITE_BUFFER            786432    /* 768 KBytes */
#define MAX_PENDING_WRITES                  (MAX_PENDING_WRITE_BUFFER / 16384)
#define MAX_SFTP_MSG_LENGTH                 MAX_TRANSFER_BLOCKSIZE

#define SFTP_WRITE_FILE                     1 /* Open file for reading.  */
#define SFTP_READ_FILE                      2 /* Open file for writting. */

#define SSH_FILEXFER_VERSION                6

#define SSH_FXP_INIT                        1          /* 3 - 6+  */
#define SSH_FXP_VERSION                     2          /* 3 - 6+  */
#define SSH_FXP_OPEN                        3          /* 3 - 6+  */
#define SSH_FXP_CLOSE                       4          /* 3 - 6+  */
#define SSH_FXP_READ                        5          /* 3 - 6+  */
#define SSH_FXP_WRITE                       6          /* 3 - 6+  */
#define SSH_FXP_LSTAT                       7          /* 3 - 6+  */
#define SSH_FXP_FSTAT                       8          /* 3 - 6+  */
#define SSH_FXP_SETSTAT                     9          /* 3 - 6+  */
#define SSH_FXP_FSETSTAT                    10         /* 3 - 6+  */
#define SSH_FXP_OPENDIR                     11         /* 3 - 6+  */
#define SSH_FXP_READDIR                     12         /* 3 - 6+  */
#define SSH_FXP_REMOVE                      13         /* 3 - 6+  */
#define SSH_FXP_MKDIR                       14         /* 3 - 6+  */
#define SSH_FXP_RMDIR                       15         /* 3 - 6+  */
#define SSH_FXP_REALPATH                    16         /* 3 - 6+  */
#define SSH_FXP_STAT                        17         /* 3 - 6+  */
#define SSH_FXP_RENAME                      18         /* 3 - 6+  */
#define SSH_FXP_READLINK                    19         /* 3 - 6+  */
#define SSH_FXP_SYMLINK                     20         /* 3       */
#define SSH_FXP_LINK                        21         /* 6+      */
#define SSH_FXP_BLOCK                       22         /* 6+      */
#define SSH_FXP_UNBLOCK                     23         /* 6+      */

/* Response types. */
#define SSH_FXP_STATUS                      101        /* 3 - 6+  */
#define SSH_FXP_HANDLE                      102        /* 3 - 6+  */
#define SSH_FXP_DATA                        103        /* 3 - 6+  */
#define SSH_FXP_NAME                        104        /* 3 - 6+  */
#define SSH_FXP_ATTRS                       105        /* 3 - 6+  */

#define SSH_FXP_EXTENDED                    200        /* 3 - 6+  */
#define SSH_FXP_EXTENDED_REPLY              201        /* 3 - 6+  */

/* Possible flags for renaming. */
#define SSH_FXF_RENAME_OVERWRITE            0x00000001 /* 6+      */
#define SSH_FXF_RENAME_ATOMIC               0x00000002 /* 6+      */
#define SSH_FXF_RENAME_NATIVE               0x00000004 /* 6+      */

/* Flags for opening a file. */
#define SSH_FXF_ACCESS_DISPOSITION          0x00000007 /* 6+      */
#define     SSH_FXF_CREATE_NEW              0x00000000 /* 6+      */
#define     SSH_FXF_CREATE_TRUNCATE         0x00000001 /* 6+      */
#define     SSH_FXF_OPEN_EXISTING           0x00000002 /* 6+      */
#define     SSH_FXF_OPEN_OR_CREATE          0x00000003 /* 6+      */
#define     SSH_FXF_TRUNCATE_EXISTING       0x00000004 /* 6+      */
#define SSH_FXF_APPEND_DATA                 0x00000008 /* 6+      */
#define SSH_FXF_APPEND_DATA_ATOMIC          0x00000010 /* 6+      */
#define SSH_FXF_TEXT_MODE                   0x00000020 /* 6+      */
#define SSH_FXF_BLOCK_READ                  0x00000040 /* 6+      */
#define SSH_FXF_BLOCK_WRITE                 0x00000080 /* 6+      */
#define SSH_FXF_BLOCK_DELETE                0x00000100 /* 6+      */
#define SSH_FXF_BLOCK_ADVISORY              0x00000200 /* 6+      */
#define SSH_FXF_NOFOLLOW                    0x00000400 /* 6+      */
#define SSH_FXF_DELETE_ON_CLOSE             0x00000800 /* 6+      */
#define SSH_FXF_ACCESS_AUDIT_ALARM_INFO     0x00001000 /* 6+      */
#define SSH_FXF_ACCESS_BACKUP               0x00002000 /* 6+      */
#define SSH_FXF_BACKUP_STREAM               0x00004000 /* 6+      */
#define SSH_FXF_OVERRIDE_OWNER              0x00008000 /* 6+      */

/* Definitions for older protocol versions. */
#define SSH_FXF_READ                        0x00000001 /* 3       */
#define SSH_FXF_WRITE                       0x00000002 /* 3       */
#define SSH_FXF_APPEND                      0x00000004 /* 3       */
#define SSH_FXF_CREAT                       0x00000008 /* 3       */
#define SSH_FXF_TRUNC                       0x00000010 /* 3       */
#define SSH_FXF_EXCL                        0x00000020 /* 3       */

/* Access mask. */
#define ACE4_READ_DATA                      0x00000001 /* 6+      */
#define ACE4_LIST_DIRECTORY                 0x00000001 /* 6+      */
#define ACE4_WRITE_DATA                     0x00000002 /* 6+      */
#define ACE4_ADD_FILE                       0x00000002 /* 6+      */
#define ACE4_APPEND_DATA                    0x00000004 /* 6+      */
#define ACE4_ADD_SUBDIRECTORY               0x00000004 /* 6+      */
#define ACE4_READ_NAMED_ATTRS               0x00000008 /* 6+      */
#define ACE4_WRITE_NAMED_ATTRS              0x00000010 /* 6+      */
#define ACE4_EXECUTE                        0x00000020 /* 6+      */
#define ACE4_DELETE_CHILD                   0x00000040 /* 6+      */
#define ACE4_READ_ATTRIBUTES                0x00000080 /* 6+      */
#define ACE4_WRITE_ATTRIBUTES               0x00000100 /* 6+      */
#define ACE4_DELETE                         0x00010000 /* 6+      */
#define ACE4_READ_ACL                       0x00020000 /* 6+      */
#define ACE4_WRITE_ACL                      0x00040000 /* 6+      */
#define ACE4_WRITE_OWNER                    0x00080000 /* 6+      */
#define ACE4_SYNCHRONIZE                    0x00100000 /* 6+      */

/* Types of files. */
#define SSH_FILEXFER_TYPE_REGULAR           1          /* 6+      */
#define SSH_FILEXFER_TYPE_DIRECTORY         2          /* 6+      */
#define SSH_FILEXFER_TYPE_SYMLINK           3          /* 6+      */
#define SSH_FILEXFER_TYPE_SPECIAL           4          /* 6+      */
#define SSH_FILEXFER_TYPE_UNKNOWN           5          /* 6+      */
#define SSH_FILEXFER_TYPE_SOCKET            6          /* 6+      */
#define SSH_FILEXFER_TYPE_CHAR_DEVICE       7          /* 6+      */
#define SSH_FILEXFER_TYPE_BLOCK_DEVICE      8          /* 6+      */
#define SSH_FILEXFER_TYPE_FIFO              9          /* 6+      */

/* File attribute flags. */                            /* Version */
#define SSH_FILEXFER_ATTR_SIZE              0x00000001 /* 3 - 6+  */
#define SSH_FILEXFER_ATTR_UIDGID            0x00000002 /* 3       */
#define SSH_FILEXFER_ATTR_PERMISSIONS       0x00000004 /* 3 - 6+  */
#define SSH_FILEXFER_ATTR_ACMODTIME         0x00000008 /* 3       */
#define SSH_FILEXFER_ATTR_ACCESSTIME        0x00000008 /* 6+      */
#define SSH_FILEXFER_ATTR_CREATETIME        0x00000010 /* 6+      */
#define SSH_FILEXFER_ATTR_MODIFYTIME        0x00000020 /* 6+      */
#define SSH_FILEXFER_ATTR_ACL               0x00000040 /* 6+      */
#define SSH_FILEXFER_ATTR_OWNERGROUP        0x00000080 /* 6+      */
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES   0x00000100 /* 6+      */
#define SSH_FILEXFER_ATTR_BITS              0x00000200 /* 6+      */
#define SSH_FILEXFER_ATTR_ALLOCATION_SIZE   0x00000400 /* 6+      */
#define SSH_FILEXFER_ATTR_TEXT_HINT         0x00000800 /* 6+      */
#define SSH_FILEXFER_ATTR_MIME_TYPE         0x00001000 /* 6+      */
#define SSH_FILEXFER_ATTR_LINK_COUNT        0x00002000 /* 6+      */
#define SSH_FILEXFER_ATTR_UNTRANSLATED_NAME 0x00004000 /* 6+      */
#define SSH_FILEXFER_ATTR_CTIME             0x00008000 /* 6+      */
#define SSH_FILEXFER_ATTR_EXTENDED          0x80000000 /* 3 - 6+  */

/* Error codes. */
#define SSH_FX_OK                           0          /* 3 - 6+  */
#define SSH_FX_EOF                          1          /* 3 - 6+  */
#define SSH_FX_NO_SUCH_FILE                 2          /* 3 - 6+  */
#define SSH_FX_PERMISSION_DENIED            3          /* 3 - 6+  */
#define SSH_FX_FAILURE                      4          /* 3 - 6+  */
#define SSH_FX_BAD_MESSAGE                  5          /* 3 - 6+  */
#define SSH_FX_NO_CONNECTION                6          /* 3 - 6+  */
#define SSH_FX_CONNECTION_LOST              7          /* 3 - 6+  */
#define SSH_FX_OP_UNSUPPORTED               8          /* 3 - 6+  */
#define SSH_FX_INVALID_HANDLE               9          /* 6+      */
#define SSH_FX_NO_SUCH_PATH                 10         /* 6+      */
#define SSH_FX_FILE_ALREADY_EXISTS          11         /* 6+      */
#define SSH_FX_WRITE_PROTECT                12         /* 6+      */
#define SSH_FX_NO_MEDIA                     13         /* 6+      */
#define SSH_FX_NO_SPACE_ON_FILESYSTEM       14         /* 6+      */
#define SSH_FX_QUOTA_EXCEEDED               15         /* 6+      */
#define SSH_FX_UNKNOWN_PRINCIPAL            16         /* 6+      */
#define SSH_FX_LOCK_CONFLICT                17         /* 6+      */
#define SSH_FX_DIR_NOT_EMPTY                18         /* 6+      */
#define SSH_FX_NOT_A_DIRECTORY              19         /* 6+      */
#define SSH_FX_INVALID_FILENAME             20         /* 6+      */
#define SSH_FX_LINK_LOOP                    21         /* 6+      */
#define SSH_FX_CANNOT_DELETE                22         /* 6+      */
#define SSH_FX_INVALID_PARAMETER            23         /* 6+      */
#define SSH_FX_FILE_IS_A_DIRECTORY          24         /* 6+      */
#define SSH_FX_BYTE_RANGE_LOCK_CONFLICT     25         /* 6+      */
#define SSH_FX_BYTE_RANGE_LOCK_REFUSED      26         /* 6+      */
#define SSH_FX_DELETE_PENDING               27         /* 6+      */
#define SSH_FX_FILE_CORRUPT                 28         /* 6+      */
#define SSH_FX_OWNER_INVALID                29         /* 6+      */
#define SSH_FX_GROUP_INVALID                30         /* 6+      */
#define SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK  31         /* 6+      */

/* Storage for whats returned in SSH_FXP_NAME reply. */
struct name_list
       {
          char         *name;
          struct stat  stat_buf;
          unsigned int stat_flag;
       };

struct stored_messages
       {
          unsigned int request_id;
          unsigned int message_length;
          char         *sm_buffer;      /* Stored message buffer. */
       };

struct sftp_connect_data
       {
          unsigned int           version;
          unsigned int           request_id;
          unsigned int           stored_replies;
          unsigned int           file_handle_length;
          unsigned int           dir_handle_length;
          unsigned int           stat_flag;
          unsigned int           pending_write_id[MAX_PENDING_WRITES];
          unsigned int           nl_pos;     /* Name list position. */
          unsigned int           nl_length;  /* Name list length. */
          int                    pending_write_counter;
          int                    max_pending_writes;
          off_t                  file_offset;
          char                   *cwd;           /* Current working dir. */
          char                   *file_handle;
          char                   *dir_handle;
          struct name_list       *nl;
          struct stat            stat_buf;
          struct stored_messages sm[MAX_SFTP_REPLY_BUFFER];
          char                   debug;
       };


/* Function prototypes */
extern unsigned int sftp_version(void);
extern int          sftp_cd(char *, int),
#ifdef WITH_SSH_FINGERPRINT
                    sftp_connect(char *, int, unsigned char, char *, char *, char *, char),
#else
                    sftp_connect(char *, int, unsigned char, char *, char *, char),
#endif
                    sftp_close_dir(void),
                    sftp_close_file(void),
                    sftp_dele(char *),
                    sftp_flush(void),
                    sftp_mkdir(char *),
                    sftp_move(char *, char *, int),
                    sftp_noop(void),
                    sftp_open_dir(char *, char),
                    sftp_open_file(int, char *, off_t, mode_t *, int, int *, char),
                    sftp_pwd(void),
                    sftp_read(char *, int),
                    sftp_readdir(char *, struct stat *),
                    sftp_stat(char *, struct stat *),
                    sftp_write(char *, int);
extern void         sftp_quit(void);

#endif /* __sftpdefs_h */
