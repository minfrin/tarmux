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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#include <archive.h>
#include <archive_entry.h>

#include "config.h"

typedef struct demux_t
{
    char *pathname;
    int64_t offset;
    int fd;
} demux_t;

void help(const char *name)
{
    printf(
            "Usage: %s [-f streamname] [-a] [-r] [file1] [file2] [...]\n"
                    "\n"
                    "This tool demultiplexes streams that have been multiplexed by the\n"
                    "tarmux tool. It expects a series of tar files containing sparse file\n"
                    "fragments that are unpacked and written to form the original stream.\n"
                    "\n"
                    "In the simplest form, tardemux reads a stream from stdin, unpacks the\n"
                    "stream and writes the first stream to stdout, leaving additional data\n"
                    "intact. This allows tardemux to be run again to extract a further\n"
                    "stream.\n"
                    "\n"
                    "If file parameters are specified, the stream is expected to contain\n"
                    "entries matching these files parameters. The fragments will be unpacked\n"
                    "and written to the given paths.\n"
                    "\n"
                    "  -f name, --file=name\tThe name of the input files from which tar\n"
                    "\t\t\tstreams will be read, defaults to stdin. Can be specified more\n"
                    "\t\t\tthan once.\n"
                    "  -a\t\t\tUnpack all pathnames in a stream to individual files.\n"
                    "  -r\t\t\tTreat the incoming stream as a raw compressed stream rather\n"
                    "\t\t\tthan a tar stream.\n"
                    "  [file1] [...]\t\tOptional files/pipes expected in the tar stream.\n"
                    "\t\t\tData will be demultiplexed and written to each file/pipe. If this\n"
                    "\t\t\tfile/pipe exists, data will be written to the existing file.\n"
                    "\n"
                    "This tool is based on libarchive, and is licensed under the Apache License,\n"
                    "Version 2.0.\n"
                    "", name);
}

void version()
{
    printf(PACKAGE_STRING "\n");
}

int transfer(struct archive *a, demux_t *demux)
{
    const void *buff;
    size_t len;
    off_t offset;
    ssize_t size;

    int rv;
    int blocks = 0;

    for (;;) {

        for (;;) {
            rv = archive_read_data_block(a, &buff, &len, &offset);
            if (rv == ARCHIVE_FATAL) {
                fprintf(stderr, "Error: %s\n", archive_error_string(a));
                return -1;
            }
            if (rv == ARCHIVE_WARN) {
                fprintf(stderr, "Warning: %s\n", archive_error_string(a));
                break;
            }
            if (rv == ARCHIVE_RETRY) {
                fprintf(stderr, "Warning: %s\n", archive_error_string(a));
                continue;
            }
            break;
        }

        if (len == 0) {
            return blocks;
        }

        if (demux->fd != STDOUT_FILENO) {
            offset = lseek(demux->fd, offset, SEEK_SET);
            if (offset < 0) {
                fprintf(stderr, "Error: Could not lseek on %s: %s\n",
                        demux->pathname, strerror(errno));
                return -1;
            }
        }

        do {
            size = write(demux->fd, buff, len);
            if (size < 0) {
                fprintf(stderr, "Error: Could not write to %s: %s\n",
                        demux->pathname, strerror(errno));
                return -1;
            }
            len -= size;
            buff += size;
        } while (len);

        blocks++;
    }

    return -1;
}

int main(int argc, char * const argv[])
{

    struct archive *a;
    struct archive_entry *entry;
    demux_t *demux = NULL;
    demux_t *sdemux = NULL;

    const char *name = argv[0];
    const char **filenames = NULL;

    size_t blocksize = 10240;

    int opt;
    int all = 0;
    int raw = 0;
    int filenames_num = 0;
    int rv;
    int demux_count;
    int i;

    while ((opt = getopt(argc, argv, "hvarf:n:-:")) != -1) {
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
            break;
        case 'h':
            help(name);
            exit(0);
        case 'v':
            version();
            exit(0);
        case 'a':
            all = 1;
            break;
        case 'r':
            raw = 1;
            break;
        case 'f':
            filenames = realloc(filenames,
                    (filenames_num + 2) * sizeof(const char *));
            filenames[filenames_num] = optarg;
            filenames_num++;
            filenames[filenames_num] = NULL;
            break;
        default:
            help(name);
            exit(1);
        }
    }

    /* make sure we don't die on sigpipe */
    signal(SIGPIPE, SIG_IGN);

    /* remaining parameters are files to mux, otherwise default to stdin */
    demux_count = argc - optind;
    if (demux_count || all) {
        demux = calloc(demux_count, sizeof(demux_t));
        for (i = 0; i < demux_count; i++) {

            demux[i].pathname = strdup(argv[optind + i]);

            if ((demux[i].fd = open(demux[i].pathname,
                    O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0666)) < 0) {
                perror(demux[i].pathname);
                exit(2);
            }

        }
    }
    else {
        sdemux = calloc(1, sizeof(demux_t));
        sdemux->fd = STDOUT_FILENO;
    }

    /* set up the input archive */
    a = archive_read_new();
    archive_read_support_filter_all(a);
    if (raw) {
        archive_read_support_format_raw(a);
    }
    else {
        archive_read_support_format_all(a);
    }

    if (!filenames) {
        archive_read_open_fd(a, STDIN_FILENO, blocksize);
    }
    else {
        if ((rv = archive_read_open_filenames(a, filenames, blocksize))) {
            fprintf(stderr, "Could not open archive(s): %s\n",
                    archive_error_string(a));
            exit(1);
        }
    }

    for (;;) {

        rv = archive_read_next_header(a, &entry);
        if (rv == ARCHIVE_EOF) {
            break;
        }
        if (rv < ARCHIVE_OK) {
            fprintf(stderr, "Error: %s\n", archive_error_string(a));
            exit(1);
        }

        /* handle demux to stdout */
        if (sdemux) {
            if (!sdemux->pathname) {
                sdemux->pathname = strdup(archive_entry_pathname(entry));
                transfer(a, sdemux);
            }
            else if (!strcmp(sdemux->pathname, archive_entry_pathname(entry))) {
                transfer(a, sdemux);
            }
            else {
                fprintf(stderr,
                        "Error: Unexpected additional path in stream, aborting: %s\n",
                        archive_entry_pathname(entry));
                exit(1);
            }
        }

        /* handle demux to individual files */
        else {
            demux_t *dm = NULL;
            int found = 0;
            for (i = 0; i < demux_count; i++) {
                if (!strcmp(demux[i].pathname, archive_entry_pathname(entry))) {
                    dm = &demux[i];
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (all) {
                    demux = realloc(demux, (demux_count + 1) * sizeof(demux_t));

                    demux[demux_count].pathname = strdup(
                            archive_entry_pathname(entry));
                    demux[demux_count].offset = 0;

                    if ((demux[demux_count].fd = open(
                            demux[demux_count].pathname,
                            O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0666))
                            < 0) {
                        perror(demux[demux_count].pathname);
                        exit(2);
                    }

                    dm = &demux[demux_count];

                    demux_count++;

                }
                else {
                    fprintf(stderr,
                            "Error: Unnamed path in stream, aborting: %s\n",
                            archive_entry_pathname(entry));
                    exit(1);
                }
            }
            if (dm) {
                rv = transfer(a, dm);
                if (rv < 0) {
                    exit(1);
                }
                else if (rv == 0) {
                    if (close(dm->fd)) {
                        fprintf(stderr, "Error: Could not close %s: %s\n",
                                dm->pathname, strerror(errno));
                        exit(1);
                    }
                    dm->fd = 0;
                }
            }
        }

    }

    /* clean up the output files */
    if (demux) {
        for (i = 0; i < demux_count; i++) {
            if (demux[i].fd) {
                close(demux[i].fd);
            }
            free(demux[i].pathname);
        }
        free(demux);
    }
    if (sdemux) {
        free(sdemux->pathname);
        free(sdemux);
    }

    exit(0);
}
