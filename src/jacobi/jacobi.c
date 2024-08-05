#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "support.h"
#define USE_VECTOR 1
#define USE_QUEUES 0
#define USE_2Q 1

#if USE_VECTOR==1
#include <riscv_vector.h>
#endif
//
// EXTRACT n bits starting at position i from x
#define GET_BITS(x, n, i) (((x) >> (i)) & ((1 << (n)) - 1))

// UNIQUE SoC PARAMETERS
#define LOG2_NUM_CXUS 1
#define NUM_CXUS (1<<LOG2_NUM_CXUS)
#define LOG2_NUM_STATES 3
#define NUM_STATES (1<<LOG2_NUM_STATES)

#define CSR_MCFU_SELECTOR 0xBC0
#define MCFU_SELECTOR_CFU_ID    0
#define MCFU_SELECTOR_STATE_ID  16
#define MCFU_SELECTOR_ENABLE    31

#define LOCAL_SCALE_FACTOR 1
#define VLEN 1024*4
#define N 130
#define TSTEPS 1

static volatile int A [N*N] __attribute__((aligned(64)));
static volatile int B [N*N] __attribute__((aligned(64)));
static volatile int A_exp [N*N] __attribute__((aligned(64)));
static volatile int B_exp [N*N] __attribute__((aligned(64)));

unsigned long xor();
unsigned long xor() { 
    static unsigned long y=2463534242; 
    y ^= (y<<13); 
    y ^= (y>>17); 
    y ^= (y<<5); 
    return y;
}

#if USE_VECTOR==0
void jacobi(int* A, int* B, const int32_t tsteps, const int32_t n);
void jacobi(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    for (int t = 0; t < tsteps; t++)
    {
        for (int i = 1; i < N - 1; i++)
            for (int j = 1; j < N - 1; j++)
                B[i*N+j] = 2 * (A[i*N+j] + A[i*N+(j-1)] + A[i*N+(1+j)] + A[(1+i)*N+j] + A[(i-1)*N+j]);
        /*
           for (int i = 1; i < N - 1; i++)
           for (int j = 1; j < N - 1; j++)
           A[i*N+j] = 2 * (B[i*N+j] + B[i*N+(j-1)] + B[i*N+(1+j)] + B[(1+i)*N+j] + B[(i-1)*N+j]);
           */
    }
}

#else

void v_jacobi_unit(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    int size_y = n-2;
    int size_x = n-2;
    int factor = 2;

    int* inputs[NUM_CXUS][NUM_STATES];
    int* outputs[NUM_CXUS][NUM_STATES];
    int* output;

    for (int state_id = 0; state_id < NUM_STATES; state_id++)
    {
        for (int cxu_id = 0; cxu_id < NUM_CXUS; cxu_id++)
        {
            inputs[cxu_id][state_id]  = A;
            outputs[cxu_id][state_id] = B;
            A += (size_y / (NUM_CXUS * NUM_STATES)) * n;
            B += (size_y / (NUM_CXUS * NUM_STATES)) * n;
        }
    }

    for (int j = 0, gvl = 0; j < size_x; j+=gvl) 
    {
        for (int state_id = 0; state_id < NUM_STATES; state_id++)
        {
            for (int cxu_id = 0; cxu_id < NUM_CXUS; cxu_id++)
            {
                asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                            (state_id << MCFU_SELECTOR_STATE_ID) |
                            (cxu_id << MCFU_SELECTOR_CFU_ID)), 
                        [csr] "i" (CSR_MCFU_SELECTOR));
                asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e32, m4, ta, mu" : [RESP_VL] "=r" (gvl) 
                        : [REQ_VL]  "r" (size_y-j));

                asm volatile ("vle32.v v4, (%0)":: "r"(&inputs[cxu_id][state_id][0*n+j+1])); //TOP
                asm volatile ("vle32.v v8, (%0)":: "r"(&inputs[cxu_id][state_id][1*n+j+1])); //CENTER
            }
        }

        for (int i = 0; i < size_y / (NUM_CXUS*NUM_STATES); ++i) 
        {
            for (int state_id = 0; state_id < NUM_STATES; state_id++)
            {
                for (int cxu_id = 0; cxu_id < NUM_CXUS; cxu_id++)
                {
                    asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                (state_id << MCFU_SELECTOR_STATE_ID) | 
                                (cxu_id << MCFU_SELECTOR_CFU_ID)), 
                            [csr] "i" (CSR_MCFU_SELECTOR));

                    asm volatile ("vle32.v v12, (%0)":: "r"(&inputs[cxu_id][state_id][2*n+j+1])); //BOTTOM
                    asm volatile ("vslide1up.vx v16, v8, %0":: "r"(inputs[cxu_id][state_id][1*n+j])); //LEFT
                    asm volatile ("vslide1down.vx v20, v8, %0":: "r"(inputs[cxu_id][state_id][1*n+j+gvl+1])); //RIGHT
                    inputs[cxu_id][state_id] += n;

                    asm volatile ("vadd.vv v24, v4,  v12");
                    asm volatile ("vadd.vv v28, v16, v20");
                    asm volatile ("vadd.vv v24, v8,  v24");
                    asm volatile ("vadd.vv v24, v24, v28");
                    asm volatile ("vmul.vx v24, v24, %0":: "r"(factor));

                    output = &outputs[cxu_id][state_id][1*n+j+1];
                    outputs[cxu_id][state_id] += n;

                    asm volatile ("vse32.v v24, (%0)" : "+r" (output));
                    asm volatile ("vmv.v.v v4, v8");
                    asm volatile ("vmv.v.v v8, v12");
                }
            }
        }
    }
}

void v_jacobi_unit5(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit5(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    int size_y = n-2;
    int size_x = n-2;
    int xConstant = 2;

    int* input  = A;
    int* output = B;
    int* p_output;

    for (int j = 0, gvl = 0; j < size_x; j+=gvl) 
    {
        asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e32, m4, ta, mu" : [RESP_VL] "=r" (gvl) 
                : [REQ_VL]  "r" (size_y-j));

        asm volatile ("vle32.v v4, (%0)":: "r"(&input[0*n+j+1]));                       //TOP
        asm volatile ("vle32.v v8, (%0)":: "r"(&input[1*n+j+1]));                       //CENTER
        asm volatile ("vle32.v v12, (%0)":: "r"(&input[2*n+j+1]));                      //BOTTOM

        for (int i = 0; i < size_y; i+=2) 
        {
            // 0
            asm volatile ("vle32.v v0, (%0)":: "r"(&input[2*n+j+1+n]));                 //BOTTOM-2
            asm volatile ("vslide1up.vx v16, v8, %0":: "r"(input[1*n+j]));              //LEFT
            asm volatile ("vslide1down.vx v20, v8, %0":: "r"(input[1*n+j+gvl+1]));      //RIGHT

            asm volatile ("vadd.vv v24, v4,  v12");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            p_output = &output[1*n+j+1];
            input += n;
            output += n;

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vmv.v.v v4, v8");
            asm volatile ("vmv.v.v v8, v12");

            // 1
            asm volatile ("vle32.v v12, (%0)":: "r"(&input[2*n+j+1+n]));                //BOTTOM
            asm volatile ("vslide1up.vx v16, v8, %0":: "r"(input[1*n+j]));              //LEFT
            asm volatile ("vslide1down.vx v20, v8, %0":: "r"(input[1*n+j+gvl+1]));      //RIGHT

            asm volatile ("vadd.vv v24, v4,  v0");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            p_output = &output[1*n+j+1];
            input += n;
            output += n;

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vmv.v.v v4, v8");
            asm volatile ("vmv.v.v v8, v0");
        }
    }
}
#define CX_FENCE_SCALAR_READ(base_address, end_address)                                               \
    do {                                                                                                \
        asm volatile ("cx_reg 1008,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
    } while(0)

#define CX_FENCE_SCALAR_WRITE(base_address, end_address)                                              \
    do {                                                                                                \
        asm volatile ("cx_reg 1009,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
    } while(0)

void v_jacobi(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    for (int t = 0; t < tsteps; t++)
    {
        v_jacobi_unit(A, B, tsteps, n);
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

    asm volatile ("fence rw, io");

    for (i = 0; i < rpt; i++)
    {
#if USE_VECTOR==0
        jacobi(A, B, TSTEPS, N);
#else
        v_jacobi(A, B, TSTEPS, N);
#endif
    }
#if USE_VECTOR==1
    CX_FENCE_SCALAR_READ(&B[0], ((char*)(&B[N*N]))-1);
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

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            int a = i+j*N;
            A[i+j*N] = 1;
            A_exp[i+j*N] = 1;
            int b = i+j*N;
            B[i*N+j] = 1;
            B_exp[i*N+j] = 1;
        }
    }
}

int verify_benchmark (int unused)
{

    for (int t = 0; t < TSTEPS * LOCAL_SCALE_FACTOR * CPU_MHZ; t++)
    {
        for (int i = 1; i < N - 1; i++){
            for (int j = 1; j < N - 1; j++){
                B_exp[i*N+j] = 2 * (A_exp[i*N+j] + A_exp[i*N+(j-1)] + A_exp[i*N+(j+1)] + A_exp[(1+i)*N+j] + A_exp[(i-1)*N+j]);
            }
        }
        /*
           for (int i = 1; i < N - 1; i++)
           for (int j = 1; j < N - 1; j++)
           A_exp[i*N+j] = 2 * (B_exp[i*N+j] + B_exp[i*N+(j-1)] + B_exp[i*N+(1+j)] + B_exp[(1+i)*N+j] + B_exp[(i-1)*N+j]);
           */
    }

    char success = 1;
    int cnt = 0;
    for (int i = 1; i < N-1; ++i) {
        for (int j = 1; j < N-1; ++j) {
            if (B[i*N+j] != B_exp[i*N+j]) {
                printf("(%d,%d) \t Actual:%d \t Expected:%d\r\n", i, j, B[i*N+j], B_exp[i*N+j]);
                success = 0;
                if (++cnt == 1) 
                    return 0;
            }
        }
    }

    return success;
}
