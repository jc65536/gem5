#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * Common configuration shared by all microbenchmarks.
 *
 * SIZE:
 *   Working-set size. Large enough to create a realistic
 *   address stream and avoid trivial cache behavior.
 *
 * ITERS:
 *   Number of loop iterations. Each benchmark generates
 *   hundreds of millions of memory operations, making
 *   them suitable for cycle-accurate simulation.
 */

#ifndef SIZE
#define SIZE  (1<<10)      /* 2^10 ints = 4KiB */
#endif

#ifndef ITERS
#define ITERS 10000
#endif

#endif
