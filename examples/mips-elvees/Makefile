#mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)) )
#mkfile_path = $(shell dirname $(MAKEFILE_LIST) )
START_MAKES = ${MAKEFILE_LIST}
mkfile_file = ${firstword  ${MAKEFILE_LIST}}
mkfile_path = ${dir ${realpath ${mkfile_file}} }
# ЭТОТ финт нужун чтобы обойти багу abspath, realpath, dir в makefile msys/sygwin 3.82 
#	- он путает c: с разделителем путей и генерит вывод как будто для некольких путей 
TARGET_	?= ${mkfile_path}
TARGET	?= ${shell cd ${TARGET_}; pwd}
#OS		= ${TARGET}/../..
OS		= ${shell cd ${TARGET}/../..; pwd}

#			$(OS)/utils/libuos/MultiCore_Configuration_Debug/libuos.a
ifdef UOS_LIB
USE_UOS	:= ${UOS_LIB}
else
USE_UOS	?= ${TARGET}/libuos.a
UOS_LIB := dummy_uos
endif

override USE_UOS	:= ${realpath ${USE_UOS}}

# cant take just $(dir ) cause of buggy make 3.82 with windows drive letter
UOS_LIB_DIR_ := $(dir ${realpath ${USE_UOS}})
UOS_LIB_DIR := $(shell cd ${UOS_LIB_DIR_}; pwd)

ifeq ("${UOS_LIB}","dummy_uos")

ifeq ("${UOS_LIB_DIR}" , "${TARGET}")
override UOS_LIB := ${USE_UOS}
endif

ifeq ("${UOS_LIB_DIR}" , "${mkfile_path}")
override UOS_LIB := ${USE_UOS}
endif


HERE_DIR_PRE := $(shell cd .; pwd)
HERE_DIR := $(shell cd ${HERE_DIR_PRE}; pwd)

ifeq ("${UOS_LIB_DIR}","${HERE_DIR}")
override UOS_LIB := ${USE_UOS}
endif

ifeq ("${UOS_LIB}","dummy_uos")
# if library - is a standalone project try to build it 
ifneq ("$(wildcard ${UOS_LIB_DIR}/makefile)", "" )
STANDALONE_UOS := ${UOS_LIB}
else
	@echo cant build uos_lib
endif
endif

endif



include ${TARGET}/target.cfg

TEST		= test-ups
TESTS		+= test-debug.bin test-task.bin test-timer.bin test-uart.bin
TESTS		+= philosophers.bin test-ram.bin
TESTS       += test-latency.bin test-tswitch.bin #test-fpu.bin
TESTS		+= test-dcache.bin #test-tcl.bin
#TESTS		+= dhrystone.bin linpack.bin whetstone.bin #livermore.bin
#TESTS		+= coremark.bin
#TESTS       += test-flash.bin test-sdhc-fast.bin print-fat32.bin test-spi.bin
#TESTS		+= test-usbdev.bin test-hid.bin

ifneq (,$(findstring NVCOM,$(CFLAGS)))
TESTS       += test-mac.bin test-eth.bin test-eth-mdb.bin
TESTS		+= test-tcp-serv.bin test-tcp-client.bin
TESTS		+= test-udp-serv.bin test-udp-client.bin
TESTS       += test-i2c.bin test-i2c-ad7416.bin test-uda1380.bin
TESTS		+= test-tlv320aic32.bin
endif

#ifneq (,$(findstring MCB03,$(CFLAGS)))
#TESTS		+= test-mcb-eth.bin test-tcp-serv.bin test-tcp-client.bin
#TESTS		+= test-spw.bin eth-to-swic.bin
#endif

ifneq (,$(findstring MC30SF6,$(CFLAGS)))
#TESTS		+= test-tcp-serv.bin test-tcp-client.bin
TESTS		+= test-usbic.bin
endif

ifneq (,$(findstring VIDEOREGBOARD,$(CFLAGS)))
TESTS		+= test-ds1307.bin test-ups.bin
endif

OUTS		= $(TESTS:%.bin=%.elf)
SREC		= $(TESTS:%.bin=%.srec)
DIS		    = $(TESTS:%.bin=%.dis)

#	add trace_make to see internals of building 
all:	$(OUTS) $(TESTS) $(SREC) $(DIS)
		$(SIZE) *.elf

$(OUTS) $(TESTS): startup.o ${USE_UOS}

ifdef STANDALONE_UOS

${USE_UOS}:
	@echo try build standalone libuos
	$(MAKE) -f${UOS_LIB_DIR}/makefile all

libuos:
	@echo try build libuos
	$(MAKE) -f${UOS_LIB_DIR}/makefile all

else
libuos: build_uos
endif

clean:
		rm -rf *~ *.[oasi] *.elf *.bin *.srec *.lst *.dis *.map .deps $(MODULES)

load:		$(TEST).elf
		$(STRIP) $(TEST).elf
		mcprog -e2 $(TEST).elf 0x1fc10000

proxy:
		mcremote &

gdb:		$(TEST).elf
		mipsel-elf32-gdb -x gdbinit $(TEST).elf

insight:	$(TEST).elf
		mipsel-elf32-insight -x gdbinit $(TEST).elf &

trace_make:
	@echo MAKEFILE_LIST:$(START_MAKES)
	@echo mkfile_path:${mkfile_path}
	@echo target:${TARGET}
	@echo uOSpath:${OS}
	@echo USE_UOS:${USE_UOS}
	@echo UOS_LIB:${UOS_LIB}
	@echo UOS_realname: ${realpath ${USE_UOS}}
	@echo UOS_LIB_DIR:${UOS_LIB_DIR}
	@echo STANDALONE_UOS:${STANDALONE_UOS}
	@echo HERE:${HERE_DIR}
	@echo MAKE:$(MAKE)

VPATH += ${TARGET}
include $(OS)/sources/rules.mak

###
