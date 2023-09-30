#include <string.h>
#include <stdint.h>
#include <stdlib.h>
//#include <stdio.h>
#include "support.h"
#define USE_VECTOR 0

#if USE_VECTOR==1
#include <riscv_vector.h>
#endif

#define CSR_MCFU_SELECTOR 0xBC0
#define MCFU_SELECTOR_CFU_ID    0
#define MCFU_SELECTOR_STATE_ID  16
#define MCFU_SELECTOR_ENABLE    31

#define LOCAL_SCALE_FACTOR 1
#define VLEN 512*4
#define IMG_H VLEN/32
#define IMG_W VLEN/32

unsigned int in  [IMG_H*IMG_W];
unsigned int out [IMG_H*IMG_W];

unsigned long xor();
unsigned long xor() { 
    static unsigned long y=2463534242; 
    y ^= (y<<13); 
    y ^= (y>>17); 
    y ^= (y<<5); 
    return y;
}

#if USE_VECTOR==0
void rgb2luma(unsigned int *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void rgb2luma(unsigned int *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    for (int i = 0; i < image_width*image_height; i++) {
        unsigned int red = (rgb[i]&0xFF0000) >> 16;
        unsigned int green = (rgb[i]&0xFF00) >> 8;
        unsigned int blue = (rgb[i]&0xFF);
        luma[i] = (25*blue + 129*green + 66*red + 128) >> 8U;
    }
}

#else
void v_rgb2luma(unsigned int *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void v_rgb2luma(unsigned int *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    size_t vl;
    vuint32m4_t mask, red, green, blue, offset, r_coeff, g_coeff, b_coeff, out, tmp;

    vl = vsetvl_e32m4 (image_width);

    for (int i = 0; i < image_height; i++){
        int state_id = i % 2;

        asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                          (state_id << MCFU_SELECTOR_STATE_ID)), 
                                               [csr] "i" (CSR_MCFU_SELECTOR)); 

        blue    = vle32_v_u32m4(rgb, vl);
        green   = vsrl_vx_u32m4(blue, 8U, vl);
        red     = vsrl_vx_u32m4(blue, 16U, vl);

        red     = vand_vx_u32m4(red, 255U, vl);
        out     = vmul_vx_u32m4(red, 66U, vl);

        green   = vand_vx_u32m4(green, 255U, vl);
        tmp     = vmul_vx_u32m4(green, 129U, vl);
        out     = vadd_vv_u32m4(out, tmp, vl);

        blue    = vand_vx_u32m4(blue, 255U, vl);
        tmp     = vmul_vx_u32m4(blue, 25U, vl);
        out     = vadd_vv_u32m4(out, tmp, vl);

        out     = vadd_vx_u32m4(out, 128U, vl);
        out     = vsrl_vx_u32m4(out, 8U, vl);

        vse32_v_u32m4 (luma, out, vl);

        luma += vl;
        rgb  += vl;
    }
}

#endif

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
    int i;

    for (i = 0; i < rpt; i++)
    {
#if USE_VECTOR==0
        rgb2luma(out, in, IMG_W, IMG_H);
#else
        v_rgb2luma(out, in, IMG_W, IMG_H);
#endif
    }

    return 0;
}

void initialise_benchmark ()
{
#if USE_VECTOR==0
    //printf("SCALAR VERSION\r\n");
#else
    //printf("VECTOR VERSION\r\n");
#endif
    asm volatile ("csrw %[csr], %[rs];" :: [rs] "rK" (1 << MCFU_SELECTOR_ENABLE), 
                                           [csr] "i" (CSR_MCFU_SELECTOR)); 

    // Load data for each frame...
    for (int i = 0; i < IMG_H*IMG_W; ++i) {
        //if (i && !(i%IMG_W)) printf("\r\n");
        in[i] = xor();
        //printf("%08x\t", in[i]);
    }
}



int verify_benchmark (int unused)
{
    unsigned int out_exp [IMG_H*IMG_W] = {0};

    for (int i = 0; i < IMG_W*IMG_H; i++) {
        unsigned int red   = (in[i] & 0xFF0000) >> 16U;
        unsigned int green = (in[i] & 0xFF00) >> 8U;
        unsigned int blue  = (in[i] & 0xFF);
        out_exp[i] = (25*blue + 129*green + 66*red + 128) >> 8U;
    }

    /*
    for (int i = 0; i < IMG_H*IMG_W; ++i) 
        if (out[i] != out_exp[i])
            printf("%d \t Actual:%02x \t Expected:%02x\r\n", i, out[i], out_exp[i]);
    */

    return 0 == memcmp (out, out_exp, IMG_H * IMG_W * sizeof (out[0]));
}
