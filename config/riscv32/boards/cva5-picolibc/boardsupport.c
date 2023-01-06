/* Copyright (C) 2017 Embecosm Limited and University of Bristol

   Contributor Graham Markall <graham.markall@embecosm.com>

   This file is part of Embench and was formerly part of the Bristol/Embecosm
   Embedded Benchmark Suite.

   SPDX-License-Identifier: GPL-3.0-or-later */

#include <support.h>
#include <stdio.h>

#define UART_BASE_REG       0x88001000

#define UART_RX_REG         (UART_BASE_REG + 0x0)
#define UART_TX_REG         (UART_BASE_REG + 0x0)
#define UART_DLL_REG        (UART_BASE_REG + 0x0)
#define UART_DLM_REG        (UART_BASE_REG + 0x4)
#define UART_LCR_REG	    (UART_BASE_REG + 0xC)
#define UART_LCR_REG_DLAB   0x00000080
#define UART_LCR_REG_WLS_8B 0x00000003
#define UART_LSR_REG	    (UART_BASE_REG + 0x14)
#define UART_LSR_REG_THRE   0x00000020
#define UART_LSR_REG_DR     0x00000001

#define UART_BAUDRATE       115200
#define UART_CLK_FREQ       100000000   

unsigned long long begin_time, end_time, user_time;
unsigned long long start_instruction_count, end_instruction_count, user_instruction_count;
unsigned long long scaled_IPC;


static int output_char(char c, FILE *file)
{
    (void) file;
    //Ensure space in buffer
    while (!((*(volatile int *)UART_LSR_REG) & UART_LSR_REG_THRE));

    *(volatile int *)UART_TX_REG = (int) c;

    return 0;  
}

static int input_char(FILE *file)
{
    (void) file;
    //Wait for character
    while (!((*(volatile int *)UART_LSR_REG) & UART_LSR_REG_DR));

    return *((volatile int *)UART_RX_REG);
}

// Adapted from XilinxProcessorIPLib/drivers/uartns550/src/xuartns550_l.c
static void set_uart_baudrate(int InputClockHz, int BaudRate)
{
    int BaudLSB;
    int BaudMSB;
    int LcrRegister;
    int Divisor;

    /*
     * Determine what the divisor should be to get the specified baud
     * rater based upon the input clock frequency and a baud clock prescaler
     * of 16
     */
    Divisor = ((InputClockHz +((BaudRate * 16UL)/2)) /
            (BaudRate * 16UL));
    /*
     * Get the least significant and most significant bytes of the divisor
     * so they can be written to 2 byte registers
     */
    BaudLSB = Divisor & 0xFF;
    BaudMSB = (Divisor >> 8) & 0xFF;

    /*
     * Get the line control register contents and set the divisor latch
     * access bit so the baud rate can be set
     */
    LcrRegister = *((volatile int *)UART_LCR_REG);
    *(volatile int *)UART_LCR_REG = LcrRegister | UART_LCR_REG_DLAB;

    /*
     * Set the baud Divisors to set rate, the initial write of 0xFF is to
     * keep the divisor from being 0 which is not recommended as per the
     * NS16550D spec sheet
     */
    *(volatile int *)UART_DLL_REG = 0xFF;
    *(volatile int *)UART_DLM_REG = BaudMSB;
    *(volatile int *)UART_DLL_REG = BaudLSB;

    /*
     * Clear the Divisor latch access bit, DLAB to allow nornal
     * operation and write to the line control register
     */
    *(volatile int *)UART_LCR_REG = LcrRegister;
}


static FILE __stdio = FDEV_SETUP_STREAM(output_char,
        input_char,
        NULL,
        _FDEV_SETUP_RW);

FILE *const stdin  = &__stdio;
FILE *const stdout = &__stdio;
FILE *const stderr = &__stdio;


void _ATTRIBUTE ((__noreturn__)) _exit (int status) {
    if (status == 0)
        printf("Result: CORRECT\r\n");
    else
        printf("Result: FAILED\r\n");

    __asm__ volatile ("addi x0, x0, 0xA" : : : "memory");//exit verilator
    while(1);
}

unsigned long long read_cycle()
{
    unsigned long long result;
    unsigned long lower;
    unsigned long upper1;
    unsigned long upper2;


    asm volatile (
            "repeat_cycle_%=: csrr %0, cycleh;\n"
            "        csrr %1, cycle;\n"     
            "        csrr %2, cycleh;\n"
            "        bne %0, %2, repeat_cycle_%=;\n" 
            : "=r" (upper1),"=r" (lower),"=r" (upper2)    // Outputs   : temp variable for load result
            : 
            : 
            );
    *(unsigned long *)(&result) = lower;
    *((unsigned long *)(&result)+1) = upper1;

    return result;
}

unsigned long long  read_inst()
{
    unsigned long long  result;
    unsigned long lower;
    unsigned long upper1;
    unsigned long upper2;

    asm volatile (
            "repeat_inst_%=: csrr %0, instreth;\n"
            "        csrr %1, instret;\n"     
            "        csrr %2, instreth;\n"
            "        bne %0, %2, repeat_inst_%=;\n" 
            : "=r" (upper1),"=r" (lower),"=r" (upper2)    // Outputs   : temp variable for load result
            : 
            : 
            );
    *(unsigned long *)(&result) = lower;
    *((unsigned long *)(&result)+1) = upper1;

    return result;
}

    void
initialise_board ()
{
    // 100 MHz clock and 115200 bauds
    set_uart_baudrate(UART_CLK_FREQ, UART_BAUDRATE);
    // Set data bits to 8
    *(volatile int *)UART_LCR_REG = UART_LCR_REG_WLS_8B;

    __asm__ volatile ("li a0, 0" : : : "memory");
}

    void __attribute__ ((noinline)) __attribute__ ((externally_visible))
start_trigger ()
{
    begin_time = read_cycle();
    start_instruction_count = read_inst();
    __asm__ volatile ("addi x0, x0, 0xC" : : : "memory");
}

    void __attribute__ ((noinline)) __attribute__ ((externally_visible))
stop_trigger ()
{
    __asm__ volatile ("addi x0, x0, 0xD" : : : "memory");
    end_time = read_cycle();
    end_instruction_count = read_inst();

    user_time = end_time - begin_time;
    user_instruction_count = end_instruction_count - start_instruction_count;
    scaled_IPC = (user_instruction_count*1000000)/user_time;

    printf("Begin time: %lld\r\n", begin_time);
    printf("End time: %lld\r\n", end_time);
    printf("User time: %lld\r\n", user_time);
    printf("Begin inst: %lld\r\n", start_instruction_count);
    printf("End inst: %lld\r\n", end_instruction_count);
    printf("User inst: %lld\r\n", user_instruction_count);
    printf("IPCx1M: %lld\r\n", scaled_IPC);
}


