#include "common.h"

/*
 * Multiple Stores to Same Address
 *
 * Pattern:
 *   store S1
 *   store S2
 *   store S3
 *   store S4
 *   load
 *
 * The load must depend on the youngest store (S4).
 * Tests correct youngest-store resolution.
 */

volatile int A[1];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        A[0]=i;
        A[0]=i+1;
        A[0]=i+2;
        A[0]=i+3;

        int x=A[0];

        if(x!=i+3){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
