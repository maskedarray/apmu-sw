APP := apmu_core_lib
XLEN:=64

#RISCV_PREFIX ?= /opt/lowrisc-riscv32/bin/riscv$(XLEN)-unknown-elf-
RISCV_PREFIX ?= riscv$(XLEN)-unknown-elf-
RISCV_GCC ?= $(RISCV_PREFIX)gcc

inc_dir  := ./common/
utils_dir = ./inc/

# RISCV_FLAGS     := -mcmodel=medany -static -std=gnu99 -DNUM_CORES=1 -O2
# RISCV_LINK_OPTS := -static -nostdlib -nostartfiles -lm -lgcc

all: build

build:
	$(RISCV_GCC) -O2 -march=rv32im -mabi=ilp32 -T -L -S $(APP).c 2>&1 | tee compile_out.txt

clean:
	rm -rf *.txt
	rm -rf *.dump
	rm -rf *.s
