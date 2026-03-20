#include "common.h"

/*
 * Path-Correlated Aliasing
 *
 * Store and load sometimes reference the same address depending
 * on control-flow path.
 *
 * Useful for evaluating predictors that incorporate path context.
 */

volatile int *A;

int main(){

    A=malloc(sizeof(int)*SIZE);

    for(int i=0;i<SIZE;i++)
        A[i]=0;

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        int s=(i&1)?(i&(SIZE-1)):((i*3)&(SIZE-1));
        int l=(i&1)?(i&(SIZE-1)):((i*7)&(SIZE-1));

        A[s]=i;

        int x=A[l];

        if(s==l && x!=i){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
