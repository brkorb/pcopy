/**
 * @file pcopy-cfg.h
 * Headers, data structures, and defines needed by pcopy.
 */
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
# define NUL '\0' //!< The NUL byte
#endif

#define THOUSAND  1000        //!< a thousand (1/a milli)
#define MILLION   1000000     //!< a million  (1/a micro)
#define BILLION   1000000000  //!< a billion  (1/a nano)

/**
 * What needs to be done by one thread.  Each thread is given a copy
 * of this structure.  The thread copies the data from "start" up to,
 * but not including, "end".
 */
typedef struct {
    char const *    fname;  //!< input file name
    char const *    dname;  //!< output file or directory
    unsigned long   start;  //!< starting offset
    unsigned long   end;    //!< ending offset
    uint64_t        lastt;  //!< nonosecends since completing last read
    uint32_t        idx;    //!< thread index
    int             fdin;   //!< input file descriptor
    int             fdout;  //!< output file descriptor
    char            buffer[sizeof(char *)]; //!< read/write buffer
} file_seg_t;

#endif /* _GNU_SOURCE */
