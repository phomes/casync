#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cadecoder.h"
#include "caencoder.h"

static int encode(int dfd, int fd) {
        CaEncoder *e = NULL;
        int r;

        assert(dfd >= 0);
        assert(fd >= 0);

        e = ca_encoder_new();
        if (!e) {
                r = -ENOMEM;
                goto finish;
        }

        r = ca_encoder_set_base_fd(e, dfd);
        if (r < 0)
                goto finish;

        dfd = -1;

        for (;;) {
                r = ca_encoder_step(e);
                if (r < 0)
                        goto finish;

                switch (r) {

                case CA_ENCODER_FINISHED:
                        r = 0;
                        goto finish;

                case CA_ENCODER_NEXT_FILE: {
                        char *path;

                        r = ca_encoder_current_path(e, &path);
                        if (r < 0)
                                goto finish;

                        fprintf(stderr, "Now looking at: %s\n", path);
                        free(path);
                }

                        /* Fall through */

                case CA_ENCODER_DATA: {
                        const void *p;
                        size_t sz;
                        ssize_t n;

                        r = ca_encoder_get_data(e, &p, &sz);
                        if (r < 0)
                                goto finish;

                        assert(r > 0);

                        n = write(fd, p, sz);
                        if (n < 0) {
                                r = -errno;
                                goto finish;
                        }

                        break;
                }

                default:
                        assert(false);
                }
        }

finish:

        if (r == 0) {
                uint64_t offset;
                off_t foffset;

                r = ca_encoder_current_archive_offset(e, &offset);
                if (r < 0)
                        goto finish;

                foffset = lseek(fd, 0, SEEK_CUR);
                if (foffset == (off_t) -1) {
                        r = -errno;
                        goto finish;
                }

                if ((off_t) offset != foffset) {
                        r = -EIO;
                        goto finish;
                }
        }

        ca_encoder_unref(e);

        if (fd >= 0)
                (void) close(fd);

        if (dfd >= 0)
                (void) close(dfd);

        return r;
}

static int decode(int fd) {
        CaDecoder *d = NULL;
        int r;

        assert(fd >= 0);

        d = ca_decoder_new();
        if (!d) {
                r = -ENOMEM;
                goto finish;
        }

        r = ca_decoder_set_base_mode(d, S_IFDIR);
        if (r < 0)
                goto finish;

        for (;;) {
                r = ca_decoder_step(d);
                if (r < 0)
                        goto finish;

                switch (r) {

                case CA_DECODER_FINISHED:
                        r = 0;
                        goto finish;

                case CA_DECODER_STEP:
                        break;

                case CA_DECODER_REQUEST:
                        r = ca_decoder_put_data_fd(d, fd, UINT64_MAX, UINT64_MAX);
                        if (r < 0)
                                goto finish;

                        break;

                case CA_DECODER_NEXT_FILE: {
                        char *path = NULL;
                        mode_t mode;

                        r = ca_decoder_current_mode(d, &mode);
                        if (r < 0)
                                goto finish;

                        r = ca_decoder_current_path(d, &path);
                        if (r < 0)
                                goto finish;

                        printf("%08o %s\n", mode, path);
                        free(path);
                        break;
                }

                case CA_DECODER_PAYLOAD:
                        /* ignore for now */
                        break;

                }
        }

finish:
        ca_decoder_unref(d);

        if (fd >= 0)
                (void) close(fd);

        return r;
}

int main(int argc, char *argv[]) {
        char t[] = "/var/tmp/castream-test.XXXXXX";
        int fd = -1, dfd = -1, r;

        dfd = open(argc > 1 ? argv[1] : ".", O_CLOEXEC|O_RDONLY|O_NOCTTY);
        if (dfd < 0) {
                r = -errno;
                goto finish;
        }

        fd = mkostemp(t, O_WRONLY|O_CLOEXEC);
        if (fd < 0) {
                r = -errno;
                goto finish;
        }

        fprintf(stderr, "Writing to: %s\n", t);

        r = encode(dfd, fd);
        dfd = fd = -1;
        if (r < 0)
                goto finish;

        fd = open(t, O_RDONLY|O_CLOEXEC);
        if (fd < 0) {
                r = -errno;
                goto finish;
        }

        r = decode(fd);
        fd = -1;

finish:
        fprintf(stderr, "Done: %s\n", strerror(-r));

        if (fd >= 0)
                (void) close(fd);

        if (dfd >= 0)
                (void) close(dfd);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
