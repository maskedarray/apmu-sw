#include "encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "pmu_defines.h"
#include "pmu_test_func.c"
#include <inttypes.h>
#include "../../apmu_core_lib/pmu_ispm.h"


// #define READ_HIT
#define READ_MISS
// #define READ_MISS_WB

// #define WRITE_HIT
// #define WRITE_MISS
// #define WRITE_MISS_WB

#define STRIDE     8    // 8 x 8 = 64 bytes

#define DELAY 1000  // actual computed is 432

#define write_32b(addr, val_)  (*(volatile uint32_t *)(long)(addr) = val_)

void end_test(uint32_t mhartid){
  printf("Exiting: %0d.\r\n", mhartid);
}

void setup_counters(){
      uint32_t num_counter = 16;

      uint32_t event_sel[] = {LLC_RD_RES_CORE_0,    // 0
                              LLC_WR_RES_CORE_0,
                              LLC_RD_RES_CORE_1,    // 1
                              LLC_WR_RES_CORE_1,
                              LLC_RD_RES_CORE_2,    // 2
                              LLC_WR_RES_CORE_2,
                              LLC_RD_RES_CORE_3,    // 3
                              LLC_WR_RES_CORE_3,
                              MEM_RD_RES_CORE_0,    // 0
                              MEM_WR_RES_CORE_0,
                              MEM_RD_RES_CORE_1,    // 1
                              MEM_WR_RES_CORE_1,
                              MEM_RD_RES_CORE_2,    // 2
                              MEM_WR_RES_CORE_2,
                              MEM_RD_RES_CORE_3,    // 3
                              MEM_WR_RES_CORE_3};   

    // TODO: change everything to cnt mem only
    uint32_t event_info[] = {CNT_MEM_ONLY,    // 0 
                             CNT_MEM_ONLY,    // 1
                             CNT_MEM_ONLY,    // 2
                             CNT_MEM_ONLY,    // 3
                             CNT_MEM_ONLY,    // 4
                             CNT_MEM_ONLY,    // 5
                             CNT_MEM_ONLY,    // 6
                             CNT_MEM_ONLY,    // 7
                             CNT_MEM_ONLY,    // 0 
                             CNT_MEM_ONLY,    // 1
                             CNT_MEM_ONLY,    // 2
                             CNT_MEM_ONLY,    // 3
                             CNT_MEM_ONLY,    // 4
                             CNT_MEM_ONLY,    // 5
                             CNT_MEM_ONLY,    // 6
                             CNT_MEM_ONLY};   // 7

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
  // Delay to close openocd
  // *******************************************************************
  {
    int start_delay = 0;
    asm volatile ("rdcycle %0" : "=r"(start_delay));
    int end_delay = 0;
    asm volatile ("rdcycle %0" : "=r"(end_delay));
    while(end_delay - start_delay < 500000000){
      asm volatile ("rdcycle %0" : "=r"(end_delay));
    }
  }
  

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

    printf("starting pmu\n");
    cpu_barrier_sync(mhartid);
    
    // **************************************************************
    // Start APMU Core
    // **************************************************************

    #if !defined(READ_MISS_WB) && !defined(WRITE_MISS)
    write_32b(PMC_STATUS_ADDR, 0);
    #endif
    printf("pmu started\n");
    
    // **************************************************************
    // Start Local Timer Based on Cpu Cycles
    // **************************************************************

    uint64_t start_cycles;
    asm volatile ("rdcycle %0" : "=r"(start_cycles));
    
    // **************************************************************
    // Start Traversing Over Array
    // **************************************************************

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

    // **************************************************************
    // End Local Timer Based on Cpu Cycles
    // **************************************************************

    uint64_t end_cycles;
    asm volatile ("rdcycle %0" : "=r"(end_cycles));

    volatile int unsigned pmu_done    = 0x99;
    // test_spm(DSPM_BASE_ADDR + 0x3000, sizeof(pmu_done), pmu_done);  //PMU end
    write_32b(DSPM_BASE_ADDR + 0x10AC, pmu_done);

    asm volatile ("fence \n");

    printf("Test Ended! Cycles: %u\n", (unsigned long long)end_cycles - start_cycles);

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
          traverse_array(array, var, 0, a_len2, 1000);
    #endif
    }
    while(1){}
  }

  end_test(mhartid);
    {
    int start_delay = 0;
    asm volatile ("rdcycle %0" : "=r"(start_delay));
    int end_delay = 0;
    asm volatile ("rdcycle %0" : "=r"(end_delay));
    while(end_delay - start_delay < 100000000){
      asm volatile ("rdcycle %0" : "=r"(end_delay));
    }
  }
  printf("PMU Start(0x11): %x\n",         read_32b((void*)0x104280A0));
  printf("PMU End Sent(0xf0f0): %x\n",    read_32b((void*)0x104280AC));
  printf("PMU End Received(0x55): %x\n",  read_32b((void*)0x104280C0));
  printf("PMU Loop Counts: %x\n",         read_32b((void*)0x104280A4));
  printf("PMU SPV_0: %x\n",               read_32b((void*)0x104280B0));
  printf("PMU val_0: %x\n",               read_32b((void*)0x104280B4));
  printf("PMU SPV_1: %x\n",               read_32b((void*)0x104280B8));
  printf("PMU val_1: %x\n",               read_32b((void*)0x104280BC));
  printf("PMU SPV_2: %x\n",               read_32b((void*)0x104280E0));
  printf("PMU val_2: %x\n",               read_32b((void*)0x104280E4));
  printf("PMU SPV_3: %x\n",               read_32b((void*)0x104280E8));
  printf("PMU val_3: %x\n",               read_32b((void*)0x104280EC));
  printf("PMU LLC RD0: %x\n",             read_32b((void*)0x10406000));
  printf("PMU LLC WR0: %x\n",             read_32b((void*)0x10407000));
  printf("PMU MEM RD0: %x\n",             read_32b((void*)0x1040E000));
  printf("PMU MEM WR0: %x\n",             read_32b((void*)0x1040F000));
  printf("Core0 Halt Count: %x\n",        read_32b((void*)0x104280D0));

  // printf("Core 0: %u\n", read_32b((void*)0x10427120));
  // printf("Core 1: %u\n", read_32b((void*)0x10427124));
  // printf("Core 2: %u\n", read_32b((void*)0x10427128));
  // printf("Core 3: %u\n", read_32b((void*)0x1042712C));
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
ISPM_DATA
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
    error_count += test_spm(DSPM_BASE_ADDR+0x1098, dspm_len, dspm_val);

    int unsigned ai_budget    = 200;
    int unsigned window_size  = 3;
    int unsigned weight_r     = 7;
    int unsigned weight_w     = 13;
    int unsigned weight_r_mem = 12;
    int unsigned weight_w_mem = 6;

    // test_spm(DSPM_BASE_ADDR + 0x1080, sizeof(ai_budget), ai_budget);  // Ai_Budget
    // test_spm(DSPM_BASE_ADDR + 0x1084, sizeof(window_size), window_size);  // WindowSize
    // test_spm(DSPM_BASE_ADDR + 0x1088, sizeof(weight_r), weight_r);  // WeightRead
    // test_spm(DSPM_BASE_ADDR + 0x108c, sizeof(weight_w), weight_w);  // WeightWrite
    // test_spm(DSPM_BASE_ADDR + 0x1090, sizeof(weight_r_mem), weight_r_mem);  // WeightRead (memory)
    // test_spm(DSPM_BASE_ADDR + 0x1094, sizeof(weight_w_mem), weight_w_mem);  // WeightWrite (memory)
    write_32b(DSPM_BASE_ADDR + 0x1080, ai_budget);  // Ai_Budget
    write_32b(DSPM_BASE_ADDR + 0x1084, window_size);  // WindowSize
    write_32b(DSPM_BASE_ADDR + 0x1088, weight_r);  // WeightRead
    write_32b(DSPM_BASE_ADDR + 0x108c, weight_w);  // WeightWrite
    write_32b(DSPM_BASE_ADDR + 0x1090, weight_r_mem);  // WeightRead (memory)
    write_32b(DSPM_BASE_ADDR + 0x1094, weight_w_mem);  // WeightWrite (memory)


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