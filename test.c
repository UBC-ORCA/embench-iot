#include <stdio.h>
#include <riscv_vector.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// #include <base.h>

//for the simulation 
#include <string.h>
//#include "support.h"

/* This scale factor will be changed to equalise the runtime of the
   benchmarks. */
#define LOCAL_SCALE_FACTOR 87

#define n 149
#define N 100
#define ORDER 50

void vec_mpy1 (short y[], const short x[], short scaler);
long int mac ( short *a,  short *b, long int sqr, long int *sum) ;
//void fir (const short array1[], const short coeff[], long int output[]);
//void fir_no_red_ld (const short x[], const short h[], long int y[]);
//long int latsynth (short b[], const short k[], long int n, long int f);
//void iir1 (const short *coefs, const short *input, long int *optr,
//	   long int *state);
//long int codebook (long int mask, long int bitchanged, long int numbasis,
//		   long int codeword, long int g, const short *d, short ddim,
//		   short theta);
//void jpegdct (short *d, short *r);

void vec_mpy1 (short y[], const short x[], short scaler)
{
  long int i;

  for (i = 0; i < 150; i++)
    y[i] += ((scaler * x[i]) >> 15);
}


// help generating a random array 

short* random_array(){
    short * A = calloc(n+1, sizeof(short));
    for (int i = 0; i<150 ; i++){
        A[i] = rand()%5;
    }
    return A;

}

// to print out the reuslt of dot product array 
//void print_array(*)


/*****************************************************
*			Dot Product	      *
*****************************************************/
//try to change the long int sqr
long int
mac (short *a,short *b, long int sqr, long int *sum)
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

int main(){
    
   // init_runtime();

    // assign the values for a and b 
    //generate random array 
    
    short* a = random_array();
    short* b = random_array();

    long int sqr, *sum;
    sum = malloc(sizeof(long int));
    sum = 0;

    sqr = mac(a, b, sqr, sum);
    // check the doit multiplcaition is correct by making a benchmark 
    // the initial runtime test implementation?
    printf("sqr equals to %ld", sqr);

    free(a);
    free(b);
    
    return 0;

}
