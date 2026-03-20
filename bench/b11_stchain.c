#include "common.h"

/*
 * Store Chain
 *
 * Value propagates through a chain of stores before being loaded.
 * Tests store-to-store propagation patterns.
 */

volatile int A[3];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        A[0]=i;
        A[1]=A[0];
        A[2]=A[1];

        int x=A[2];

        if(x!=i){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
