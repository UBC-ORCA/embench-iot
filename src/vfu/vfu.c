#include "support.h"

/* This scale factor will be changed to equalise the runtime of the
   benchmarks. */
#define LOCAL_SCALE_FACTOR 1
#define NUM_ELEMS 16

#include <stdlib.h>
#include <string.h>
#include <riscv_vector.h>

#ifdef __TURBOC__
#pragma warn -cln
#endif

long int squares[NUM_ELEMS];

void
initialise_benchmark (void)
{
    int mcfu_selector = 1 << 31;
    asm volatile (
            "        csrw 0xBC0, %0;\n"     
            : 
            :  "r" (mcfu_selector)
            : 
            );
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
    size_t vl = vsetvl_e32m4(NUM_ELEMS);

    vint32m4_t vv = vid_v_i32m4(vl);
    vv = vmul_vv_i32m4(vv, vv, vl);
    vse32_v_i32m4(&squares[0], vv, vl);

    return 0;
}


int
verify_benchmark (int r)
{
    long int results[NUM_ELEMS];
    for (int i = 0; i < NUM_ELEMS; ++i) 
        results[i] = i * i; 

    return 0 == memcmp(&squares[0], &results[0], NUM_ELEMS * sizeof(long int));
}


/* vim: set ts=3 sw=3 et: */


/*
   Local Variables:
   mode: C
   c-file-style: "gnu"
   End:
*/
