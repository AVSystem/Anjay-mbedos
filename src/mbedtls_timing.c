/*
 * Copyright 2020 AVSystem <avsystem@avsystem.com>
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
#include <avsystem/commons/avs_time.h>

#include "timing_alt.h"
/*
 * For forward declarations of:
 * - mbedtls_x509_crt_parse_path
 * - mbedtls_x509_crt_parse_file
 * - mbedtls_pk_parse_keyfile
 */
#define MBEDTLS_FS_IO
#include "mbedtls/entropy_poll.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"

/*
 * mbedtls_timing_set_delay, mbedtls_timing_get_delay and
 * mbedtls_timing_get_timer implementations are copy-pasted from
 * mbedtls/library/timing.c .
 */

/*
 * Set delays to watch
 */
void mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms) {
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;

    ctx->int_ms = int_ms;
    ctx->fin_ms = fin_ms;

    if (fin_ms != 0)
        (void) mbedtls_timing_get_timer(&ctx->timer, 1);
}

/*
 * Get number of delays expired
 */
int mbedtls_timing_get_delay(void *data) {
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;
    unsigned long elapsed_ms;

    if (ctx->fin_ms == 0)
        return (-1);

    elapsed_ms = mbedtls_timing_get_timer(&ctx->timer, 0);

    if (elapsed_ms >= ctx->fin_ms)
        return (2);

    if (elapsed_ms >= ctx->int_ms)
        return (1);

    return (0);
}

unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time *val,
                                       int reset) {
    AVS_STATIC_ASSERT(sizeof(struct mbedtls_timing_hr_time)
                              >= sizeof(avs_time_monotonic_t),
                      avs_time_monotonic_fits);
    avs_time_monotonic_t *start = (avs_time_monotonic_t *) val;
    avs_time_monotonic_t offset = avs_time_monotonic_now();

    if (reset) {
        *start = offset;
        return 0;
    }

    int64_t delta;
    avs_time_duration_to_scalar(&delta, AVS_TIME_MS,
                                avs_time_monotonic_diff(offset, *start));
    return (unsigned long) delta;
}

/** We do not implement any kind of filesystem; these mocks are required
 * by avs_commons SSL implementation, even though we do not use them */
int mbedtls_x509_crt_parse_path(mbedtls_x509_crt *chain, const char *path) {
    (void) chain;
    (void) path;
    puts("filesystem not supported");
    return -1;
}
int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path) {
    (void) chain;
    (void) path;
    puts("filesystem not supported");
    return -1;
}
int mbedtls_pk_parse_keyfile(mbedtls_pk_context *ctx,
                             const char *path,
                             const char *password) {
    (void) ctx;
    (void) path;
    (void) password;
    puts("filesystem not supported");
    return -1;
}
