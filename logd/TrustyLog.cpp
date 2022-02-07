/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TrustyLog.h"
#include <private/android_logger.h>
#include "LogBuffer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TRUSTY_LINE_BUFFER_SIZE 256
static const char trustyprefix[] = "trusty";

TrustyLog::TrustyLog(LogBuffer* buf, int fdRead) : SocketListener(fdRead, false), logbuf(buf) {}

void TrustyLog::create(LogBuffer* buf) {
    if (access("/sys/module/trusty_log/parameters/log_size", F_OK)) {
        /* this device has the old driver which doesn't support poll() */
        return;
    }

    int fd = TEMP_FAILURE_RETRY(open("/dev/trusty-log0", O_RDONLY | O_NDELAY | O_CLOEXEC));
    if (fd >= 0) {
        TrustyLog* tl = new TrustyLog(buf, fd);
        if (tl->startListener()) {
            delete tl;
        }
    }
}

bool TrustyLog::onDataAvailable(SocketClient* cli) {
    char buffer[4096];
    char linebuffer[TRUSTY_LINE_BUFFER_SIZE + sizeof(trustyprefix) + 1];
    ssize_t len = 0;
    for (;;) {
        ssize_t retval = 0;
        if (len < (ssize_t)(sizeof(buffer) - 1)) {
            retval = TEMP_FAILURE_RETRY(
                    read(cli->getSocket(), buffer + len, sizeof(buffer) - 1 - len));
        }
        if (retval > 0) {
            len += retval;
        }
        if ((retval <= 0) && (len <= 0)) {
            // nothing read and nothing to read
            break;
        }

        // log the complete lines we have so far
        char* linestart = buffer;
        for (;;) {
            char* lineend = static_cast<char*>(memchr(linestart, '\n', len));
            if (lineend) {
                size_t linelen = lineend - linestart + 1;
                *linebuffer = ANDROID_LOG_INFO;
                strcpy(linebuffer + 1, trustyprefix);
                strncpy(linebuffer + 1 + sizeof(trustyprefix), linestart, linelen - 1);
                timespec tp;
                clock_gettime(CLOCK_REALTIME, &tp);
                log_time now = log_time(tp.tv_sec, tp.tv_nsec);
                logbuf->Log(LOG_ID_KERNEL, now, AID_ROOT, 0 /*pid*/, 0 /*tid*/, linebuffer,
                            sizeof(trustyprefix) + linelen + 1);
                linestart += linelen;
                len -= linelen;
            } else {
                if (len) {
                    // there's some unterminated data left at the end of the
                    // buffer. Move it to the front and try to append more in
                    // the outer loop.
                    memmove(buffer, linestart, len);
                }
                break;
            }
        }
    }
    return true;
}
