/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <mutex>

static inline int FutexCall(volatile void* ftx, int op, int value, const timespec* timeout,
                            int bitset) {
    int saved_errno = errno;
    int result = syscall(__NR_futex, ftx, op, value, timeout, NULL, bitset);
    if (result == -1) {
        result = -errno;
        errno = saved_errno;
    }
    return result;
}

static inline int FutexWake(volatile void* ftx) {
    return FutexCall(ftx, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, 0);
}

static inline int FutexWait(volatile void* ftx, int value) {
    return FutexCall(ftx, FUTEX_WAIT_BITSET_PRIVATE, value, nullptr, FUTEX_BITSET_MATCH_ANY);
}

extern std::mutex logd_lock;
