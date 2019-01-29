#ifndef __COMMON_ERRORS_H_
#define __COMMON_ERRORS_H_

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#define errno_assert(x) \
    do {\
        if (!(x)) {\
            perror (NULL);\
            /*fprintf (stderr, "%s (%s:%d)\n", #x, __FILE__, __LINE__); \*/ \
            syslog(LOG_ERR, "%s (%s:%d)\n", #x, __FILE__, __LINE__); \
            abort ();\
        }\
    } while (0)

#endif /*__COMMON_ERRORS_H_ */
