/* Virtual File System: SFTP file system.
   The internal functions: connections

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

#include <netdb.h>              /* struct hostent */
#include <sys/socket.h>         /* AF_INET */
#include <netinet/in.h>         /* struct in_addr */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"

#include "lib/util.h"
#include "lib/tty/tty.h"        /* tty_enable_interrupt_key () */
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
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
            return -1;
        }

        vfs_print_message (_("sftp: making connection to %s"), super->path_element->host);

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

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

static void
sftpfs_recognize_auth_types (struct vfs_s_super *super)
{
    char *userauthlist;
    sftpfs_super_data_t *super_data;

    super_data = (sftpfs_super_data_t *) super->data;

    super_data->auth_type = NONE;

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list (super_data->session, super->path_element->user,
                                          strlen (super->path_element->user));

    if (strstr (userauthlist, "password") != NULL)
        super_data->auth_type |= PASSWORD;
    if (strstr (userauthlist, "keyboard-interactive") != NULL)
        super_data->auth_type |= PASSWORD;
    if (strstr (userauthlist, "publickey") != NULL)
        super_data->auth_type |= PUBKEY;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftpfs_open_connection_ssh_agent (struct vfs_s_super *super)
{
    sftpfs_super_data_t *super_data;
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;
    int rc;

    super_data = (sftpfs_super_data_t *) super->data;
    super_data->agent = NULL;

    if ((super_data->auth_type & PUBKEY) == 0)
        return FALSE;

    /* Connect to the ssh-agent */
    super_data->agent = libssh2_agent_init (super_data->session);
    if (super_data->agent == NULL)
        return FALSE;

    if (libssh2_agent_connect (super_data->agent) != 0)
        return FALSE;


    if (libssh2_agent_list_identities (super_data->agent) != 0)
        return FALSE;

    while (TRUE)
    {
        rc = libssh2_agent_get_identity (super_data->agent, &identity, prev_identity);
        if (rc == 1)
            break;

        if (rc < 0)
            return FALSE;

        if (libssh2_agent_userauth (super_data->agent, super->path_element->user, identity) == 0)
            break;

        prev_identity = identity;
    }
    if (rc != 0)
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftpfs_open_connection_ssh_key (struct vfs_s_super *super)
{
    sftpfs_super_data_t *super_data;
    char *p, *passwd;
    gboolean ret_value = FALSE;

    super_data = (sftpfs_super_data_t *) super->data;

    if ((super_data->auth_type & PUBKEY) == 0)
        return FALSE;

    if (super_data->privkey == NULL)
        return FALSE;

    if (libssh2_userauth_publickey_fromfile (super_data->session, super->path_element->user,
                                             super_data->pubkey, super_data->privkey,
                                             super->path_element->password) == 0)
    {
        return TRUE;
    }

    p = g_strdup_printf (_("sftp: Enter passphrase for %s "), super->path_element->user);

    passwd = vfs_get_password (p);
    g_free (p);
    if (passwd == NULL)
    {
        vfs_print_message (_("sftp: Passphrase is empty."));
        return FALSE;
    }

    if (libssh2_userauth_publickey_fromfile (super_data->session, super->path_element->user,
                                             super_data->pubkey, super_data->privkey, passwd) == 0)
    {
        ret_value = TRUE;
    }
    g_free (passwd);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftpfs_open_connection_ssh_password (struct vfs_s_super *super)
{
    sftpfs_super_data_t *super_data;
    char *p, *passwd;
    gboolean ret_value = FALSE;

    super_data = (sftpfs_super_data_t *) super->data;

    if ((super_data->auth_type & PASSWORD) == 0)
        return FALSE;

    if (libssh2_userauth_password (super_data->session, super->path_element->user,
                                   super->path_element->password) == 0)
    {
        return TRUE;
    }

    p = g_strdup_printf (_("sftp: Enter password for %s "), super->path_element->user);

    passwd = vfs_get_password (p);
    g_free (p);
    if (passwd == NULL)
    {
        vfs_print_message (_("sftp: Password is empty."));
        return FALSE;
    }
    if (libssh2_userauth_password (super_data->session, super->path_element->user,
                                   super->path_element->password) == 0)
    {
        ret_value = TRUE;
    }

    g_free (passwd);
    return ret_value;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
sftpfs_open_connection (struct vfs_s_super *super)
{
    int rc;
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
        goto deinit_by_error;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    super_data->fingerprint = libssh2_hostkey_hash (super_data->session, LIBSSH2_HOSTKEY_HASH_SHA1);

    sftpfs_recognize_auth_types (super);

    if (!sftpfs_open_connection_ssh_agent (super) && !sftpfs_open_connection_ssh_key (super)
        && !sftpfs_open_connection_ssh_password (super))
        goto deinit_by_error;

    super_data->sftp_session = libssh2_sftp_init (super_data->session);

    if (super_data->sftp_session == NULL)
        goto deinit_by_error;

    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking (super_data->session, 1);

    return 0;

  deinit_by_error:

    if (super_data->agent != NULL)
    {
        libssh2_agent_disconnect (super_data->agent);
        libssh2_agent_free (super_data->agent);
        super_data->agent = NULL;
    }
    if (super_data->session != NULL)
    {
        libssh2_session_disconnect (super_data->session, "Shutdown with errors");
        libssh2_session_free (super_data->session);
        super_data->session = NULL;
    }
    close (super_data->socket_handle);
    vfs_print_message (_("sftp: Connect to host failed."));
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
