In the apmu_core_lib, do make clean all. 

Copy the code after fence instruction from .S file till the end of function into the program.s file in rv32-benchmark-ibex folder. 

In rv32-benchmar-ibex folder do the make fix replace and then do make assemble

Remember to add a 0x33 at start to create a nop instruction at start


TODO:
 - The core programs the counters event selection and event info bits
 - The core starts the pmu, the pmu will read initial value of counters and then wait and then read final value of counters
 - PMU will write the difference of counter values to DSPM
 - Application core will finish the synthetic test and then read the DSPM for result


Working now:
 - Goto he-soc/pmu_throughput and open riscv gdb. Do source gdb_script then hit ctrl+c after some time, and then do source pt_dspm.script
 - For uart do cat /dev/ttyUSB
 - For programming board and openocd:
 xsdb -eval "connect -url TCP:localhost:3121;targets -set -nocase -filter {name =~ \"*xcvu9p*\"}; fpga -f $ALSAQR_BITSTREAM/Mar18/alsaqr_xilinx.bit"; sudo openocd -f $ALSAQR_CFG/zcu102-multicore.cfg 
 
 - Write Miss not showing any results