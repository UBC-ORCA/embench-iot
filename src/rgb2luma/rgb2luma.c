#include <string.h>
#include <stdint.h>
#include <stdlib.h>
//#include <stdio.h>
#include "support.h"
#define USE_VECTOR 1
#define USE_QUEUES 1

// EXTRACT n bits starting at position i from x
#define GET_BITS(x, n, i) (((x) >> (i)) & ((1 << (n)) - 1))

// UNIQUE SoC PARAMETERS
#define LOG2_NUM_CXUS 0
#define NUM_CXUS (1<<LOG2_NUM_CXUS)
#define LOG2_NUM_STATES 2
#define NUM_STATES (1<<LOG2_NUM_STATES)

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

#define CX_FENCE_SCALAR_READ(base_address, end_address)                                               \
    do {                                                                                                \
        asm volatile ("cx_reg 1008,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
    } while(0)

#define CX_FENCE_SCALAR_WRITE(base_address, end_address)                                              \
    do {                                                                                                \
        asm volatile ("cx_reg 1009,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
    } while(0)


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

#if USE_VECTOR==0
void rgb2luma(unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void rgb2luma(unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    for (int i = 0; i < image_width*image_height; i++) {
        unsigned int red = (rgb[i]&0xFF0000) >> 16;
        unsigned int green = (rgb[i]&0xFF00) >> 8;
        unsigned int blue = (rgb[i]&0xFF);
        luma[i] = (25*blue + 129*green + 66*red + 128) >> 8U;
    }
}

#else
void v_rgb2luma(unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void v_rgb2luma(unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    unsigned char* luma_orig = luma;
    unsigned int*  rgb_orig = rgb;

    for (int j = 0, vl = 0; j < image_width; j += vl) 
    {
        luma = luma_orig + j;
        rgb  = rgb_orig + j;

        for (int state_id = 0; state_id < NUM_STATES; state_id++)
        {
            for (int cxu_id = 0; cxu_id < NUM_CXUS; cxu_id++)
            {
                asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                            (state_id << MCFU_SELECTOR_STATE_ID) |
                            (cxu_id << MCFU_SELECTOR_CFU_ID)), 
                        [csr] "i" (CSR_MCFU_SELECTOR));
                asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e16, m2, ta, mu" : [RESP_VL] "=r" (vl) 
                        : [REQ_VL]  "r" (image_width-j));
            }
        }

        for (int i = 0; i < image_height; i++)
        {
            int cxu_id   = GET_BITS(i, LOG2_NUM_CXUS, 0);
            int state_id = GET_BITS(i, LOG2_NUM_STATES, 0+LOG2_NUM_CXUS);

            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                        (state_id << MCFU_SELECTOR_STATE_ID) |
                        (cxu_id << MCFU_SELECTOR_CFU_ID)), 
                    [csr] "i" (CSR_MCFU_SELECTOR));

            asm volatile ("vsetvli x0, x0, e16, m2, ta, mu");
            asm volatile ("vle32.v v4, (%0)":: "r"(rgb)); 

            asm volatile ("vnsrl.wi  v8,  v4, 16");
            asm volatile ("vnsrl.wi v10,  v4, 8");
            asm volatile ("vnsrl.wi v12,  v4, 0");

            // {R8[j],G8[j],B8[j]} = RGB32[j]
            asm volatile ("vand.vx   v8,  v8, %0":: "r"(255));
            asm volatile ("vand.vx  v10, v10, %0":: "r"(255));
            asm volatile ("vand.vx  v12, v12, %0":: "r"(255));
            // LUMA16[j] = 66*R8[j] + 129*G8[j] + 25*B8[j] + 128
            asm volatile ("vmul.vx   v8,  v8, %0":: "r"(66));
            asm volatile ("vmul.vx  v10, v10, %0":: "r"(129));
            asm volatile ("vmul.vx  v12, v12, %0":: "r"(25));
            asm volatile ("vadd.vv  v14,  v8, v10");
            asm volatile ("vadd.vx  v16, v12, %0":: "r"(128));
            asm volatile ("vadd.vv  v14, v14, v16");
            // LUMA8[j] = LUMA16[j] >> 8

            asm volatile ("vsetvli x0, x0, e8, m1, ta, mu");
            asm volatile ("vnsrl.wi v14, v14, 8");

            asm volatile ("vse8.v   v14, (%0)" : "+r" (luma));

            luma += image_width;
            rgb  += image_width;
        }
    }
}

// MICROBENCHMARK
void v_rgb2luma_prefetch(uint8_t *luma, uint32_t *rgb, const uint32_t img_w, const uint32_t img_h) {
    asm volatile ("vsetvli x0, %[REQ_VL], e16, m2, ta, mu" :: [REQ_VL]  "r" (img_w));
    asm volatile ("vle32.v v4, (%0)":: "r"(rgb));

    for ( uint32_t i = 0, vl; i < IMG_H; i += 2 ) {

        asm volatile ("vsetvli x0, %[REQ_VL], e16, m2, ta, mu" :: [REQ_VL]  "r" (img_w));

        // prefetch next row
        asm volatile ("vle32.v v24, (%0)":: "r"(rgb + img_w));
        // {R8[j],G8[j],B8[j]} = RGB32[j]
        asm volatile ("vnsrl.wi  v8,  v4, 16");
        asm volatile ("vnsrl.wi v10,  v4, 8");
        asm volatile ("vnsrl.wi v12,  v4, 0");
        asm volatile ("vand.vx   v8,  v8, %0":: "r"(255));
        asm volatile ("vand.vx  v10, v10, %0":: "r"(255));
        asm volatile ("vand.vx  v12, v12, %0":: "r"(255));
        // LUMA16[j] = 66*R8[j] + 129*G8[j] + 25*B8[j] + 128
        asm volatile ("vmul.vx   v8,  v8, %0":: "r"(66));
        asm volatile ("vmul.vx  v10, v10, %0":: "r"(129));
        asm volatile ("vmul.vx  v12, v12, %0":: "r"(25));
        asm volatile ("vadd.vv  v14,  v8, v10");
        asm volatile ("vadd.vx  v16, v12, %0":: "r"(128));
        asm volatile ("vadd.vv  v14, v14, v16");
        // LUMA8[j] = LUMA16[j] >> 8
        asm volatile ("vsetvli x0, x0, e8, m1, ta, mu");
        asm volatile ("vnsrl.wi v14, v14, 8");
        asm volatile ("vse8.v   v14, (%0)" : "+r" (luma));

        luma += img_w;
        rgb  += img_w;

        asm volatile ("vsetvli x0, %[REQ_VL], e16, m2, ta, mu" :: [REQ_VL]  "r" (img_w));

        // prefetch next row
        asm volatile ("vle32.v v4, (%0)":: "r"(rgb + img_w));
        // {R8[j],G8[j],B8[j]} = RGB32[j]
        asm volatile ("vnsrl.wi  v8,  v24, 16");
        asm volatile ("vnsrl.wi v10,  v24, 8");
        asm volatile ("vnsrl.wi v12,  v24, 0");
        asm volatile ("vand.vx   v8,  v8, %0":: "r"(255));
        asm volatile ("vand.vx  v10, v10, %0":: "r"(255));
        asm volatile ("vand.vx  v12, v12, %0":: "r"(255));
        // LUMA16[j] = 66*R8[j] + 129*G8[j] + 25*B8[j] + 128
        asm volatile ("vmul.vx   v8,  v8, %0":: "r"(66));
        asm volatile ("vmul.vx  v10, v10, %0":: "r"(129));
        asm volatile ("vmul.vx  v12, v12, %0":: "r"(25));
        asm volatile ("vadd.vv  v14,  v8, v10");
        asm volatile ("vadd.vx  v16, v12, %0":: "r"(128));
        asm volatile ("vadd.vv  v14, v14, v16");
        // LUMA8[j] = LUMA16[j] >> 8
        asm volatile ("vsetvli x0, x0, e8, m1, ta, mu");
        asm volatile ("vnsrl.wi v14, v14, 8");
        asm volatile ("vse8.v   v14, (%0)" : "+r" (luma));

        luma += img_w;
        rgb  += img_w;
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
#if USE_VECTOR==1
    asm volatile ("fence rw, io");
#endif
    int i;

    for (i = 0; i < rpt; i++)
    {
#if USE_VECTOR==0
        rgb2luma(out, in, IMG_W, IMG_H);
#else
        v_rgb2luma(out, in, IMG_W, IMG_H);
#endif
    }
#if USE_VECTOR==1
    CX_FENCE_SCALAR_READ(&out[0], ((char*)(&out[IMG_H*IMG_W]))-1);
#endif

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
        //in[i] = 0x808080;
        //in[i] = 0xdeadbeef;
        //in[i] = i;
        //printf("%08x\t", in[i]);
    }
}



int verify_benchmark (int unused)
{
    unsigned char out_exp [IMG_H*IMG_W] = {0};

    for (int i = 0; i < IMG_W*IMG_H; i++) {
        /*
           unsigned int red   = (in[i] & 0xFF0000) >> 16U;
           unsigned int green = (in[i] & 0xFF00) >> 8U;
           unsigned int blue  = (in[i] & 0xFF);
           out_exp[i] = (1*blue + 1*green + 1*red + 0) >> 8U;
           */
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
       return 0;
       }
       }
       }
       */

    return 0 == memcmp (out, out_exp, IMG_H * IMG_W * sizeof (out[0]));
}
