#include "encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "pmu_defines.h"
#include "pmu_test_func.c"
#include <inttypes.h>


// #define READ_HIT
// #define READ_MISS
// #define READ_MISS_WB

// #define WRITE_HIT
#define WRITE_MISS
// #define WRITE_MISS_WB

#define STRIDE     8    // 8 x 8 = 64 bytes

#define DELAY 85500

#define write_32b(addr, val_)  (*(volatile uint32_t *)(long)(addr) = val_)

void end_test(uint32_t mhartid){
  printf("Exiting: %0d.\r\n", mhartid);
}

void setup_counters(){
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
}

void partition_cache(){
    write_32b(0x50 + 0x10401000, 0xFFFFFF00);
    write_32b(0x54 + 0x10401000, 0xFFFF00FF);
    write_32b(0x58 + 0x10401000, 0xFF00FFFF);
    write_32b(0x5c + 0x10401000, 0x00FFFFFF);
}

void print_init(){
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
}

static inline cpu_barrier_sync();
void setup_pmu(void);
static inline traverse_array(volatile uint64_t* array_addr, uint32_t dummy_var, int start_index, int end_index, int reps);

// *********************************************************************
// Main Function
// *********************************************************************
int main(int argc, char const *argv[]) {

  uint32_t mhartid;
  asm volatile (
    "csrr %0, 0xF14\n"
    : "=r" (mhartid)
  );

  volatile uint64_t *array = (uint64_t*)(uint64_t)(0x83000000 + mhartid * 0x01000000);
  uint32_t a_len2;       // 512 KB
  uint32_t var;

  #if defined (READ_HIT) || defined (WRITE_HIT) || defined (READ_MISS_WB) || defined (WRITE_MISS)
    // a_len2 = 61440;       // 480 KB
    a_len2 = 65536;       // 512 KB
  #elif defined (READ_MISS) || defined (WRITE_MISS_WB)
    a_len2 = 262144;        // 2MB
  #endif

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
    partition_cache();

    print_init();
    
    // **************************************************************
    // Set up APMU counters.
    // **************************************************************

    setup_counters();

    // **************************************************************
    // Set up APMU core.
    // **************************************************************

    setup_pmu();

    printf("************\r\n");

    // **************************************************************
    // Prime the cache
    // **************************************************************

    for (int a_idx = 0; a_idx < a_len2; a_idx+=STRIDE) {
      #if defined (READ_HIT) || defined(READ_MISS) || defined (WRITE_MISS)
        asm volatile (
          "ld   %0, 0(%1)\n"
          : "=r"(var)
          : "r"(array - a_idx)
        );
      #elif defined(WRITE_HIT) || defined(WRITE_MISS_WB) || defined (READ_MISS_WB)
        asm volatile (
          "sd   %0, 0(%1)\n"
          :: "r"(var),
            "r"(array - a_idx)
        );
      #endif
    }

    
    cpu_barrier_sync(mhartid);

    // Start the APMU core.
    #if !defined(READ_MISS_WB) && !defined(WRITE_MISS)
    write_32b(PMC_STATUS_ADDR, 0);
    #endif
    
    
    // uint64_t start_cycles;
    // asm volatile ("rdcycle %0" : "=r"(start_cycles));
    

    #if defined (READ_MISS_WB) || defined (WRITE_MISS)
      a_len2 = 65536; 
      // Now traverse from 512KB to 1024KB and perform:
      // For READ_MISS_WB perform reads
      // Fore WRITE_MISS perform write
      traverse_array(array, var, a_len2, a_len2+21845, 1);  
      write_32b(PMC_STATUS_ADDR, 0);
      traverse_array(array, var, a_len2+21845, a_len2+65536, 1);  

    #else
        traverse_array(array, var, 0, a_len2, 100);
    #endif

      
    // uint64_t end_cycles;
    // asm volatile ("rdcycle %0" : "=r"(end_cycles));


    // if(end_cycles - start_cycles >= UINT32_MAX){
    //   printf("print max\n");
    // }
    // printf("Test Ended! Cycles: %u\n", (unsigned long long)end_cycles - start_cycles);

  // *******************************************************************
  // Core 1-3
  // *******************************************************************
  } else {
 
    // **************************************************************
    // Prime the cache
    // **************************************************************
    #if defined (READ_HIT) || defined (WRITE_HIT) || defined (READ_MISS_WB) || defined (WRITE_MISS)
      for (int a_idx = 0; a_idx < a_len2; a_idx+=STRIDE) {
        #if defined (READ_HIT) || defined (WRITE_MISS)
          asm volatile (
            "ld   %0, 0(%1)\n"
            : "=r"(var)
            : "r"(array - a_idx)
          );
        #elif defined(WRITE_HIT) || defined (READ_MISS_WB)
          asm volatile (
            "sd   %0, 0(%1)\n"
            :: "r"(var),
              "r"(array - a_idx)
          );
        #endif
      }
    #endif


    cpu_barrier_sync(mhartid);
    // printf("mhartid: %d", mhartid);

    while (1) {
      // traverse_array(array, var, 0, a_len2, 1);
      #if defined (READ_MISS_WB) || defined (WRITE_MISS)
        a_len2 = 65536; 
        // Now traverse from 512KB to 1024KB and perform:
        // For READ_MISS_WB perform reads
        // Fore WRITE_MISS perform write
        traverse_array(array, var, a_len2, a_len2+65536, 1);  
        // traverse_array(array, var, a_len2+21845, a_len2+65536, 1);  

      #else
          traverse_array(array, var, 0, a_len2, 100);
    #endif
    }
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

static inline traverse_array(volatile uint64_t* array_addr, uint32_t dummy_var, int start_index, int end_index, int reps){
  for (int j=0; j<reps; j++){
    for (int a_idx = start_index; a_idx < end_index; a_idx+=STRIDE) {
      #if defined(READ_HIT) || defined (READ_MISS) || defined (READ_MISS_WB)
        asm volatile (
          "ld   %0, 0(%1)\n"
          : "=r"(dummy_var)
          : "r"(array_addr - a_idx)
        );
      #elif defined(WRITE_HIT) || defined (WRITE_MISS_WB) || defined (WRITE_MISS)
        asm volatile (
          "sd   %0, 0(%1)\n"
          :: "r"(dummy_var),
            "r"(array_addr - a_idx)
        );
      #endif
    }
  }
}

void setup_pmu(void){
    uint32_t error_count = 0;
      uint32_t program[] = {
    0x33,
    0x10427137,
    0x104287b7,
    0x807a703,
    0xc13,
    0x80000737,
    0x847a603,
    0xfff74713,
    0x887a303,
    0x100b93,
    0x8c7a883,
    0x800b13,
    0x907a803,
    0x900a93,
    0x947a503,
    0x200a13,
    0x793,
    0x300993,
    0xa00913,
    0xb00493,
    0x400413,
    0x500093,
    0xc00393,
    0xd00293,
    0x600f93,
    0x700f13,
    0xe00e93,
    0xf00e13,
    0xc0687,
    0xd12023,
    0x12683,
    0xe6f6b3,
    0xd12023,
    0xb8687,
    0xd12223,
    0x412683,
    0xe6f6b3,
    0xd12223,
    0xb0687,
    0xd12423,
    0x812683,
    0xe6f6b3,
    0xd12423,
    0xa8687,
    0xd12623,
    0xc12583,
    0xb010c93,
    0x279693,
    0xe5f5b3,
    0xb12623,
    0xdc86b3,
    0xf606a583,
    0x12583,
    0x412d83,
    0xc12d03,
    0x812c83,
    0x26585b3,
    0x31d8db3,
    0x2ad0d33,
    0x1b585b3,
    0x30c8cb3,
    0x1a585b3,
    0x19585b3,
    0xf6b6a023,
    0xa0587,
    0xb12023,
    0x12583,
    0xe5f5b3,
    0xb12023,
    0x98587,
    0xb12223,
    0x412583,
    0xe5f5b3,
    0xb12223,
    0x90587,
    0xb12423,
    0x812583,
    0xe5f5b3,
    0xb12423,
    0x48587,
    0xb12623,
    0xc12583,
    0xe5f5b3,
    0xb12623,
    0xf886a583,
    0x12583,
    0x412d83,
    0xc12d03,
    0x812c83,
    0x26585b3,
    0x31d8db3,
    0x2ad0d33,
    0x1b585b3,
    0x30c8cb3,
    0x1a585b3,
    0x19585b3,
    0xf8b6a423,
    0x40587,
    0xb12023,
    0x12583,
    0xe5f5b3,
    0xb12023,
    0x8587,
    0xb12223,
    0x412583,
    0xe5f5b3,
    0xb12223,
    0x38587,
    0xb12423,
    0x812583,
    0xe5f5b3,
    0xb12423,
    0x28587,
    0xb12623,
    0xc12583,
    0xe5f5b3,
    0xb12623,
    0xfb06a583,
    0x12583,
    0x412d83,
    0xc12d03,
    0x812c83,
    0x26585b3,
    0x31d8db3,
    0x2ad0d33,
    0x1b585b3,
    0x30c8cb3,
    0x1a585b3,
    0x19585b3,
    0xfab6a823,
    0xf8587,
    0xb12023,
    0x12583,
    0xe5f5b3,
    0xb12023,
    0xf0587,
    0xb12223,
    0x412583,
    0xe5f5b3,
    0xb12223,
    0xe8587,
    0xb12423,
    0x812583,
    0xe5f5b3,
    0xb12423,
    0xe0587,
    0xb12623,
    0xc12583,
    0x178793,
    0xe5f5b3,
    0xb12623,
    0xfd86a583,
    0x12583,
    0x412d83,
    0xc12d03,
    0x812c83,
    0x26585b3,
    0x31d8db3,
    0x2ad0d33,
    0x1b585b3,
    0x30c8cb3,
    0x1a585b3,
    0x19585b3,
    0xfcb6ac23,
    0xdec7e0e3,
    0x40c787b3,
    0xdcc7ece3,
    0x40c787b3,
    0xfec7fae3,
    0xdcdff06f
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
}

static inline cpu_barrier_sync(int mhartid){
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
}