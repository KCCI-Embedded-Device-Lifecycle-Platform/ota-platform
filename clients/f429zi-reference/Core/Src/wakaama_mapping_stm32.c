#include "liblwm2m.h"
#include "rng.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

void *lwm2m_malloc(size_t size)
{
    return malloc(size);
}

void lwm2m_free(void *pointer)
{
    free(pointer);
}

char *lwm2m_strdup(const char *source)
{
    size_t length;
    char *copy;

    if (source == NULL)
        return NULL;

    length = strlen(source) + 1U;
    copy = lwm2m_malloc(length);

    if (copy != NULL)
        memcpy(copy, source, length);

    return copy;
}

int lwm2m_strncmp(
    const char *left,
    const char *right,
    size_t length)
{
    return strncmp(left, right, length);
}

static unsigned char ascii_lower(unsigned char value)
{
    if (value >= 'A' && value <= 'Z')
        return (unsigned char)(value + ('a' - 'A'));

    return value;
}

int lwm2m_strcasecmp(
    const char *left,
    const char *right)
{
    while (*left != '\0' && *right != '\0')
    {
        unsigned char left_value =
            ascii_lower((unsigned char)*left);

        unsigned char right_value =
            ascii_lower((unsigned char)*right);

        if (left_value != right_value)
            return (int)left_value - (int)right_value;

        left++;
        right++;
    }

    return (int)ascii_lower((unsigned char)*left) -
           (int)ascii_lower((unsigned char)*right);
}

time_t lwm2m_gettime(void)
{
    static uint32_t previous_tick;
    static uint64_t elapsed_ms;

    uint32_t current_tick = HAL_GetTick();

    elapsed_ms +=
        (uint32_t)(current_tick - previous_tick);

    previous_tick = current_tick;

    return (time_t)(elapsed_ms / 1000U);
}

int _gettimeofday(
    struct timeval *time_value,
    void *timezone)
{
    uint32_t milliseconds;

    (void)timezone;

    if (time_value == NULL)
        return -1;

    milliseconds = HAL_GetTick();
    time_value->tv_sec =
        (time_t)(milliseconds / 1000U);
    time_value->tv_usec =
        (suseconds_t)((milliseconds % 1000U) * 1000U);

    return 0;
}

int lwm2m_seed(void)
{
    uint32_t random_value;

    if (HAL_RNG_GenerateRandomNumber(
            &hrng,
            &random_value) == HAL_OK)
    {
        return (int)random_value;
    }

    /* RNG 오류 시 bring-up용 fallback */
    return (int)(
        HAL_GetUIDw0() ^
        HAL_GetUIDw1() ^
        HAL_GetUIDw2() ^
        HAL_GetTick());
}

void lwm2m_printf(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
}
