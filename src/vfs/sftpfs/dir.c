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
} sftpfs_dir_data_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void *
sftpfs_opendir (const vfs_path_t * vpath)
{
    sftpfs_dir_data_t *sftpfs_dir;
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    const vfs_path_element_t *path_element;
    LIBSSH2_SFTP_HANDLE *handle;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return NULL;

    super_data = (sftpfs_super_data_t *) super->data;

    handle =
        libssh2_sftp_opendir (super_data->sftp_session, sftpfs_fix_filename (path_element->path));
    if (handle == NULL)
    {
        if (libssh2_session_last_errno (super_data->session) != LIBSSH2_ERROR_EAGAIN)
            return NULL;
        sftpfs_waitsocket (super_data);
        if (handle == NULL)
            return NULL;
    }

    sftpfs_dir = g_new0 (sftpfs_dir_data_t, 1);
    sftpfs_dir->handle = handle;

    return (void *) sftpfs_dir;
}

/* --------------------------------------------------------------------------------------------- */

void *
sftpfs_readdir (void *data)
{
    char mem[BUF_MEDIUM];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    sftpfs_dir_data_t *sftpfs_dir = (sftpfs_dir_data_t *) data;

    static union vfs_dirent sftpfs_dirent;

    int rc;

    rc = libssh2_sftp_readdir (sftpfs_dir->handle, mem, sizeof (mem), &attrs);

    /* rc is the length of the file name in the mem buffer */
    if (rc <= 0)
    {
        vfs_print_message (_("sftp: Listing done."));
        return NULL;
    }

    if (mem[0] != '\0')
        vfs_print_message (_("sftp: (Ctrl-G break) Listing... %s"), mem);

    g_strlcpy (sftpfs_dirent.dent.d_name, mem, BUF_MEDIUM);
    compute_namelen (&sftpfs_dirent.dent);
    return &sftpfs_dirent;
}

/* --------------------------------------------------------------------------------------------- */

int
sftpfs_closedir (void *data)
{
    int ret;
    sftpfs_dir_data_t *sftpfs_dir = (sftpfs_dir_data_t *) data;
    ret = libssh2_sftp_closedir (sftpfs_dir->handle);
    g_free (sftpfs_dir);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
