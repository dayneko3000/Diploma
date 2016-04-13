/*
  Fine Tuning File System
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function BBFS found http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  His code is licensed under the GNU GPLv3.
  
  gcc -Wall `pkg-config fuse --cflags --libs` -o facfs facfs.c
*/

#include "params.h"
#include "db.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

const char * debug_path =  "/home/roman/Desktop/UntitledFolder/debug.txt";

// Report errors to logfile and give -errno to caller
static int ft_error(char *str)
{
    int ret = -errno;
    
    log_msg("    ERROR %s: %s\n", str, strerror(errno));
    
    return ret;
}

// Check whether the given user is permitted to perform the given operation on the given 

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void ft_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, FT_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

    log_msg("    ft_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    FT_DATA->rootdir, path, fpath);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int ft_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    
    ft_fullpath(fpath, path);

    int prm = (get_dir_prms(path, fuse_get_context()->uid, fuse_get_context()->gid, 1) & GETATTR) != 0;

    if (!prm)
    {
        retstat = -EACCES;
    }
    else
    {
        retstat = lstat(fpath, statbuf);
        if (retstat != 0)
            retstat = ft_error("ft_getattr lstat");
        else
            statbuf->st_mode = get_field(path, "mode");
    }
    
    log_stat(statbuf);
    
    return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
int ft_readlink(const char *path, char *link, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("ft_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & READLINK) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = readlink(fpath, link, size - 1);
        if (retstat < 0)
            retstat = ft_error("ft_readlink readlink");
        else  {
            link[retstat] = '\0';
            retstat = 0;
        }
    }
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int ft_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    
    ft_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, rdev)' but this
    int prm = (get_dir_prms(path, fuse_get_context()->uid, fuse_get_context()->gid, 1) & MKNOD) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        //  is more portable
        if (S_ISREG(mode)) {
            retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
            if (retstat < 0)
                retstat = ft_error("ft_mknod open");
            else {
                retstat = close(retstat);
                if (retstat < 0)
                    retstat = ft_error("ft_mknod close");
                else
                {
                    add_file(path, fuse_get_context()->uid, fuse_get_context()->gid, mode & ~(fuse_get_context()->umask));
                    //update_permissions(path, mode);
                }
            }
        } else
            if (S_ISFIFO(mode)) {
                retstat = mkfifo(fpath, mode);
                if (retstat < 0)
                    retstat = ft_error("ft_mknod mkfifo");
                else
                {
                    add_file(path, fuse_get_context()->uid, fuse_get_context()->gid, mode & ~(fuse_get_context()->umask));
                    //update_permissions(path, mode);
                }
            } else {
                retstat = mknod(fpath, mode, dev);
                if (retstat < 0)
                    retstat = ft_error("ft_mknod mknod");
                else
                {
                    add_file(path, fuse_get_context()->uid, fuse_get_context()->gid, mode & ~(fuse_get_context()->umask));
                   // update_permissions(path, mode);
                }
            }
    }
    
    return retstat;
}

/** Create a directory */
int ft_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_dir_prms(path, fuse_get_context()->uid, fuse_get_context()->gid, 1) & MKDIR) != 0;

    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = mkdir(fpath, mode);
        
        if (retstat < 0)
            retstat = ft_error("ft_mkdir mkdir");
        else
        {
            add_file(path, fuse_get_context()->uid, fuse_get_context()->gid, DIR_MODE & ~(fuse_get_context()->umask));
            //update_permissions(path, mode);
        }

    }
    
    return retstat;
}

/** Remove a file */
int ft_unlink(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("ft_unlink(path=\"%s\")\n",
	    path);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & UNLINK) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = unlink(fpath);
        if (retstat < 0)
            retstat = ft_error("ft_unlink unlink");
        else
        {
            remove_file(path);
        }
    }
    
    return retstat;
}

/** Remove a directory */
int ft_rmdir(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("ft_rmdir(path=\"%s\")\n",
	    path);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & RMDIR) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = rmdir(fpath);
        if (retstat < 0)
            retstat = ft_error("ft_rmdir rmdir");
        else
        {
            remove_file(fpath);
        }
    }
    
    return retstat;
}

/** Create a symbolic link */
int ft_symlink(const char *path, const char *link)
{
    int retstat = 0;
    char flink[PATH_MAX];
    char fpath[PATH_MAX];
    
    log_msg("\nft_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    
    ft_fullpath(flink, link);
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & SYMLINK) != 0;

    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = symlink(path, flink);
        if (retstat < 0)
            retstat = ft_error("ft_symlink symlink");
        else
        {
            //(link, fuse_get_context()->uid, fuse_get_context()->gid, fuse_get_context()->umask);
        }
    }
    return retstat;
}

/** Rename a file */
int ft_rename(const char *path, const char *newpath)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    log_msg("\nft_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    
    ft_fullpath(fpath, path);
    ft_fullpath(fnewpath, newpath);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & RENAME) != 0;
    
    if (!prm)
    {
        retstat = -EACCES;
        FILE *f = fopen ("/home/roman/Desktop/UntitledFolder/dbdebug.txt","a+");
        fprintf (f,"2 %s", path);
        fclose (f);
    }
    else
    {
        retstat = rename(fpath, fnewpath);
        if (retstat < 0)
            retstat = ft_error("ft_rename rename");
        else
        {
            update_path(path, newpath);
        }
    }
    return retstat;
}

/** Create a hard link to a file */
int ft_link(const char *path, const char *newpath)
{
    int retstat = 0;
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    log_msg("\nft_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    
    ft_fullpath(fpath, path);
    ft_fullpath(fnewpath, newpath);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & LINK) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = link(fpath, fnewpath);
        if (retstat < 0)
        {
            retstat = ft_error("ft_link link");
        }
        else
        {
            //(newpath, fuse_get_context()->uid, fuse_get_context()->gid, fuse_get_context()->umask);
        }
    }
    
    return retstat;
}

/** Change the permission bits of a file */
int ft_chmod(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & CHMOD) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = chmod(fpath, mode);
        if (retstat < 0)
            retstat = ft_error("ft_chmod chmod");
        else
            update_permissions(path, mode);
    }
    
    return retstat;
}

/** Change the owner and group of a file */
int ft_chown(const char *path, uid_t uid, gid_t gid)
  
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & CHOWN) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = chown(fpath, uid, gid);
        if (retstat < 0)
        {
            retstat = ft_error("ft_chown chown");
        }
        else
            update_owners(path, uid, gid);
    }
    
    return retstat;
}

/** Change the size of a file */
int ft_truncate(const char *path, off_t newsize)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & TRUNCATE) != 0;

    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = truncate(fpath, newsize);
        if (retstat < 0)
            ft_error("ft_truncate truncate");
    }
    
    return retstat;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int ft_utime(const char *path, struct utimbuf *ubuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & UTIME) != 0;
    
    if (!prm)
    {
        retstat = -EACCES;
    }
    else
    {
        retstat = utime(fpath, ubuf);
        if (retstat < 0)
            retstat = ft_error("ft_utime utime");
    }
    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int ft_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    log_msg("\nft_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);
    
    ft_fullpath(fpath, path);
    
    int prms = get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid);
    
    if ((prms & OPEN) == 0 || ((prms & WRITE) == 0 && ((fi->flags & O_RDWR) != 0 || (fi->flags & O_WRONLY) != 0))
            || ((prms & READ) == 0 && ((fi->flags & O_RDWR) != 0 || (fi->flags & O_RDONLY) != 0)))
       retstat = -EACCES;
    else
    {
        fd = open(fpath, fi->flags);
        if (fd < 0)
            retstat = ft_error("ft_open open");

        fi->fh = fd;
        log_fi(fi);
    }
    
    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int ft_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
     
    log_msg("\nft_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
    
    char fpath[PATH_MAX];
    ft_fullpath(fpath, path); //but I need fpath for db operations
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & READ) != 0;
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        retstat = pread(fi->fh, buf, size, offset);
        if (retstat < 0)
            retstat = ft_error("ft_read read");
    }
    
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int ft_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nft_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi
	    );
    log_fi(fi);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & WRITE) != 0;
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        retstat = pwrite(fi->fh, buf, size, offset);
        if (retstat < 0)
            retstat = ft_error("ft_write pwrite");
    }
    
    return retstat;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int ft_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & STATFS) != 0;
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        // get stats for underlying filesystem
        retstat = statvfs(fpath, statv);
        if (retstat < 0)
            retstat = ft_error("ft_statfs statvfs");

        log_statvfs(statv);
    }
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int ft_flush(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nft_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
    //do nothing
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int ft_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    //closing the file is always allowed
    log_msg("\nft_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    retstat = close(fi->fh);
    
    return retstat;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int ft_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    ft_fullpath(fpath, path); //I need fpath for db operations
    
    log_msg("\nft_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
            path, datasync, fi);
    log_fi(fi);

    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
        retstat = fdatasync(fi->fh);
    else
#endif	
        retstat = fsync(fi->fh);

    if (retstat < 0)
        ft_error("ft_fsync fsync");
    
    return retstat;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int ft_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	    path, name, value, size, flags);
    ft_fullpath(fpath, path);
    
    retstat = lsetxattr(fpath, name, value, size, flags);
    if (retstat < 0)
	retstat = ft_error("ft_setxattr lsetxattr");
    
    return retstat;
}

/** Get extended attributes */
int ft_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    ft_fullpath(fpath, path);
    
    retstat = lgetxattr(fpath, name, value, size);
    if (retstat < 0)
	retstat = ft_error("ft_getxattr lgetxattr");
    else
	log_msg("    value = \"%s\"\n", value);
    
    return retstat;
}

/** List extended attributes */
int ft_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char *ptr;
    
    log_msg("ft_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
	    path, list, size
	    );
    ft_fullpath(fpath, path);
    
    retstat = llistxattr(fpath, list, size);
    if (retstat < 0)
	retstat = ft_error("ft_listxattr llistxattr");
    
    log_msg("    returned attributes (length %d):\n", retstat);
    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
	log_msg("    \"%s\"\n", ptr);
    
    return retstat;
}

/** Remove extended attributes */
int ft_removexattr(const char *path, const char *name)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_removexattr(path=\"%s\", name=\"%s\")\n",
	    path, name);
    ft_fullpath(fpath, path);
    
    retstat = lremovexattr(fpath, name);
    if (retstat < 0)
	retstat = ft_error("ft_removexattr lrmovexattr");
    
    return retstat;
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int ft_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & OPENDIR) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        dp = opendir(fpath);
        if (dp == NULL)
            retstat = ft_error("ft_opendir opendir");

        fi->fh = (intptr_t) dp;

        log_fi(fi);
    }
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int ft_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    char fpath[PATH_MAX];
    struct dirent *de;
    
    log_msg("\nft_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & READDIR) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        // Every directory contains at least two entries: . and ..  If my
        // first call to the system readdir() returns NULL I've got an
        // error; near as I can tell, that's the only condition under
        // which I can get an error from readdir()
        de = readdir(dp);
        if (de == 0) {
            retstat = ft_error("ft_readdir readdir");
            return retstat;
        }

        // This will copy the entire directory into the buffer.  The loop exits
        // when either the system readdir() returns NULL, or filler()
        // returns something non-zero.  The first case just means I've
        // read the whole directory; the second means the buffer is full.
        do {
            log_msg("calling filler with name %s\n", de->d_name);
            if (filler(buf, de->d_name, NULL, 0) != 0) {
                log_msg("    ERROR ft_readdir filler:  buffer full");
                return -ENOMEM;
            }
        } while ((de = readdir(dp)) != NULL);

        log_fi(fi);
    }
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int ft_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nft_releasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    log_fi(fi);
    //closing is always avalible
    closedir((DIR *) (uintptr_t) fi->fh);
    
    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
int ft_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nft_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    //do nothing
    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */

void *ft_init(struct fuse_conn_info *conn)
{
    log_msg("\nft_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    return FT_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void ft_destroy(void *userdata)
{
    //free(FT_DATA);
    log_msg("\nft_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int ft_access(const char *path, int mask)
{
    int retstat = 0;
    char fpath[PATH_MAX];
   
    log_msg("\nft_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
    ft_fullpath(fpath, path);
    
    retstat = access(fpath, mask);

    if (retstat < 0)
        retstat = ft_error("ft_access access");
    
    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int ft_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    int fd;
    
    log_msg("\nft_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    ft_fullpath(fpath, path);

    int prm = (get_dir_prms(path, fuse_get_context()->uid, fuse_get_context()->gid, 1) & CREATE) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        fd = creat(fpath, mode);
        fi->fh = fd;
        log_fi(fi);
        
        if (fd < 0)
        {
            retstat = ft_error("ft_create creat");
        }
        else
        {
            add_file(path, fuse_get_context()->uid, fuse_get_context()->gid, mode & ~(fuse_get_context()->umask));
            //update_permissions(path, mode);
        }
    }
    
    return retstat;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int ft_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
	    path, offset, fi);
    log_fi(fi);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & FTRUNCATE) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = ftruncate(fi->fh, offset);
        if (retstat < 0)
            retstat = ft_error("ft_ftruncate ftruncate");
    }
    
    return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int ft_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nft_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);

    if (!strcmp(path, "/"))
	return ft_getattr(path, statbuf);
    
    ft_fullpath(fpath, path);
    
    int prm = (get_prms(path, fuse_get_context()->uid, fuse_get_context()->gid) & FGETATTR) != 0;
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = fstat(fi->fh, statbuf);
        if (retstat < 0)
            retstat = ft_error("ft_fgetattr fstat");
    }
    
    log_stat(statbuf);
    
    return retstat;
}

struct fuse_operations ft_oper = {
  .getattr = ft_getattr,
  .readlink = ft_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = ft_mknod,
  .mkdir = ft_mkdir,
  .unlink = ft_unlink,
  .rmdir = ft_rmdir,
  .symlink = ft_symlink,
  .rename = ft_rename,
  .link = ft_link,
  .chmod = ft_chmod,
  .chown = ft_chown,
  .truncate = ft_truncate,
  .utime = ft_utime,
  .open = ft_open,
  .read = ft_read,
  .write = ft_write,
  /** Just a placeholder, don't set */ 
  .statfs = ft_statfs,
  .flush = ft_flush,
  .release = ft_release,
  .fsync = ft_fsync,
  
#ifdef HAVE_SYS_XATTR_H
  .setxattr = ft_setxattr,
  .getxattr = ft_getxattr,
  .listxattr = ft_listxattr,
  .removexattr = ft_removexattr,
#endif
  
  .opendir = ft_opendir,
  .readdir = ft_readdir,
  .releasedir = ft_releasedir,
  .fsyncdir = ft_fsyncdir,
  .init = ft_init,
  .destroy = ft_destroy,
  .access = ft_access,
  .create = ft_create,
  .ftruncate = ft_ftruncate,
  .fgetattr = ft_fgetattr
};

void ft_usage()
{
    fprintf(stderr, "usage:  facfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct ft_state *ft_data;

    // facfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running facfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0)) {
	fprintf(stderr, "Running FACFS as root opens unnacceptable security holes\n");
	return 1;
    }
    
    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	ft_usage();

    ft_data = malloc(sizeof(struct ft_state));
    if (ft_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    // -log in argv[1] for set loging on
    
    if (strcmp("-log", argv[1]) == 0)
    {
        ft_data->logfile = log_open();
        argc --;
        argv ++;
    }
    else
        ft_data->logfile = NULL;
        
    
    ft_data->rootdir = realpath(argv[argc - 2], NULL);
    argv[argc - 2] = argv[argc - 1];
    argv[argc - 1] = NULL;
    argc --;
    
    char *mountdir = realpath(argv[argc - 1], NULL);
    init_db(ft_data->rootdir, mountdir, getuid(), getgid());
    free(mountdir);
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &ft_oper, ft_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    free(ft_data->rootdir);
    free(ft_data);
    
    return fuse_stat;
}
