#include "common.h"

/*
 * Multiple Loads After Store
 *
 * One store produces data consumed by multiple loads.
 * Tests handling of fan-out dependences.
 */

volatile int A[1];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        A[0]=i;

        int x1=A[0];
        int x2=A[0];
        int x3=A[0];

        if(x1!=i || x2!=i || x3!=i){
            printf("FAIL\n");
            return 1;
        }

        sum+=x1+x2+x3;
    }

    printf("PASS %lld\n",sum);
}
