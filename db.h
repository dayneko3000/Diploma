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

#define STANDART_PRMS -1

#define PREMISSIONS_SIZE 30
#define QUERY_MAX 1000
#define PATH_MAX 500
#define PRM_MAX 50
#define DEEP_MAX 200
#define UID_MAX 50
#define EXIST_MAX 2

#define FULL_PRMS 67108863
#define READ_ONLY_PRMS 29746687

#define EXECUTE (1 << 0)
#define GETATTR (1 << 1)
#define CHMOD (1 << 2)
#define FSYNCDIR (1 << 3)
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
#define CREATE (1 << 24)

#define START_PRMS CHOWN | FSYNCDIR | FSYNC | READLINK | STATFS | FSYNC | OPEN | TRUNCATE | FTRUNCATE

#define DIR_MODE 16893
#define MODE_MASK 65024

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sqlite3.h"



char * root_path;
sqlite3 *db;

int get_dir_prms(const char *, const uid_t *, const gid_t *, const int);
//int select_file_prm(const char *, const char *, const int);
int get_prms(const char *, const uid_t *, const gid_t *);
int add_file(const char *, const uid_t *, const gid_t *, const mode_t);
int get_field(const char *, const char *);
int update_owners(const char *, const uid_t *, const gid_t *);
int update_path(const char *, const char *);
int update_permissions(const char *, const mode_t);
int remove_file(const char *);
int init_db(const char *, const char *, const uid_t *, const gid_t *);
//char * dir_path(const char *);
#endif /* DB_H */

