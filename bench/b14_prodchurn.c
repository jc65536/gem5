#include "common.h"

/*
 * Producer-Set Churn
 *
 * The producing store changes periodically.
 * Tests adaptation to shifting dependence patterns.
 */

volatile int A[1];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        if((i%64)<32)
            A[0]=10;
        else
            A[0]=20;

        int x=A[0];

        int expected=((i%64)<32)?10:20;

        if(x!=expected){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
