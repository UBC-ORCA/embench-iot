#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "support.h"

#if USE_VECTOR==1
#include <riscv_vector.h>
#include "cx_api.h"
#endif

#define GET_BITS(x, n, i) (((x) >> (i)) & ((1 << (n)) - 1))

// UNIQUE SoC PARAMETERS
int LOG2_NUM_CXUS = CXU_ID;
#define NUM_CXUS (1<<LOG2_NUM_CXUS)
int LOG2_NUM_STATES = STATE_ID;
#define NUM_STATES (1<<LOG2_NUM_STATES)

#define LOCAL_SCALE_FACTOR 1
#define VLEN 1024*4
#define IMG_H VLEN/32
#define IMG_W VLEN/32

#define CX_GUID_VECTOR 1

static volatile unsigned int in  [IMG_H*IMG_W];
static volatile unsigned char out [IMG_H*IMG_W];

void mac (unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height);
void mac (unsigned char *luma, unsigned int *rgb, const int32_t image_width, const int32_t image_height)
{
    int result_mac;
    int result_relu;
    int result_mac_relu;
    int opA = 5;
    int opB = 10;

    //MAC
    MCX_SELECT(1, 0);
    asm volatile ("cx_reg 0,%0,%1,%2;\n" : "=r" (result_mac) : "r" (opA),"r" (opB));
    
    printf("MAC: %d\r\n", result_mac);

    //RELU
    MCX_SELECT(2, 0);
    asm volatile ("cx_reg 0,%0,%1,%2;\n" : "=r" (result_relu) : "r" (result_mac),"r" (result_mac));

    printf("RELU: %d\r\n", result_relu);


    //MAC-RELU
    MCX_SELECT(3, 0);
    asm volatile ("cx_reg 0,%0,%1,%2;\n" : "=r" (result_mac_relu) : "r" (opA),"r" (opB));

    printf("MAC-RELU: %d\r\n", result_mac_relu);

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
    int i;

    for (i = 0; i < rpt; i++)
    {
       mac(out, in, IMG_W, IMG_H);
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

    return 1;
}
