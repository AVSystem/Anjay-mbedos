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

#ifndef AVS_MBED_THREADING_STRUCT_H
#define AVS_MBED_THREADING_STRUCT_H

extern "C" {
#include <avsystem/commons/avs_condvar.h>
#include <avsystem/commons/avs_mutex.h>
} // extern "C"

#include "mbed.h"

struct avs_mutex {
    rtos::Mutex mbed_mtx;
};

struct avs_condvar {
    // NOTE: We cannot really use rtos::ConditionVariable, even on Mbed OS >=5.7
    // because it requires providing the related mutex at creation time,
    // which is incompatible with avs_compat_threading API

    struct Waiter {
        rtos::Semaphore sem;
        Waiter *next;

        Waiter() : sem(0), next(NULL) {}
    };

    rtos::Mutex waiters_mtx;
    Waiter *first_waiter;

    avs_condvar() : waiters_mtx(), first_waiter(NULL) {}
};

#endif /* AVS_MBED_THREADING_STRUCT_H */
