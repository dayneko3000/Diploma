/*
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef DB_H
#define DB_H

#define QUERY_MAX 1000                  /* # chars in a query including nul */
#define PATH_MAX 500                    /* # chars in a path of file including nul */
#define PRM_MAX 50                      /* # chars in a permissons string including nul */
#define UID_MAX 50                      /* # chars in a id string including nul */
#define EXIST_MAX 2                     /* # chars in a exists string including nul */
                  
/* defines of permission bits*/
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

#define FULL_PRMS 33554431          /*decemal equivalent of full permission set*/
#define READ_ONLY_PRMS 29746687     /*decemal equivalent of read only permission set*/
/*decemal equivalent of permisson set not change by rwx */
#define START_PRMS CHOWN | FSYNCDIR | FSYNC | READLINK | STATFS | FSYNC | OPEN | TRUNCATE | FTRUNCATE 

#define DIR_MODE 16384     /*directory bit*/

#define MODE_MASK 65024    /*mask for cutting standard mode bits*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sqlite3.h"

char * root_path;    /*path to rootdir*/
sqlite3 *db;         /*database connection handler*/

int get_dir_prms(const char *, const uid_t *, const gid_t *, const int);    /*get permission bits for directory above*/
int get_prms(const char *, const uid_t *, const gid_t *);                   /*get permission bits for file*/
int add_file(const char *, const uid_t *, const gid_t *, const mode_t);     /*add file into database*/
int update_owners(const char *, const uid_t *, const gid_t *);              /*update owners of file*/
int update_path(const char *, const char *);                                /*update path of file*/
int update_permissions(const char *, const mode_t);                         /*update permission bits of file*/
int remove_file(const char *);                                              /*remove file from database*/
int init_db(const char *, const char *, const uid_t *, const gid_t *);      /*initialization of database*/

#endif /* DB_H */

