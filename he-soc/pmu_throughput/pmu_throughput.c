#include "encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "pmu_defines.h"
#include "pmu_test_func.c"
#include <inttypes.h>


// #define READ_HIT
#define READ_MISS
// #define READ_MISS_WB

// #define WRITE_HIT
// #define WRITE_MISS
// #define WRITE_MISS_WB

// #define INTF_READ_HIT
// #define INTF_READ_MISS
// #define INTF_READ_MISS_WB

// #define INTF_WRITE_HIT
// #define INTF_WRITE_MISS
#define INTF_WRITE_MISS_WB

#define STRIDE     8    // 8 x 8 = 64 bytes

#define DELAY 10000000

#define write_32b(addr, val_)  (*(volatile uint32_t *)(long)(addr) = val_)

void end_test(uint32_t mhartid){
  printf("Exiting: %0d.\r\n", mhartid);
}

void setup_counters(){
      uint32_t num_counter = 16;

      uint32_t event_sel[] = {LLC_RD_RES_CORE_0,    // 0
                              LLC_RD_RES_CORE_1,    // 1
                              LLC_RD_RES_CORE_2,    // 2
                              LLC_RD_RES_CORE_3,    // 3
                              LLC_WR_RES_CORE_0,    // 4
                              LLC_WR_RES_CORE_1,    // 5
                              LLC_WR_RES_CORE_2,    // 6
                              LLC_WR_RES_CORE_3,    // 7
                              MEM_RD_RES_CORE_0,    // 8
                              MEM_RD_RES_CORE_1,    // 9
                              MEM_RD_RES_CORE_2,    // 10
                              MEM_RD_RES_CORE_3,    // 11
                              MEM_WR_RES_CORE_0,    // 12
                              MEM_WR_RES_CORE_1,    // 13
                              MEM_WR_RES_CORE_2,    // 14
                              MEM_WR_RES_CORE_3};   // 15


    uint32_t event_info[] = {CNT_MEM_ONLY,    // 0 
                             CNT_MEM_ONLY,    // 1
                             CNT_MEM_ONLY,    // 2
                             CNT_MEM_ONLY,    // 3
                             CNT_MEM_ONLY,    // 4
                             CNT_MEM_ONLY,    // 5
                             CNT_MEM_ONLY,    // 6
                             CNT_MEM_ONLY,    // 7
                             CNT_MEM_ONLY,    // 8 
                             CNT_MEM_ONLY,    // 9
                             CNT_MEM_ONLY,    // 10
                             CNT_MEM_ONLY,    // 11
                             CNT_MEM_ONLY,    // 12
                             CNT_MEM_ONLY,    // 13
                             CNT_MEM_ONLY,    // 14
                             CNT_MEM_ONLY,};  // 15

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
static inline traverse_array_intf(volatile uint64_t* array_addr, uint32_t dummy_var, int start_index, int end_index, int reps);
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
  uint32_t a_len2_intf;       // 512 KB
  uint32_t var;

  #if defined (READ_HIT) || defined (WRITE_HIT) || defined (READ_MISS_WB) || defined (WRITE_MISS)
    // a_len2 = 61440;       // 480 KB
    a_len2 = 65536;       // 512 KB
  #elif defined (READ_MISS) || defined (WRITE_MISS_WB)
    a_len2 = 262144;        // 2MB
  #endif

  #if defined (INTF_READ_HIT) || defined (INTF_WRITE_HIT) || defined (INTF_READ_MISS_WB) || defined (INTF_WRITE_MISS)
    // a_len2 = 61440;       // 480 KB
    a_len2_intf = 65536;       // 512 KB
  #elif defined (INTF_READ_MISS) || defined (INTF_WRITE_MISS_WB)
    a_len2_intf = 262144;        // 2MB
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
    #if defined (INTF_READ_HIT) || defined (INTF_WRITE_HIT) || defined (INTF_READ_MISS_WB) || defined (INTF_WRITE_MISS)
      for (int a_idx = 0; a_idx < a_len2_intf; a_idx+=STRIDE) {
        #if defined (INTF_READ_HIT) || defined (INTF_WRITE_MISS)
          asm volatile (
            "ld   %0, 0(%1)\n"
            : "=r"(var)
            : "r"(array - a_idx)
          );
        #elif defined(INTF_WRITE_HIT) || defined (INTF_READ_MISS_WB)
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
        traverse_array(array, var, a_len2_intf, a_len2_intf+65536, 1);  
        // traverse_array(array, var, a_len2+21845, a_len2+65536, 1);  

      #else
          traverse_array_intf(array, var, 0, a_len2_intf, 100);
    #endif
    }
  }

  end_test(mhartid);
  printf("Time: %u\n", read_32b((void*)0x10427100));
// Core 0
printf("LLC read core 0: %u\n", read_32b((void*)0x10427120));
printf("LLC write core 0: %u\n", read_32b((void*)0x10427130));
printf("Memory read core 0: %u\n", read_32b((void*)0x10427140));
printf("Memory write core 0: %u\n", read_32b((void*)0x10427150));

// Core 1
printf("LLC read core 1: %u\n", read_32b((void*)0x10427124));
printf("LLC write core 1: %u\n", read_32b((void*)0x10427134));
printf("Memory read core 1: %u\n", read_32b((void*)0x10427144));
printf("Memory write core 1: %u\n", read_32b((void*)0x10427154));

// Core 2
printf("LLC read core 2: %u\n", read_32b((void*)0x10427128));
printf("LLC write core 2: %u\n", read_32b((void*)0x10427138));
printf("Memory read core 2: %u\n", read_32b((void*)0x10427148));
printf("Memory write core 2: %u\n", read_32b((void*)0x10427158));

// Core 3
printf("LLC read core 3: %u\n", read_32b((void*)0x1042712C));
printf("LLC write core 3: %u\n", read_32b((void*)0x1042713C));
printf("Memory read core 3: %u\n", read_32b((void*)0x1042714C));
printf("Memory write core 3: %u\n", read_32b((void*)0x1042715C));


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

static inline traverse_array_intf(volatile uint64_t* array_addr, uint32_t dummy_var, int start_index, int end_index, int reps){
  for (int j=0; j<reps; j++){
    for (int a_idx = start_index; a_idx < end_index; a_idx+=STRIDE) {
      #if defined(INTF_READ_HIT) || defined (INTF_READ_MISS) || defined (INTF_READ_MISS_WB)
        asm volatile (
          "ld   %0, 0(%1)\n"
          : "=r"(dummy_var)
          : "r"(array_addr - a_idx)
        );
      #elif defined(INTF_WRITE_HIT) || defined (INTF_WRITE_MISS_WB) || defined (INTF_WRITE_MISS)
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
0x104277b7,
0x807a603,
0x104047b7,
0x7a683,
0xd93,
0xd8d87,
0x800007b7,
0xfff7c793,
0xfdfdb3,
0x100d13,
0xd0d07,
0x200c93,
0xfd7d33,
0xc8c87,
0x300c13,
0xfcfcb3,
0xc0c07,
0x400b93,
0xfc7c33,
0xb8b87,
0x500b13,
0xfbfbb3,
0xb0b07,
0x600a93,
0xfb7b33,
0xa8a87,
0x700a13,
0xfafab3,
0xa0a07,
0x800993,
0xfa7a33,
0x98987,
0x900913,
0xf9f9b3,
0x90907,
0xa00713,
0xf97933,
0x70707,
0xf77733,
0xe12223,
0xb00713,
0x70707,
0xf77733,
0xe12423,
0xc00713,
0x70707,
0xf77733,
0xe12623,
0xd00713,
0x70707,
0xf77733,
0xe12823,
0xe00713,
0x70707,
0xf77733,
0xe12a23,
0xf00593,
0x58587,
0xf5f7b3,
0x10404737,
0xf12c23,
0x72783,
0x40d787b3,
0xfef67ce3,
0x493,
0x48487,
0x100413,
0x40407,
0x200093,
0x8087,
0x300393,
0x38387,
0x400293,
0x28287,
0x500f93,
0xf8f87,
0x600f13,
0xf0f07,
0x700e93,
0xe8e87,
0x800e13,
0xe0e07,
0x900313,
0x30307,
0xa00713,
0x70707,
0xb00893,
0xe12e23,
0x88887,
0xc00813,
0x80807,
0xd00513,
0x50507,
0xe00593,
0x58587,
0xf00613,
0x60607,
0x80000737,
0xfff74713,
0x104276b7,
0xe4f4b3,
0x10f6a023,
0xe47433,
0x41b484b3,
0x1296a023,
0xe0f0b3,
0x41a40433,
0x1286a223,
0x1c12783,
0xe3f3b3,
0x419080b3,
0x1216a423,
0xe2f2b3,
0x418383b3,
0x1276a623,
0xe37333,
0xefffb3,
0x41728bb3,
0x1376a823,
0xef7f33,
0x41230933,
0x416f8b33,
0xe7f333,
0x412783,
0x1366aa23,
0xeefeb3,
0x415f0ab3,
0x1356ac23,
0xee7e33,
0x414e8a33,
0x1346ae23,
0x40f30333,
0x413e09b3,
0xe8f7b3,
0x812883,
0x1536a023,
0x1526a223,
0x1466a423,
0x411787b3,
0x14f6a623,
0xc12783,
0xe87833,
0xe5f5b3,
0x40f80833,
0xe577b3,
0x1012503,
0x1506a823,
0x40a787b3,
0x14f6aa23,
0x1412783,
0x40f585b3,
0xe677b3,
0x1812703,
0x14b6ac23,
0x40e787b3,
0x14f6ae23,
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