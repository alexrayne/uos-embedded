TARGET		= $(CURDIR)
OS			= $(shell cd ../..; pwd)
include target.cfg

#PROG		= uuloader
PROG            = milprog

TEST		 = test-ve1eth
TESTS           += test-ve1eth.bin test-arinc.bin  test-timer-lcd.bin test-task-lcd.bin
TESTS		+= test-task.bin test-timer.bin test-dma.bin test-irq.bin
TESTS		+= test-latency.bin test-tswitch.bin phil-uart.bin #test-uart.bin
TESTS		+= test-mil_std_bc.bin test-mil_std_rt.bin test-arinc.bin test-arinc-poll.bin
# TESTS		+= test-led.bin test-debug.bin
OUTS		= $(TESTS:%.bin=%.elf)
HEX		    = $(TESTS:%.hex=%.elf)

all:		$(OUTS) $(HEX) $(TESTS) $(TEST).dis
		$(SIZE) $(OUTS)



load:		$(TEST).bin
			$(PROG) -a $< 0x00000000
			

$(OUTS) $(TESTS): startup.o libuos.a

clean:
		rm -rf *~ *.[oasi] *.elf *.lst *.dis *.bin *.map *.hex .deps $(TESTS) $(MODULES) udpping


include $(OS)/sources/rules.mak
