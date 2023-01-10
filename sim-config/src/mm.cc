#include <stdio.h>
#include <riscv_vector.h>
#include <cstdlib>
#include <assert.h>

// #include "base.h"
// #include "menu.h"
// #include "riscv.h"

// #include "perf.h"
#include "mm.h"

void print_matrix_int(int32_t* A, int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%d ", A[i * N + j]);	
        }
        printf("\n");
    }
}

int32_t* random_matrix_int(int N) {
    int32_t* A = new int32_t[N * N];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
           // A[i * N + j] = rand() % 5;
 	    A[i * N + j] = 8;

        }
    }
    return A;
}

void check_mm_equal_int(int32_t* rvv_mm, int32_t* scalar_mm, int N) {
    print_matrix_int(scalar_mm, N);
    print_matrix_int(rvv_mm, N);
    int err = 0;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (rvv_mm[i * N + j] - scalar_mm[i * N + j] != 0) err++;

            if (rvv_mm[i*N+j] != scalar_mm[i*N+j] & err < 15) printf("Got %x, expected %x\n", rvv_mm[i*N+j], scalar_mm[i*N+j]);
        }
    }
    if (err > 0)	printf("Failed with %d errors\n.", err);
    else	printf("Correct values from the matrix multiplication.\n");
}

void rvv_mm_test() {
    const int N = 16;
   // printf("performing integer matrix mulitplication... \n");
    int32_t* A = random_matrix_int(N);
    int32_t* B = random_matrix_int(N);
/*
    printf("original matrix...\n");
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");
    print_matrix_int(A, N);
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");

    int vl = vsetvl_e32m4(N);

    vint32m4_t vA, vB, vC, vTemp;

    printf("begin simple simple scalar operations...\n");
    int i;
    int32_t value = 3;

    for(i=0;i<N;i++){
    	vA = vle32_v_i32m4(&A[i * N], vl);
	vTemp = vadd_vx_i32m4(vA,value,vl);
        vse32_v_i32m4(&A[i * N ], vTemp, vl);

    }
    
    printf("after scalr addition by 3...\n");
   // printf("original matrix...");
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");
    print_matrix_int(A, N);
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");

    int32_t value2 = 2;

   for(i=0;i<N;i++){
    	vA = vle32_v_i32m4(&A[i * N], vl);
	vTemp = vmul_vx_i32m4(vA,value2,vl);
        vse32_v_i32m4(&A[i * N ], vTemp, vl);

    }

    printf("after scalr multiplication by 2...\n");
   // printf("original matrix...");
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");
    print_matrix_int(A, N);
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");

    for(i=0;i<N;i++){
    	vA = vle32_v_i32m4(&A[i * N], vl);
	vB = vle32_v_i32m4(&B[i * N], vl);
	vTemp = vsub_vv_i32m4(vA,vB,vl);
        vse32_v_i32m4(&A[i * N ], vTemp, vl);

    }

    printf("after vector subtraction by B...\n");
   // printf("original matrix...");
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");
    print_matrix_int(A, N);
    printf("---------------- \n");
    printf("---------------- \n");
    printf("---------------- \n");



    printf("end...\n");
*/
   int vl_1_1 = vsetvl_e32m1(1);
   int vl_2_1 = vsetvl_e32m2(1);
   int vl_4_1 = vsetvl_e32m4(1);
   int vl_8_1 = vsetvl_e32m8(1);
   
   printf("vl values with stride set to 1 ... \n");
 printf("vl for LMUL1: %d \n", vl_1_1);
    printf("vl for LMUL2: %d \n", vl_2_1);
    printf("vl for LMUL4: %d \n", vl_4_1);
    printf("vl for LMUL8: %d \n\n", vl_8_1);





   int vl_1 = vsetvl_e32m1(1000);
   int vl_2 = vsetvl_e32m2(1000);
   int vl_4 = vsetvl_e32m4(1000);
   int vl_8 = vsetvl_e32m8(1000);

   printf("vlmax values(setting stride to 1000) \n");
    printf("vlmax for LMUL1: %d \n", vl_1);
    printf("vlmax LMUL2: %d \n", vl_2);
    printf("vlmax LMUL4: %d \n", vl_4);
    printf("vlmax LMUL8: %d \n\n", vl_8);


    printf("VLEN results ... \n");
printf("VLEN for LMUL1: %d \n", 32*4);
printf("VLEN for LMUL2: %d \n", 32*8/2);
printf("VLEN for LMUL4: %d \n", 32*16/4);
printf("VLEN for LMUL8: %d \n", 32*32/8);

 /////   int32_t* B = random_matrix_int(N);

    // int start = perf_get_mcycle();
    // so gcc stops reordering it
 /////   printf("---------------- \n");
  /////  print_matrix_int(A, N);
   ////// printf("---------------- \n");
   ///// print_matrix_int(B, N);
  /////  printf("---------------- \n");
   //// int32_t D [N * N] = {0};
  /////  int32_t value;
  /////  for (int i = 0; i < N; i++) {
      ////  for (int k = 0; k < N; k++) {
         ////   value = A[i * N + k]; 
          ////  for (int j = 0; j < N; j++) {
             /////   D[i * N + j] += value * B[k * N + j];
          ////  }
      ////  }
   ///// }

    // int end = perf_get_mcycle();

   //// vint32m4_t vA, vB, vC, vTmp;

   //// int32_t C [N * N] = {0};

   //// int stride = N;

    // int start_v = perf_get_mcycle();

   ///// int vl = vsetvl_e32m4(stride);

    // vC = vmv_v_x_i32m4(0,stride);

   ///// for (int i = 0; i < N; i++) {
        /////for (int k = 0; k < N; k++) {
           //// value = A[i * N + k];
           //// int j;
           ///// for (j = 0; j < (N/vl)*vl; j+=vl) {
                // C[i * N + j] += A[i * N + k] * B[k * N + j];	

              /////  vB = vle32_v_i32m4(&B[k * N + j ], vl);
              //////  vC = vle32_v_i32m4(&C[i * N + j ], vl);

                // accumulator, constant, vector, number of elements
              /////  vTmp = vmul_vx_i32m4(vB, value, vl);
              /////  vC = vadd_vv_i32m4(vC, vTmp, vl);

               ///// vse32_v_i32m4(&C[i * N + j ], vC, vl);
          /////  }
          /////  vB = vle32_v_i32m4(&B[k * N + j ], N - j);
          //////  vC = vle32_v_i32m4(&C[i * N + j ], N - j);

            // accumulator, constant, vector, number of elements
          /////  vTmp = vmul_vx_i32m4(vB, value, N - j);
          ////  vC = vadd_vv_i32m4(vC, vTmp, N - j);
           ///// vse32_v_i32m4(&C[i * N + j ], vC, N - j);
      ////  }
  /////  }
    // int end_v = perf_get_mcycle();

   ///// check_mm_equal_int(C, D, N);

  ////  delete A;
   ////// delete B;

    // printf("Timestamps: %d, %d, %d, %d\n", start, end, start_v, end_v);
    // printf("Cycle count: %d\n", (end_v - start_v));
    // print_float("Speedup", (float)(end-start)/(float)(end_v-start_v)); // SCALAR OVER VECTOR DUMMY
}
