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
#define CPU_MHZ 3.5*1000
//#define n 149
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

short* random_array_short(){
    short * A_short = calloc(150, sizeof(short));
    for (int i = 0; i< 150 ; i++){
        A_short[i] = rand()%100;
    }
    return A_short;

}
long int* random_array_long(){
    long int  * A_long = calloc(ORDER, sizeof(long));
    for (int i = 0; i<ORDER ; i++){
        A_long[i] = rand()%100;
    }
    return A_long;

}

// to print out the reuslt of dot product array 
//void print_array(*)


/*****************************************************
*			Dot Product	      *
*****************************************************/
//try to change the long int sqr
// change the const short into short pointer 
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


// function to print out the array 
void print_array_long(long int* A){
     for (int i = 0; i < N - ORDER; i++){
      printf("%ld", A[i]);
     }
    printf("\n");
}

void print_array_short(short int* A){
     for (int i = 0; i < N - ORDER; i++){
      printf("%ld", A[i]);
     }
    printf("\n");
}



/*****************************************************
*		FIR Filter		     *
*****************************************************/
void fir (const short array1[], const short coeff[], long int output[])
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
    //return output;

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

/*******************************************************
*	Lattice Synthesis	           *
* This function doesn't follow the typical DSP multiply two vector operation, but it will point out the compiler's flexibility   ********************************************************/
long int
latsynth (short b[], const short k[], long int n)
{
  long int i;
  long int f;

  f -= b[n - 1] * k[n - 1]; 
  for (i = n - 2; i >= 0; i--)
    {
      f -= b[i] * k[i];
      b[i + 1] = b[i] + ((k[i] * (f >> 16)) >> 16);
    }
  b[0] = f >> 16;
  return f;
}

/*****************************************************
*			IIR Filter		     *
*****************************************************/
void
iir1 (const short *coefs, const short *input, long int *optr, long int *state)
{
  long int x;
  long int t;
  long int n_new;

  x = input[0];
  for (n_new = 0; n_new < 50; n_new++)
    {
      t = x + ((coefs[2] * state[0] + coefs[3] * state[1]) >> 15);
      x = t + ((coefs[0] * state[0] + coefs[1] * state[1]) >> 15);
      state[1] = state[0];
      state[0] = t;
      coefs += 4;		/* point to next filter coefs  */
      state += 2;		/* point to next filter states */
    }
  *optr++ = x;
}

/*****************************************************
*	Vocoder Codebook Search 	     *
*****************************************************/
long int
codebook (long int mask, long int bitchanged, long int numbasis,
	  long int codeword, long int g, const short *d, short ddim,
	  short theta)
/*
 * dfm (mask=d  bitchanged=1 numbasis=17  codeword=e[0] , g=d, d=a, ddim=c,
 * theta =1
 */
{
  long int j;


  /*
   * Remove along with the code below.
   *
   long int        tmpMask;

   tmpMask = mask << 1;
   */
  for (j = bitchanged + 1; j <= numbasis; j++)
    {



/*
 * The following code is removed since it gave a memory access exception.
 * It is OK since the return value does not control the flow.
 * The loop always iterates a fixed number of times independent of the loop body.

    if (theta == !(!(codeword & tmpMask)))
			g += *(d + bitchanged * ddim + j);
		else
			g -= *(d + bitchanged * ddim + j);
		tmpMask <<= 1;
*/
    }
  return g;
}

/*****************************************************
*		JPEG Discrete Cosine Transform 		     *
*****************************************************/
void
jpegdct (short *d, short *r)
{
  long int t[12];
  short i, j, k, m, n, p;
  for (k = 1, m = 0, n = 13, p = 8; k <= 8;
       k += 7, m += 3, n += 3, p -= 7, d -= 64)
    {
      for (i = 0; i < 8; i++, d += p)
	{
	  for (j = 0; j < 4; j++)
	    {
	      t[j] = d[k * j] + d[k * (7 - j)];
	      t[7 - j] = d[k * j] - d[k * (7 - j)];
	    }
	  t[8] = t[0] + t[3];
	  t[9] = t[0] - t[3];
	  t[10] = t[1] + t[2];
	  t[11] = t[1] - t[2];
	  d[0] = (t[8] + t[10]) >> m;
	  d[4 * k] = (t[8] - t[10]) >> m;
	  t[8] = (short) (t[11] + t[9]) * r[10];
	  d[2 * k] = t[8] + (short) ((t[9] * r[9]) >> n);
	  d[6 * k] = t[8] + (short) ((t[11] * r[11]) >> n);
	  t[0] = (short) (t[4] + t[7]) * r[2];
	  t[1] = (short) (t[5] + t[6]) * r[0];
	  t[2] = t[4] + t[6];
	  t[3] = t[5] + t[7];
	  t[8] = (short) (t[2] + t[3]) * r[8];
	  t[2] = (short) t[2] * r[1] + t[8];
	  t[3] = (short) t[3] * r[3] + t[8];
	  d[7 * k] = (short) (t[4] * r[4] + t[0] + t[2]) >> n;
	  d[5 * k] = (short) (t[5] * r[6] + t[1] + t[3]) >> n;
	  d[3 * k] = (short) (t[6] * r[5] + t[1] + t[2]) >> n;
	  d[1 * k] = (short) (t[7] * r[7] + t[0] + t[3]) >> n;
	}
    }
}

static short a[200];
static short b[200];
static short c;
static long int d;
static int e;
static long int output[200];


void
initialise_benchmark (void)
{
}


static int benchmark_body (int  rpt);

void
warm_caches (int  heat)
{
  int  res = benchmark_body (heat);

  return;
}


int
benchmark (void)
{
  return benchmark_body (LOCAL_SCALE_FACTOR * CPU_MHZ);
}


static int __attribute__ ((noinline))
benchmark_body (int rpt)
{
  int j;

  for (j = 0; j < rpt; j++)
    {
      short unsigned int in_a[200] = {
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400
      };
      short unsigned int in_b[200] = {
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000
      };
      c = 0x3;
      d = 0xAAAA;
      e = 0xEEEE;

      for (int i = 0; i < 200; i++)
	{
	  a[i] = in_a[i];
	  b[i] = in_b[i];
	}
      /*
       * Declared as memory variable so it doesn't get optimized out
       */

      vec_mpy1 (a, b, c);
      c = mac (a, b, (long int) c, (long int *) output);
      fir (a, b, output);
      fir_no_red_ld (a, b, output);
      d = latsynth (a, b, N);
      iir1 (a, b, &output[100], output);
      e = codebook (d, 1, 17, e, d, a, c, 1);
      jpegdct (a, b);
    }
  return 0;
}

int
verify_benchmark (int unused)
{
  long int exp_output[200] =
    { 3760, 4269, 3126, 1030, 2453, -4601, 1981, -1056, 2621, 4269,
    3058, 1030, 2378, -4601, 1902, -1056, 2548, 4269, 2988, 1030,
    2300, -4601, 1822, -1056, 2474, 4269, 2917, 1030, 2220, -4601,
    1738, -1056, 2398, 4269, 2844, 1030, 2140, -4601, 1655, -1056,
    2321, 4269, 2770, 1030, 2058, -4601, 1569, -1056, 2242, 4269,
    2152, 1030, 1683, -4601, 1627, -1056, 2030, 4269, 2080, 1030,
    1611, -4601, 1555, -1056, 1958, 4269, 2008, 1030, 1539, -4601,
    1483, -1056, 1886, 4269, 1935, 1030, 1466, -4601, 1410, -1056,
    1813, 4269, 1862, 1030, 1393, -4601, 1337, -1056, 1740, 4269,
    1789, 1030, 1320, -4601, 1264, -1056, 1667, 4269, 1716, 1030,
    1968, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  return (0 == memcmp (output, exp_output, 200 * sizeof (output[0])))
    && (10243 == c) && (-441886230 == d) && (-441886230 == e);
}


int main(){
    
   // init_runtime();

    // assign the values for a and b 
    //generate random array 
    
    //----------------------to test the mac--------------//
    //short* a = random_array();
    //short* b = random_array();
    //long int sqr, *sum;
    //sum = malloc(sizeof(long int));
    //*sum = 0;
    //sqr = mac(a, b, sqr, sum);
    // check the doit multiplcaition is correct by making a benchmark 
    // the initial runtime test implementation?
    //printf("sqr equals to %ld", sqr);
    //free(a);
    //free(b);

    // //-------------------------to test the fir------------//
    // short* array1 = random_array_short();
    // short* coeff = random_array_short();
    
    // long* output_fir = random_array_long();
    // fir(array1, coeff,output_fir);
    // print_array_short(array1);
    // print_array_short(coeff);
    // print_array_long(output_fir); 
    // printf("The test is shown as above\n");
     
     //-------------------------to test the fir_no_red_ld------------//
    // short* array1 = random_array_short();
    // short* coeff = random_array_short();
    
    // long* output_fir = random_array_long();
    // fir_no_red_ld(array1, coeff,output_fir);
    // print_array_short(array1);
    // print_array_short(coeff);
    // print_array_long(output_fir); 
    // printf("The test is shown as above\n");
     
    //-------------------to test the latsynth-------------------//
    //  short* b = random_array_short();
    //  short* k = random_array_short();
    //  long int f;

    //  f = latsynth(b,k,n,f);
    //  printf("%ld",f);

   //------------------to test the iir1----------------------//
      // short* coefs = random_array_short();
      // short* input = random_array_short();
      // long*  optr = random_array_long();
      // long*  state = random_array_long();
      
      // iir1(coefs, input,optr, state);
      
      // printf("this is coefs\n");
      // print_array_short(coefs);
      // printf("this is input\n");
      // print_array_short(input);
      // printf("this is optr\n");
      // print_array_long(optr); 
      // printf("The test is shown as above\n");

    //---------------to test jpegdct----------------------------//
      short* d = random_array_short();  // how do i set the value of d
      short* r = random_array_short();
      int result,unused;

     
     __attribute__((noinline));
     result = verify_benchmark(unused);
    
     printf("the result is %d",result);
     

    return 0;

}
