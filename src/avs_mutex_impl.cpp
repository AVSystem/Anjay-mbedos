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

#include "avs_mbed_threading_structs.h"

int avs_mutex_create(avs_mutex_t **out_mutex) {
    AVS_ASSERT(!*out_mutex, "possible attempt to reinitialize a mutex");

    *out_mutex = new (std::nothrow) avs_mutex;
    return *out_mutex ? 0 : -1;
}

int avs_mutex_lock(avs_mutex_t *mutex) {
#if PREREQ_MBED_OS(6, 0, 0)
    mutex->mbed_mtx.lock();
    return 0;
#else  // PREREQ_MBED_OS(6, 0, 0)
    return mutex->mbed_mtx.lock() == osOK ? 0 : -1;
#endif // PREREQ_MBED_OS(6, 0, 0)
}

int avs_mutex_try_lock(avs_mutex_t *mutex) {
    return mutex->mbed_mtx.trylock() ? 0 : -1;
}

int avs_mutex_unlock(avs_mutex_t *mutex) {
#if PREREQ_MBED_OS(6, 0, 0)
    mutex->mbed_mtx.unlock();
    return 0;
#else  // PREREQ_MBED_OS(6, 0, 0)
    return mutex->mbed_mtx.unlock() == osOK ? 0 : -1;
#endif // PREREQ_MBED_OS(6, 0, 0)
}

void avs_mutex_cleanup(avs_mutex_t **mutex) {
    if (!*mutex) {
        return;
    }

    delete *mutex;
    *mutex = nullptr;
}
