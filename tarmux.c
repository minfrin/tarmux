/**
 *    (C) 2016 Graham Leggett
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <getopt.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include <archive.h>
#include <archive_entry.h>

#include "config.h"

#ifndef HAVE_CLOCK_GETTIME
/* clock_gettime is not implemented on MacOSX */
#include <sys/time.h>
#define CLOCK_REALTIME 0
int clock_gettime(int clk_id, struct timespec *t)
{
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (!rv) {
        t->tv_sec = now.tv_sec;
        t->tv_nsec = now.tv_usec * 1000;
    }
    return rv;
}
#endif

typedef struct mux_t
{
    struct archive_entry *entry;
    const char *pathname;
    int64_t index;
    int fd;
} mux_t;

void help(const char *name)
{
    printf(
            "Usage: %s [-r] [-f streamname] [-n sourcename] [file1] [file2] [...]\n"
                    "\n"
                    "This tool multiplexes streams such that they may be combined on one\n"
                    "system and then split apart on another. It does so by wrapping each\n"
                    "stream in a series of tar files, each tar file representing a sparse\n"
                    "fragment of the original stream, creating a tar stream.\n"
                    "\n"
                    "In the simplest form, tarmux reads data from stdin, and then outputs\n"
                    "the tar stream to stdout. The corresponding tardemux command reverses\n"
                    "this process by reading to the end of the tar stream, but no further.\n"
                    "This allows streams to be concatenated and later separated from one\n"
                    "another.\n"
                    "\n"
                    "If file parameters are specified, data is read from each file concurrently\n"
                    "and added to the tar stream. If pipe parameters are specified, data\n"
                    "can be read and multiplexed from other processes. When multiple file or\n"
                    "pipe parameters are specified, data is read concurrently and interleaved\n"
                    "until the last file or pipe has closed.\n"
                    "\n"
                    "  -f name, --file=name\t\tThe name of the output file to which tar\n"
                    "\t\t\t\tstreams will be appended, defaults to stdout.\n"
                    "  -n pathname, --name=pathname\tThe pathname to embed in the tar\n"
                    "\t\t\t\tfiles when the input is stdin. Defaults to '-'.\n"
                    "  [file1] [...]\t\t\tOptional files/pipes whose content will be included in\n"
                    "\t\t\t\tthe tar stream. Regardless of the type of source, data is\n"
                    "\t\t\t\tembedded as a regular file in the tar stream.\n"
                    "\n"
                    "This tool is based on libarchive, and is licensed under the Apache License,\n"
                    "Version 2.0.\n"
                    "", name);
}

void version()
{
    printf(PACKAGE_STRING "\n");
}

static void entry_pathindex(mux_t *mux)
{
    char *name;

    name = malloc(
            (snprintf(NULL, 0, "%s.%" PRId64 "", mux->pathname, mux->index) + 1)
                    * sizeof(char));
    sprintf(name, "%s.%" PRId64 "", mux->pathname, mux->index);

    archive_entry_copy_pathname(mux->entry, name);

    free(name);

    mux->index++;

}

int main(int argc, char * const argv[])
{

    struct archive *a;
    mux_t *mux;
    struct pollfd *fds;

    const char *name = argv[0];
    const char *out_file = "-";
    const char *stdin_name = "-";
    unsigned char *buffer;

    size_t buffer_size = 1024 * 1024;

    int out_fd = STDOUT_FILENO;
    int opt;
    int mux_count;
    int i;
    int remaining;
    int raw = 0;
    int rv;

    while ((opt = getopt(argc, argv, "hvrf:n:-:")) != -1) {
        switch (opt) {
        case '-':
            if (!strcmp(optarg, "help")) {
                help(name);
                exit(0);
            }
            else if (!strcmp(optarg, "version")) {
                version();
                exit(0);
            }
            else if (!strcmp(optarg, "name")) {
                stdin_name = optarg;
                exit(0);
            }
            break;
        case 'h':
            help(name);
            exit(0);
        case 'v':
            version();
            exit(0);
        case 'r':
            raw = 1;
            break;
        case 'f':
            out_file = optarg;
            break;
        case 'n':
            stdin_name = optarg;
            break;
        default:
            help(name);
            exit(1);
        }
    }

    /* make sure we don't die on sigpipe */
    signal(SIGPIPE, SIG_IGN);

    /* make sure our tar stream is open for append */
    if (strcmp(out_file, "-")) {
        if ((out_fd = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0666))
                < 0) {
            perror(out_file);
            exit(1);
        }
    }

    /* set up the output tar archive */
    a = archive_write_new();

    if (raw) {
#ifdef HAVE_ARCHIVE_WRITE_SET_FORMAT_RAW
        archive_write_set_format_raw(a);
#else
        fprintf(stderr,
                "Error: Raw mode not supported on this platform, aborting.\n");
        exit(2);
#endif
    }
    else {
        archive_write_set_format_pax_restricted(a);
    }

    archive_write_open_fd(a, out_fd);

    /* remaining parameters are files to mux, otherwise default to stdin */
    mux_count = argc - optind;
    mux = calloc(mux_count > 0 ? mux_count : 1, sizeof(mux_t));
    fds = calloc(mux_count > 0 ? mux_count : 1, sizeof(struct pollfd));
    for (i = 0; i < mux_count; i++) {
        struct stat st;

        mux[i].entry = archive_entry_new();
        mux[i].pathname = argv[optind + i];

        archive_entry_set_filetype(mux[i].entry, AE_IFREG);
        archive_entry_copy_sourcepath(mux[i].entry, argv[optind + i]);

        if ((mux[i].fd = open(archive_entry_sourcepath(mux[i].entry),
                O_RDONLY | O_NONBLOCK)) < 0) {
            perror(archive_entry_sourcepath(mux[i].entry));
            exit(2);
        }

        if ((rv = fstat(mux[i].fd, &st))) {
            perror(mux[i].pathname);
            exit(1);
        }
        archive_entry_copy_stat(mux[i].entry, &st);

        fds[i].fd = mux[i].fd;
        fds[i].events = POLLIN;

    }
    if (mux_count == 0) {
        struct timespec tp;

        mux[0].entry = archive_entry_new();
        mux[0].pathname = stdin_name;

        archive_entry_set_filetype(mux[0].entry, AE_IFREG);
        archive_entry_copy_sourcepath(mux[0].entry, stdin_name);

        mux[0].fd = STDIN_FILENO;

        clock_gettime(CLOCK_REALTIME, &tp);
        archive_entry_set_atime(mux[0].entry, tp.tv_sec, tp.tv_nsec);
        archive_entry_set_birthtime(mux[0].entry, tp.tv_sec, tp.tv_nsec);
        archive_entry_set_ctime(mux[0].entry, tp.tv_sec, tp.tv_nsec);

        archive_entry_set_perm(mux[0].entry, 0666);

        fds[0].fd = mux[0].fd;
        fds[0].events = POLLIN;

        mux_count = 1;

    }

    /* sanity check - we can only use raw if we're muxing one file */
    if (raw) {
        if (mux_count > 1) {
            fprintf(stderr,
                    "Error: Raw mode cannot be used with multiple files, aborting.\n");
            exit(3);
        }
        else {
            if ((rv = archive_write_header(a, mux[0].entry))) {
                fprintf(stderr, "Could not write header: %s\n",
                        archive_error_string(a));
                exit(1);
            }
        }
    }

    /* create a buffer for our needs */
    buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Could not allocate buffer.\n");
        exit(3);
    }

    remaining = mux_count;

    while (remaining) {
        int rc;

        rc = poll(fds, mux_count, -1);
        if (rc < 0) {
            perror("Error: failure during poll");
            exit(2);
        }

        for (i = 0; i < mux_count; i++) {
            if ((fds[i].revents & POLLIN) || (fds[i].revents & POLLHUP)) {
                ssize_t offset = 0;
                size_t size = buffer_size;

                do {
                    ssize_t len;

                    len = read(fds[i].fd, buffer + offset, size);
                    if (len < 0) {
                        if (EOF == errno) {
                            len = 0;
                            break;
                        }
                        else {
                            perror(archive_entry_sourcepath(mux[i].entry));
                            exit(4);
                        }
                    }
                    else if (len == 0) {
                        break;
                    }

                    offset += len;
                    size -= len;

                    /* if we would block, leave */
                    if (poll(&fds[i], 1, 0) < 1) {
                        break;
                    }

                } while (size);

                if (!raw) {

                    entry_pathindex(&mux[i]);

                    archive_entry_set_size(mux[i].entry, offset);

                    if ((rv = archive_write_header(a, mux[i].entry))) {
                        fprintf(stderr, "Could not write header: %s\n",
                                archive_error_string(a));
                        exit(1);
                    }

                }

                offset = archive_write_data(a, buffer, offset);
                if (offset < 0) {
                    fprintf(stderr, "Error: Could not write data: %s\n",
                            archive_error_string(a));
                    exit(4);
                }

                if (offset == 0) {

                    if ((rv = archive_write_finish_entry(a))) {
                        fprintf(stderr, "Could not write finish entry: %s\n",
                                archive_error_string(a));
                        exit(1);
                    }

                    fds[i].events = 0;
                    archive_entry_free(mux[i].entry);
                    close(mux[i].fd);

                    remaining--;

                }

            }

        }

    }

    if ((rv = archive_write_close(a))) {
        fprintf(stderr, "Could not close write: %s\n", archive_error_string(a));
        exit(1);
    }

    archive_write_free(a);

    close(out_fd);
    free(buffer);
    free(fds);
    free(mux);

    exit(0);

}
