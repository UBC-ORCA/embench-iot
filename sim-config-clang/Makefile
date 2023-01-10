#.PHONY: run

RISCV := /home/ychin02

CC := riscv32-unknown-elf-gcc
CXX := riscv32-unknown-elf-g++

RISCV_FLAGS := \
-mno-relax \
-v \
-I inc \
-I $(RISCV)/gnu/riscv32-unknown-elf/include \
-ffreestanding \
-ffunction-sections \
-fdata-sections \
-fno-common \
-fomit-frame-pointer \
-Wno-error\
-Wsign-compare\
-Wdouble-promotion\
-Wno-unused-value\
-Wshadow\
-Wno-unused-variable\
-Wno-unused-but-set-variable\
-Wno-missing-field-initializers\
-Wunused-function\
-Wno-maybe-uninitialized\
-Wno-deprecated-register\
-Wno-format\
-Wswitch\
-Wvla\
-g \
-O3 \
-fno-builtin \
-std=c++11 \
-fstrict-aliasing \
-fno-rtti \
-fno-exceptions \
-fno-threadsafe-statics\
-fmessage-length=0 \
-Wall \
-Wextra \
-Wstrict-aliasing \
-Wno-unused-parameter \
-funroll-loops

rvv:
	$(CXX) $(RISCV_FLAGS) -march=rv32imav -mabi=ilp32 src/mm.cc src/tests.cc -o riscv32-sim-config
run:
	$(RISCV)/spike/bin/spike --isa=rv32imav $(RISCV)/pk/riscv32-unknown-elf/bin/pk riscv32-sim-config

run2:
	$(RISCV)/spike/bin/spike --isa=rv32imfv $(RISCV)/pk/riscv32-unknown-elf/bin/pk test_scalarAdd

run3:
	$(RISCV)/spike/bin/spike --isa=rv32imfv $(RISCV)/pk/riscv32-unknown-elf/bin/pk test_scalarAddNew

runvl:
	$(RISCV)/spike/bin/spike --isa=rv32imfv $(RISCV)/pk/riscv32-unknown-elf/bin/pk vl_32bitf


runvlmax:
	$(RISCV)/spike/bin/spike --isa=rv32imfv $(RISCV)/pk/riscv32-unknown-elf/bin/pk vl_max






