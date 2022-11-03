//this is created to test the gprof, if not working, this is for testing the new timer library
#include <stdio.h>
#include <riscv_vector.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define LOCAL_SCALE_FACTOR 87
#define CPU_MHZ 3.5*1000

short* random_array(){
    short * A_short = calloc(150, sizeof(short));
    for (int i = 0; i< 150 ; i++){
        A_short[i] = rand()%100;
    }
    return A_short;
}

long int
mac (short *a,short *b,  long int sqr,long int *sum)
{
  long int i;
  long int dotp = *sum;

  for (i = 0; i < 150; i++)
    {
      dotp += b[i] * a[i];
      sqr += b[i] * b[i];
    }

  *sum = dotp;
  return sqr;
}

int  main (){
    short* a = random_array();
    short* b = random_array();
    long int sqr, *sum;
    sum = malloc(sizeof(long int));
    *sum = 0;
    sqr = mac(a, b, sqr, sum);
    // check the doit multiplcaition is correct by making a benchmark 
    // the initial runtime test implementation?
    printf("sqr equals to %ld", sqr);
    free(a);
    free(b);

    return 0;

}
