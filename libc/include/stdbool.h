#ifndef STDBOOL_H
#define STDBOOL_H

/* Define the bool type as _Bool if it's not already defined */
#ifndef __cplusplus
typedef _Bool bool;
#define true 1
#define false 0
#endif /* __cplusplus */

#endif

