ifdef nogui
	sim_flags = batch-mode=1 
endif

#ifdef simple_pad
#	rtl_flags = simple-padframe=1
#	sim_flags += simple-padframe=1 
#endif

current_dir = $(shell pwd)

ifdef CLUSTER_BIN
	cc-elf-y = -DCLUSTER_BIN_PATH=\"$(current_dir)/stimuli/cluster.bin\"  -DCLUSTER_BIN
endif

ifdef USE_HYPER
	cc-elf-y += -DUSE_HYPER
endif
SW_HOME?=$(shell pwd)/..
utils_dir = $(SW_HOME)/inc/

directories = . drivers/inc drivers/src string_lib/inc string_lib/src padframe/inc padframe/src fpga_padframe/inc fpga_padframe/src udma udma/cpi udma/i2c udma/spim udma/uart udma/sdio apb_timer gpio

INC=$(foreach d, $(directories), -I$(utils_dir)$d)


inc_dir := $(SW_HOME)/common/

RISCV_PREFIX ?= riscv$(XLEN)-unknown-elf-
RISCV_GCC ?= $(RISCV_PREFIX)gcc

RISCV_OBJDUMP ?= $(RISCV_PREFIX)objdump --disassemble-all --disassemble-zeroes --section=.text --section=.text.startup --section=.text.init --section=.data

RISCV_FLAGS     := -mcmodel=medany -static -std=gnu99 -DNUM_CORES=1 -O3 -ffast-math -fno-common -fno-builtin-printf $(INC)
RISCV_LINK_OPTS := -static -nostdlib -nostartfiles -lm -lgcc

clean:
	rm -f $(APP).riscv
	rm -f $(APP).dump
	rm -f *.slm

build:
	$(RISCV_GCC) $(RISCV_FLAGS) -T $(inc_dir)/test.ld $(RISCV_LINK_OPTS) $(cc-elf-y) $(inc_dir)/crt.S  $(inc_dir)/syscalls.c -L $(inc_dir) $(APP).c -o $(APP).riscv
	
dis:
	$(RISCV_OBJDUMP) $(APP).riscv > $(APP).dump

dump:
	$(SW_HOME)/elf_to_slm.py --binary=$(APP).riscv --vectors=hyperram0.slm
	cp hyperram*.slm  $(HW_HOME)/
	cp $(APP).riscv  $(HW_HOME)/
# echo $(APP).riscv | tee -a  $(HW_HOME)/regression.list
	echo $(APP).riscv

all: clean build dis

rtl: 
	 $(MAKE) -C  $(SW_HOME)/../hardware/ all 

sim:
	$(MAKE) -C  $(SW_HOME)/../hardware/ sim $(sim_flags) elf-bin=$(shell pwd)/$(APP).riscv
