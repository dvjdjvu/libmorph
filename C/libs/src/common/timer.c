/* Функции замера времени. Для сравнительных тестов во время оптимизации */

#include <time.h>
#include <stdlib.h>

void *start_timer() {
    struct timespec *ts_start = malloc(sizeof(*ts_start));
    clock_gettime(CLOCK_REALTIME, ts_start);
    return ts_start;
}

long int stop_timer(void *prev_time) {
    struct timespec ts_stop;
    struct timespec *ts_start = prev_time;
    long int result;
    clock_gettime(CLOCK_REALTIME, &ts_stop);
    result = 1000*(ts_stop.tv_sec - ts_start->tv_sec) + (ts_stop.tv_nsec - ts_start->tv_nsec)/1000000;
    free(prev_time);
    return result;
}
