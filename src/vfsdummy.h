/* Replacement for vfs.h if VFS support is disabled */

#ifndef __VFSDUMMY_H
#define __VFSDYMMY_H

#undef USE_NETCODE

/* Flags of VFS classes */
#define VFSF_LOCAL 1		/* Class is local (not virtual) filesystem */
#define VFSF_NOLINKS 2		/* Hard links not supported */

#define O_LINEAR 0

#define mc_close close
#define mc_read read
#define mc_write write
#define mc_lseek lseek
#define mc_opendir opendir
#define mc_readdir readdir
#define mc_closedir closedir
#define mc_telldir telldir
#define mc_seekdir seekdir

#define mc_stat stat
#define mc_mknod mknod
#define mc_link link
#define mc_mkdir mkdir
#define mc_rmdir rmdir
#define mc_fstat fstat
#define mc_lstat lstat

#define mc_readlink readlink
#define mc_symlink symlink
#define mc_rename rename

#define mc_open open
#define mc_utime utime
#define mc_chmod chmod
#define mc_chown chown
#define mc_chdir chdir
#define mc_unlink unlink

#define mc_mmap mmap
#define mc_munmap munmap

static inline int
return_zero (void)
{
    return 0;
}

#define mc_ctl(a,b,c) return_zero()
#define mc_setctl(a,b,c) return_zero()

#define mc_get_current_wd(x,size) get_current_wd (x, size)
#define mc_getlocalcopy(x) NULL
#define mc_ungetlocalcopy(x,y,z) do { } while (0)

#define vfs_init() do { } while (0)
#define vfs_shut() do { } while (0)

#define vfs_current_is_local() 1
#define vfs_file_is_local(x) 1
#define vfs_canon(p) g_strdup (canonicalize_pathname(p))
#define vfs_strip_suffix_from_filename(x) g_strdup(x)

#define vfs_file_class_flags(x) (VFSF_LOCAL)
#define vfs_get_class(x) (struct vfs_class *)(NULL)

#define vfs_translate_url(s) g_strdup(s)
#define vfs_release_path(x)
#define vfs_add_current_stamps() do { } while (0)
#define vfs_timeout_handler() do { } while (0)
#define vfs_timeouts() 0

#define ftpfs_flushdir() do { } while (0)

#endif				/* !__VFSDUMMY_H */