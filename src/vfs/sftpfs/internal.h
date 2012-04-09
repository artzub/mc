/**
 * \file
 * \brief Header: SFTP FS
 */

#ifndef MC__VFS_SFTPFS_INTERNAL_H
#define MC__VFS_SFTPFS_INTERNAL_H

#include "lib/vfs/vfs.h"
#include "lib/vfs/xdirentry.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SFTP_DEFAULT_PORT 22

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    int auth_pw;

    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;

    int socket_handle;
    const char *fingerprint;
} sftpfs_super_data_t;


/*** global variables defined in .c file *********************************************************/

extern GString *sftpfs_filename_buffer;
extern struct vfs_class sftpfs_class;

/*** declarations of public functions ************************************************************/

struct vfs_class *sftpfs_init_class (void);
struct vfs_s_subclass *sftpfs_init_subclass (void);
gboolean sftpfs_open_file (vfs_file_handler_t * file_handler, int flags, mode_t mode);
void sftpfs_fill_connection_data (struct vfs_s_super *super);

/*** inline functions ****************************************************************************/
#endif /* MC__VFS_SFTPFS_INTERNAL_H */
