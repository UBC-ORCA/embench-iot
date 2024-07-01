#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#define USE_VECTOR 1
#define USE_QUEUES 0
#define USE_2Q 0

#if USE_VECTOR==1
#include <riscv_vector.h>
#endif

#define CSR_MCFU_SELECTOR 0xBC0
#define MCFU_SELECTOR_CFU_ID    0
#define MCFU_SELECTOR_STATE_ID  16
#define MCFU_SELECTOR_ENABLE    31
#define MCFU_SELECTOR_STATE_ID_MASK     (1 << MCFU_SELECTOR_STATE_ID)

#define LOCAL_SCALE_FACTOR 100
#define VLEN 256*4
#define N VLEN/32

#define IMG_H 32 //908
#define IMG_W 32 //768
#define IMG_PITCH_MAX 256 //258
#define FILTER_WIDTH 3
#define FILTER_HEIGHT 3

#define CX_FENCE_SCALAR_READ(base_address, end_address)                                               \
  do {                                                                                                \
    asm volatile ("cfu_reg 1008,x0,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
  } while(0)

#define CX_FENCE_SCALAR_WRITE(base_address, end_address)                                              \
  do {                                                                                                \
    asm volatile ("cfu_reg 1009,x0,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
  } while(0)


uint8_t in [IMG_H*IMG_W] __attribute__((aligned(64)));
uint8_t out [IMG_H*IMG_W] __attribute__((aligned(64)));

unsigned long xor();
unsigned long xor() { 
    static unsigned long y=2463534242; 
    y ^= (y<<13); 
    y ^= (y>>17); 
    y ^= (y<<5); 
    return y;
}

uint8_t scalar_bubble_uword(uint8_t *array, const int32_t filter_size);
uint8_t scalar_bubble_uword(uint8_t *array, const int32_t filter_size)
{
    uint8_t min, temp;
    for (int j = 0; j < filter_size/2; j++){
        min = array[j];
        for (int i = j+1; i < filter_size; i++){
            if(array[i] < min){
                temp = min;
                min = array[i];
                array[i] = temp;
            }
        }
        array[j] = min;
    }

    min = array[filter_size/2];
    for (int i = (filter_size/2)+1; i < filter_size; i++){
        if (array[i] < min){
            min = array[i];
        }
    }
    return min;
}

void median(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void median(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
    uint8_t array[filter_height*filter_width];

    for(int y=0; y<image_height-2; y++){
        for(int x=0; x<image_width-2; x++){
            for(int j=0; j<filter_height; j++){
                for(int i=0; i<filter_width; i++){
                    array[j*filter_width+i] = input[(y+j)*image_pitch+(x+i)];
                }
            }
            output[y*image_pitch+x] = scalar_bubble_uword(array, filter_height*filter_width);
        }
    }
}

#define VSET(VLEN,VTYPE,LMUL)                                                          \
  do {                                                                                 \
  asm volatile ("vsetvli t0, %[A]," #VTYPE "," #LMUL ", ta, mu \n" :: [A] "r" (VLEN)); \
  } while(0)

#define VMINMAXDN(vreg0, vreg1, vreg2)                      \
  do {                                                      \
    asm volatile("vmv.v.v   "#vreg0", "#vreg1"");            \
    asm volatile("vminu.vv  "#vreg1", "#vreg0", "#vreg2"");  \
    asm volatile("vmaxu.vv  "#vreg2", "#vreg0", "#vreg2"");  \
  } while(0)

#define VMINMAXUP(vreg0, vreg1, vreg2)                      \
  do {                                                      \
    asm volatile("vmv.v.v   "#vreg0", "#vreg1"");            \
    asm volatile("vmaxu.vv  "#vreg1", "#vreg0", "#vreg2"");  \
    asm volatile("vminu.vv  "#vreg2", "#vreg0", "#vreg2"");  \
  } while(0)

void v_median(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void v_median(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
     VSET(image_pitch-2, e8, m1);

     for (int i = 0; i < image_height - 2; i++) {
         asm volatile ("vle8.v v1, (%0)":: "r"(input + 0*image_width + 0));
         asm volatile ("vle8.v v2, (%0)":: "r"(input + 0*image_width + 1));
         asm volatile ("vle8.v v3, (%0)":: "r"(input + 0*image_width + 2));

         asm volatile ("vle8.v v4, (%0)":: "r"(input + 1*image_width + 0));
         asm volatile ("vle8.v v5, (%0)":: "r"(input + 1*image_width + 1));
         asm volatile ("vle8.v v6, (%0)":: "r"(input + 1*image_width + 2));

         asm volatile ("vle8.v v7, (%0)":: "r"(input + 2*image_width + 0));
         asm volatile ("vle8.v v8, (%0)":: "r"(input + 2*image_width + 1));
         asm volatile ("vle8.v v9, (%0)":: "r"(input + 2*image_width + 2));

         //Stage 1
         VMINMAXDN(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXUP(v0, v5, v6);
         VMINMAXDN(v0, v8, v9);

         //Stage 2
         VMINMAXUP(v0, v1, v3);
         VMINMAXUP(v0, v2, v4);
         VMINMAXDN(v0, v7, v9);

         //Stage 3
         VMINMAXUP(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXDN(v0, v5, v9);
         VMINMAXDN(v0, v7, v8);

         //Stage 4
         VMINMAXDN(v0, v1, v9);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 5
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         //Stage 6
         VMINMAXDN(v0, v1, v5);
         VMINMAXDN(v0, v2, v6);
         VMINMAXDN(v0, v3, v7);
         VMINMAXDN(v0, v4, v8);

         //Stage 7
         VMINMAXDN(v0, v1, v3);
         VMINMAXDN(v0, v2, v4);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 8
         VMINMAXDN(v0, v1, v2);
         VMINMAXDN(v0, v3, v4);
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         asm volatile ("vse8.v v5, (%0)" : "+r" (output));  \

         output += image_width;
         input  += image_width;
     }
}


void v_median_opt1(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void v_median_opt1(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
     size_t vl;
     asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e8, m1, ta, mu"  : [RESP_VL] "=r" (vl) 
                                                                    : [REQ_VL]  "r" (image_pitch-2));

     asm volatile ("vle8.v v1, (%0)":: "r"(input + 0*image_width + 0));
     asm volatile ("vslide1down.vx v2, v1, %0":: "r"(input[0*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v3, v2, %0":: "r"(input[0*image_width + 1*vl + 1]));

     asm volatile ("vle8.v v4, (%0)":: "r"(input + 1*image_width + 0));
     asm volatile ("vslide1down.vx v5, v4, %0":: "r"(input[1*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v6, v5, %0":: "r"(input[1*image_width + 1*vl + 1]));

     for (int i = 0; i < image_height - 2; i++) {
         asm volatile ("vle8.v v7, (%0)":: "r"(input + 2*image_width + 0));
         asm volatile ("vslide1down.vx v8, v7, %0":: "r"(input[2*image_width + 1*vl + 0]));
         asm volatile ("vslide1down.vx v9, v8, %0":: "r"(input[2*image_width + 1*vl + 1]));
         asm volatile ("vmv.v.v v14, v4");
         asm volatile ("vmv.v.v v15, v5");
         asm volatile ("vmv.v.v v16, v6");
         asm volatile ("vmv.v.v v17, v7");
         asm volatile ("vmv.v.v v18, v8");
         asm volatile ("vmv.v.v v19, v9");

         //Stage 1
         VMINMAXDN(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXUP(v0, v5, v6);
         VMINMAXDN(v0, v8, v9);

         //Stage 2
         VMINMAXUP(v0, v1, v3);
         VMINMAXUP(v0, v2, v4);
         VMINMAXDN(v0, v7, v9);

         //Stage 3
         VMINMAXUP(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXDN(v0, v5, v9);
         VMINMAXDN(v0, v7, v8);

         //Stage 4
         VMINMAXDN(v0, v1, v9);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 5
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         //Stage 6
         VMINMAXDN(v0, v1, v5);
         VMINMAXDN(v0, v2, v6);
         VMINMAXDN(v0, v3, v7);
         VMINMAXDN(v0, v4, v8);

         //Stage 7
         VMINMAXDN(v0, v1, v3);
         VMINMAXDN(v0, v2, v4);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 8
         VMINMAXDN(v0, v1, v2);
         VMINMAXDN(v0, v3, v4);
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         asm volatile ("vse8.v v5, (%0)" : "+r" (output));
         asm volatile ("vmv.v.v v1, v14");
         asm volatile ("vmv.v.v v2, v15");
         asm volatile ("vmv.v.v v3, v16");
         asm volatile ("vmv.v.v v4, v17");
         asm volatile ("vmv.v.v v5, v18");
         asm volatile ("vmv.v.v v6, v19");

         output += image_width;
         input  += image_width;
     }
}


void v_median_opt4(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void v_median_opt4(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
     size_t vl;
     asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e8, m1, ta, mu"  : [RESP_VL] "=r" (vl) 
                                                                    : [REQ_VL]  "r" (image_pitch-2));

     asm volatile ("vle8.v v1, (%0)":: "r"(input + 0*image_width + 0));
     asm volatile ("vslide1down.vx v2, v1, %0":: "r"(input[0*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v3, v2, %0":: "r"(input[0*image_width + 1*vl + 1]));

     asm volatile ("vle8.v v4, (%0)":: "r"(input + 1*image_width + 0));
     asm volatile ("vslide1down.vx v5, v4, %0":: "r"(input[1*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v6, v5, %0":: "r"(input[1*image_width + 1*vl + 1]));

     asm volatile ("vle8.v v7, (%0)":: "r"(input + 2*image_width + 0));
     for (int i = 0; i < image_height - 2; i+=2) {
         asm volatile ("vle8.v v27, (%0)":: "r"(input + 2*image_width + image_width + 0));
         asm volatile ("vslide1down.vx v8, v7, %0":: "r"(input[2*image_width + 1*vl + 0]));
         asm volatile ("vslide1down.vx v9, v8, %0":: "r"(input[2*image_width + 1*vl + 1]));

         asm volatile ("vmv.v.v v14, v4");
         asm volatile ("vmv.v.v v15, v5");
         asm volatile ("vmv.v.v v16, v6");
         asm volatile ("vmv.v.v v17, v7");
         asm volatile ("vmv.v.v v18, v8");
         asm volatile ("vmv.v.v v19, v9");

         //Stage 1
         VMINMAXDN(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXUP(v0, v5, v6);
         VMINMAXDN(v0, v8, v9);

         //Stage 2
         VMINMAXUP(v0, v1, v3);
         VMINMAXUP(v0, v2, v4);
         VMINMAXDN(v0, v7, v9);

         //Stage 3
         VMINMAXUP(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXDN(v0, v5, v9);
         VMINMAXDN(v0, v7, v8);

         //Stage 4
         VMINMAXDN(v0, v1, v9);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 5
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         //Stage 6
         VMINMAXDN(v0, v1, v5);
         VMINMAXDN(v0, v2, v6);
         VMINMAXDN(v0, v3, v7);
         VMINMAXDN(v0, v4, v8);

         //Stage 7
         VMINMAXDN(v0, v1, v3);
         VMINMAXDN(v0, v2, v4);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 8
         VMINMAXDN(v0, v1, v2);
         VMINMAXDN(v0, v3, v4);
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         asm volatile ("vse8.v v5, (%0)" : "+r" (output));
         asm volatile ("vmv.v.v v1, v14");
         asm volatile ("vmv.v.v v2, v15");
         asm volatile ("vmv.v.v v3, v16");
         asm volatile ("vmv.v.v v4, v17");
         asm volatile ("vmv.v.v v5, v18");
         asm volatile ("vmv.v.v v6, v19");

         output += image_width;
         input  += image_width;

         asm volatile ("vle8.v v7, (%0)":: "r"(input + 2*image_width + image_width + 0));
         asm volatile ("vslide1down.vx v8, v27, %0":: "r"(input[2*image_width + 1*vl + 0]));
         asm volatile ("vslide1down.vx v9, v8, %0":: "r"(input[2*image_width + 1*vl + 1]));

         asm volatile ("vmv.v.v v14, v4");
         asm volatile ("vmv.v.v v15, v5");
         asm volatile ("vmv.v.v v16, v6");
         asm volatile ("vmv.v.v v17, v27");
         asm volatile ("vmv.v.v v18, v8");
         asm volatile ("vmv.v.v v19, v9");

         //Stage 1
         VMINMAXDN(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXUP(v0, v5, v6);
         VMINMAXDN(v0, v8, v9);

         //Stage 2
         VMINMAXUP(v0, v1, v3);
         VMINMAXUP(v0, v2, v4);
         VMINMAXDN(v0, v27, v9);

         //Stage 3
         VMINMAXUP(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXDN(v0, v5, v9);
         VMINMAXDN(v0, v27, v8);

         //Stage 4
         VMINMAXDN(v0, v1, v9);
         VMINMAXDN(v0, v5, v27);
         VMINMAXDN(v0, v6, v8);

         //Stage 5
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v27, v8);

         //Stage 6
         VMINMAXDN(v0, v1, v5);
         VMINMAXDN(v0, v2, v6);
         VMINMAXDN(v0, v3, v27);
         VMINMAXDN(v0, v4, v8);

         //Stage 7
         VMINMAXDN(v0, v1, v3);
         VMINMAXDN(v0, v2, v4);
         VMINMAXDN(v0, v5, v27);
         VMINMAXDN(v0, v6, v8);

         //Stage 8
         VMINMAXDN(v0, v1, v2);
         VMINMAXDN(v0, v3, v4);
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v27, v8);

         asm volatile ("vse8.v v5, (%0)" : "+r" (output));
         asm volatile ("vmv.v.v v1, v14");
         asm volatile ("vmv.v.v v2, v15");
         asm volatile ("vmv.v.v v3, v16");
         asm volatile ("vmv.v.v v4, v17");
         asm volatile ("vmv.v.v v5, v18");
         asm volatile ("vmv.v.v v6, v19");

         output += image_width;
         input  += image_width;
     }
}

void v_median_opt2(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void v_median_opt2(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
     uint8_t* inputs[2]  = {input, input + (image_height/2 - 1) * image_width};
     uint8_t* outputs[2] = {output, output + (image_height/2 - 1) * image_width};
    
     size_t vl;
     for (int state_id = 0; state_id < 2; state_id++)
     {
         asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                          (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                [csr] "i" (CSR_MCFU_SELECTOR));
         asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e8, m1, ta, mu"  : [RESP_VL] "=r" (vl) 
                                                                        : [REQ_VL]  "r" (image_pitch-2));

         asm volatile ("vle8.v v1, (%0)":: "r"(inputs[state_id] + 0*image_width));
         asm volatile ("vslide1down.vx v2, v1, %0":: "r"(inputs[state_id][0*image_width + vl + 0]));
         asm volatile ("vslide1down.vx v3, v2, %0":: "r"(inputs[state_id][0*image_width + vl + 1]));

         asm volatile ("vle8.v v4, (%0)":: "r"(inputs[state_id] + 1*image_width));
         asm volatile ("vslide1down.vx v5, v4, %0":: "r"(inputs[state_id][1*image_width + vl + 0]));
         asm volatile ("vslide1down.vx v6, v5, %0":: "r"(inputs[state_id][1*image_width + vl + 1]));

     }

     for (int i = 0; i < image_height - 2; i++) 
     {
         int state_id = i & 1;
         asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                          (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                [csr] "i" (CSR_MCFU_SELECTOR));

         asm volatile ("vle8.v v7, (%0)":: "r"(inputs[state_id] + 2*image_width));
         asm volatile ("vslide1down.vx v8, v7, %0":: "r"(inputs[state_id][2*image_width + vl + 0]));
         asm volatile ("vslide1down.vx v9, v8, %0":: "r"(inputs[state_id][2*image_width + vl + 1]));
         asm volatile ("vmv.v.v v14, v4");
         asm volatile ("vmv.v.v v15, v5");
         asm volatile ("vmv.v.v v16, v6");
         asm volatile ("vmv.v.v v17, v7");
         asm volatile ("vmv.v.v v18, v8");
         asm volatile ("vmv.v.v v19, v9");

         //Stage 1
         VMINMAXDN(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXUP(v0, v5, v6);
         VMINMAXDN(v0, v8, v9);

         //Stage 2
         VMINMAXUP(v0, v1, v3);
         VMINMAXUP(v0, v2, v4);
         VMINMAXDN(v0, v7, v9);

         //Stage 3
         VMINMAXUP(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXDN(v0, v5, v9);
         VMINMAXDN(v0, v7, v8);

         //Stage 4
         VMINMAXDN(v0, v1, v9);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 5
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         //Stage 6
         VMINMAXDN(v0, v1, v5);
         VMINMAXDN(v0, v2, v6);
         VMINMAXDN(v0, v3, v7);
         VMINMAXDN(v0, v4, v8);

         //Stage 7
         VMINMAXDN(v0, v1, v3);
         VMINMAXDN(v0, v2, v4);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 8
         VMINMAXDN(v0, v1, v2);
         VMINMAXDN(v0, v3, v4);
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         asm volatile ("vse8.v v5, (%0)" : "+r" (outputs[state_id]));
         asm volatile ("vmv.v.v v1, v14");
         asm volatile ("vmv.v.v v2, v15");
         asm volatile ("vmv.v.v v3, v16");
         asm volatile ("vmv.v.v v4, v17");
         asm volatile ("vmv.v.v v5, v18");
         asm volatile ("vmv.v.v v6, v19");

         outputs[state_id] += image_width;
         inputs[state_id]  += image_width;
     }
}


void v_median_opt3(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void v_median_opt3(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
     uint8_t* inputs[2]  = {input, input + (image_height/2 - 1) * image_width};
     uint8_t* outputs[2] = {output, output + (image_height/2 - 1) * image_width};
    
     size_t vl;
     for (int state_id = 0; state_id < 2; state_id++)
     {
         asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                          (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                [csr] "i" (CSR_MCFU_SELECTOR));
         asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e8, m1, ta, mu"  : [RESP_VL] "=r" (vl) 
                                                                        : [REQ_VL]  "r" (image_pitch-2));

         asm volatile ("vle8.v v1, (%0)":: "r"(inputs[state_id] + 0*image_width));
         asm volatile ("vslide1down.vx v2, v1, %0":: "r"(inputs[state_id][0*image_width + vl + 0]));
         asm volatile ("vslide1down.vx v3, v2, %0":: "r"(inputs[state_id][0*image_width + vl + 1]));

         asm volatile ("vle8.v v4, (%0)":: "r"(inputs[state_id] + 1*image_width));
         asm volatile ("vslide1down.vx v5, v4, %0":: "r"(inputs[state_id][1*image_width + vl + 0]));
         asm volatile ("vslide1down.vx v6, v5, %0":: "r"(inputs[state_id][1*image_width + vl + 1]));

     }

     for (int i = 0; i < image_height/2 - 1; i++) 
     {
         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             asm volatile ("vle8.v v7, (%0)":: "r"(inputs[state_id] + 2*image_width));
             asm volatile ("vslide1down.vx v8, v7, %0":: "r"(inputs[state_id][2*image_width + vl + 0]));
             asm volatile ("vslide1down.vx v9, v8, %0":: "r"(inputs[state_id][2*image_width + vl + 1]));
             asm volatile ("vmv.v.v v14, v4");
             asm volatile ("vmv.v.v v15, v5");
             asm volatile ("vmv.v.v v16, v6");
             asm volatile ("vmv.v.v v17, v7");
             asm volatile ("vmv.v.v v18, v8");
             asm volatile ("vmv.v.v v19, v9");
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 1
             VMINMAXDN(v0, v1, v2);
             VMINMAXUP(v0, v3, v4);
             VMINMAXUP(v0, v5, v6);
             VMINMAXDN(v0, v8, v9);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 2
             VMINMAXUP(v0, v1, v3);
             VMINMAXUP(v0, v2, v4);
             VMINMAXDN(v0, v7, v9);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 3
             VMINMAXUP(v0, v1, v2);
             VMINMAXUP(v0, v3, v4);
             VMINMAXDN(v0, v5, v9);
             VMINMAXDN(v0, v7, v8);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 4
             VMINMAXDN(v0, v1, v9);
             VMINMAXDN(v0, v5, v7);
             VMINMAXDN(v0, v6, v8);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 5
             VMINMAXDN(v0, v5, v6);
             VMINMAXDN(v0, v7, v8);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 6
             VMINMAXDN(v0, v1, v5);
             VMINMAXDN(v0, v2, v6);
             VMINMAXDN(v0, v3, v7);
             VMINMAXDN(v0, v4, v8);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 7
             VMINMAXDN(v0, v1, v3);
             VMINMAXDN(v0, v2, v4);
             VMINMAXDN(v0, v5, v7);
             VMINMAXDN(v0, v6, v8);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             //Stage 8
             VMINMAXDN(v0, v1, v2);
             VMINMAXDN(v0, v3, v4);
             VMINMAXDN(v0, v5, v6);
             VMINMAXDN(v0, v7, v8);
         }

         for (int state_id = 0; state_id < 2; state_id++)
         {
             asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                              (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                    [csr] "i" (CSR_MCFU_SELECTOR));
             asm volatile ("vse8.v v5, (%0)" : "+r" (outputs[state_id]));
             asm volatile ("vmv.v.v v1, v14");
             asm volatile ("vmv.v.v v2, v15");
             asm volatile ("vmv.v.v v3, v16");
             asm volatile ("vmv.v.v v4, v17");
             asm volatile ("vmv.v.v v5, v18");
             asm volatile ("vmv.v.v v6, v19");

             outputs[state_id] += image_width;
             inputs[state_id]  += image_width;
         }
     }
}

/*
void v_median_alt(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch);
void v_median_alt(uint8_t* output, uint8_t* input, int32_t filter_height, int32_t filter_width, int32_t image_height, int32_t image_width, int32_t image_pitch) 
{
     int cst = image_height/2 - 1;
     size_t vl;

     asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                      (0 << MCFU_SELECTOR_STATE_ID)), 
                                            [csr] "i" (CSR_MCFU_SELECTOR));
     asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e8, m1, ta, mu"  : [RESP_VL] "=r" (vl) 
                                                                    : [REQ_VL]  "r" (image_pitch-2));

     asm volatile ("vle8.v v1, (%0)":: "r"(input + 0*image_width + 0));
     asm volatile ("vslide1down.vx v2, v1, %0":: "r"(input[0*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v3, v2, %0":: "r"(input[0*image_width + 1*vl + 1]));
     asm volatile ("vle8.v v4, (%0)":: "r"(input + 1*image_width + 0));
     asm volatile ("vslide1down.vx v5, v4, %0":: "r"(input[1*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v6, v5, %0":: "r"(input[1*image_width + 1*vl + 1]));

     asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                      (1 << MCFU_SELECTOR_STATE_ID)), 
                                            [csr] "i" (CSR_MCFU_SELECTOR));
     asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e8, m1, ta, mu"  : [RESP_VL] "=r" (vl) 
                                                                    : [REQ_VL]  "r" (image_pitch-2));

     asm volatile ("vle8.v v1, (%0)":: "r"(input + (cst + 0)*image_width + 0));
     asm volatile ("vslide1down.vx v2, v1, %0":: "r"(input[(cst + 0)*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v3, v2, %0":: "r"(input[(cst + 0)*image_width + 1*vl + 1]));
     asm volatile ("vle8.v v4, (%0)":: "r"(input + (cst + 1)*image_width + 0));
     asm volatile ("vslide1down.vx v5, v4, %0":: "r"(input[(cst + 1)*image_width + 1*vl + 0]));
     asm volatile ("vslide1down.vx v6, v5, %0":: "r"(input[(cst + 1)*image_width + 1*vl + 1]));

     for (int i = 0; i < image_height - 2; i++) 
     {
         int state_id = i & 1;
         asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                          (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                [csr] "i" (CSR_MCFU_SELECTOR));

         asm volatile ("vle8.v v7, (%0)":: "r"(input + (i/2 + state_id*cst + 2)*image_width + 0));
         asm volatile ("vslide1down.vx v8, v7, %0":: "r"(input[(i/2 + state_id*cst + 2)*image_width + 1*vl + 0]));
         asm volatile ("vslide1down.vx v9, v8, %0":: "r"(input[(i/2 + state_id*cst + 2)*image_width + 1*vl + 1]));


         //Stage 1
         VMINMAXDN(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXUP(v0, v5, v6);
         VMINMAXDN(v0, v8, v9);

         //Stage 2
         VMINMAXUP(v0, v1, v3);
         VMINMAXUP(v0, v2, v4);
         VMINMAXDN(v0, v7, v9);

         //Stage 3
         VMINMAXUP(v0, v1, v2);
         VMINMAXUP(v0, v3, v4);
         VMINMAXDN(v0, v5, v9);
         VMINMAXDN(v0, v7, v8);

         //Stage 4
         VMINMAXDN(v0, v1, v9);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 5
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         //Stage 6
         VMINMAXDN(v0, v1, v5);
         VMINMAXDN(v0, v2, v6);
         VMINMAXDN(v0, v3, v7);
         VMINMAXDN(v0, v4, v8);

         //Stage 7
         VMINMAXDN(v0, v1, v3);
         VMINMAXDN(v0, v2, v4);
         VMINMAXDN(v0, v5, v7);
         VMINMAXDN(v0, v6, v8);

         //Stage 8
         VMINMAXDN(v0, v1, v2);
         VMINMAXDN(v0, v3, v4);
         VMINMAXDN(v0, v5, v6);
         VMINMAXDN(v0, v7, v8);

         uint8_t* adjusted_output = &output[(i/2 + state_id*cst)*image_width];
         asm volatile ("vse8.v v5, (%0)" : "+r" (adjusted_output));
         asm volatile ("vmv.v.v v1, v4");
         asm volatile ("vmv.v.v v2, v5");
         asm volatile ("vmv.v.v v3, v6");
         asm volatile ("vmv.v.v v4, v7");
         asm volatile ("vmv.v.v v5, v8");
         asm volatile ("vmv.v.v v6, v9");

         //output += image_width;
         //input  += image_width;
     }
}
*/

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

    asm volatile ("fence rw, io");

    for (i = 0; i < rpt; i++)
    {
#if USE_VECTOR==0
        median(out, in, FILTER_HEIGHT, FILTER_WIDTH, IMG_H, IMG_W, IMG_W);
#else
        v_median_opt3(out, in, FILTER_HEIGHT, FILTER_WIDTH, IMG_H, IMG_W, IMG_W);
#endif
    }

#if USE_VECTOR==1
    CX_FENCE_SCALAR_READ(&out[0], ((char*)(&out[IMG_H*IMG_W]))-1);
#endif

    /*
    if (IMG_W <= IMG_PITCH_MAX) {
        rvv_median(out, in, in_row, tmp, IMG_W, IMG_H, IMG_W);
    } else {
        int iters = IMG_W/IMG_PITCH_MAX;
        for (int i = 0; i <= iters; i++){
            int img_pitch = (i < iters) ? IMG_PITCH_MAX : (IMG_W - i*(IMG_PITCH_MAX - 2));
            rvv_median(out + i*(IMG_PITCH_MAX - 2), in + i*(IMG_PITCH_MAX - 2), in_row, tmp, IMG_W, IMG_H, img_pitch);
        }
    }
    */

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
    for (int i = 0; i < IMG_H; ++i) {
        for (int j = 0; j < IMG_W; ++j) {
            in[i*IMG_W+j] = (i*IMG_W+j) & 0xff;
        }
    }
}


int verify_benchmark (int unused)
{
    uint8_t out_exp [IMG_H*IMG_W];
    median(out_exp, in, FILTER_HEIGHT, FILTER_WIDTH, IMG_H, IMG_W, IMG_W);

    uint8_t success = 1;
    for (int i = 0; i < IMG_H-2; ++i) {
        for (int j = 0; j < IMG_W-2; ++j) {
            //printf("(%d,%d) \t Actual:%d \t Expected:%d\r\n", i, j, out[i*IMG_W+j], out_exp[i*IMG_W+j]);
            if (out[i*IMG_W+j] != out_exp[i*IMG_W+j]) {
                //printf("(%d,%d) \t Actual:%02x \t Expected:%02x\r\n", i, j, out[i*IMG_W+j], out_exp[i*IMG_W+j]);
                success = 0;
            }
        }
    }
    
    return success;
}
