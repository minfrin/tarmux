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
#include <getopt.h>
#include <poll.h>
#include <sys/fcntl.h>

#include <archive.h>
#include <archive_entry.h>

#include "config.h"

typedef struct mux_t
{
    struct archive_entry *entry;
    int64_t offset;
    int fd;
} mux_t;

void help(const char *name)
{
    printf("Usage: %s [-f tarname] [-n sourcename] [file1] [file2] [...]",
            name);
}

void version()
{
    printf(PACKAGE_STRING "\n");
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

    while ((opt = getopt(argc, argv, "hvf:-:")) != -1) {
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

    /* remaining parameters are files to mux, otherwise default to stdin */
    mux_count = argc - optind;
    mux = calloc(mux_count > 0 ? mux_count : 1, sizeof(mux_t));
    fds = calloc(mux_count > 0 ? mux_count : 1, sizeof(struct pollfd));
    for (i = 0; i < mux_count; i++) {

        mux[i].entry = archive_entry_new();

        archive_entry_set_filetype(mux[i].entry, AE_IFREG);
        archive_entry_set_pathname(mux[i].entry, argv[optind + i]);
        archive_entry_copy_sourcepath(mux[i].entry, argv[optind + i]);

        if ((mux[i].fd = open(archive_entry_sourcepath(mux[i].entry),
                O_RDONLY | O_NONBLOCK)) < 0) {
            perror(archive_entry_sourcepath(mux[i].entry));
            exit(2);
        }

        fds[i].fd = mux[i].fd;
        fds[i].events = POLLIN;

    }
    if (mux_count == 0) {

        mux[0].entry = archive_entry_new();

        archive_entry_set_filetype(mux[0].entry, AE_IFREG);
        archive_entry_set_pathname(mux[0].entry, stdin_name);
        archive_entry_copy_sourcepath(mux[0].entry, stdin_name);

        mux[0].fd = STDIN_FILENO;

        fds[0].fd = mux[0].fd;
        fds[0].events = POLLIN | POLLHUP | POLLERR;

        mux_count = 1;

    }

    /* make sure our tar stream is open for append */
    if (strcmp(out_file, "-")) {
        if ((out_fd = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0666))
                < 0) {
            perror(out_file);
            exit(1);
        }
    }

    /* create a buffer for our needs */
    buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Could not allocate buffer.\n");
        exit(3);
    }

    /* set up the output tar archive */
    a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    archive_write_open_fd(a, out_fd);

    remaining = mux_count;

    while (remaining) {
        int rc;

        rc = poll(fds, mux_count, -1);

        for (i = 0; i < mux_count; i++) {
            if (fds[i].revents & POLLIN) {
                ssize_t offset = 0;
                size_t size = buffer_size;

                do {
                    ssize_t len;
                    len = read(fds[i].fd, buffer + offset, size);
                    if (len < 0) {
                        perror(archive_entry_sourcepath(mux[i].entry));
                        exit(4);
                    }
                    else if (len == 0) {
                        break;
                    }
                    offset += len;
                    size -= len;
                } while (size);

                archive_entry_sparse_clear(mux[i].entry);
                archive_entry_sparse_add_entry(mux[i].entry, mux[i].offset,
                        offset);
                mux[i].offset += offset;

                archive_entry_set_size(mux[i].entry, mux[i].offset);

                archive_write_header(a, mux[i].entry);
                archive_write_data(a, buffer, offset);

                if (offset == 0) {

                    fds[i].events = 0;
                    archive_entry_free(mux[i].entry);
                    close(mux[i].fd);

                    remaining--;

                }

            }
        }

    }

    archive_write_close(a);
    archive_write_free(a);

    close(out_fd);
    free(buffer);
    free(fds);
    free(mux);

    exit(0);

}
