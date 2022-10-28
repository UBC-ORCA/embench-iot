/* Copyright (C) 2017 Embecosm Limited and University of Bristol

   Contributor Graham Markall <graham.markall@embecosm.com>

   This file is part of Embench and was formerly part of the Bristol/Embecosm
   Embedded Benchmark Suite.

   SPDX-License-Identifier: GPL-3.0-or-later */

#include <support.h>
#include <stdio.h>

#define TX_BUFFER_EMPTY 0x00000020
#define RX_HAS_DATA 	0x00000001

#define UART_RX_TX_REG 0x60001000
#define LINE_STATUS_REG_ADDR	(UART_RX_TX_REG + 0x14)

unsigned long long begin_time, end_time, user_time;
unsigned long long start_instruction_count, end_instruction_count, user_instruction_count;
unsigned long long scaled_IPC;


static int output_char(char c, FILE *file)
{
  (void) file;
  //Ensure space in buffer
  while (!((*(unsigned char *)LINE_STATUS_REG_ADDR) & TX_BUFFER_EMPTY));

  *(unsigned char*)UART_RX_TX_REG = (unsigned char) c;
    return c;  
}

static int input_char(FILE *file)
{
  (void) file;
  //Wait for character
  while (!((*(unsigned char *)LINE_STATUS_REG_ADDR) & RX_HAS_DATA));

  return *((unsigned char*)UART_RX_TX_REG);
}


static FILE __stdio = FDEV_SETUP_STREAM(output_char,
                                        input_char,
                                        NULL,
                                        _FDEV_SETUP_RW);

FILE *const stdin = &__stdio; __strong_reference(stdin, stdout); __strong_reference(stdin, stderr);
FILE *const stdout = &__stdio;// __strong_reference(stdin, stdout); __strong_reference(stdin, stderr);

void _ATTRIBUTE ((__noreturn__)) _exit (int status) {
   if (status == 0)
      printf("Result: CORRECT\n");
   else
      printf("Result: FAILED\n");

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


