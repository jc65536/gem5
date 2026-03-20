#include "common.h"

/*
 * Alternating Producer Stores
 *
 * Pattern:
 *   store S1 or S2 depending on branch
 *   load
 *
 * The producer alternates between two stores based on control flow.
 * Tests path-sensitive dependence prediction.
 */

volatile int A[1];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        if(i&1)
            A[0]=10;
        else
            A[0]=20;

        int x=A[0];

        int expected=(i&1)?10:20;

        if(x!=expected){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
