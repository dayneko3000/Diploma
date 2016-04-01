/*
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

/* 
 * File:   db.h
 * Author: Roman
 *
 * Created on January 9, 2016, 3:03 PM
 */

#ifndef DB_H
#define DB_H

#define OWNER_PRMS -3
#define GROUP_PRMS -4
#define OTHER_PRMS -5

#define PREMISSIONS_SIZE 30
#define QUERY_MAX 500
#define PATH_MAX 500
#define DEEP_MAX 200
#define UID_MAX 50
#define EXIST_MAX 2

#define FULL 67108863
#define READ_ONLY 29746687

#define EXECUTE (1 << 0)
#define GETATTR (1 << 1)
#define CHMOD (1 << 2)
#define ACCESS (1 << 3)
#define TRUNCATE (1 << 4)
#define CHOWN (1 << 5)
#define FSYNC (1 << 6)
#define UTIME (1 << 7)
#define READLINK (1 << 8)
#define UNLINK (1 << 9)
#define SYMLINK (1 << 10)
#define RENAME (1 << 11)
#define LINK (1 << 12)
#define FTRUNCATE (1 << 13)
#define FGETATTR (1 << 14)
#define OPEN (1 << 15)
#define READ (1 << 16)
#define WRITE (1 << 17)
#define STATFS (1 << 18)
#define RMDIR (1 << 19)
#define MKNOD (1 << 20)
#define MKDIR (1 << 21)
#define OPENDIR (1 << 22)
#define READDIR (1 << 23)
#define FSYNCDIR (1 << 24)
#define CREATE (1 << 25)



#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sqlite3.h"



char * root_path;
sqlite3 *db;

int select_dir_prm(const char *, const uid_t *, const gid_t *, const int, const int);
int select_file_prm(const char *, const int, const int);
int select_prm(const char *, const uid_t *, const gid_t *, const int);
int put_file_prm(const char *, const uid_t *, const gid_t *);
int upd_own(const char *, const uid_t *, const gid_t *);
int upd_path(const char *, const char *, const char *);
int upd_prms(const char *, const mode_t);
int rem_prm(const char *);
int init_db(const char *, const char *, const uid_t *, const gid_t *);
//char * dir_path(const char *);
#endif /* DB_H */

