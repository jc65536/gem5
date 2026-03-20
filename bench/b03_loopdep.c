#include "common.h"

/*
 * Loop-Carried Dependence
 *
 * Pattern:
 *   store(i-1)
 *   load(i)
 *
 * The load reads the value written in the previous iteration.
 * Classic loop-carried memory dependence.
 */

volatile int A[1];

int main(){

    A[0]=0;

    long long sum=0;

    for(long long i=1;i<ITERS;i++){

        int x=A[0];

        if(x!=i-1){
            printf("FAIL\n");
            return 1;
        }

        A[0]=i;

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
