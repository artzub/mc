/* Virtual File System: SFTP file system.
   The internal functions: files

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2011
   Slava Zanko <slavazanko@gmail.com>, 2011, 2012

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    LIBSSH2_SFTP_HANDLE *handle;
    int flags;
    mode_t mode;
} sftpfs_file_handler_data_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline const char *
sftpfs_fix_filename (const char *file_name)
{
    g_string_printf (sftpfs_filename_buffer, "%c%s", PATH_SEP, file_name);
    return sftpfs_filename_buffer->str;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Open new SFTP file.
 *
 * @param file_handler the file handler data
 * @param flags flags (see man 2 open)
 * @param mode mode (see man 2 open)
 * return TRUE if connection was created successfully, FALSE otherwise
 */

gboolean
sftpfs_open_file (vfs_file_handler_t * file_handler, int flags, mode_t mode)
{
    unsigned long sftp_open_flags = 0;
    int sftp_open_mode = 0;
    sftpfs_file_handler_data_t *file_handler_data;
    sftpfs_super_data_t *super_data;
    char *name;

    (void) mode;

    super_data = (sftpfs_super_data_t *) file_handler->ino->super->data;
    file_handler_data = g_new0 (sftpfs_file_handler_data_t, 1);

    if ((flags & O_CREAT) != 0 || (flags & O_WRONLY) != 0)
    {
        sftp_open_flags = ((flags & O_WRONLY) != 0 ? LIBSSH2_FXF_WRITE : 0) |
            ((flags & O_CREAT) != 0 ? LIBSSH2_FXF_CREAT : 0) |
            ((flags & ~O_APPEND) != 0 ? LIBSSH2_FXF_TRUNC : 0);
        sftp_open_mode = LIBSSH2_SFTP_S_IRUSR |
            LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH;
    }
    else
        sftp_open_flags = LIBSSH2_FXF_READ;

    name = vfs_s_fullpath (&sftpfs_class, file_handler->ino);
    if (name == NULL)
    {
        g_free (file_handler_data);
        return FALSE;
    }

    file_handler_data->handle =
        libssh2_sftp_open (super_data->sftp_session, sftpfs_fix_filename (name),
                           sftp_open_flags, sftp_open_mode);
    g_free (name);

    if (file_handler_data->handle == NULL)
    {
        g_free (file_handler_data);
        return FALSE;
    }

    file_handler_data->flags = flags;
    file_handler_data->mode = mode;
    file_handler->data = file_handler_data;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

