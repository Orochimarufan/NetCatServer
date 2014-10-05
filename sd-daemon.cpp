/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>

#include <sstream>

#include "sd-daemon.h"

int s2int(const char *s)
{
    int i;
    std::istringstream is(s);
    is >> i;
    return i;
}

int sd_listen_fds(int unset_environment) {
        const char *e;
        unsigned n;
        int r, fd, pid;

        e = getenv("LISTEN_PID");
        if (!e) {
                r = 0;
                goto finish;
        }

        pid = s2int(e);

        /* Is this for us? */
        if (getpid() != pid) {
                r = 0;
                goto finish;
        }

        e = getenv("LISTEN_FDS");
        if (!e) {
                r = 0;
                goto finish;
        }

        n = s2int(e);

        for (fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + (int) n; fd ++) {
                r = fcntl(fd, F_SETFD, FD_CLOEXEC);
                if (r < 0)
                        goto finish;
        }

        r = (int) n;

finish:
        if (unset_environment) {
                unsetenv("LISTEN_PID");
                unsetenv("LISTEN_FDS");
        }

        return r;
}
