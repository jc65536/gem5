#include "common.h"

/*
 * Long Store Distance Dependence
 *
 * Pattern:
 *   store A[i]
 *   store B
 *   store C
 *   store D
 *   load  A[i]
 *
 * The load depends on a store several instructions earlier.
 * Tests predictor ability to track distant producers.
 */

volatile int *A;

int main(){

    A=malloc(sizeof(int)*SIZE);

    for(int i=0;i<SIZE;i++)
        A[i]=0;

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        int idx=i&(SIZE-1);

        A[idx]=i;
        A[(idx+1)&(SIZE-1)]=i+1;
        A[(idx+2)&(SIZE-1)]=i+2;
        A[(idx+3)&(SIZE-1)]=i+3;

        int x=A[idx];

        if(x!=i){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
