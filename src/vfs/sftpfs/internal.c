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

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"
#include "lib/vfs/netutil.h"

#include "internal.h"

/*** global variables ****************************************************************************/

GString *sftpfs_filename_buffer = NULL;

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

static int
sftpfs_open_socket (struct vfs_s_super *super)
{
    struct addrinfo hints, *res, *curr_res;
    int my_socket = 0;
    char *host = NULL;
    char port[BUF_TINY];
    int e;

    if (super->path_element->host == NULL || *super->path_element->host == '\0')
    {
        vfs_print_message (_("sftp: Invalid host name."));
        g_free (host);
        return -1;
    }

    sprintf (port, "%hu", (unsigned short) super->path_element->port);
    if (port == NULL)
    {
        vfs_print_message (_("sftp: Invalid port value."));
        return -1;
    }

    tty_enable_interrupt_key ();        /* clear the interrupt flag */

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

#ifdef AI_ADDRCONFIG
    /* By default, only look up addresses using address types for
     * which a local interface is configured (i.e. no IPv6 if no IPv6
     * interfaces, likewise for IPv4 (see RFC 3493 for details). */
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    e = getaddrinfo (super->path_element->host, port, &hints, &res);

#ifdef AI_ADDRCONFIG
    if (e == EAI_BADFLAGS)
    {
        /* Retry with no flags if AI_ADDRCONFIG was rejected. */
        hints.ai_flags = 0;
        e = getaddrinfo (super->path_element->host, port, &hints, &res);
    }
#endif

    if (e != 0)
    {
        tty_disable_interrupt_key ();
        vfs_print_message (_("sftp: %s"), gai_strerror (e));
        return -1;
    }

    for (curr_res = res; curr_res != NULL; curr_res = curr_res->ai_next)
    {
        my_socket = socket (curr_res->ai_family, curr_res->ai_socktype, curr_res->ai_protocol);

        if (my_socket < 0)
        {
            if (curr_res->ai_next != NULL)
                continue;

            tty_disable_interrupt_key ();
            vfs_print_message (_("sftp: %s"), unix_error_string (errno));
            freeaddrinfo (res);
            sftpfs_errno_int = errno;
            return -1;
        }

        vfs_print_message (_("sftp: making connection to %s"), super->path_element->host);

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

        sftpfs_errno_int = errno;
        close (my_socket);

        if (errno == EINTR && tty_got_interrupt ())
            vfs_print_message (_("sftp: connection interrupted by user"));
        else if (res->ai_next == NULL)
            vfs_print_message (_("sftp: connection to server failed: %s"),
                               unix_error_string (errno));
        else
            continue;

        freeaddrinfo (res);
        tty_disable_interrupt_key ();
        return -1;
    }

    freeaddrinfo (res);
    tty_disable_interrupt_key ();
    return my_socket;
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

void
sftpfs_fill_connection_data (struct vfs_s_super *super)
{
    
}

/* --------------------------------------------------------------------------------------------- */

int
sftpfs_open_connection (struct vfs_s_super *super)
{
    int rc;
    char *userauthlist;
    sftpfs_super_data_t *super_data;

    super_data = (sftpfs_super_data_t *) super->data;

    /* TODO: shouled we call init on any connection? */
    if (libssh2_init (0) != 0)
        return -1;

    /* Create a session instance */
    super_data->session = libssh2_session_init ();
    if (super_data->session == NULL)
        return -1;

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    super_data->socket_handle = sftpfs_open_socket (super);

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_startup (super_data->session, super_data->socket_handle);
    if (rc != 0)
    {
        vfs_print_message (_("sftp: Failure establishing SSH session: (%d)"), rc);
        goto err_conn;
    }
!!!!!!!!!!!!!!!!!!!
    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    SUP->fingerprint = libssh2_hostkey_hash (SUP->session, LIBSSH2_HOSTKEY_HASH_SHA1);
    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list (SUP->session, super->path_element->user,
                                          strlen (super->path_element->user));

    if (sftpfs_auth_method == SFTP_AUTH_AUTO)
    {
        if (strstr (userauthlist, "password") != NULL)
            SUP->auth_pw |= 1;
        if (strstr (userauthlist, "keyboard-interactive") != NULL)
            SUP->auth_pw |= 2;
        if (strstr(userauthlist, "publickey") != NULL)
            SUP->auth_pw |= 4;
    }
    else
    {
        SUP->auth_pw = 1;
        if (sftpfs_auth_method == SFTP_AUTH_PUBLICKEY)
            SUP->auth_pw = 4;
        if (sftpfs_auth_method == SFTP_AUTH_SSHAGENT)
            SUP->auth_pw = 8;

    }

    if (super->path_element->password == NULL && (SUP->auth_pw < 8))
    {
        char *p;

        if ((SUP->auth_pw & 4) != 0)
            p = g_strconcat (_("sftp: Enter passphrase for"), " ", super->path_element->user,
                             " ", NULL);
        else
            p = g_strconcat (_("sftp: Enter password for"), " ", super->path_element->user,
                             " ", NULL);
        super->path_element->password = vfs_get_password (p);
        g_free (p);

        if (super->path_element->password == NULL)
        {
            vfs_print_message (_("sftp: Password is empty."));
            goto err_conn;
        }
    }

    if ((SUP->auth_pw & 8) != 0)
    {
        LIBSSH2_AGENT *agent = NULL;
        struct libssh2_agent_publickey *identity, *prev_identity = NULL;

        /* Connect to the ssh-agent */
        agent = libssh2_agent_init (SUP->session);
        if (!agent)
        {
            vfs_print_message (_("sftp: Failure initializing ssh-agent support"));
            goto err_conn;
        }
        if (libssh2_agent_connect (agent))
        {
            vfs_print_message (_("sftp: Failure connecting to ssh-agent"));
            goto err_conn;
        }
        if (libssh2_agent_list_identities (agent))
        {
            vfs_print_message (_("sftp: Failure requesting identities to ssh-agent"));
            goto err_conn;
        }
        while (TRUE)
        {
            rc = libssh2_agent_get_identity (agent, &identity, prev_identity);
            if (rc == 1)
                break;
            if (rc < 0)
            {
                vfs_print_message (_("sftp: Failure obtaining identity from ssh-agent support"));
                goto err_conn;
            }
            if (libssh2_agent_userauth (agent, super->path_element->user, identity))
            {
                vfs_print_message (_("sftp: Authentication with public key %s failed"),
                    identity->comment);
            }
            else
                break;

            prev_identity = identity;
        }
    }
    else if ((SUP->auth_pw & 4) != 0)
    {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile (SUP->session, super->path_element->user,
                                                 sftpfs_pubkey, sftpfs_privkey,
                                                 super->path_element->password))
        {
            vfs_print_message (_("sftp: Authentication by public key failed"));
            goto err_conn;
        }

        vfs_print_message (_("sftp: Authentication by public key succeeded"));
    }
    else if ((SUP->auth_pw & 1) != 0)
    {
        /* We could authenticate via password */
        if (libssh2_userauth_password (SUP->session, super->path_element->user,
                                       super->path_element->password))
        {
            vfs_print_message (_("sftp: Authentication by password failed."));
            goto err_conn;
        }
    }
    else
    {
        vfs_print_message (_("sftp: No supported authentication methods found!"));
        goto err_conn;
    }

    SUP->sftp_session = libssh2_sftp_init (SUP->session);

    if (SUP->sftp_session == NULL)
        goto err_conn;
    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking (SUP->session, 1);

    return 0;

  err_conn:
    SUP->sftp_session = NULL;

    return -1;

}

/* --------------------------------------------------------------------------------------------- */
