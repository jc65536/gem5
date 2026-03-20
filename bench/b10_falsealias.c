#include "common.h"

/*
 * False Alias Pattern
 *
 * Store and load addresses usually differ but occasionally match.
 * Tests predictor ability to avoid unnecessary serialization.
 */

volatile int *A;

int main(){

    A=malloc(sizeof(int)*SIZE);

    for(int i=0;i<SIZE;i++)
        A[i]=0;

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        int s=(i*31)&(SIZE-1);
        int l=(i*17)&(SIZE-1);

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
