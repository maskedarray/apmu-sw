#include "apmu_defines.h"
#include "apmu_core_lib.h"

#define DEBUG_HALT      0x200
#define DEBUG_RESUME    0x208

#define TIMER_ADDR      0x10404000
#define DSPM_BASE_ADDR  0x10427000

#define debug_halt(core_id)     write_32b(DEBUG_HALT, core_id)
#define debug_resume(core_id)   write_32b(DEBUG_RESUME, core_id)

#define HALT            101
#define RESUME          303

void throughput_measure() {
    asm volatile ("fence");
    asm volatile ("li sp,272789504");

    int unsigned start_time;
    int unsigned end_time;

    int unsigned start_counter_0, end_counter_0;
    int unsigned start_counter_1, end_counter_1;
    int unsigned start_counter_2, end_counter_2;
    int unsigned start_counter_3, end_counter_3;

    int unsigned start_lat_counter_0, end_lat_counter_0;
    int unsigned start_lat_counter_1, end_lat_counter_1;
    int unsigned start_lat_counter_2, end_lat_counter_2;
    int unsigned start_lat_counter_3, end_lat_counter_3;

    int counter_idx;

    volatile int *init = (int*)(DSPM_BASE_ADDR + 0x80);
    int unsigned DELAY = *init;

    start_time = read_32b(TIMER_ADDR);

    // Read initial value of counter.
    // C0
    counter_idx = 0;
    counter_read(start_counter_0, counter_idx);
    start_counter_0 = start_counter_0 & 0x7FFFFFFF;
    // C1
    counter_idx = 1;
    counter_read(start_counter_1, counter_idx);
    start_counter_1 = start_counter_1 & 0x7FFFFFFF;
    // C2
    counter_idx = 2;
    counter_read(start_counter_2, counter_idx);
    start_counter_2 = start_counter_2 & 0x7FFFFFFF;
    // C3
    counter_idx = 3;
    counter_read(start_counter_3, counter_idx);
    start_counter_3 = start_counter_3 & 0x7FFFFFFF;

    // // C4
    // counter_idx = 4;
    // counter_read(start_lat_counter_0, counter_idx);
    // start_lat_counter_0 = start_lat_counter_0 & 0x7FFFFFFF;
    // // C5
    // counter_idx = 5;
    // counter_read(start_lat_counter_1, counter_idx);
    // start_lat_counter_1 = start_lat_counter_1 & 0x7FFFFFFF;
    // // C6
    // counter_idx = 6;
    // counter_read(start_lat_counter_2, counter_idx);
    // start_lat_counter_2 = start_lat_counter_2 & 0x7FFFFFFF;
    // // C7
    // counter_idx = 7;
    // counter_read(start_lat_counter_3, counter_idx);
    // start_lat_counter_3 = start_lat_counter_3 & 0x7FFFFFFF;
    
    // Wait until DELAY cycles have elapsed.
    while (1) {
        end_time = read_32b(TIMER_ADDR);
        if ((end_time - start_time) > DELAY) {
            break;
        }
        // int unsigned done = read_32b(DSPM_BASE_ADDR+0x2510);
        // if (done){
        //     break;
        // }
    }
    
    // Read final value of counter.
    // C0
    counter_idx = 0;
    counter_read(end_counter_0, counter_idx);
    end_counter_0 = end_counter_0 & 0x7FFFFFFF;
    // C1
    counter_idx = 1;
    counter_read(end_counter_1, counter_idx);
    end_counter_1 = end_counter_1 & 0x7FFFFFFF;
    // C2
    counter_idx = 2;
    counter_read(end_counter_2, counter_idx);
    end_counter_2 = end_counter_2 & 0x7FFFFFFF;
    // C3
    counter_idx = 3;
    counter_read(end_counter_3, counter_idx);
    end_counter_3 = end_counter_3 & 0x7FFFFFFF;

    // // C4
    // counter_idx = 4;
    // counter_read(end_lat_counter_0, counter_idx);
    // end_lat_counter_0 = end_lat_counter_0 & 0x7FFFFFFF;
    // // C5
    // counter_idx = 5;
    // counter_read(end_lat_counter_1, counter_idx);
    // end_lat_counter_1 = end_lat_counter_1 & 0x7FFFFFFF;
    // // C6
    // counter_idx = 6;
    // counter_read(end_lat_counter_2, counter_idx);
    // end_lat_counter_2 = end_lat_counter_2 & 0x7FFFFFFF;
    // // C7
    // counter_idx = 7;
    // counter_read(end_lat_counter_3, counter_idx);
    // end_lat_counter_3 = end_lat_counter_3 & 0x7FFFFFFF;

    // Write output to DSPM.
    write_32b(DSPM_BASE_ADDR+0x100, end_time-start_time);
    write_32b(DSPM_BASE_ADDR+0x120, end_counter_0-start_counter_0);
    write_32b(DSPM_BASE_ADDR+0x124, end_counter_1-start_counter_1);
    write_32b(DSPM_BASE_ADDR+0x128, end_counter_2-start_counter_2);
    write_32b(DSPM_BASE_ADDR+0x12c, end_counter_3-start_counter_3);

    // write_32b(DSPM_BASE_ADDR+0x130, end_lat_counter_0-start_lat_counter_0);
    // write_32b(DSPM_BASE_ADDR+0x134, end_lat_counter_1-start_lat_counter_1);
    // write_32b(DSPM_BASE_ADDR+0x138, end_lat_counter_2-start_lat_counter_2);
    // write_32b(DSPM_BASE_ADDR+0x13c, end_lat_counter_3-start_lat_counter_3);

    while (1);
}

// C0: Reads to LLC by CUA.
// C1: Writes to LLC by CUA.
// C2: Reads to memory by CUA.
// C3: Writes to memory by CUA.
// C4: Read request latency.
// C5: Write request latency.
void mempol() {
    asm volatile ("fence");
    asm volatile ("li sp,272789504");

    // Used to loop over PMU counters.
    int unsigned counter_idx = 0;
    // Used to loop over cores.
    int unsigned core_idx = 0;
    // Read and write requests from counters.
    volatile int unsigned cnt_n_read,  cnt_n_write, cnt_n_mem_read,  cnt_n_mem_write;

    // 0x1080: Ai_Budget
    volatile int *init = (int*)(DSPM_BASE_ADDR + 0x1080);
    int unsigned ai_budget = *init;
    // 0x1084: WindowSize
    init = (int*)(DSPM_BASE_ADDR + 0x1084);
    int unsigned window_size = *init;
    // 0x1088: WeightRead
    init = (int*)(DSPM_BASE_ADDR + 0x1088);
    int unsigned weight_r = *init;
    // 0x108c: WeightWrite
    init = (int*)(DSPM_BASE_ADDR + 0x108c);
    int unsigned weight_w = *init;
    // 0x1088: WeightRead
    init = (int*)(DSPM_BASE_ADDR + 0x1090);
    int unsigned weight_r_mem = *init;
    // 0x108c: WeightWrite
    init = (int*)(DSPM_BASE_ADDR + 0x1094);
    int unsigned weight_w_mem = *init;

    
    int unsigned i          = 0;
    int unsigned i_temp     = 0;
    int unsigned val        = 0;
    
    volatile int unsigned history_0[10];
    volatile int unsigned history_1[10];
    volatile int unsigned history_2[10];
    volatile int unsigned history_3[10];

    int unsigned t_lrt_0    = window_size;
    int unsigned t_lrt_1    = window_size;
    int unsigned t_lrt_2    = window_size;
    int unsigned t_lrt_3    = window_size;

    int unsigned spv_0      = 0;
    int unsigned spv_1      = 0;
    int unsigned spv_2      = 0;
    int unsigned spv_3      = 0;

    int unsigned spv_lrt_0  = 0;
    int unsigned spv_lrt_1  = 0;
    int unsigned spv_lrt_2  = 0;
    int unsigned spv_lrt_3  = 0;
    
    // All cores are resumed at start.
    int unsigned halt_status_0 = RESUME;
    int unsigned halt_status_1 = RESUME;
    int unsigned halt_status_2 = RESUME;
    int unsigned halt_status_3 = RESUME;

    while (1) {
        // ************************************
        // Core 0.
        // ************************************
        // Read necessary PMU counters.
        // C0 -- LLC Read
        counter_idx = 0;
        counter_read(cnt_n_read, counter_idx);
        cnt_n_read = cnt_n_read & 0x7FFFFFFF;

        // C1 -- LLC Write
        counter_idx = 1;
        counter_read(cnt_n_write, counter_idx);
        cnt_n_write = cnt_n_write & 0x7FFFFFFF;

        // C8 -- Mem Read
        counter_idx = 8;
        counter_read(cnt_n_mem_read, counter_idx);
        cnt_n_mem_read = cnt_n_mem_read & 0x7FFFFFFF;

        // C9 -- Mem Write
        counter_idx = 9;
        counter_read(cnt_n_mem_write, counter_idx);
        cnt_n_mem_write = cnt_n_mem_write & 0x7FFFFFFF;
        
        // Derive set point.
        if (t_lrt_0 < window_size) {
            spv_0     = spv_lrt_0 + t_lrt_0 * ai_budget;
            t_lrt_0   += 1;
        } else {
            spv_0 = history_0[i] + (window_size * ai_budget);
        }

        // TODO 
        // val = weight_r * cnt_n_read + weight_w * cnt_n_write;
        val = weight_r * cnt_n_read + weight_w * cnt_n_write + \
              weight_r_mem * cnt_n_mem_read + weight_w_mem * cnt_n_mem_write;

        // Over-budget.
        if ((spv_0 - val) < 0) {
            history_0[i] = spv_0;
            t_lrt_0      = 0;
            spv_lrt_0    = spv_0;
            // Halt core if resumed.
            if (halt_status_0 == RESUME) {
                core_idx = 0;
                write_32b(DEBUG_HALT, core_idx);
                halt_status_0 = HALT;
            }
        } else {
            history_0[i] = val;
            // Resume core if halted.
            if (halt_status_0 == HALT) {
                core_idx = 0;
                write_32b(DEBUG_RESUME, core_idx); 
                halt_status_0 = RESUME;
            }
        }

        // ************************************
        // Core 1.
        // ************************************
        // Read necessary PMU counters.
        // C2
        counter_idx = 2;
        counter_read(cnt_n_read, counter_idx);
        cnt_n_read = cnt_n_read & 0x7FFFFFFF;

        // C3
        counter_idx = 3;
        counter_read(cnt_n_write, counter_idx);
        cnt_n_write = cnt_n_write & 0x7FFFFFFF;

        // C10
        counter_idx = 10;
        counter_read(cnt_n_mem_read, counter_idx);
        cnt_n_mem_read = cnt_n_mem_read & 0x7FFFFFFF;

        // C11
        counter_idx = 11;
        counter_read(cnt_n_mem_write, counter_idx);
        cnt_n_mem_write = cnt_n_mem_write & 0x7FFFFFFF;
        
        // Derive set point.
        if (t_lrt_1 < window_size) {
            spv_1     = spv_lrt_1 + t_lrt_1 * ai_budget;
            t_lrt_1   += 1;
        } else {
            spv_1 = history_1[i] + (window_size * ai_budget);
        }

        // TODO 
        // val = weight_r * cnt_n_read + weight_w * cnt_n_write;
        val = weight_r * cnt_n_read + weight_w * cnt_n_write + \
              weight_r_mem * cnt_n_mem_read + weight_w_mem * cnt_n_mem_write;
        
        // Over-budget.
        if ((spv_1 - val) < 0) {
            history_1[i] = spv_1;
            t_lrt_1      = 0;
            spv_lrt_1    = spv_1;
            // Halt core if resumed.
            if (halt_status_1 == RESUME) {
                core_idx = 1;
                write_32b(DEBUG_HALT, core_idx);
                halt_status_1 = HALT;
            }
        } else {
            history_1[i] = val;
            // Resume core if halted.
            if (halt_status_1 == HALT) {
                core_idx = 1;
                write_32b(DEBUG_RESUME, core_idx); 
                halt_status_1 = RESUME;
            }
        }

        // ************************************
        // Core 2.
        // ************************************
        // Read necessary PMU counters.
        // C4
        counter_idx = 4;
        counter_read(cnt_n_read, counter_idx);
        cnt_n_read = cnt_n_read & 0x7FFFFFFF;

        // C5
        counter_idx = 5;
        counter_read(cnt_n_write, counter_idx);
        cnt_n_write = cnt_n_write & 0x7FFFFFFF;

        // C12
        counter_idx = 12;
        counter_read(cnt_n_mem_read, counter_idx);
        cnt_n_mem_read = cnt_n_mem_read & 0x7FFFFFFF;

        // C13
        counter_idx = 13;
        counter_read(cnt_n_mem_write, counter_idx);
        cnt_n_mem_write = cnt_n_mem_write & 0x7FFFFFFF;
        
        // Derive set point.
        if (t_lrt_2 < window_size) {
            spv_2     = spv_lrt_2 + t_lrt_2 * ai_budget;
            t_lrt_2   += 1;
        } else {
            spv_2 = history_2[i] + (window_size * ai_budget);
        }

        // TODO 
        // val = weight_r * cnt_n_read + weight_w * cnt_n_write;
        val = weight_r * cnt_n_read + weight_w * cnt_n_write + \
              weight_r_mem * cnt_n_mem_read + weight_w_mem * cnt_n_mem_write;
        
        // Over-budget.
        if ((spv_2 - val) < 0) {
            history_2[i] = spv_2;
            t_lrt_2      = 0;
            spv_lrt_2    = spv_2;
            // Halt core if resumed.
            if (halt_status_2 == RESUME) {
                core_idx = 2;
                write_32b(DEBUG_HALT, core_idx);
                halt_status_2 = HALT;
            }
        } else {
            history_2[i] = val;
            // Resume core if halted.
            if (halt_status_2 == HALT) {
                core_idx = 2;
                write_32b(DEBUG_RESUME, core_idx); 
                halt_status_2 = RESUME;
            }
        }

        // ************************************
        // Core 3.
        // ************************************
        // Read necessary PMU counters.
        // C6
        counter_idx = 6;
        counter_read(cnt_n_read, counter_idx);
        cnt_n_read = cnt_n_read & 0x7FFFFFFF;

        // C7
        counter_idx = 7;
        counter_read(cnt_n_write, counter_idx);
        cnt_n_write = cnt_n_write & 0x7FFFFFFF;

         // C14
        counter_idx = 14;
        counter_read(cnt_n_mem_read, counter_idx);
        cnt_n_mem_read = cnt_n_mem_read & 0x7FFFFFFF;

        // C15
        counter_idx = 15;
        counter_read(cnt_n_mem_write, counter_idx);
        cnt_n_mem_write = cnt_n_mem_write & 0x7FFFFFFF;
        
        // Derive set point.
        if (t_lrt_3 < window_size) {
            spv_3     = spv_lrt_3 + t_lrt_3 * ai_budget;
            t_lrt_3   += 1;
        } else {
            spv_3 = history_3[i] + (window_size * ai_budget);
        }

        // TODO 
        // val = weight_r * cnt_n_read + weight_w * cnt_n_write;
        val = weight_r * cnt_n_read + weight_w * cnt_n_write + \
              weight_r_mem * cnt_n_mem_read + weight_w_mem * cnt_n_mem_write;
        
        // Over-budget.
        if ((spv_3 - val) < 0) {
            history_3[i] = spv_3;
            t_lrt_3      = 0;
            spv_lrt_3    = spv_3;
            // Halt core if resumed.
            if (halt_status_3 == RESUME) {
                core_idx = 3;
                write_32b(DEBUG_HALT, core_idx);
                halt_status_3 = HALT;
            }
        } else {
            history_3[i] = val;
            // Resume core if halted.
            if (halt_status_3 == HALT) {
                core_idx = 3;
                write_32b(DEBUG_RESUME, core_idx); 
                halt_status_3 = RESUME;
            }
        }

        // i = (i+1) % window_size;

        i = i+1;
        if(i >= window_size){
            i = i - window_size;
            while (i >= window_size) {
                i = i - window_size;
            }
        }
    }
    while(1) {}
}


