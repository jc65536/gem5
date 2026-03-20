#include "common.h"

/*
 * Random Control Flow Producers
 *
 * Producer store depends on pseudo-random branch decisions.
 * Tests predictor robustness under irregular control flow.
 */

volatile int A[1];

int main(){

    unsigned seed=1;

    long long sum=0;

    for(long long i=0;i<ITERS;i++){

        seed = seed*1103515245 + 12345;

        if(seed&1)
            A[0]=5;
        else
            A[0]=9;

        int x=A[0];

        int expected=(seed&1)?5:9;

        if(x!=expected){
            printf("FAIL\n");
            return 1;
        }

        sum+=x;
    }

    printf("PASS %lld\n",sum);
}
