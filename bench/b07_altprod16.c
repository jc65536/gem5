#include "common.h"

/*
 * Producer Switching Pattern
 *
 * The producer store changes every fixed number of iterations.
 * Requires tracking longer dynamic context history.
 */

volatile int A[1];

int main(){

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        if((i>>4)&1)
            A[0]=100;
        else
            A[0]=200;

        int x=A[0];

        int expected=((i>>4)&1)?100:200;

        if(x!=expected){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
