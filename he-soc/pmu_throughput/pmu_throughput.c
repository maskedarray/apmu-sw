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
    
    // Start the APMU core.
    #if !defined(READ_MISS_WB) && !defined(WRITE_MISS)
    write_32b(PMC_STATUS_ADDR, 0);
    #endif
    printf("pmu started\n");
    
    uint64_t start_cycles;
    asm volatile ("rdcycle %0" : "=r"(start_cycles));
    

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

      
    uint64_t end_cycles;
    asm volatile ("rdcycle %0" : "=r"(end_cycles));

    test_spm(DSPM_BASE_ADDR + 0x10AC, sizeof(int), 0x0000f0f0);  //PMU end
    if(end_cycles - start_cycles >= UINT32_MAX){
      printf("print max\n");
    }
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
          // traverse_array(array, var, 0, a_len2, 1000);
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
  printf("PMU RD RES0: %x\n",             read_32b((void*)0x10406000));
  printf("PMU RD RES1: %x\n",             read_32b((void*)0x10408000));
  printf("PMU RD RES2: %x\n",             read_32b((void*)0x1040A000));
  printf("PMU RD RES3: %x\n",             read_32b((void*)0x1040C000));

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
0x10427137,
0x104287b7,
0x987a583,
0x807aa83,
0x847a703,
0x887a383,
0x8c7a283,
0x907af83,
0x947af03,
0x4012023,
0x4012223,
0x4012423,
0x4012623,
0x4012823,
0x4012a23,
0x4012c23,
0x4012e23,
0x6012023,
0x6012223,
0x6012423,
0x6012623,
0x6012823,
0x6012a23,
0x6012c23,
0x6012e23,
0x8012023,
0x8012223,
0x8012423,
0x8012623,
0x8012823,
0x8012a23,
0x8012c23,
0xe12c23,
0x8012e23,
0xa012023,
0xa012223,
0xa012423,
0xa012623,
0xa012823,
0xa012a23,
0xa012c23,
0xa012e23,
0xc012023,
0xc012223,
0xc012423,
0xc012623,
0xc012823,
0xc012a23,
0xc012c23,
0x170e13,
0x2ea8b33,
0xc012e23,
0x1100713,
0xae7a023,
0x12f00793,
0xf12a23,
0xf12823,
0xf12623,
0xf12423,
0xf7b7,
0x80000737,
0xf078793,
0xe0993,
0xe0913,
0xe0493,
0xe0413,
0x93,
0x2012623,
0x2012423,
0x2012223,
0x2012023,
0x313,
0x10404637,
0xfff74713,
0x100c13,
0x10428eb7,
0x6500a13,
0x12f00b93,
0xf12e23,
0x62783,
0x693,
0x68687,
0x2d12823,
0x3012683,
0xe6f6b3,
0x2d12823,
0xc0687,
0x2d12a23,
0x3412683,
0xe6f6b3,
0x2d12a23,
0x800693,
0x68687,
0x2d12c23,
0x3812683,
0xe6f6b3,
0x2d12c23,
0x900693,
0x68687,
0x2d12e23,
0x3c12683,
0xe6f6b3,
0x2d12e23,
0x37c47e63,
0x3540833,
0x2012683,
0x231513,
0x140413,
0xd80833,
0x3012d03,
0x3412683,
0x3c12c83,
0x3812883,
0x27d0d33,
0x25686b3,
0x3ec8cb3,
0xdd06b3,
0x3f888b3,
0x19686b3,
0x11688b3,
0xb1eaa23,
0x411806b3,
0x3606ca63,
0xe010693,
0xa686b3,
0xf716a023,
0x812683,
0x1469863,
0x20002423,
0x12f00693,
0xd12423,
0x200693,
0x68687,
0x2d12823,
0x3012683,
0xe6f6b3,
0x2d12823,
0x300693,
0x68687,
0x2d12a23,
0x3412683,
0xe6f6b3,
0x2d12a23,
0xa00693,
0x68687,
0x2d12c23,
0x3812683,
0xe6f6b3,
0x2d12c23,
0xb00693,
0x68687,
0x2d12e23,
0x3c12683,
0xe6f6b3,
0x2d12e23,
0x29c4fc63,
0x35488b3,
0x2412683,
0x148493,
0xd888b3,
0x3012d83,
0x3412683,
0x3c12d03,
0x3812c83,
0x27d8db3,
0x25686b3,
0x3ed0d33,
0xdd86b3,
0x3fc8cb3,
0x1a686b3,
0x1968cb3,
0x419886b3,
0x2806c063,
0xe010693,
0xa686b3,
0xf996a423,
0xc12683,
0x1469863,
0x21802423,
0x12f00693,
0xd12623,
0x400693,
0x68687,
0x2d12823,
0x3012683,
0xe6f6b3,
0x2d12823,
0x500693,
0x68687,
0x2d12a23,
0x3412683,
0xe6f6b3,
0x2d12a23,
0xc00693,
0x68687,
0x2d12c23,
0x3812683,
0xe6f6b3,
0x2d12c23,
0xd00693,
0x68687,
0x2d12e23,
0x3c12683,
0xe6f6b3,
0x2d12e23,
0x1bc97e63,
0x35908b3,
0x2812683,
0x190913,
0xd888b3,
0x3012d83,
0x3412683,
0x3c12d03,
0x3812c83,
0x27d8db3,
0x25686b3,
0x3ed0d33,
0xdd86b3,
0x3fc8cb3,
0x1a686b3,
0x1968cb3,
0x419886b3,
0x2406c063,
0xe010693,
0xa686b3,
0xfb96a823,
0x1012683,
0x1469a63,
0x200693,
0x20d02423,
0x12f00693,
0xd12823,
0x600693,
0x68687,
0x2d12823,
0x3012683,
0xe6f6b3,
0x2d12823,
0x700693,
0x68687,
0x2d12a23,
0x3412683,
0xe6f6b3,
0x2d12a23,
0xe00693,
0x68687,
0x2d12c23,
0x3812683,
0xe6f6b3,
0x2d12c23,
0xf00693,
0x68687,
0x2d12e23,
0x3c12683,
0xe6f6b3,
0x2d12e23,
0xdc9fe63,
0x35988b3,
0x2c12683,
0x198993,
0xd888b3,
0x3012d83,
0x3412683,
0x3c12d03,
0x3812c83,
0x27d8db3,
0x25686b3,
0x3ed0d33,
0xdd86b3,
0x3fc8cb3,
0x1a686b3,
0x1968cb3,
0x419886b3,
0x1406c263,
0xe010693,
0xa68533,
0x1412683,
0xfd952c23,
0x1469a63,
0x300693,
0x20d02423,
0x12f00693,
0xd12a23,
0x1812503,
0x62683,
0x130313,
0x2a37333,
0x40f68533,
0xf6f663,
0xfff68693,
0x40f68533,
0xb57e63,
0x62683,
0xfff68893,
0x40f68533,
0xfef6f8e3,
0x40f88533,
0xfeb566e3,
0x108093,
0xa1ea223,
0xb0ea823,
0xacea783,
0x1c12683,
0xc8d790e3,
0x104286b7,
0x5500713,
0xc06a783,
0xe78463,
0xce6a023,
0x6f,
0xff1ff06f,
0xe010693,
0xa686b3,
0xfd86a883,
0x11b08b3,
0xf29ff06f,
0xe010693,
0xa686b3,
0xfb06a883,
0x11b08b3,
0xe49ff06f,
0xe010693,
0xa686b3,
0xf886a883,
0x11b08b3,
0xd6dff06f,
0x231513,
0xe010693,
0xa686b3,
0xf606a803,
0x1680833,
0xc89ff06f,
0xe010693,
0xa686b3,
0xf916a423,
0xc12683,
0x3112223,
0x100493,
0xd97696e3,
0x21802023,
0x6500693,
0xd12623,
0xd7dff06f,
0xe010693,
0xa686b3,
0xf706a023,
0x812683,
0x3012023,
0x100413,
0xc9769ce3,
0x20002023,
0x6500693,
0xd12423,
0xc89ff06f,
0xe010693,
0xa68533,
0x1412683,
0xfd152c23,
0x3112623,
0x100993,
0xed7696e3,
0x300693,
0x20d02023,
0x6500693,
0xd12a23,
0xeb9ff06f,
0xe010693,
0xa686b3,
0xfb16a823,
0x1012683,
0x3112423,
0x100913,
0xdd7698e3,
0x200693,
0x20d02023,
0x6500693,
0xd12823,
0xdbdff06f
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

    test_spm(DSPM_BASE_ADDR + 0x1080, sizeof(ai_budget), ai_budget);  // Ai_Budget
    test_spm(DSPM_BASE_ADDR + 0x1084, sizeof(window_size), window_size);  // WindowSize
    test_spm(DSPM_BASE_ADDR + 0x1088, sizeof(weight_r), weight_r);  // WeightRead
    test_spm(DSPM_BASE_ADDR + 0x108c, sizeof(weight_w), weight_w);  // WeightWrite
    test_spm(DSPM_BASE_ADDR + 0x1090, sizeof(weight_r_mem), weight_r_mem);  // WeightRead (memory)
    test_spm(DSPM_BASE_ADDR + 0x1094, sizeof(weight_w_mem), weight_w_mem);  // WeightWrite (memory)

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