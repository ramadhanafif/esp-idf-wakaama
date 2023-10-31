#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portable.h"

void output_buffer(const uint8_t *buffer, size_t length, int indent)
{
    size_t i;

    if (length == 0)
        printf("\n");

    if (buffer == NULL)
        return;

    i = 0;
    while (i < length) {
        uint8_t array[16];
        int j;

        for (int x = 0; x < indent; x++)
            printf("    ");

        memcpy(array, buffer + i, 16);
        for (j = 0; j < 16 && i + j < length; j++) {
            printf("%02X ", array[j]);
            if (j % 4 == 3)
                printf(" ");
        }
        if (length > 16) {
            while (j < 16) {
                printf("   ");
                if (j % 4 == 3)
                    printf(" ");
                j++;
            }
        }
        printf(" ");
        for (j = 0; j < 16 && i + j < length; j++) {
            if (isprint(array[j]))
                printf("%c", array[j]);
            else
                printf(".");
        }
        printf("\n");
        i += 16;
    }
}

void *lwm2m_malloc(size_t s)
{
    return pvPortMalloc(s);
}

void lwm2m_free(void *p)
{
    vPortFree(p);
}

char *lwm2m_strdup(const char *str)
{
    return strdup(str);
}

// Compare at most the n first bytes of s1 and s2, return 0 if they match
int lwm2m_strncmp(const char *s1, const char *s2, size_t n)
{
    return strncmp(s1, s2, n);
}

int lwm2m_strcasecmp(const char *str1, const char *str2)
{
    return strcasecmp(str1, str2);
}

time_t lwm2m_gettime(void)
{
    return (time_t)esp_timer_get_time() / 1000000ULL;
}
// Get a seed (which must not repeat when the device reboots) for generating a random number
int lwm2m_seed(void)
{
    return (int)esp_random();
}

void lwm2m_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
