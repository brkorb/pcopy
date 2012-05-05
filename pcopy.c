/**
 * @mainpage pcopy Parallel Copy
 *
 * @section intro Introduction
 *
 * @include README
 */
/**
 * @file pcopy.c
 * This file defines the procedures called from generated code.
 * pcopy() and pcopy_usage(), plus the support routines they need.
 *
 * @file pcopy-opts.c
 * This file contains pure generated code, derived from pcopy-opts.def.
 * It includes a main() procedure which will call pcopy() for each file
 * name found either on the command line or from reading standard input.
 *
 * @file pcopy-opts.h
 * This file contains pure generated code, derived from pcopy-opts.def.
 */

#include "pcopy-opts.h"

/** the number of lines reserved at the top of the status page */
#define HDR_LINES 2

/** the line number to use for displaying info from a particular thread */
#define ENTRY_LINE  (HDR_LINES + 2 * fseg->idx)

/** the minimum value of the two arguments */
#define pc_min(_1, _2) (((_1) < (_2)) ? (_1) : (_2))

/**
 * strdup or die.  Do not return without memory.
 *
 * @param[in] istr  input string
 * @results   a newly allocated string
 */
static inline char *
strdup_or_die(char const * istr)
{
    char * res = strdup(istr);
    if (res == NULL)
        fserr(PCOPY_EXIT_NO_MEM, "strdup", istr);
    return res;
}

/**
 * The input read was short.  Formulate a somewhat more complex
 * "fserr()" exit call.
 *
 * @param[in,out] fseg  The descriptor of the segment that was being copied.
 * @param[in]     rdlen the length of the attempted read.
 * @param[in]     rdct  the count of bytes actually read.
 * @noreturn
 */
static void
short_read(file_seg_t * fseg, ssize_t rdlen, ssize_t rdct)
{
    static char const fmt[] =
        "th %2d read %d bytes not %d at 0x%08lX\n";
    char msgbf[sizeof(fmt) + 64];
    sprintf(msgbf, fmt,
            fseg->idx, (int)rdct, (int)rdlen, fseg->start);
    fserr(PCOPY_EXIT_FS_ERR_IN, msgbf, fseg->fname);
    /* NOTREACHED */
}

/**
 * Return nanoseconds since start of copy.
 *
 * @returns nanoseconds since program start as a 64 bit unsigned integer.
 */
uint64_t
get_time_delta(void)
{
    static struct timeval stv = {.tv_sec = 0};

    struct timeval ctv;

    if (gettimeofday(&ctv, NULL) < 0)
        fserr(PCOPY_EXIT_FAILURE, "gettimeofday", "current");

    if (stv.tv_sec == 0)
        stv = ctv;

    if (stv.tv_sec == ctv.tv_sec)
        return (ctv.tv_usec - stv.tv_usec) * THOUSAND;

    uint64_t res = ctv.tv_sec - stv.tv_sec - 1;
    res *= MILLION;
    res += (MILLION - stv.tv_usec) + ctv.tv_usec;
    return res * THOUSAND; // micros to nanos
}

/**
 * Start up the copy of a segment.  Once the open is complete and
 * the seek to the correct offset is done, let the main thread know
 * it can continue with the next thread.  We start one at a time.
 *
 * @param[in,out] fseg  The descriptor of the segment to copy.
 */
void
copy_start(file_seg_t * fseg)
{
    static char const st_fmt[]  = "th %2d reading 0x%08lX";

    if (! HAVE_OPT(QUIET)) {
        pthread_mutex_lock(&tty_mutex);
        if (fseg->idx == 0) {
            move(0,0);
            printw("copying %s", fseg->fname);
            move(1,0);
            printw("copy to %s", fseg->dname);
        }

        move(ENTRY_LINE, 0);
        printw(st_fmt, fseg->idx, fseg->start);
        refresh();
        pthread_mutex_unlock(&tty_mutex);
    }

    fseg->fdin = open(fseg->fname, O_RDONLY);
    if (fseg->fdin < 0)
        fserr(PCOPY_EXIT_FS_ERR_IN, "open (r)", fseg->fname);

    fseg->fdout = open(fseg->dname, O_RDWR | O_CREAT, 0600);
    if (fseg->fdout < 0)
        fserr(PCOPY_EXIT_FS_ERR_OUT, "open (w)", fseg->dname);

    if (fseg->start > 0) {
        if (lseek(fseg->fdin,  fseg->start, SEEK_SET) != fseg->start)
            fserr(PCOPY_EXIT_FS_ERR_IN, "seek (in)", fseg->fname);

        if (lseek(fseg->fdout, fseg->start, SEEK_SET) != fseg->start)
            fserr(PCOPY_EXIT_FS_ERR_OUT, "seek (out)", fseg->dname);
    }

    if (posix_fadvise(fseg->fdin, fseg->start, fseg->end - fseg->start,
                      POSIX_FADV_SEQUENTIAL) != 0)
        fserr(PCOPY_EXIT_FS_ERR_IN, "fadvise(r)", fseg->fname);

    if (posix_fadvise(fseg->fdout, fseg->start, fseg->end - fseg->start,
                      POSIX_FADV_SEQUENTIAL) != 0)
        fserr(PCOPY_EXIT_FS_ERR_OUT, "fadvise(w)", fseg->dname);

    pthread_mutex_lock(&th_start_mutex);
    pthread_cond_signal(&th_start_cond);
    pthread_mutex_unlock(&th_start_mutex);

    fseg->lastt = get_time_delta();
}

/**
 *  Show copy progress.  The display happens if "quiet" has not been
 *  specified.  This function will also sleep, if "flow-rate" has
 *  been specified and the time since the last segment read has been
 *  too short.
 *
 * @param[in,out] fseg  file segment descriptor.  "lastt" may be updated.
 */
void
copy_progress(file_seg_t * fseg)
{
    static char const prg_fmt[] =  "      read to 0x%08lX togo 0x%08lX";

    if (! HAVE_OPT(QUIET)) {
        pthread_mutex_lock(&tty_mutex);
        move(ENTRY_LINE + 1, 0);
        printw(prg_fmt, fseg->start, fseg->end - fseg->start);
        refresh();
        pthread_mutex_unlock(&tty_mutex);
    }

    if (HAVE_OPT(FLOW_RATE)) {
        uint64_t cdelta     = get_time_delta();     // delta from start
        uint64_t chunk_time = cdelta - fseg->lastt; // since chunk start

        /*
         * If the time used is more than 1/100 of a second less than
         * the time that is supposed to be consumed, then nanosleep.
         */
        if (chunk_time < nsec_per_iteration - (10*MILLION)) {
            uint64_t slptm = nsec_per_iteration - chunk_time;
            struct timespec ts = {
                .tv_sec  = (slptm > BILLION) ? (slptm / BILLION) : 0,
                .tv_nsec = (slptm > BILLION) ? (slptm % BILLION) : slptm
            };
            (void)nanosleep(&ts, NULL);
            fseg->lastt = get_time_delta();

        } else {
            fseg->lastt = cdelta;
        }
    }
}

/**
 * Copy wrapup.  If "quiet" has not been specified, then
 * print a message to the curses screen about this thread being done.
 *
 * @param[in] fseg  file segment descriptor.
 */
void
copy_wrap(file_seg_t * fseg)
{
    if (HAVE_OPT(QUIET))
        return;

    pthread_mutex_lock(&tty_mutex);
    move(ENTRY_LINE + 1, 0);
    clrtoeol();
    move(ENTRY_LINE, 0);
    clrtoeol();
    printw("th %2d DONE - copied 0x%08X bytes in %lu seconds",
           fseg->idx, fseg->end - fseg->start, get_time_delta() / BILLION);
    refresh();
    pthread_mutex_unlock(&tty_mutex);
}

/**
 * Copy thread code.  This copies one segment in parallel with the other
 * segments.  It is copied in reads and writes of several k-bytes at a time.
 *
 * @param[in] arg the file_seg_t pointer describing the segment to copy.
 */
static void *
do_copy(void * arg)
{
    file_seg_t * fseg = arg;
    size_t const readlim  =
        pc_min(fseg->end - fseg->start, OPT_VALUE_MAX_CHUNK);

    copy_start(fseg);

    for (;;) {
        ssize_t rdlen = fseg->end - fseg->start;
        if (rdlen > readlim)
            rdlen = readlim;
        ssize_t rdct = read(fseg->fdin, fseg->buffer, rdlen);
        if (rdct <= 0)
            short_read(fseg, rdlen, rdct);

        rdlen = write(fseg->fdout, fseg->buffer, rdct);
        if (rdlen != rdct)
            fserr(PCOPY_EXIT_FS_ERR_OUT, "write", fseg->dname);

        fseg->start += rdlen;
        if (fseg->start >= fseg->end)
            break;
        copy_progress(fseg);
    }

    copy_wrap(fseg);
    free(arg);
    pthread_exit(0);
}

/**
 * Make a destination name.  If the destination option is not provided,
 * the destination is the base name of the input file.  If the destination
 * is a directory, the base name of the source is always appended.
 * If the destination is a file, then make certain we only copy one file
 * to this destination.
 *
 * @param[in] sname  source file name.
 * @returns   the name of the corresponding output file.
 */
static inline char const *
make_dest_name(char const * sname)
{
    static char const no_dir[] =
        "no directory component in source name '%s'\n"
        "and no destination directory was specified.\n";

    if (! HAVE_OPT(DESTINATION)) {
        char const * p = strrchr(sname, '/');
        if (p == NULL)
            usage_message(no_dir, sname);
        unlink(++p);
        return strdup_or_die(p);
    }

    {
        static bool been_here = false;
        char const * dname = OPT_ARG(DESTINATION);
        struct stat sb;

        if (stat(dname, &sb) == 0) {
            if (S_ISDIR(sb.st_mode)) {
                char const * bn = strrchr(sname, '/');
                char * p;
                bn = bn ? (bn + 1) : sname;
                p  = malloc(strlen(dname) + strlen(bn) + 2);
                if (p == NULL)
                    fserr(PCOPY_EXIT_NO_MEM, "allocating", "destination name");
                sprintf(p, "%s/%s", dname, bn);
                unlink(p);
                return p;
            }
            if (S_ISREG(sb.st_mode)) {
                if (been_here)
                    die(PCOPY_EXIT_BAD_CONFIG,
                        "destination for multiple sources is one file");
                been_here = true;
                unlink(dname);
                return strdup_or_die(dname);
            }
            errno = EINVAL;
            fserr(PCOPY_EXIT_BAD_CONFIG,
                  "invalid destination fs type (not dir or file)", dname);
        }

        /*
         * We could not stat "dname".  It must be a file name.  Make sure any
         * directory part exists.  If we call this routine again, we should
         * successfully stat our destination file and trip over the
         * "been_here" flag.
         */
        dname = strdup_or_die(dname);
        been_here = true;
        {
            static char const bad_stat[] = "stat-ing dir portion";
            char * p = strrchr(dname, '/');
            if (p == NULL) {
                /*
                 * No directory part.  Destination file is for current dir.
                 */
                return dname;
            }

            *p = NUL;
            if (stat(dname, &sb) != 0)
                fserr(PCOPY_EXIT_BAD_CONFIG, bad_stat, OPT_ARG(DESTINATION));
            if (! S_ISDIR(sb.st_mode)) {
                errno = ENOTDIR;
                fserr(PCOPY_EXIT_BAD_CONFIG, bad_stat, OPT_ARG(DESTINATION));
            }
            *p = '/';
        }

        return dname;
    }
}

/**
 * Allocate and initialize a file_seg_t.
 *
 * @param[in] sfile   source file name
 * @param[in] dfile   destination file name
 * @param[in] start   start offset
 * @param[in] seg_len length of segment to process
 *
 * @returns the allocated and initialized result
 */
file_seg_t *
new_file_seg(char const * sfile, char const * dfile, size_t start, size_t seg_len)
{
    file_seg_t * res = malloc(sizeof(*res) + seg_len);
    if (res == NULL)
        fserr(PCOPY_EXIT_NO_MEM, "alloc copy space", sfile);

    memset(res, NUL, sizeof(*res));

    res->start  = start;
    res->end    = start + seg_len;
    res->fname  = sfile;
    res->dname  = dfile;
    return res;
}

/**
 * The callout function that invokes the optionUsage() function, but first
 * makes sure "endwin()" gets called.
 *
 * @param opts  the AutoOpts option description structure
 * @param ex    the program exit code.
 */
void
pcopy_usage(tOptions * opts, int ex)
{
    if (curses_active) endwin();
    optionUsage(opts, ex);
}

/**
 * kick off a thread.  And do the cond-wait dance to ensure we don't have
 * a gazillion of these running at once.
 *
 * @param[in] pth   The pointer to the pthread identifier
 * @param[in] arg   The file_seg_t pointer specifying what is to be copied.
 */
static inline void
start_copy(pthread_t * pth, void * arg)
{
    pthread_create(pth, NULL, do_copy, arg);
    pthread_mutex_lock(&th_start_mutex);
    pthread_cond_wait(&th_start_cond, &th_start_mutex);
    pthread_mutex_unlock(&th_start_mutex);
    struct timespec ts = { .tv_sec = 0, .tv_nsec = BILLION / 10 };
    (void)nanosleep(&ts, NULL);
}

/**
 * Copy parts of a file in parallel.  They are copied into place in the output
 * file.  Various options determine where to put the output file, how to split
 * it up, and how many bytes per second should be consumed with the process.
 *
 * @param[in] fn the input file name.
 */
void
pcopy(char const * fn)
{
    char const * dname = make_dest_name(fn);
    size_t base_size;
    size_t seg_size;
    size_t xfer_sz;
    int    ix = 0;

    if (strcmp(fn, dname) == 0)
        die(PCOPY_EXIT_BAD_CONFIG,
            "source and destination must be distinct:\n\t%s", fn);

    {
        struct stat sb;
        if (stat(fn, &sb) < 0)
            fserr(PCOPY_EXIT_FAILURE, "cannot re-stat", fn);
        if (! S_ISREG(sb.st_mode)) {
            errno = EINVAL;
            fserr(PCOPY_EXIT_FS_ERR_IN, "regular file check", fn);
        }
        base_size = sb.st_size;
        xfer_sz   = sb.st_blksize;
    }

    {
        size_t sz = base_size / OPT_VALUE_THREAD_CT;

        /*
         * Now make sure seg_size is an even multiple of xfer_sz
         * and that thread-ct * size >= base_size
         */
        seg_size = xfer_sz * (sz / xfer_sz);
        if ((seg_size * OPT_VALUE_THREAD_CT) < base_size)
            seg_size += xfer_sz;
    }

    {
        size_t sz = (OPT_VALUE_MAX_CHUNK / xfer_sz) * xfer_sz;
        if (sz == 0)
            sz = xfer_sz;
        OPT_VALUE_MAX_CHUNK = sz;
    }

    if (HAVE_OPT(FLOW_RATE)) {
        /* bytes per second divided by thread count: */
        float fr = (float)OPT_VALUE_FLOW_RATE;
        fr /= (float)OPT_VALUE_THREAD_CT;

        /* bytes/sec divided by byts/chunk -> chunks/second */
        fr /= (float)OPT_VALUE_MAX_CHUNK;

        /*
         * nanoseconds per second div. by chunks per second
         * yields nanoseconds per chunk (iteration).
         */
        nsec_per_iteration = BILLION / fr;
    }

    memset(pth_list, 0, OPT_VALUE_THREAD_CT * sizeof(*pth_list));

    for (xfer_sz = 0; xfer_sz < base_size; ix++) {
        file_seg_t * fseg = new_file_seg(fn, dname, xfer_sz, seg_size);

        xfer_sz    += seg_size;
        fseg->end   = xfer_sz;
        if (fseg->end > base_size)
            fseg->end = base_size;
        fseg->idx   = ix;
        start_copy(pth_list + ix, fseg);
    }

    for (int j = 0; j < ix; j++)
        pthread_join(pth_list[j], NULL);
    free((void *)dname);
}
