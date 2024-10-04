#include <string.h>
#include <stdint.h>
#include <stdlib.h>
//#include <stdio.h>
#include "support.h"
#include "../../../cx_runtime2.0/include/ci.h"
#include <riscv_vector.h>
#define USE_VECTOR 1

// EXTRACT n bits starting at position i from x
#define GET_BITS(x, n, i) (((x) >> (i)) & ((1 << (n)) - 1))

// UNIQUE SoC PARAMETERS
#define LOG2_NUM_CXUS 0
#define NUM_CXUS (1<<LOG2_NUM_CXUs)
#define LOG2_NUM_STATES 3
#define NUM_STATES (1<<LOG2_NUM_STATES)

// CX FENCE
#define CX_FENCE_SCALAR_READ(base_address, end_address)                                               \
  do {                                                                                                \
    asm volatile ("cx_reg 1008,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
  } while(0)

#define CX_FENCE_SCALAR_WRITE(base_address, end_address)                                              \
  do {                                                                                                \
    asm volatile ("cx_reg 1009,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
  } while(0)


#define LOCAL_SCALE_FACTOR 1
#define VLEN 1024*4
#define IMG_H VLEN/32
#define IMG_W VLEN/32

#define CX_GUID_VECTOR 1

static volatile unsigned int in  [IMG_H*IMG_W];
static volatile unsigned char out [IMG_H*IMG_W];

unsigned long xor();
unsigned long xor() { 
    static unsigned long y=2463534242; 
    y ^= (y<<13); 
    y ^= (y>>17); 
    y ^= (y<<5); 
    return y;
}

void rgb2luma(unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height, cx_sel_t *vec_sel);
void rgb2luma(unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height, cx_sel_t *vec_sel)
{
    size_t vl;
    
    for (int i = 0; i < image_height; i+=1){

        cx_sel(vec_sel[GET_BITS(i, LOG2_NUM_STATES, 0)]);
        
        asm volatile ("vsetvli x0, %[REQ_VL], e16, m2, ta, mu" :: [REQ_VL]  "r" (image_width));
        asm volatile ("vle32.v v4, (%0)":: "r"(rgb)); 

        asm volatile ("vnsrl.wi  v8,  v4, 16");
        asm volatile ("vnsrl.wi v10,  v4, 8");
        asm volatile ("vnsrl.wi v12,  v4, 0");

        asm volatile ("vand.vx   v8,  v8, %0":: "r"(255U));
        asm volatile ("vand.vx  v10, v10, %0":: "r"(255U));
        asm volatile ("vand.vx  v12, v12, %0":: "r"(255U));

        asm volatile ("vmul.vx   v8,  v8, %0":: "r"(66U));
        asm volatile ("vmul.vx  v10, v10, %0":: "r"(129U));
        asm volatile ("vmul.vx  v12, v12, %0":: "r"(25U));

        asm volatile ("vadd.vv  v14,  v8, v10");
        asm volatile ("vadd.vx  v16, v12, %0":: "r"(128U));
        asm volatile ("vadd.vv  v14, v14, v16");

        asm volatile ("vsetvli x0, x0, e8, m1, ta, mu");
        asm volatile ("vnsrl.wi v14, v14, 8");

        asm volatile ("vse8.v   v14, (%0)" : "+r" (luma));

        luma += image_width;
        rgb  += image_width;
    }

    cx_sel(CX_LEGACY);
}

static int benchmark_body (int  rpt);

void warm_caches (int  heat)
{
    int  res = benchmark_body (heat);

    return;
}

int benchmark (void)
{
    return benchmark_body (LOCAL_SCALE_FACTOR * CPU_MHZ);
}


static int __attribute__ ((noinline)) benchmark_body (int rpt)
{
    // Enable CX
    asm volatile ("fence rw, io");
    int i;
    cx_select_t vec_sel[NUM_STATES];
    for (i = 0; i < NUM_STATES; i++) {
        vec_sel[i] = cx_open(CX_GUID_VECTOR, CX_NO_VIRT, -1);
    }
    for (i = 0; i < rpt; i++)
    {
       rgb2luma(out, in, IMG_W, IMG_H, vec_sel);
    }
    cx_sel(vec_sel[0]);
    CX_FENCE_SCALAR_READ(&out[0], ((char*)(&out[IMG_H*IMG_W]))-1);

    for (int i = 0; i < NUM_STATES; i++) {
        cx_close(vec_sel[i]);
    }
    return 0;
}

void initialise_benchmark ()
{
    // Load data for each frame...
    for (int i = 0; i < IMG_H*IMG_W; ++i) {
        in[i] = xor();
    }
}



int verify_benchmark (int unused)
{
    unsigned char out_exp [IMG_H*IMG_W] = {0};

    for (int i = 0; i < IMG_W*IMG_H; i++) {
        unsigned int red   = (in[i] & 0xFF0000) >> 16U;
        unsigned int green = (in[i] & 0xFF00) >> 8U;
        unsigned int blue  = (in[i] & 0xFF);
        out_exp[i] = (25*blue + 129*green + 66*red + 128) >> 8U;
    }

    return 0 == memcmp (out, out_exp, IMG_H * IMG_W * sizeof (out[0]));
}
