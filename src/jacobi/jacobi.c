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

#define CSR_MCFU_SELECTOR 0xBC0
#define MCFU_SELECTOR_CFU_ID    0
#define MCFU_SELECTOR_STATE_ID  16
#define MCFU_SELECTOR_ENABLE    31

#define LOCAL_SCALE_FACTOR 1
#define VLEN 1024*4
#define N VLEN/32
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

void v_jacobi_unit0(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit0(int* A, int* B, const int32_t tsteps, const int32_t n)
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

        for (int i = 0; i < size_y; ++i) 
        {
            asm volatile ("vle32.v v4, (%0)":: "r"(&input[0*n+j+1]));                   //TOP
            asm volatile ("vle32.v v8, (%0)":: "r"(&input[1*n+j+1]));                   //CENTER
            asm volatile ("vle32.v v12, (%0)":: "r"(&input[2*n+j+1]));                  //BOTTOM
            asm volatile ("vslide1up.vx v16, v8, %0":: "r"(input[1*n+j]));              //LEFT
            asm volatile ("vslide1down.vx v20, v8, %0":: "r"(input[1*n+j+1+gvl]));      //RIGHT
            asm volatile ("vadd.vv v24, v4,  v12");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            p_output = &output[1*n+j+1];
            input += n;
            output += n;

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
        }
    }
}

void v_jacobi_unit1(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit1(int* A, int* B, const int32_t tsteps, const int32_t n)
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

        for (int i = 0; i < size_y; ++i) 
        {
            asm volatile ("vle32.v v12, (%0)":: "r"(&input[2*n+j+1]));                  //BOTTOM
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
        }
    }
}

void v_jacobi_unit2(int* A, int* B, const int32_t tsteps, const int32_t n, int cxu_id);
void v_jacobi_unit2(int* A, int* B, const int32_t tsteps, const int32_t n, int cxu_id)
{
    int size_y = n-2;
    int size_x = n-2;
    int xConstant = 2;

    int* inputs[2]  = {A, A + (size_y/2) * n};
    int* outputs[2] = {B, B + (size_y/2) * n};
    int* p_output;

    for (int j = 0, gvl = 0; j < size_x; j+=gvl) 
    {
        for (int state_id = 0; state_id < 2; state_id++)
        {
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_STATE_ID) |
                                                             (cxu_id << MCFU_SELECTOR_CFU_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));
            asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e32, m4, ta, mu" : [RESP_VL] "=r" (gvl) 
                                                                           : [REQ_VL]  "r" (size_y-j));

            asm volatile ("vle32.v v4, (%0)":: "r"(&inputs[state_id][0*n+j+1]));                    //TOP
            asm volatile ("vle32.v v8, (%0)":: "r"(&inputs[state_id][1*n+j+1]));                    //CENTER
        }

        for (int i = 0; i < size_y; ++i) 
        {
            int state_id = i & 1;
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_STATE_ID) | 
                                                             (cxu_id << MCFU_SELECTOR_CFU_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));

            asm volatile ("vle32.v v12, (%0)":: "r"(&inputs[state_id][2*n+j+1]));                   //BOTTOM
            asm volatile ("vslide1up.vx v16, v8, %0":: "r"(inputs[state_id][1*n+j]));               //LEFT
            asm volatile ("vslide1down.vx v20, v8, %0":: "r"(inputs[state_id][1*n+j+gvl+1]));       //RIGHT

            asm volatile ("vadd.vv v24, v4,  v12");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            p_output = &outputs[state_id][1*n+j+1];
            inputs[state_id] += n;
            outputs[state_id] += n;

            //printf("Address: %x\r\n", p_output);

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vmv.v.v v4, v8");
            asm volatile ("vmv.v.v v8, v12");
        }
    }
}
void v_jacobi_unit8(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit8(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    int size_y = n-2;
    int size_x = n-2;
    int xConstant = 2;

    int* inputs[2]  = {A, A + (size_y/2) * n};
    int* outputs[2] = {B, B + (size_y/2) * n};
    int* p_output;

    for (int j = 0, gvl = 0; j < size_x; j+=gvl) 
    {
        for (int state_id = 0; state_id < 2; state_id++)
        {
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_CFU_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));
            asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e32, m4, ta, mu" : [RESP_VL] "=r" (gvl) 
                                                                           : [REQ_VL]  "r" (size_y-j));

            asm volatile ("vle32.v v4, (%0)":: "r"(&inputs[state_id][0*n+j+1]));                    //TOP
            asm volatile ("vle32.v v8, (%0)":: "r"(&inputs[state_id][1*n+j+1]));                    //CENTER
        }

        for (int i = 0; i < size_y; ++i) 
        {
            int state_id = i & 1;
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_CFU_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));

            asm volatile ("vle32.v v12, (%0)":: "r"(&inputs[state_id][2*n+j+1]));                   //BOTTOM
            asm volatile ("vslide1up.vx v16, v8, %0":: "r"(inputs[state_id][1*n+j]));               //LEFT
            asm volatile ("vslide1down.vx v20, v8, %0":: "r"(inputs[state_id][1*n+j+gvl+1]));       //RIGHT

            asm volatile ("vadd.vv v24, v4,  v12");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            p_output = &outputs[state_id][1*n+j+1];
            inputs[state_id] += n;
            outputs[state_id] += n;

            //printf("Address: %x\r\n", p_output);

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vmv.v.v v4, v8");
            asm volatile ("vmv.v.v v8, v12");
        }
    }
}

void v_jacobi_unit9(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit9(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    int size_y = n-2;
    int size_x = n-2;
    int xConstant = 2;

    int* inputs[2]  = {A, A + (size_y/2) * n};
    int* outputs[2] = {B, B + (size_y/2) * n};
    int* p_output;

    for (int j = 0, gvl = 0; j < size_x; j+=gvl) 
    {
        for (int state_id = 0; state_id < 2; state_id++)
        {
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));
            asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e32, m4, ta, mu" : [RESP_VL] "=r" (gvl) 
                                                                           : [REQ_VL]  "r" (size_y-j));

            asm volatile ("vle32.v v4, (%0)":: "r"(&inputs[state_id][0*n+j+1]));                    //TOP
            asm volatile ("vle32.v v8, (%0)":: "r"(&inputs[state_id][1*n+j+1]));                    //CENTER
        }

        for (int i = 0; i < size_y; ++i) 
        {
            int state_id = i & 1;
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));

            asm volatile ("vle32.v v12, (%0)":: "r"(&inputs[state_id][2*n+j+1]));                   //BOTTOM
            asm volatile ("vslide1up.vx v16, v8, %0":: "r"(inputs[state_id][1*n+j]));               //LEFT
            asm volatile ("vslide1down.vx v20, v8, %0":: "r"(inputs[state_id][1*n+j+gvl+1]));       //RIGHT

            asm volatile ("vadd.vv v24, v4,  v12");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            p_output = &outputs[state_id][1*n+j+1];
            inputs[state_id] += n;
            outputs[state_id] += n;

            //printf("Address: %x\r\n", p_output);

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vmv.v.v v4, v8");
            asm volatile ("vmv.v.v v8, v12");
        }
    }
}

void v_jacobi_unit3(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit3(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    int size_y = n-2;
    int size_x = n-2;
    int xConstant = 2;

    int* inputs[2]  = {A, A + (size_y/2) * n};
    int* outputs[2] = {B, B + (size_y/2) * n};
    int* p_output;

    for (int j = 0, gvl = 0; j < size_x; j+=gvl) 
    {
        for (int state_id = 0; state_id < 2; state_id++)
        {
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));
            asm volatile ("vsetvli %[RESP_VL], %[REQ_VL], e32, m4, ta, mu" : [RESP_VL] "=r" (gvl) 
                                                                           : [REQ_VL]  "r" (size_y-j));

            asm volatile ("vle32.v v4, (%0)":: "r"(&inputs[state_id][0*n+j+1]));                    //TOP
            asm volatile ("vle32.v v8, (%0)":: "r"(&inputs[state_id][1*n+j+1]));                    //CENTER
        }

        for (int i = 0; i < size_y; ++i) 
        {
            int state_id = i & 1;
            asm volatile ("csrw %[csr], %[rs];" :: [rs] "r" ((1 << MCFU_SELECTOR_ENABLE) | 
                                                             (state_id << MCFU_SELECTOR_STATE_ID)), 
                                                   [csr] "i" (CSR_MCFU_SELECTOR));

            if (state_id == 0)
            {
                asm volatile ("vle32.v v12, (%0)":: "r"(&inputs[0][2*n+j+1]));
                asm volatile ("vslide1up.vx v16, v8, %0":: "r"(inputs[0][1*n+j]));
                asm volatile ("vslide1down.vx v20, v8, %0":: "r"(inputs[0][1*n+j+gvl+1]));
            }
            else 
            {
                asm volatile ("vle32.v v12, (%0)":: "r"(&inputs[1][2*n+j+1]));
                asm volatile ("vslide1up.vx v16, v8, %0":: "r"(inputs[1][1*n+j]));
                asm volatile ("vslide1down.vx v20, v8, %0":: "r"(inputs[1][1*n+j+gvl+1]));
            }

            asm volatile ("vadd.vv v24, v4,  v12");
            asm volatile ("vadd.vv v28, v16, v20");
            asm volatile ("vadd.vv v24, v8,  v24");
            asm volatile ("vadd.vv v24, v24, v28");
            asm volatile ("vmul.vx v24, v24, %0":: "r"(xConstant));

            if (state_id == 0)
            {
                p_output = &outputs[0][1*n+j+1];
                inputs[0] += n;
                outputs[0] += n;
            }
            else 
            {
                p_output = &outputs[1][1*n+j+1];
                inputs[1] += n;
                outputs[1] += n;
            }

            asm volatile ("vse32.v v24, (%0)" : "+r" (p_output));
            asm volatile ("vmv.v.v v4, v8");
            asm volatile ("vmv.v.v v8, v12");
        }
    }
}

void v_jacobi_unit4(int* A, int* B, const int32_t tsteps, const int32_t n);
void v_jacobi_unit4(int* A, int* B, const int32_t tsteps, const int32_t n)
{
    vint32m4_t    xU;
    vint32m4_t    xUtmp;
    vint32m4_t    xUtmp2;
    vint32m4_t    xUleft;
    vint32m4_t    xUright;
    vint32m4_t    xUtop;
    vint32m4_t    xUbottom;
    vint32m4_t    xUbottom_2;
    
    int size_y = n-2;
    int size_x = n-2;

    unsigned long int gvl = __riscv_vsetvl_e32m4(size_y); //PLCT

    int xConstant = 2;

    for (int j = 0; j < size_x; j += gvl) 
    {
        gvl         = __riscv_vsetvl_e32m4(size_y-j); //PLCT
        xUtop       = __riscv_vle32_v_i32m4(&A[0*n+j+1],gvl);
        xU          = __riscv_vle32_v_i32m4(&A[1*n+j+1],gvl);
        xUbottom    = __riscv_vle32_v_i32m4(&A[2*n+j+1],gvl);

        for (int i = 0; i < size_y; i+=2) 
        {
            xUbottom_2  = __riscv_vle32_v_i32m4(&A[2*n+j+1+n],gvl);
            xUleft      = __riscv_vslide1up_vx_i32m4(xU,A[1*n+j],gvl);
            xUright     = __riscv_vslide1down_vx_i32m4(xU,A[1*n+j+gvl+1],gvl);
            xUtmp       = __riscv_vadd_vv_i32m4(xUtop,xUbottom,gvl);
            xUtmp2      = __riscv_vadd_vv_i32m4(xUleft,xUright,gvl);
            xUtmp       = __riscv_vadd_vv_i32m4(xU,xUtmp,gvl);
            xUtmp       = __riscv_vadd_vv_i32m4(xUtmp,xUtmp2,gvl);
            xUtmp       = __riscv_vmul_vx_i32m4(xUtmp,xConstant,gvl);
            __riscv_vse32_v_i32m4(&B[1*n+j+1], xUtmp,gvl);
            xUtop       = xU;
            xU          = xUbottom;

            A += n;
            B += n;

            xUbottom    = __riscv_vle32_v_i32m4(&A[2*n+j+1+n],gvl);
            xUleft      = __riscv_vslide1up_vx_i32m4(xU,A[1*n+j],gvl);
            xUright     = __riscv_vslide1down_vx_i32m4(xU,A[1*n+j+gvl+1],gvl);
            xUtmp       = __riscv_vadd_vv_i32m4(xUtop,xUbottom_2,gvl);
            xUtmp2      = __riscv_vadd_vv_i32m4(xUleft,xUright,gvl);
            xUtmp       = __riscv_vadd_vv_i32m4(xU,xUtmp,gvl);
            xUtmp       = __riscv_vadd_vv_i32m4(xUtmp,xUtmp2,gvl);
            xUtmp       = __riscv_vmul_vx_i32m4(xUtmp,xConstant,gvl);
            __riscv_vse32_v_i32m4(&B[1*n+j+1], xUtmp,gvl);
            xUtop       = xU;
            xU          = xUbottom_2;

            A += n;
            B += n;
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
      v_jacobi_unit9(A, B, tsteps, n);
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
            A[i+j*N] = a;
            A_exp[i+j*N] = a;
            int b = i+j*N;
            B[i*N+j] = b;
            B_exp[i*N+j] = b;
        }
    }
}

int verify_benchmark (int unused)
{
    
    for (int t = 0; t < TSTEPS * LOCAL_SCALE_FACTOR * CPU_MHZ; t++)
    {
      for (int i = 1; i < N - 1; i++)
	     for (int j = 1; j < N - 1; j++)
	       B_exp[i*N+j] = 2 * (A_exp[i*N+j] + A_exp[i*N+(j-1)] + A_exp[i*N+(j+1)] + A_exp[(1+i)*N+j] + A_exp[(i-1)*N+j]);
      /*
      for (int i = 1; i < N - 1; i++)
	     for (int j = 1; j < N - 1; j++)
	       A_exp[i*N+j] = 2 * (B_exp[i*N+j] + B_exp[i*N+(j-1)] + B_exp[i*N+(1+j)] + B_exp[(1+i)*N+j] + B_exp[(i-1)*N+j]);
      */
    }

    char success = 1;
    for (int i = 1; i < N-1; ++i) {
        for (int j = 1; j < N-1; ++j) {
            if (A[i*N+j] != A_exp[i*N+j]) {
                //printf("(%d,%d) \t Actual:%02x \t Expected:%02x\r\n", i, j, A[i*N+j], A_exp[i*N+j]);
                success = 0;
            }
        }
    }
    
    return success;
}
