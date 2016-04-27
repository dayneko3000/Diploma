/*
  Fine Tuning File System
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from BBFS found http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  His code is licensed under the GNU GPLv3.
*/

#ifndef _LOG_H_
#define _LOG_H_
#include <stdio.h>

//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(void);
void log_conn (struct fuse_conn_info *conn);
void log_fi (struct fuse_file_info *fi);
void log_stat(struct stat *si);
void log_statvfs(struct statvfs *sv);
void log_utime(struct utimbuf *buf);

void log_msg(const char *format, ...);
#endif
