#include <string.h>
#include <stdint.h>
#include <stdlib.h>
//#include <stdio.h>
#include "support.h"
#define USE_VECTOR 1
#define USE_QUEUES 1
#define USE_2Q 1

#if USE_VECTOR==1
#include <riscv_vector.h>
#endif

#define CSR_MCFU_SELECTOR 0xBC0
#define MCFU_SELECTOR_CFU_ID    0
#define MCFU_SELECTOR_STATE_ID  16
#define MCFU_SELECTOR_ENABLE    31

#define LOCAL_SCALE_FACTOR 1
#define VLEN 1024*4
#define IMG_H VLEN/32
#define IMG_W VLEN/32

static volatile unsigned int in  [IMG_H*IMG_W];
static volatile unsigned short out [IMG_H*IMG_W];

unsigned long xor();
unsigned long xor() { 
    static unsigned long y=2463534242; 
    y ^= (y<<13); 
    y ^= (y>>17); 
    y ^= (y<<5); 
    return y;
}

#if USE_VECTOR==0
void rgb2luma(unsigned short *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void rgb2luma(unsigned short *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    for (int i = 0; i < image_width*image_height; i++) {
        unsigned int red = (rgb[i]&0xFF0000) >> 16;
        unsigned int green = (rgb[i]&0xFF00) >> 8;
        unsigned int blue = (rgb[i]&0xFF);
        luma[i] = (25*blue + 129*green + 66*red + 128) >> 8U;
    }
}

#else
void v_rgb2luma(unsigned short *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void v_rgb2luma(unsigned short *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    size_t vl;
    vuint32m4_t red, green, blue, out, tmp, vrgb;
    vuint16m2_t red16, green16, blue16, out16, tmp16, vrgb16;

#if USE_QUEUES==0
    vuint32m4_t red2, green2, blue2, out2, tmp2, vrgb2;
    vuint16m2_t red16_2, green16_2, blue16_2, out16_2, tmp16_2, vrgb16_2;

    vl      = vsetvl_e32m4 (image_width);
    vrgb    = vle32_v_u32m4(rgb, vl);
    for (int i = 0; i < image_height; i+=2){
        vl      = vsetvl_e32m4 (image_width);
        vrgb2   = vle32_v_u32m4(rgb+image_width, vl);
        vl      = vsetvl_e16m2 (image_width);

        red16   = vnsrl_wx_u16m2(vrgb, 16U, vl);
        green16 = vnsrl_wx_u16m2(vrgb, 8U, vl);
        blue16  = vnsrl_wx_u16m2(vrgb, 0U, vl);

        red16   = vand_vx_u16m2(red16, 255U, vl);
        green16 = vand_vx_u16m2(green16, 255U, vl);
        blue16  = vand_vx_u16m2(blue16, 255U, vl);

        red16   = vmul_vx_u16m2(red16, 66U, vl);
        green16 = vmul_vx_u16m2(green16, 129U, vl);
        blue16  = vmul_vx_u16m2(blue16, 25U, vl);

        out16   = vadd_vv_u16m2(red16, green16, vl);
        tmp16   = vadd_vx_u16m2(blue16, 128U, vl);
        out16   = vadd_vv_u16m2(out16, tmp16, vl);
        out16   = vsrl_vx_u16m2(out16, 8U, vl);

        vse16_v_u16m2 (luma, out16, vl);

        luma += image_width;
        rgb  += image_width;

        vl        = vsetvl_e32m4 (image_width);
        vrgb      = vle32_v_u32m4(rgb+image_width, vl);
        vl        = vsetvl_e16m2 (image_width);

        red16_2   = vnsrl_wx_u16m2(vrgb2, 16U, vl);
        green16_2 = vnsrl_wx_u16m2(vrgb2, 8U, vl);
        blue16_2  = vnsrl_wx_u16m2(vrgb2, 0U, vl);

        red16_2   = vand_vx_u16m2(red16_2, 255U, vl);
        green16_2 = vand_vx_u16m2(green16_2, 255U, vl);
        blue16_2  = vand_vx_u16m2(blue16_2, 255U, vl);

        red16_2   = vmul_vx_u16m2(red16_2, 66U, vl);
        green16_2 = vmul_vx_u16m2(green16_2, 129U, vl);
        blue16_2  = vmul_vx_u16m2(blue16_2, 25U, vl);

        out16_2   = vadd_vv_u16m2(red16_2, green16_2, vl);
        tmp16_2   = vadd_vx_u16m2(blue16_2, 128U, vl);
        out16_2   = vadd_vv_u16m2(out16_2, tmp16_2, vl);
        out16_2   = vsrl_vx_u16m2(out16_2, 8U, vl);

        vse16_v_u16m2 (luma, out16_2, vl);

        luma += image_width;
        rgb  += image_width;
    }
#else
    for (int i = 0; i < image_height; i+=1){
#if USE_2Q==1
        int state_id = i & 1;

        asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                          (state_id << MCFU_SELECTOR_STATE_ID)), 
                                               [csr] "i" (CSR_MCFU_SELECTOR));
#endif
        vl      = vsetvl_e32m4 (image_width);
        vrgb    = vle32_v_u32m4(rgb, vl);
        vl      = vsetvl_e16m2 (image_width);

        red16   = vnsrl_wx_u16m2(vrgb, 16U, vl);
        green16 = vnsrl_wx_u16m2(vrgb, 8U, vl);
        blue16  = vnsrl_wx_u16m2(vrgb, 0U, vl);

        red16   = vand_vx_u16m2(red16, 255U, vl);
        green16 = vand_vx_u16m2(green16, 255U, vl);
        blue16  = vand_vx_u16m2(blue16, 255U, vl);

        red16   = vmul_vx_u16m2(red16, 66U, vl);
        green16 = vmul_vx_u16m2(green16, 129U, vl);
        blue16  = vmul_vx_u16m2(blue16, 25U, vl);

        out16   = vadd_vv_u16m2(red16, green16, vl);
        tmp16   = vadd_vx_u16m2(blue16, 128U, vl);
        out16   = vadd_vv_u16m2(out16, tmp16, vl);
        out16   = vsrl_vx_u16m2(out16, 8U, vl);

        vse16_v_u16m2 (luma, out16, vl);

        luma += image_width;
        rgb  += image_width;
    }
#endif
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


static int /*__attribute__((optimize("-O2")))*/ __attribute__ ((noinline)) benchmark_body (int rpt)
{
    /*
    printf("------------------------------\r\n");
    printf("%x\r\n", &out[0]);
    printf("%x\r\n", &out[IMG_H*IMG_W-1]);
    printf("------------------------------\r\n");
    */

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
    unsigned short out_exp [IMG_H*IMG_W] = {0};

    for (int i = 0; i < IMG_W*IMG_H; i++) {
        unsigned int red   = (in[i] & 0xFF0000) >> 16U;
        unsigned int green = (in[i] & 0xFF00) >> 8U;
        unsigned int blue  = (in[i] & 0xFF);
        out_exp[i] = (25*blue + 129*green + 66*red + 128) >> 8U;
    }

    /*
    for (int j = 0; j < IMG_H; ++j) {
        for (int k = 0; k < IMG_W; ++k) {
            int i = j*IMG_W + k;
            if (out[i] != out_exp[i]) {
                printf("%d = (%d,%d) \t Actual:%02x \t Expected:%02x\r\n", i, j, k, out[i], out_exp[i]);
            }
        }
    }
    */

    return 0 == memcmp (out, out_exp, IMG_H * IMG_W * sizeof (out[0]));
}
