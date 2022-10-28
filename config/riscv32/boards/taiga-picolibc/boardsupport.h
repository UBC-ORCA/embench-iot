/* Copyright (C) 2017 Embecosm Limited and University of Bristol

   Contributor Graham Markall <graham.markall@embecosm.com>

   This file is part of Embench and was formerly part of the Bristol/Embecosm
   Embedded Benchmark Suite.

   SPDX-License-Identifier: GPL-3.0-or-later */

#define CPU_MHZ 1

extern unsigned long long            Begin_Time,
                End_Time,
                User_Time;
extern unsigned long long           start_instruction_count,
                end_instruction_count,
                user_instruction_count;
extern  unsigned long long           scaled_IPC;
