/* Virtual File System: SFTP file system.
   The internal functions

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
#include <errno.h>

#include "lib/global.h"

#include "internal.h"

/*** global variables ****************************************************************************/

GString *sftpfs_filename_buffer = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const char *
sftpfs_fix_filename (const char *file_name)
{
    g_string_printf (sftpfs_filename_buffer, "%c%s", PATH_SEP, file_name);
    return sftpfs_filename_buffer->str;
}

/* --------------------------------------------------------------------------------------------- */

int
sftpfs_waitsocket (sftpfs_super_data_t * super_data)
{
    struct timeval timeout = { 10, 0 };
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    FD_ZERO (&fd);
    FD_SET (super_data->socket_handle, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions (super_data->session);

    if ((dir & LIBSSH2_SESSION_BLOCK_INBOUND) != 0)
        readfd = &fd;

    if ((dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0)
        writefd = &fd;

    return select (super_data->socket_handle + 1, readfd, writefd, NULL, &timeout);
}

/* --------------------------------------------------------------------------------------------- */

int
sftpfs_lstat (const vfs_path_t * vpath, struct stat *buf)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL)
        return -1;

    super_data = (sftpfs_super_data_t *) super->data;
    if (super_data->sftp_session == NULL)
        return -1;

    res = libssh2_sftp_stat_ex (super_data->sftp_session, sftpfs_fix_filename (path_element->path),
                                sftpfs_filename_buffer->len, LIBSSH2_SFTP_LSTAT, &attrs);

    if (res < 0)
    {
        char *err = NULL;
        int err_len;

        libssh2_session_last_error (super_data->session, &err, &err_len, 1);
        g_free (err);
        return -1;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
sftpfs_stat (const vfs_path_t * vpath, struct stat *buf)
{
    struct vfs_s_super *super;
    sftpfs_super_data_t *super_data;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int res;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    if (vfs_s_get_path (vpath, &super, 0) == NULL)
        return -1;

    if (super == NULL)
        return -1;

    super_data = (sftpfs_super_data_t *) super->data;
    if (super_data->sftp_session == NULL)
        return -1;

    res = libssh2_sftp_stat_ex (super_data->sftp_session, sftpfs_fix_filename (path_element->path),
                                sftpfs_filename_buffer->len, LIBSSH2_SFTP_STAT, &attrs);

    if (res < 0)
    {
        char *err = NULL;
        int err_len;

        libssh2_session_last_error (super_data->session, &err, &err_len, 1);
        g_free (err);
        return -1;
    }

    buf->st_nlink = 1;
    if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        buf->st_uid = attrs.uid;
        buf->st_gid = attrs.gid;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        buf->st_atime = attrs.atime;
        buf->st_mtime = attrs.mtime;
        buf->st_ctime = attrs.mtime;
    }

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        buf->st_size = attrs.filesize;

    if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        buf->st_mode = attrs.permissions;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
