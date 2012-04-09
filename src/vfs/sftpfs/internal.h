/**
 * \file
 * \brief Header: SFTP FS
 */

#ifndef MC__VFS_SFTPFS_INTERNAL_H
#define MC__VFS_SFTPFS_INTERNAL_H

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/vfs/vfs.h"
#include "lib/vfs/xdirentry.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SFTP_DEFAULT_PORT 22

/*** enums ***************************************************************************************/

typedef enum
{
    NONE = 0,
    PUBKEY = (1 << 0),
    PASSWORD = (1 << 1)
} sftpfs_auth_type_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    sftpfs_auth_type_t auth_type;

    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;

    LIBSSH2_AGENT *agent;

    char *pubkey;
    char *privkey;

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
void sftpfs_fill_connection_data_from_config (struct vfs_s_super *super);
int sftpfs_open_connection (struct vfs_s_super *super);

/*** inline functions ****************************************************************************/
#endif /* MC__VFS_SFTPFS_INTERNAL_H */
