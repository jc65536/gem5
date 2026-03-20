#include "common.h"

/*
 * Pointer-Chasing Alias Pattern
 *
 * Loads and stores operate through pointers in a linked structure.
 * Introduces aliasing through indirect memory accesses.
 */

typedef struct Node{
    int value;
    struct Node* next;
}Node;

Node *nodes;

int main(){

    nodes=malloc(sizeof(Node)*SIZE);

    for(int i=0;i<SIZE;i++){
        nodes[i].next=&nodes[(i+1)&(SIZE-1)];
        nodes[i].value=0;
    }

    Node* cur=&nodes[0];

    for(long long i=0;i<ITERS;i++){

        cur->value=i;

        int x=cur->value;

        if(x!=i){
            printf("FAIL\n");
            return 1;
        }

        cur=cur->next;
    }

    printf("PASS\n");
}
