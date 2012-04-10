/* Virtual File System: SFTP file system.
   The VFS class functions

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
#include "lib/vfs/gc.h"

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_class sftpfs_class;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for VFS-class init action.
 *
 * @param me structure of VFS class
 */
static int
sftpfs_cb_init (struct vfs_class *me)
{
    (void) me;

    if (libssh2_init (0) != 0)
        return 0;

    sftpfs_filename_buffer = g_string_new ("");
    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for VFS-class deinit action.
 *
 * @param me structure of VFS class
 */

static void
sftpfs_cb_done (struct vfs_class *me)
{
    (void) me;

    g_string_free (sftpfs_filename_buffer, TRUE);
    libssh2_exit ();
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for opening connection.
 *
 * @param vpath connection descriptor
 * @param flags flags (see man 2 open)
 * @param mode mode (see man 2 open)
 * @return the file handler data if success, NULL otherwise
 */

static void *
sftpfs_cb_open (const vfs_path_t * vpath, int flags, mode_t mode)
{
    vfs_file_handler_t *file_handler;
    const vfs_path_element_t *path_element;
    struct vfs_s_super *super;
    const char *path_super;
    struct vfs_s_inode *path_inode;
    gboolean is_changed = FALSE;

    path_element = vfs_path_get_by_index (vpath, -1);

    path_super = vfs_s_get_path (vpath, &super, 0);
    if (path_super == NULL)
        return NULL;

    path_inode = vfs_s_find_inode (path_element->class, super, path_super, LINK_FOLLOW, FL_NONE);
    if (path_inode != NULL && ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)))
    {
        path_element->class->verrno = EEXIST;
        return NULL;
    }

    if (path_inode == NULL)
    {
        char *dirname, *name;
        struct vfs_s_entry *ent;
        struct vfs_s_inode *dir;

        dirname = g_path_get_dirname (path_super);
        name = g_path_get_basename (path_super);
        dir = vfs_s_find_inode (path_element->class, super, dirname, LINK_FOLLOW, FL_DIR);
        if (dir == NULL)
        {
            g_free (dirname);
            g_free (name);
            return NULL;
        }
        ent = vfs_s_generate_entry (path_element->class, name, dir, 0755);
        path_inode = ent->ino;
        vfs_s_insert_entry (path_element->class, dir, ent);
        g_free (dirname);
        g_free (name);
        is_changed = TRUE;
    }

    if (S_ISDIR (path_inode->st.st_mode))
    {
        path_element->class->verrno = EISDIR;
        return NULL;
    }

    file_handler = g_new0 (vfs_file_handler_t, 1);
    file_handler->pos = 0;
    file_handler->ino = path_inode;
    file_handler->handle = -1;
    file_handler->changed = is_changed;
    file_handler->linear = 0;
    file_handler->data = NULL;

    if (!sftpfs_open_file (file_handler, flags, mode))
    {
        g_free (file_handler);
        return NULL;
    }

    vfs_rmstamp (path_element->class, (vfsid) super);
    super->fd_usage++;
    file_handler->ino->st.st_nlink++;
    return file_handler;
}


/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS class structure.
 *
 * @return the VFS class structure.
 */

struct vfs_class *
sftpfs_init_class (void)
{
    memset (&sftpfs_class, 0, sizeof (struct vfs_class));
    sftpfs_class.name = "sftpfs";
    sftpfs_class.prefix = "sftp";
    sftpfs_class.flags = VFSF_NOLINKS;

    sftpfs_class.init = sftpfs_cb_init;
    sftpfs_class.done = sftpfs_cb_done;
    sftpfs_class.open = sftpfs_cb_open;
    /*
       sftpfs_class.read = sftpfs_read;
       sftpfs_class.write = sftpfs_write;
       sftpfs_class.close = sftpfs_close;
       sftpfs_class.opendir = sftpfs_opendir;
       sftpfs_class.readdir = sftpfs_readdir;
       sftpfs_class.closedir = sftpfs_closedir;
       sftpfs_class.stat = sftpfs_stat;
       sftpfs_class.lstat = sftpfs_lstat;
       sftpfs_class.fstat = sftpfs_fstat;
       sftpfs_class.chmod = sftpfs_chmod;
       sftpfs_class.chown = sftpfs_chown;
       sftpfs_class.utime = sftpfs_utime;
       sftpfs_class.readlink = sftpfs_readlink;
       sftpfs_class.symlink = sftpfs_symlink;
       sftpfs_class.link = sftpfs_link;
       sftpfs_class.unlink = sftpfs_unlink;
       sftpfs_class.rename = sftpfs_rename;
       sftpfs_class.ferrno = sftpfs_errno;
       sftpfs_class.lseek = sftpfs_lseek;
       sftpfs_class.mknod = sftpfs_mknod;
       sftpfs_class.mkdir = sftpfs_mkdir;
       sftpfs_class.rmdir = sftpfs_rmdir;
     */
    return &sftpfs_class;
}

/* --------------------------------------------------------------------------------------------- */
