/* BEEBS cubic benchmark

   This version, copyright (C) 2013-2019 Embecosm Limited and University of
   Bristol

   Contributor: James Pallister <james.pallister@bristol.ac.uk>
   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of Embench and was formerly part of the Bristol/Embecosm
   Embedded Benchmark Suite.

   SPDX-License-Identifier: GPL-3.0-or-later

   The original code is from http://www.snippets.org/. */

/* +++Date last modified: 05-Jul-1997 */

/*
 **  CUBIC.C - Solve a cubic polynomial
 **  public domain by Ross Cottrell
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "snipmath.h"

void
SolveCubic (double a, double b, double c, double d, int *solutions, double *x)
{
  long double a1 = (long double) (b / a);
  long double a2 = (long double) (c / a);
  long double a3 = (long double) (d / a);
  long double Q = (a1 * a1 - 3.0L * a2) / 9.0L;
  long double R = (2.0L * a1 * a1 * a1 - 9.0L * a1 * a2 + 27.0L * a3) / 54.0L;
  double R2_Q3 = (double) (R * R - Q * Q * Q);

  double theta;
  
  printf("Q is %ld\n",Q);
  printf("R is %ld\n",R );
  printf("R2_Q3 is %ld\n",R2_Q3);


  if (R2_Q3 <= 0)
    {
      *solutions = 3;
      theta = acos (((double) R) / sqrt ((double) (Q * Q * Q)));
      x[0] = -2.0 * sqrt ((double) Q) * cos (theta / 3.0) - a1 / 3.0;
      x[1] =
	-2.0 * sqrt ((double) Q) * cos ((theta + 2.0 * PI) / 3.0) - a1 / 3.0;
      x[2] =
	-2.0 * sqrt ((double) Q) * cos ((theta + 4.0 * PI) / 3.0) - a1 / 3.0;
    }
  else
    {
      *solutions = 1;
      x[0] = pow (sqrt (R2_Q3) + fabs ((double) R), 1 / 3.0);
      x[0] += ((double) Q) / x[0];
      x[0] *= (R < 0.0L) ? 1 : -1;
      x[0] -= (double) (a1 / 3.0L);
    }
}

int main(){
  double a =  rand()%5;
  double b =  rand()%5;
  double c =  rand()%5;
  double d =  rand()%5;
  
  int *solutions;
  solutions = malloc(sizeof(int));
  
  // int array_size;
  // array_size = 3;

  double *x;
  x = malloc(sizeof(double));
  


  SolveCubic (a, b, c, d, solutions, x);

  printf("a is %lf\n",a);
  printf("b is %lf\n",b);
  printf("c is %lf\n",c);
  printf("d is %lf\n",d);


  printf("solutions is %lf\n",*solutions);
  printf("x is %lf\n",*x);
  
  //, b is %lf, c is %lf, d is %lf,solution is %ld, x is %lf", a, b, c, d, *solutions, *x );
  return 0;

}


/* vim: set ts=3 sw=3 et: */


/*
   Local Variables:
   mode: C
   c-file-style: "gnu"
   End:
*/
