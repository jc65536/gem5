#include "common.h"

/*
 * Immediate Store → Load Dependence
 *
 * Pattern:
 *   store A[i]
 *   load  A[i]
 *
 * The load always depends on the immediately preceding store.
 * This is a baseline case where the correct producer is trivial.
 */

volatile int *A;

int main() {

    A = malloc(sizeof(int)*SIZE);

    for (int i=0;i<SIZE;i++)
        A[i]=0;

    long long sum=0;

    for (long long i=0;i<ITERS;i++) {

        int idx = i&(SIZE-1);

        A[idx]=i;
        int x=A[idx];

        if(x!=i){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
