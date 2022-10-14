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
void fir (const short array1[], const short coeff[], long int output[]);
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
// change the const short into short pointer 
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

/*****************************************************
*		FIR Filter		     *
*****************************************************/
void
fir (const short array1[], const short coeff[], long int output[])
{
  long int i, j, sum;

  for (i = 0; i < N - ORDER; i++)
    {
      sum = 0;
      for (j = 0; j < ORDER; j++)
	      {
	         sum += array1[i + j] * coeff[j];
	      }
      output[i] = sum >> 15; // the output would have divided by 15*4
      
    }
}
// FIR 
// the filtered amplitude would be coeff[j]
// the total element in the array would be #ORDER defined as 50 
// output array would have 50 element, where the sum is generated everytime


/****************************************************
*	FIR Filter with Redundant Load Elimination

By doing two outer loops simultaneously, you can potentially  reuse data (depending on the DSP architecture).
x and h  only  need to be loaded once, therefore reducing redundant loads.
This reduces memory bandwidth and power.
*****************************************************/
void
fir_no_red_ld (const short x[], const short h[], long int y[])
{
  long int i, j;
  long int sum0, sum1;
  short x0, x1, h0, h1;
  for (j = 0; j < 100; j += 2)
    {
      sum0 = 0;
      sum1 = 0;
      x0 = x[j];
      for (i = 0; i < 32; i += 2)
	{
	  x1 = x[j + i + 1];
	  h0 = h[i];
	  sum0 += x0 * h0;
	  sum1 += x1 * h0;
	  x0 = x[j + i + 2];
	  h1 = h[i + 1];
	  sum0 += x1 * h1;
	  sum1 += x0 * h1;
	}
      y[j] = sum0 >> 15;
      y[j + 1] = sum1 >> 15;
    }
}

int main(){
    
   // init_runtime();

    // assign the values for a and b 
    //generate random array 
    
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
