#include "common.h"

/*
 * Sparse Dependence
 *
 * Pattern:
 *   occasional store
 *   frequent load
 *
 * The load rarely depends on a store. Most iterations have no
 * true dependence, which can expose overly conservative predictors.
 */

volatile int *A;

int main(){

    A=malloc(sizeof(int)*SIZE);

    for(int i=0;i<SIZE;i++)
        A[i]=0;

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        int idx=(i*17)&(SIZE-1);

        if((i&63)==0)
            A[idx]=i;

        int x=A[idx];

        if((i&63)==0 && x!=i){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
