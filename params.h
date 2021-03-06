/*
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  There are a couple of symbols that need to be #defined before
  #including all the headers.
*/

#ifndef _PARAMS_H_
#define _PARAMS_H_

#define FUSE_USE_VERSION 26

#define _XOPEN_SOURCE 500

// maintain ftfs state in here
#include <limits.h>
#include <stdio.h>
struct ft_state {
    FILE *logfile;
    char *rootdir;
};
#define FT_DATA ((struct ft_state *) fuse_get_context()->private_data)

#endif
