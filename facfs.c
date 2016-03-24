/*
  Flexible Access Control File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  facfs.log, in the directory from which you run facfs.

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
static int fac_error(char *str)
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
static void fac_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, FAC_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

    log_msg("    fac_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    FAC_DATA->rootdir, path, fpath);
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
int fac_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    
    fac_fullpath(fpath, path);

    int prm = select_dir_prm(path, fuse_get_context()->uid, GETATTR, 1);

    if (!prm)
    {
        retstat = -EACCES;
    }
    else
    {
        retstat = lstat(fpath, statbuf);
        if (retstat != 0)
            retstat = fac_error("fac_getattr lstat");
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
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to fac_readlink()
// fac_readlink() code by Bernardo F Costa (thanks!)
int fac_readlink(const char *path, char *link, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("fac_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, READLINK);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = readlink(fpath, link, size - 1);
        if (retstat < 0)
            retstat = fac_error("fac_readlink readlink");
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
// shouldn't that comment be "if" there is no.... ?
int fac_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    
    fac_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, rdev)' but this
    int prm = select_dir_prm(path, fuse_get_context()->uid, MKNOD, 1);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        //  is more portable
        if (S_ISREG(mode)) {
            retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
            if (retstat < 0)
                retstat = fac_error("fac_mknod open");
            else {
                retstat = close(retstat);
                if (retstat < 0)
                    retstat = fac_error("fac_mknod close");
                else
                    put_file_prm(fpath, fuse_get_context()->uid);
            }
        } else
            if (S_ISFIFO(mode)) {
                retstat = mkfifo(fpath, mode);
                if (retstat < 0)
                    retstat = fac_error("fac_mknod mkfifo");
                else
                    put_file_prm(fpath, fuse_get_context()->uid);
            } else {
                retstat = mknod(fpath, mode, dev);
                if (retstat < 0)
                    retstat = fac_error("fac_mknod mknod");
                else
                    put_file_prm(fpath, fuse_get_context()->uid);
            }
    }
    
    return retstat;
}

/** Create a directory */
int fac_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
    
    fac_fullpath(fpath, path);
    
    int prm = select_dir_prm(path, fuse_get_context()->uid, MKDIR, 1);

    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = mkdir(fpath, mode);
        
        if (retstat < 0)
            retstat = fac_error("fac_mkdir mkdir");
        else
            put_dir_prm(path, fuse_get_context()->uid);

    }
    
    return retstat;
}

/** Remove a file */
int fac_unlink(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("fac_unlink(path=\"%s\")\n",
	    path);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, UNLINK);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = unlink(fpath);
        if (retstat < 0)
            retstat = fac_error("fac_unlink unlink");
        else
        {
            rem_prm(path);
        }
    }
    
    return retstat;
}

/** Remove a directory */
int fac_rmdir(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("fac_rmdir(path=\"%s\")\n",
	    path);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, RMDIR);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = rmdir(fpath);
        if (retstat < 0)
            retstat = fac_error("fac_rmdir rmdir");
        else
        {
            rem_prm(fpath);
        }
    }
    
    return retstat;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int fac_symlink(const char *path, const char *link)
{
    int retstat = 0;
    char flink[PATH_MAX];
    char fpath[PATH_MAX];
    
    log_msg("\nfac_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    
    fac_fullpath(flink, link);
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, SYMLINK);

    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = symlink(path, flink);
        if (retstat < 0)
            retstat = fac_error("fac_symlink symlink");
    }
    return retstat;
}

/** Rename a file */
// both path and newpath are fs-relative
int fac_rename(const char *path, const char *newpath)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    log_msg("\nfac_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    
    fac_fullpath(fpath, path);
    fac_fullpath(fnewpath, newpath);
    
    int prm = select_prm(path, fuse_get_context()->uid, RENAME);
    
    if (!prm)
    {
        retstat = -EACCES;
    }
    else
    {
        retstat = rename(fpath, fnewpath);
        if (retstat < 0)
            retstat = fac_error("fac_rename rename");
        else
        {
            upd_prm(path, newpath, "prm_list");
            upd_prm(path, newpath, "own_list");
        }
    }
    return retstat;
}

/** Create a hard link to a file */
int fac_link(const char *path, const char *newpath)
{
    int retstat = 0;
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    log_msg("\nfac_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    
    fac_fullpath(fpath, path);
    fac_fullpath(fnewpath, newpath);
    
    int prm = select_prm(path, fuse_get_context()->uid, LINK);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = link(fpath, fnewpath);
        if (retstat < 0)
        {
            retstat = fac_error("fac_link link");
        }
        else
        {
            //put_prm(fnewpath);
        //put_prm(fnewpath, fuse_get_context()->uid);/!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            
        }
    }
    
    return retstat;
}

/** Change the permission bits of a file */
int fac_chmod(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, CHMOD);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = chmod(fpath, mode);
        if (retstat < 0)
            retstat = fac_error("fac_chmod chmod");
    }
    
    return retstat;
}

/** Change the owner and group of a file */
int fac_chown(const char *path, uid_t uid, gid_t gid)
  
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, CHOWN);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = chown(fpath, uid, gid);
        if (retstat < 0)
        {
            retstat = fac_error("fac_chown chown");
        }
    }
    
    return retstat;
}

/** Change the size of a file */
int fac_truncate(const char *path, off_t newsize)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, TRUNCATE);

    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = truncate(fpath, newsize);
        if (retstat < 0)
            fac_error("fac_truncate truncate");
    }
    
    return retstat;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int fac_utime(const char *path, struct utimbuf *ubuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, UTIME);
    
    if (!prm)
    {
        retstat = -EACCES;
    }
    else
    {
        retstat = utime(fpath, ubuf);
        if (retstat < 0)
            retstat = fac_error("fac_utime utime");
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
int fac_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, OPEN);
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        fd = open(fpath, fi->flags);
        if (fd < 0)
            retstat = fac_error("fac_open open");

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
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int fac_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
     
    log_msg("\nfac_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
    
    char fpath[PATH_MAX];
    fac_fullpath(fpath, path); //but I need fpath for db operations
    
    int prm = select_prm(path, fuse_get_context()->uid, READ);
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        retstat = pread(fi->fh, buf, size, offset);
        if (retstat < 0)
            retstat = fac_error("fac_read read");
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
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int fac_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfac_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi
	    );
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
    char fpath[PATH_MAX];
    fac_fullpath(fpath, path); //but I need fpath for db operations
    
    int prm = select_prm(path, fuse_get_context()->uid, WRITE);
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        retstat = pwrite(fi->fh, buf, size, offset);
        if (retstat < 0)
            retstat = fac_error("fac_write pwrite");
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
int fac_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, STATFS);
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        // get stats for underlying filesystem
        retstat = statvfs(fpath, statv);
        if (retstat < 0)
            retstat = fac_error("fac_statfs statvfs");

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
int fac_flush(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfac_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
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
int fac_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    //closing the file is always allowed
    log_msg("\nfac_release(path=\"%s\", fi=0x%08x)\n",
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
int fac_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    fac_fullpath(fpath, path); //but i need fpath for db operations
    
    int prm = select_prm(path, fuse_get_context()->uid, FSYNC);
    
    if (!prm)
       retstat = -EACCES;
    else
    {
        log_msg("\nfac_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
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
            fac_error("fac_fsync fsync");
    }
    
    return retstat;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int fac_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	    path, name, value, size, flags);
    fac_fullpath(fpath, path);
    
    retstat = lsetxattr(fpath, name, value, size, flags);
    if (retstat < 0)
	retstat = fac_error("fac_setxattr lsetxattr");
    
    return retstat;
}

/** Get extended attributes */
int fac_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    fac_fullpath(fpath, path);
    
    retstat = lgetxattr(fpath, name, value, size);
    if (retstat < 0)
	retstat = fac_error("fac_getxattr lgetxattr");
    else
	log_msg("    value = \"%s\"\n", value);
    
    return retstat;
}

/** List extended attributes */
int fac_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char *ptr;
    
    log_msg("fac_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
	    path, list, size
	    );
    fac_fullpath(fpath, path);
    
    retstat = llistxattr(fpath, list, size);
    if (retstat < 0)
	retstat = fac_error("fac_listxattr llistxattr");
    
    log_msg("    returned attributes (length %d):\n", retstat);
    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
	log_msg("    \"%s\"\n", ptr);
    
    return retstat;
}

/** Remove extended attributes */
int fac_removexattr(const char *path, const char *name)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_removexattr(path=\"%s\", name=\"%s\")\n",
	    path, name);
    fac_fullpath(fpath, path);
    
    retstat = lremovexattr(fpath, name);
    if (retstat < 0)
	retstat = fac_error("fac_removexattr lrmovexattr");
    
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
int fac_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, OPENDIR);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        dp = opendir(fpath);
        if (dp == NULL)
            retstat = fac_error("fac_opendir opendir");

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
int fac_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    char fpath[PATH_MAX];
    struct dirent *de;
    
    log_msg("\nfac_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, READDIR);
    
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
            retstat = fac_error("fac_readdir readdir");
            return retstat;
        }

        // This will copy the entire directory into the buffer.  The loop exits
        // when either the system readdir() returns NULL, or filler()
        // returns something non-zero.  The first case just means I've
        // read the whole directory; the second means the buffer is full.
        do {
            log_msg("calling filler with name %s\n", de->d_name);
            if (filler(buf, de->d_name, NULL, 0) != 0) {
                log_msg("    ERROR fac_readdir filler:  buffer full");
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
int fac_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfac_releasedir(path=\"%s\", fi=0x%08x)\n",
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
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int fac_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfac_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
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
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *fac_init(struct fuse_conn_info *conn)
{
    log_msg("\nfac_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    return FAC_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void fac_destroy(void *userdata)
{
    log_msg("\nfac_destroy(userdata=0x%08x)\n", userdata);
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
int fac_access(const char *path, int mask)
{
    int retstat = 0;
    char fpath[PATH_MAX];
   
    log_msg("\nfac_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, ACCESS);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = access(fpath, mask);

        if (retstat < 0)
            retstat = fac_error("fac_access access");
    }
    
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
int fac_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    int fd;
    
    log_msg("\nfac_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    fac_fullpath(fpath, path);

    int prm = select_dir_prm(path, fuse_get_context()->uid, CREATE, 1);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        fd = creat(fpath, mode);
        fi->fh = fd;
        log_fi(fi);
        
        if (fd < 0)
        {
            retstat = fac_error("fac_create creat");
        }
        else
            put_file_prm(path, fuse_get_context()->uid);
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
int fac_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
	    path, offset, fi);
    log_fi(fi);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, FTRUNCATE);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = ftruncate(fi->fh, offset);
        if (retstat < 0)
            retstat = fac_error("fac_ftruncate ftruncate");
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
int fac_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfac_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
	return fac_getattr(path, statbuf);
    
    fac_fullpath(fpath, path);
    
    int prm = select_prm(path, fuse_get_context()->uid, FGETATTR);
    
    if (!prm)
        retstat = -EACCES;
    else
    {
        retstat = fstat(fi->fh, statbuf);
        if (retstat < 0)
            retstat = fac_error("fac_fgetattr fstat");
    }
    
    log_stat(statbuf);
    
    return retstat;
}

struct fuse_operations fac_oper = {
  .getattr = fac_getattr,
  .readlink = fac_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = fac_mknod,
  .mkdir = fac_mkdir,
  .unlink = fac_unlink,
  .rmdir = fac_rmdir,
  .symlink = fac_symlink,
  .rename = fac_rename,
  .link = fac_link,
  .chmod = fac_chmod,
  .chown = fac_chown,
  .truncate = fac_truncate,
  .utime = fac_utime,
  .open = fac_open,
  .read = fac_read,
  .write = fac_write,
  /** Just a placeholder, don't set */ // huh???
  .statfs = fac_statfs,
  .flush = fac_flush,
  .release = fac_release,
  .fsync = fac_fsync,
  
#ifdef HAVE_SYS_XATTR_H
  .setxattr = fac_setxattr,
  .getxattr = fac_getxattr,
  .listxattr = fac_listxattr,
  .removexattr = fac_removexattr,
#endif
  
  .opendir = fac_opendir,
  .readdir = fac_readdir,
  .releasedir = fac_releasedir,
  .fsyncdir = fac_fsyncdir,
  .init = fac_init,
  .destroy = fac_destroy,
  .access = fac_access,
  .create = fac_create,
  .ftruncate = fac_ftruncate,
  .fgetattr = fac_fgetattr
};

void fac_usage()
{
    fprintf(stderr, "usage:  facfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct fac_state *fac_data;

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
	fac_usage();

    fac_data = malloc(sizeof(struct fac_state));
    if (fac_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    fac_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    fac_data->logfile = log_open();
    
    init_db(fac_data->rootdir, realpath(argv[argc-1], NULL), getuid());
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &fac_oper, fac_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
