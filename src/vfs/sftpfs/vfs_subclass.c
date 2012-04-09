/* Virtual File System: SFTP file system.
   The VFS subclass functions

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

#include "lib/global.h"
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct vfs_s_subclass sftpfs_subclass;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for checking if connection is equal to existing connection.
 *
 * @param vpath_element connetion
 * @param super data with exists connection
 * @param vpath unused
 * @param cookie unused
 * @return TRUE if connections is equal, FALSE otherwise
 */

static gboolean
sftpfs_cb_is_equal_connection (const vfs_path_element_t * vpath_element, struct vfs_s_super *super,
                            const vfs_path_t * vpath, void *cookie)
{
    int port;
    char *user_name;
    int result;

    (void) vpath;
    (void) cookie;

    if (vpath_element->user != NULL)
        user_name = vpath_element->user;
    else
        user_name = vfs_get_local_username ();

    if (vpath_element->port != 0)
        port = vpath_element->port;
    else
        port = SFTP_DEFAULT_PORT;

    result = ((strcmp (vpath_element->host, super->path_element->host) == 0)
              && (strcmp (user_name, super->path_element->user) == 0)
              && (port == super->path_element->port));

    if (user_name != vpath_element->user)
        g_free (user_name);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftpfs_cb_open_connection (struct vfs_s_super *super,
                     const vfs_path_t * vpath, const vfs_path_element_t * vpath_element)
{
    char *user_name;

    (void) vpath;

    if (vpath_element->host == NULL || *vpath_element->host == '\0')
    {
        vfs_print_message (_("sftp: Invalid host name."));
        vpath_element->class->verrno = EPERM;
        return 0;
    }

    if (vpath_element->user != NULL)
    {
        user_name = vfs_get_local_username ();
        if (user_name == NULL)
        {
            vpath_element->class->verrno = EPERM;
            return 0;
        }
    }

    super->data = g_new0 (sftpfs_super_data_t, 1);
    super->path_element = vfs_path_element_clone (vpath_element);
    if (super->path_element->user == NULL)
        super->path_element->user = user_name;

    sftpfs_fill_connection_data (super);

    return sftpfs_open_connection (super);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of VFS subclass structure.
 *
 * @return the VFS subclass structure.
 */

struct vfs_s_subclass *
sftpfs_init_subclass (void)
{
    memset (&sftpfs_subclass, 0, sizeof (struct vfs_s_subclass));
    sftpfs_subclass.flags = VFS_S_REMOTE;
    sftpfs_subclass.archive_same = sftpfs_cb_is_equal_connection;
    sftpfs_subclass.open_archive = sftpfs_cb_open_connection;
    /*
       sftpfs_subclass.free_archive = sftpfs_free_archive;
       sftpfs_subclass.dir_load = sftpfs_dir_load;
     */
    return &sftpfs_subclass;
}

/* --------------------------------------------------------------------------------------------- */
