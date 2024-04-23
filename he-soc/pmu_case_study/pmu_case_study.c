#include "encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "pmu_defines.h"
#include "pmu_test_func.c"

/// The CUA will always miss in the L1 but after one run of the loop, it will never miss in the LLC.

/// This define controls the number of non-CUA cores that are causing RD / WR contention.
#define NUM_NON_CUA 3
// #define START_CORE
#define AVG_LAT 31
#define INTF_WR

// Array size: 320 kB   (LLC Hits)
// #define LEN_NONCUA   40960    
// Array size: 2048 kB  (LLC Misses)
#define LEN_NONCUA   262144

/// Core jumps over 8 64-bit elements every iteration.
/// It skips 64B (or one cacheline) per iteration. 
/// Always causing an L1 miss.
#define JUMP_CUA     8    
#define JUMP_NONCUA  8  
// Starting index for non-CUA.
#define START_NONCUA 0

void mem_sweep_all_sizes (uint32_t num_counter, uint32_t *print_info);
void mem_sweep_two_cases (uint32_t num_counter, uint32_t *print_info);

void end_test(uint32_t mhartid){
  printf("Exiting: %0d.\r\n", mhartid);
  uart_wait_tx_done();
}

#define ARRAY_LEN 80000      // 512 KB size array, 64-bit element each

// *********************************************************************
// Main Function
// *********************************************************************
int main(int argc, char const *argv[]) {
  uint32_t mhartid;
  asm volatile (
    "csrr %0, 0xF14\n"
    : "=r" (mhartid)
  );

  uint32_t error_count = 0;

  // *******************************************************************
  // Core 0
  // *******************************************************************
  if (mhartid == 0) {   
    #ifdef FPGA_EMULATION
    uint32_t baud_rate = 9600;
    uint32_t test_freq = 100000000;
    #else
    set_flls();
    uint32_t baud_rate = 115200;
    uint32_t test_freq = 50000000;
    #endif
    uart_set_cfg(0,(test_freq/baud_rate)>>4);

    // Partition the cache.
    write_32b(0x50 + 0x10401000, 0xFFFFFF00);
    write_32b(0x54 + 0x10401000, 0xFFFF00FF);
    write_32b(0x58 + 0x10401000, 0xFF00FFFF);
    write_32b(0x5c + 0x10401000, 0x00FFFFFF);

    // Wait before running the program.
    uint32_t var;
    volatile uint64_t *tarray = (uint64_t*) 0x83000000;
    for (int a_idx = START_NONCUA; a_idx < LEN_NONCUA / 1024; a_idx +=JUMP_NONCUA) {
      printf(".");
      #ifdef INTF_RD
        asm volatile (
          "ld   %0, 0(%1)\n"
          : "=r"(var)
          : "r"(tarray - a_idx)
        );
      #elif defined(INTF_WR)
        var = a_idx;
        asm volatile (
          "sd   %0, 0(%1)\n"
          :: "r"(var),
              "r"(tarray - a_idx)
        );
      #endif
    }

  asm volatile ("fence");
  asm volatile ("here:");
  uint32_t volatile loop_tar = 0xFFFF;
  uint32_t volatile loop_var = 0;
  // asm volatile ("li %0,0xFFFFFFFFFFFFFFFF" : "=r"(loop_tar));
  // asm volatile ("addi %0,%0,42" : "+r"(loop_var));

  while (loop_var < loop_tar) {
    printf(".");
    loop_var += 1;
  }

  // asm volatile ("bne x1,x1,-4");
  
    // printf("Timer: %d \r\n", timer);

    #ifdef INTF_RD
      printf("Read contention, non-CUA array size: %d, stride: %d.\r\n", LEN_NONCUA, JUMP_NONCUA);      
    #endif

    #ifdef INTF_WR
      printf("Write contention, non-CUA array size: %d, stride: %d.\r\n", LEN_NONCUA, JUMP_NONCUA);      
    #endif

    // **************************************************************
    // Set up PMU.
    // **************************************************************
    uint32_t num_counter = 10;
                            // CORE 0
    uint32_t event_sel[] = {LLC_RD_RES_CORE_0,    // 0
                            LLC_WR_RES_CORE_0,    // 1
                            MEM_RD_RES_CORE_0,    // 2
                            MEM_WR_RES_CORE_0,    // 3
                            LLC_RD_RES_CORE_0,    // 4
                            LLC_WR_RES_CORE_0,    // 5
                            MEM_RD_RES_CORE_0,    // 6
                            MEM_WR_RES_CORE_0,    // 7
                            LLC_RD_RES_CORE_0,    // 8
                            LLC_WR_RES_CORE_0     // 9

                            };

                             // CORE 0
    uint32_t event_info[] = {CNT_MEM_ONLY,    // 0 
                             CNT_MEM_ONLY,    // 1
                             CNT_MEM_ONLY,    // 2
                             CNT_MEM_ONLY,    // 3
                             ADD_RESP_LAT,    // 4
                             ADD_RESP_LAT,    // 5
                             ADD_RESP_LAT,    // 6
                             ADD_RESP_LAT,    // 7
                             ADD_RESP_LAT,    // 8
                             ADD_RESP_LAT     // 9
                            };

                             // CORE 0    
    uint32_t print_info[] = {-1,-1,-1,-1,
                             -1,-1,-1,-1,
                             -1,-1
                            };

    write_32b_regs(EVENT_SEL_BASE_ADDR, num_counter, event_sel, COUNTER_BUNDLE_SIZE);
    write_32b_regs(EVENT_INFO_BASE_ADDR, num_counter, event_info, COUNTER_BUNDLE_SIZE);

    // Wipe out both SPMs.
    for (uint32_t ispm_addr=ISPM_BASE_ADDR; ispm_addr<DSPM_BASE_ADDR; ispm_addr=ispm_addr+4) {
      write_32b(ispm_addr, 0x0);
    }

    for (uint32_t dspm_addr=DSPM_BASE_ADDR; dspm_addr<DSPM_END_ADDR; dspm_addr=dspm_addr+4) {
      write_32b(dspm_addr, 0x0);
    }

    // **************************************************************
    // Set up PMU core.
    // **************************************************************
    uint32_t program[] = {
    0x33,
    0x10427137,
    0x2012423,
    0x2012623,
    0x4012023,
    0x4012223,
    0x104277b7,
    0x807a683,
    0x10427737,
    0x8470713,
    0x4d12423,
    0x72703,
    0x104286b7,
    0x20068693,
    0x4e12623,
    0x10428637,
    0x10404737,
    0x72483,
    0x104285b7,
    0xd12623,
    0x60060693,
    0x10428537,
    0xd12823,
    0x62058693,
    0x10428837,
    0xd12a23,
    0x64050693,
    0x10428437,
    0x104280b7,
    0x104283b7,
    0x104282b7,
    0x10428fb7,
    0x10428f37,
    0x10428eb7,
    0x10428e37,
    0x10428ab7,
    0x10428a37,
    0x10428d37,
    0x10428cb7,
    0x10428c37,
    0x10428bb7,
    0x10428b37,
    0x104299b7,
    0x10429937,
    0x80000737,
    0xd12c23,
    0x66080693,
    0x313,
    0x893,
    0x10100793,
    0xfff70713,
    0x440413,
    0x808093,
    0x1038393,
    0x71028293,
    0x720f8f93,
    0x730f0f13,
    0x740e8e93,
    0x750e0e13,
    0x300a8a93,
    0x400a0a13,
    0xd12e23,
    0x100d0d13,
    0x500c8c93,
    0x520c0c13,
    0x540b8b93,
    0x560b0b13,
    0x498993,
    0x890913,
    0x693,
    0x68687,
    0x2d12823,
    0x3012683,
    0xe6f6b3,
    0x2d12823,
    0x100693,
    0x68687,
    0x2d12c23,
    0x3812683,
    0xe6f6b3,
    0x2d12c23,
    0x400693,
    0x68687,
    0x2d12a23,
    0x500693,
    0x68687,
    0x2d12e23,
    0x3c12683,
    0x3412603,
    0x26d693,
    0xc686b3,
    0x4d12023,
    0x3812683,
    0x3012583,
    0x4812603,
    0x26d693,
    0xb686b3,
    0x2c686b3,
    0x4d12223,
    0x3012603,
    0x104286b7,
    0xc6a023,
    0x3812683,
    0xd42023,
    0x4012683,
    0xd0a023,
    0x4412683,
    0xd3a023,
    0x104046b7,
    0x6a683,
    0x2d12623,
    0x4012603,
    0x4412683,
    0xc6e663,
    0x10300693,
    0x1cd78463,
    0x4012603,
    0x4412683,
    0xc6f663,
    0x10100693,
    0x14d78463,
    0x693,
    0x613,
    0xc2a023,
    0xdfa023,
    0xff2023,
    0x4012683,
    0x4412603,
    0xd636b3,
    0x16b693,
    0xdea023,
    0x4012683,
    0x4412603,
    0xd636b3,
    0xde2023,
    0x4012603,
    0x4412683,
    0xc6e663,
    0x10300693,
    0x10d78463,
    0x4012603,
    0x4412683,
    0x6c6fe63,
    0x10100693,
    0x6d79a63,
    0x100613,
    0x20c02023,
    0x200793,
    0x20f02023,
    0x300793,
    0x20f02023,
    0x2c12683,
    0x10300793,
    0x6686b3,
    0x40968333,
    0xc12683,
    0x66a023,
    0xcaa023,
    0x2c12683,
    0x1012603,
    0xda2023,
    0x4012683,
    0xd62023,
    0x4412683,
    0x1412603,
    0xd62023,
    0x3012683,
    0x1812603,
    0xd62023,
    0x3812683,
    0x1c12603,
    0xd62023,
    0x2c12483,
    0x3012603,
    0x4c12683,
    0xe4d67ce3,
    0x2c12603,
    0x104296b7,
    0x10429537,
    0xc6a023,
    0xf9a023,
    0x1192023,
    0x652623,
    0x4012683,
    0x104295b7,
    0x10429637,
    0xd5a823,
    0x4412683,
    0x10429837,
    0xd62a23,
    0x3012d83,
    0x104296b7,
    0x1b82c23,
    0x3812d83,
    0x10429837,
    0x1b6ae23,
    0x3412d83,
    0x104296b7,
    0x3b82023,
    0x3c12803,
    0x306a223,
    0xdf1ff06f,
    0x10300693,
    0x613,
    0xebdff06f,
    0x100793,
    0x20f02423,
    0x200613,
    0x20c02423,
    0x300793,
    0x20f02423,
    0x2c12683,
    0x10100793,
    0x11686b3,
    0x409688b3,
    0x11d2023,
    0xcaa023,
    0x2c12683,
    0xda2023,
    0x4012683,
    0xdca023,
    0x4412683,
    0xdc2023,
    0x3012683,
    0xdba023,
    0x3812683,
    0xdb2023,
    0x2c12483,
    0xf25ff06f,
    0x4012683,
    0x4412603,
    0x693,
    0x10100613,
    0xe49ff06f
	};

    // Write it at `DSPM_BASE_ADDR + 0x80`.
    uint32_t dspm_val[] = {
      AVG_LAT                          // Average latency threshold      
    };

    uint32_t dspm_len = sizeof(dspm_val) / sizeof(dspm_val[0]);

    printf("Test Parameters\r\n");
    printf("Average latency threshold: %0d\r\n", dspm_val[0]);

    // Set up PMU Timer by writing to PMU Period Register.
    write_32b(PERIOD_ADDR, 0xFFFFFFFF);
    write_32b(PERIOD_ADDR + 0x4, 0xFFFFFFFF);

    uint32_t instruction;
    uint32_t program_size = sizeof(program) / sizeof(program[0]);

    // Set up PMU core and SPMs.
    write_32b(PMC_STATUS_ADDR, 1);
    error_count += test_spm(ISPM_BASE_ADDR, program_size, program);
    error_count += test_spm(DSPM_BASE_ADDR+0x80, dspm_len, dspm_val);

    printf("SPMs loaded. (%0d)\r\n", error_count);

    // **************************************************************
    // Run Test
    // **************************************************************
    uint32_t counter_rst[num_counter];
    uint32_t counter_init[num_counter];
    uint32_t counter_final[num_counter];
    uint32_t counter_data[num_counter+1];
    uint64_t start_cycle, end_cycle, cycle_taken;
    uint64_t result;
    uint32_t pmu_timer_lower[2], pmu_timer_upper[2];
    uint64_t pmu_timer_start, pmu_timer_end, pmu_timer_delta;

    // Set up synthetic benchmark array.
    volatile uint64_t *array = (uint64_t*) 0x83000000;
    for (uint32_t i=0; i < ARRAY_LEN; i++) {
      array[i] = my_rand(i*10);
    }

    // Start PMU Timer by writing to PMU Period Register.
    write_32b(PERIOD_ADDR, 0xFFFFFFFF);
    write_32b(PERIOD_ADDR + 0x4, 0xFFFFFFFF);

    printf("Test started!\r\n");

    // Reset all 32 counters.
    for (uint32_t i=0; i<32; i++) {
      counter_rst[i] = 0;
    }   
    write_32b_regs(COUNTER_BASE_ADDR, 32, counter_rst, COUNTER_BUNDLE_SIZE);
    // Read PMU counters.
    read_32b_regs(COUNTER_BASE_ADDR, num_counter, counter_init, COUNTER_BUNDLE_SIZE);
    // Start PMU core.
    #ifdef START_CORE
      write_32b(PMC_STATUS_ADDR, 0);
    #endif
    // Read cycle CSR.
    start_cycle = read_csr(cycle);
    // Read PMU Timer.
    pmu_timer_lower[0] = read_32b(TIMER_ADDR);
    pmu_timer_upper[0] = read_32b(TIMER_ADDR+0x4);  

    /// *****************
    /// This is the test.
    /// *****************
    uint32_t tmp;
    // for (uint64_t repeat=0; repeat<100; repeat++) {
    //   tmp = 0;
    //   for (uint64_t i=0; i < ARRAY_LEN-2; i++) {
    //     tmp = (array[i] % ARRAY_LEN)*(array[i+1] % ARRAY_LEN)*(array[i+2] % ARRAY_LEN) + repeat;
    //   }
    //   array[repeat] = tmp;
    // }

    for (uint64_t repeat=0; repeat<100; repeat++) {
      tmp = 0;
      for (uint64_t i=0; i < ARRAY_LEN-2; i++) {
        array[i] = repeat*repeat + i*i;
      }
    }

    /// *****************************
    /// Collect post-test statistics.
    /// *****************************
    // Read cycle CSR.
    end_cycle   = read_csr(cycle);
    cycle_taken = (end_cycle - start_cycle);
    // Stop PMU core.
    #ifdef START_CORE
      write_32b(PMC_STATUS_ADDR, 1);
    #endif
    // Read PMU counters.
    read_32b_regs(COUNTER_BASE_ADDR, num_counter, counter_final, COUNTER_BUNDLE_SIZE);
    // Read PMU Timer.
    pmu_timer_lower[1] = read_32b(TIMER_ADDR);
    pmu_timer_upper[1] = read_32b(TIMER_ADDR+0x4);

    // Calculate PMU Timer Delta.
    pmu_timer_start = (pmu_timer_upper[0] << 32) + pmu_timer_lower[0];
    pmu_timer_end   = (pmu_timer_upper[1] << 32) + pmu_timer_lower[1];
    pmu_timer_delta = pmu_timer_end - pmu_timer_start;

    // **************************************************************
    // Print Results.
    // **************************************************************
    // Print execution time.
    printf("Test over! Time taken (%x_%x to %x_%x): %x_%x\r\n", 
                (start_cycle >> 32), (start_cycle & 0xFFFFFFFF),
                (end_cycle >> 32),   (end_cycle & 0xFFFFFFFF),
                (cycle_taken >> 32), (cycle_taken & 0xFFFFFFFF));

    printf("PMU Timer(%x_%x to %x_%x): %x_%x\r\n",
                (pmu_timer_start >> 32), (pmu_timer_start & 0xFFFFFFFF),
                (pmu_timer_end >> 32),   (pmu_timer_end & 0xFFFFFFFF),
                (pmu_timer_delta >> 32), (pmu_timer_delta & 0xFFFFFFFF));

    // Write counter data to ISPM.
    for (uint32_t i=0; i<num_counter; i++) {
      counter_data[i] = (counter_final[i] & 0x7FFFFFFF)-(counter_init[i] & 0x7FFFFFFF); 
    }
    counter_data[num_counter] = 0x12345678;
	  write_32b_regs(ISPM_BASE_ADDR, num_counter+1, counter_data, 0x4);

    // Write PMU Timer Start at DSPM_BASE_ADDR + 0x68. 
    // Write PMU Timer Delta at DSPM_BASE_ADDR + 0x70.
    // Write CSR Cycle Delta at DSPM_BASE_ADDR + 0x78.
    write_32b(DSPM_BASE_ADDR+0x68, (pmu_timer_lower[0] & 0xFFFFFFFF));
    write_32b(DSPM_BASE_ADDR+0x6c, (pmu_timer_upper[0] >> 32));

    write_32b(DSPM_BASE_ADDR+0x70, (cycle_taken & 0xFFFFFFFF));
    write_32b(DSPM_BASE_ADDR+0x74, (cycle_taken >> 32));

    write_32b(DSPM_BASE_ADDR+0x78, (pmu_timer_delta & 0xFFFFFFFF));
    write_32b(DSPM_BASE_ADDR+0x7c, (pmu_timer_delta >> 32));

    write_32b_regs(ISPM_BASE_ADDR, 32, counter_data, 0x4);

    for (uint32_t i=0; i<num_counter; i++) {
        if (print_info[i] == -1) { 
            printf("%d:%d", i, counter_data[i]);
            uart_wait_tx_done();
        } else {
            if (counter_data[i] != 0) {
                printf("%d:%u", i, counter_data[i] / counter_data[print_info[i]]);
                uart_wait_tx_done();
            } else {
                printf("%d:%u", i, counter_data[i]);
                uart_wait_tx_done();
            }
        }
        printf("(%d,%d)",counter_init[i]&0x7FFFFFFF, counter_final[i]&0x7FFFFFFF);
        uart_wait_tx_done();
        if (i != num_counter-1) printf(",");
        else                    printf(".\r\n");
    }

    printf("CVA6-0 Over! %d\r\n", error_count);
    uart_wait_tx_done();

    end_test(mhartid);
    uart_wait_tx_done();

    while (1);

  // *******************************************************************
  // Core 1-3
  // *******************************************************************
  } else if (mhartid <= NUM_NON_CUA) {
    uint64_t var;
    volatile uint64_t *array = (uint64_t*)(uint64_t)(0x83000000 + mhartid * 0x01000000);
    asm volatile ("interfering_cores:");
    while (1) {       
      // 32'd1048576/2 = 0x0010_0000/2 elements.
      // Each array element is 64-bit, the array size is 0x0040_0000 = 4MB.
      // This will always exhaust the 2MB LLC.
      // Each core has 0x0100_0000 (16MB) memory.
      // It will iterate over 4MB array from top address to down.
      
      for (int a_idx = START_NONCUA; a_idx < LEN_NONCUA; a_idx +=JUMP_NONCUA) {
        #ifdef INTF_RD
          asm volatile (
            "ld   %0, 0(%1)\n"
            : "=r"(var)
            : "r"(array - a_idx)
          );
        #elif defined(INTF_WR)
          asm volatile (
            "sd   %0, 0(%1)\n"
            :: "r"(var),
               "r"(array - a_idx)
          );
        #endif
      }       
    }
  } else {
    // end_test(mhartid);
    uart_wait_tx_done();
    while (1);
  }
  
  return 0;
}