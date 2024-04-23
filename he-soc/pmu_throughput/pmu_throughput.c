#include "encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "pmu_defines.h"
#include "pmu_test_func.c"

// #define READ_HIT
// #define READ_MISS
// #define READ_MISS_WB

// #define WRITE_HIT
// #define WRITE_MISS
// #define WRITE_MISS_WB

#define STRIDE     8    // 8 x 8 = 64 bytes

#define DELAY 10000000

#define write_32b(addr, val_)  (*(volatile uint32_t *)(long)(addr) = val_)

void end_test(uint32_t mhartid){
  printf("Exiting: %0d.\r\n", mhartid);
}

// *********************************************************************
// Main Function
// *********************************************************************
int main(int argc, char const *argv[]) {
  uint32_t mhartid;
  asm volatile (
    "csrr %0, 0xF14\n"
    : "=r" (mhartid)
  );

  uint32_t dspm_base_addr;
  uint32_t read_target;
  uint32_t error_count = 0;

  volatile uint64_t *array = (uint64_t*)(uint64_t)(0x83000000 + mhartid * 0x01000000);
  uint32_t a_len2 = 61440;
  uint32_t var;

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

    uint32_t ARRAY_SIZE = 0;
    printf("************\r\n");
    #if defined (READ_HIT)
      printf("READ_HIT case\n.");      
    #elif defined (READ_MISS)
      printf("READ_MISS case\n.");
    #elif defined (READ_MISS_WB)
      printf("READ_MISS_WB case\n.");
    #elif defined (WRITE_HIT)
      printf("WRITE_HIT case\n.");
    #elif defined (WRITE_MISS)
      printf("WRITE_MISS case\n.");
    #elif defined (WRITE_MISS_WB)
      printf("WRITE_MISS_WB case\n.");
    #endif
    
    #if defined (READ_HIT) || defined (WRITE_HIT)
      ARRAY_SIZE = 61440;     // 480 KB
      a_len2 = 61440;
    #elif defined (READ_MISS) || defined (READ_MISS_WB) || \
          defined (WRITE_MISS) || defined (WRITE_MISS_WB)
      ARRAY_SIZE = 262144;    // 2MB
      a_len2 = 262144;
    #endif

    // **************************************************************
    // Set up APMU counters.
    // **************************************************************
    uint32_t num_counter = 8;

    #if defined (READ_HIT) || defined (READ_MISS) || defined (READ_MISS_WB)
      uint32_t event_sel[] = {LLC_RD_RES_CORE_0,    // 0
                              LLC_RD_RES_CORE_1,    // 1
                              LLC_RD_RES_CORE_2,    // 2
                              LLC_RD_RES_CORE_3,    // 3
                              LLC_RD_RES_CORE_0,    // 4
                              LLC_RD_RES_CORE_1,    // 5
                              LLC_RD_RES_CORE_2,    // 6
                              LLC_RD_RES_CORE_3};   // 7
    #elif defined (WRITE_HIT) || defined (WRITE_MISS) || defined (WRITE_MISS_WB)
      uint32_t event_sel[] = {LLC_WR_RES_CORE_0,    // 0
                              LLC_WR_RES_CORE_1,    // 1
                              LLC_WR_RES_CORE_2,    // 2
                              LLC_WR_RES_CORE_3,    // 3
                              LLC_WR_RES_CORE_0,    // 4
                              LLC_WR_RES_CORE_1,    // 5
                              LLC_WR_RES_CORE_2,    // 6
                              LLC_WR_RES_CORE_3};   // 7
    #endif

    uint32_t event_info[] = {CNT_MEM_ONLY,    // 0 
                             CNT_MEM_ONLY,    // 1
                             CNT_MEM_ONLY,    // 2
                             CNT_MEM_ONLY,    // 3
                             ADD_RESP_LAT,    // 4
                             ADD_RESP_LAT,    // 5
                             ADD_RESP_LAT,    // 6
                             ADD_RESP_LAT};   // 7

    write_32b_regs(EVENT_SEL_BASE_ADDR, num_counter, event_sel, COUNTER_BUNDLE_SIZE);
    write_32b_regs(EVENT_INFO_BASE_ADDR, num_counter, event_info, COUNTER_BUNDLE_SIZE);

    // **************************************************************
    // Set up APMU core.
    // **************************************************************
    uint32_t program[] = {
    0x33,
    0x10427137,
    0x104277b7,
    0x807a603,
    0x104047b7,
    0x7a683,
    0xa13,
    0xa0a07,
    0x800007b7,
    0xfff78793,
    0xfa7a33,
    0x100993,
    0x98987,
    0x200913,
    0xf9f9b3,
    0x90907,
    0x300393,
    0xf97933,
    0x38387,
    0x400f13,
    0xf3f3b3,
    0xf0f07,
    0x500e93,
    0xff7f33,
    0xe8e87,
    0x600e13,
    0xfefeb3,
    0xe0e07,
    0x700313,
    0xfe7e33,
    0x30307,
    0x10404737,
    0xf37333,
    0x72783,
    0x40d787b3,
    0xfef67ce3,
    0x693,
    0x68687,
    0x100493,
    0x48487,
    0x200413,
    0x40407,
    0x300893,
    0x88887,
    0x400813,
    0x80807,
    0x500513,
    0x50507,
    0x600593,
    0x58587,
    0x700613,
    0x60607,
    0x80000737,
    0xfff70713,
    0xe6f6b3,
    0x104272b7,
    0x10f2a023,
    0x10427fb7,
    0x414687b3,
    0xe4f6b3,
    0x12ffa023,
    0x104270b7,
    0x413687b3,
    0xe476b3,
    0x12f0a223,
    0x412686b3,
    0xe8f7b3,
    0x104274b7,
    0x12d4a423,
    0x407787b3,
    0xe876b3,
    0x10427437,
    0x12f42623,
    0x41e686b3,
    0xe577b3,
    0x104278b7,
    0x12d8a823,
    0x41d787b3,
    0xe5f6b3,
    0x10427837,
    0x12f82a23,
    0x10427537,
    0x41c686b3,
    0xe677b3,
    0x104275b7,
    0x406787b3,
    0x12d52c23,
    0x12f5ae23,
    0x6f
	  };

    uint32_t dspm_val[] = {
      DELAY                       // Delay      
    };

    uint32_t dspm_len = sizeof(dspm_val) / sizeof(dspm_val[0]);

    // Set up PMU Timer by writing to PMU Period Register.
    write_32b(PERIOD_ADDR, 0xFFFFFFFF);
    write_32b(PERIOD_ADDR + 0x4, 0xFFFFFFFF);

    uint32_t instruction;
    uint32_t program_size = sizeof(program) / sizeof(program[0]);

    // Set up PMU core and SPMs.
    write_32b(DSPM_BASE_ADDR+0x2510,0);

    write_32b(PMC_STATUS_ADDR, 1);
    error_count += test_spm(ISPM_BASE_ADDR, program_size, program);
    error_count += test_spm(DSPM_BASE_ADDR+0x80, dspm_len, dspm_val);

    printf("SPMs and counters loaded. (%0d)\r\n", error_count);
    printf("************\r\n");

    #if defined (READ_HIT) || defined (WRITE_HIT) || defined(READ_MISS) || defined (WRITE_MISS)  
      for (int a_idx = 0; a_idx < a_len2; a_idx+=STRIDE) {
        #ifdef READ_HIT
          asm volatile (
            "ld   %0, 0(%1)\n"
            : "=r"(var)
            : "r"(array - a_idx)
          );
        #elif defined(WRITE_HIT)
          asm volatile (
            "sd   %0, 0(%1)\n"
            :: "r"(var),
              "r"(array - a_idx)
          );
        #elif defined(READ_MISS)
          asm volatile (
            "sd   %0, 0(%1)\n"
            :: "r"(var),
              "r"(array - a_idx)
          );
        #endif
      }
    #endif

    write_32b(DSPM_BASE_ADDR+0x2500,1);
    int barrier = 0;
    while (1) {
      barrier = 0;
      barrier += read_32b(DSPM_BASE_ADDR+0x2500);
      barrier += read_32b(DSPM_BASE_ADDR+0x2504);
      barrier += read_32b(DSPM_BASE_ADDR+0x2508);
      barrier += read_32b(DSPM_BASE_ADDR+0x250c);

      if (barrier == 4) {
        break;
      }
    }
    // printf("mhartid: %d", mhartid);

    // Start the APMU core.
    write_32b(PMC_STATUS_ADDR, 0);

    #if defined (READ_HIT)  || defined (WRITE_HIT) || \
        defined (READ_MISS) || defined (WRITE_MISS_WB)
      while (1) {
        for (int a_idx = 0; a_idx < a_len2; a_idx+=STRIDE) {
          #if defined(READ_HIT) || defined (READ_MISS)
            asm volatile (
              "ld   %0, 0(%1)\n"
              : "=r"(var)
              : "r"(array - a_idx)
            );
          #elif defined(WRITE_HIT) || defined (WRITE_MISS_WB)
            asm volatile (
              "sd   %0, 0(%1)\n"
              :: "r"(var),
                "r"(array - a_idx)
            );
          #endif
        }
      }
    #endif

    

  // *******************************************************************
  // Core 1-3
  // *******************************************************************
  } else {
 
    #if defined (READ_HIT) || defined (WRITE_HIT)
      for (int a_idx = 0; a_idx < a_len2; a_idx+=STRIDE) {
        #ifdef READ_HIT
          asm volatile (
            "ld   %0, 0(%1)\n"
            : "=r"(var)
            : "r"(array - a_idx)
          );
        #elif defined(WRITE_HIT)
          asm volatile (
            "sd   %0, 0(%1)\n"
            :: "r"(var),
              "r"(array - a_idx)
          );
        #endif
      }
    #endif


    write_32b(DSPM_BASE_ADDR+0x2500+mhartid*4,1);
    int barrier = 0;
    while (1) {
      barrier = 0;
      barrier += read_32b(DSPM_BASE_ADDR+0x2500);
      barrier += read_32b(DSPM_BASE_ADDR+0x2504);
      barrier += read_32b(DSPM_BASE_ADDR+0x2508);
      barrier += read_32b(DSPM_BASE_ADDR+0x250c);

      if (barrier == 4) {
        break;
      }
    }
    // printf("mhartid: %d", mhartid);

    #if defined (READ_HIT)  || defined (WRITE_HIT) || \
        defined (READ_MISS) || defined (WRITE_MISS_WB)
      while (1) {
        for (int a_idx = 0; a_idx < a_len2; a_idx+=STRIDE) {
          #if defined(READ_HIT) || defined (READ_MISS)
            asm volatile (
              "ld   %0, 0(%1)\n"
              : "=r"(var)
              : "r"(array - a_idx)
            );
          #elif defined(WRITE_HIT) || defined (WRITE_MISS_WB)
            asm volatile (
              "sd   %0, 0(%1)\n"
              :: "r"(var),
                "r"(array - a_idx)
            );
          #endif
        }
      }
    #endif
  }

  end_test(mhartid);
  printf("Time: %u\n", read_32b((void*)0x10427100));
  printf("Core 0: %u\n", read_32b((void*)0x10427120));
  printf("Core 1: %u\n", read_32b((void*)0x10427124));
  printf("Core 2: %u\n", read_32b((void*)0x10427128));
  printf("Core 3: %u\n", read_32b((void*)0x1042712C));
  while(1){}
  return 0;
}