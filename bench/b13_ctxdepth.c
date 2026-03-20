#include "common.h"

/*
 * Context Depth Explosion
 *
 * Multiple nested branches select different producer stores.
 * Requires deeper control-flow context to predict correctly.
 */

volatile int A[1];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        if(i&1){
            if(i&2) A[0]=1;
            else A[0]=2;
        } else{
            if(i&4) A[0]=3;
            else A[0]=4;
        }

        int x=A[0];

        int expected;
        if(i&1)
            expected=(i&2)?1:2;
        else
            expected=(i&4)?3:4;

        if(x!=expected){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
