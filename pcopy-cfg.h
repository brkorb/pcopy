
#ifndef  _GNU_SOURCE
#define  _GNU_SOURCE 1
#define  _DARWIN_FEATURE_64_BIT_INODE

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <ncurses.h>  
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

#ifndef  NUL
# define NUL '\0'
#endif

#define THOUSAND  1000
#define MILLION   1000000
#define BILLION   1000000000

typedef struct {
    char const *    fname;
    char const *    dname;
    unsigned long   start;
    unsigned long   end;
    uint64_t        lastt;
    uint32_t        idx;
    int             fdin, fdout;
    char            buffer[sizeof(char *)];
} file_seg_t;

#endif /* _GNU_SOURCE */
