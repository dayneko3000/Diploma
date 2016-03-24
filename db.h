/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   db.h
 * Author: Roman
 *
 * Created on January 9, 2016, 3:03 PM
 */

#ifndef DB_H
#define DB_H
#define NOT_OWNER_PRMS -5
#define OWNER_PRMS -4

#define PREMISSIONS_SIZE 30

#define GETATTR 1 << 0
#define READLINK 1 << 1
#define MKNOD 1 << 2
#define MKDIR 1 << 3
#define UNLINK 1 << 4
#define RMDIR 1 << 5
#define SYMLINK 1 << 6
#define RENAME 1 << 7
#define LINK 1 << 8
#define CHMOD 1 << 9
#define CHOWN 1 << 10
#define TRUNCATE 1 << 11
#define UTIME 1 << 12
#define OPEN 1 << 13
#define READ 1 << 14
#define WRITE 1 << 15
#define STATFS 1 << 16
#define FLUSH 1 << 17
#define RELEASE 1 << 18
#define FSYNC 1 << 19
#define OPENDIR 1 << 20
#define READDIR 1 << 21
#define RELEASEDIR 1 << 22
#define FSYNCDIR 1 << 23
#define INIT 1 << 24
#define DESTROY 1 << 25
#define ACCESS 1 << 26
#define CREATE 1 << 27
#define FTRUNCATE 1 << 28
#define FGETATTR 1 << 29

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sqlite3.h"



char * root_path;
sqlite3 *db;

int select_dir_prm(char *, uid_t *, int, int);
int select_file_prm(char *, int, int);
int select_prm(const char *, uid_t *, int);
int put_prm(char *);
int upd_prm(char *, char *, char *);
int rem_prm(char *);
int init_db(char *, char *, uid_t *);
char * dir_path(const char *);
#endif /* DB_H */

