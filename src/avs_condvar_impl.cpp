/*
 * Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
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
 */

#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_memory.h>

#include "avs_mbed_hacks.h"
#include "avs_mbed_threading_structs.h"

using namespace rtos;

// Code partially inspired by:
// https://github.com/yaahallo/nachos/blob/master/threads/Condition.java
// The Nachos project is the code examples for the Operating Systems course on
// the University of California, see:
// https://eng.ucmerced.edu/crf/engineering/cse-150-operating-systems/
// Copyright (c) 1992-2001 The Regents of the University of California.
// All rights reserved. Used under BSD license
// (https://github.com/yaahallo/nachos/blob/master/README)

int avs_condvar_create(avs_condvar_t **out_condvar) {
    AVS_ASSERT(!*out_condvar,
               "possible attempt to reinitialize a condition variable");

    *out_condvar = new (std::nothrow) avs_condvar;
    return *out_condvar ? 0 : -1;
}

int avs_condvar_notify_all(avs_condvar_t *condvar) {
    ScopedLock<Mutex> lock(condvar->waiters_mtx);
    avs_condvar::Waiter *waiter = condvar->first_waiter;
    while (waiter) {
        waiter->sem.release();
        waiter = waiter->next;
    }
    return 0;
}

int avs_condvar_wait(avs_condvar_t *condvar,
                     avs_mutex_t *mutex,
                     avs_time_monotonic_t deadline) {
#if MBED_MAJOR_VERSION > 5 \
        || (MBED_MAJOR_VERSION == 5 && MBED_MINOR_VERSION >= 7)
    // Precondition: mutex is locked by the current thread
    AVS_ASSERT(mutex->mbed_mtx.get_owner() == ThisThread::get_id(),
               "attempted to use a condition variable a mutex not locked by "
               "the current thread");
#endif // MBED_MAJOR_VERSION > 5 || (MBED_MAJOR_VERSION == 5 &&
       // MBED_MINOR_VERSION >= 7)

    int64_t wait_ms;
    if (avs_time_duration_to_scalar(
                &wait_ms, AVS_TIME_MS,
                avs_time_monotonic_diff(deadline, avs_time_monotonic_now()))
        || wait_ms > (int64_t) UINT32_MAX) {
        wait_ms = osWaitForever;
    } else if (wait_ms < 0) {
        wait_ms = 0;
    }

    avs_condvar::Waiter waiter;
    {
        ScopedLock<Mutex> lock(condvar->waiters_mtx);
        // Insert waiter as the first element on the list
        waiter.next = condvar->first_waiter;
        condvar->first_waiter = &waiter;
    }
    mutex->mbed_mtx.unlock();
#if MBED_MAJOR_VERSION >= 6
    bool timed_out =
            !waiter.sem.try_acquire_for(std::chrono::milliseconds(wait_ms));
#elif MBED_MAJOR_VERSION > 5 \
        || (MBED_MAJOR_VERSION == 5 && MBED_MINOR_VERSION >= 13)
    bool timed_out = !waiter.sem.try_acquire_for(wait_ms);
#else  // MBED_MAJOR_VERSION > 5 || (MBED_MAJOR_VERSION == 5 &&
       // MBED_MINOR_VERSION >= 13)
    bool timed_out = (waiter.sem.wait(wait_ms) <= 0);
#endif // MBED_MAJOR_VERSION > 5 || (MBED_MAJOR_VERSION == 5 &&
       // MBED_MINOR_VERSION >= 13)
    mutex->mbed_mtx.lock();
    {
        ScopedLock<Mutex> lock(condvar->waiters_mtx);
        avs_condvar::Waiter **waiter_node_ptr = &condvar->first_waiter;
        while (*waiter_node_ptr && *waiter_node_ptr != &waiter) {
            waiter_node_ptr = &(*waiter_node_ptr)->next;
        }
        AVS_ASSERT(
                *waiter_node_ptr == &waiter,
                "waiter node inexplicably disappeared from condition variable");
        if (*waiter_node_ptr == &waiter) {
            // detach it
            *waiter_node_ptr = (*waiter_node_ptr)->next;
        }
    }
    return timed_out ? AVS_CONDVAR_TIMEOUT : 0;
}

void avs_condvar_cleanup(avs_condvar_t **condvar) {
    if (!*condvar) {
        return;
    }

    delete *condvar;
    *condvar = nullptr;
}
