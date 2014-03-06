NAME = banddnicdcharger_test
CC = gcc 
FLAGS = -Wall -Wextra -lpthread -DTEST_HARNESS -DF_CPU=8000000 -DMEMORY_POOL_BYTES=128 -DTX_BUFFER_SIZE=15 -std=c99

LIBS_DIR = ../Libs

INCLUDE_DIRS = \
	-I$(LIBS_DIR)/AVR \
	-I$(LIBS_DIR)/AVR/Harness \
	-I$(LIBS_DIR)/Common \
	-I$(LIBS_DIR)/Devices \
	-I$(LIBS_DIR)/Generics \
	-I$(LIBS_DIR)/Protocols \
	-I$(LIBS_DIR)/Utility \

CFILES = \
	main.c \
	$(LIBS_DIR)/AVR/lib_clk.c \
	$(LIBS_DIR)/AVR/lib_io.c \
	$(LIBS_DIR)/AVR/lib_fuses.c \
	$(LIBS_DIR)/AVR/lib_sleep.c \
	$(LIBS_DIR)/AVR/lib_wdt.c \
	$(LIBS_DIR)/AVR/lib_adc.c \
	$(LIBS_DIR)/AVR/lib_tmr8_tick.c \
	$(LIBS_DIR)/AVR/Harness/lib_tmr8_tick_harness_functions.c \
	$(LIBS_DIR)/Generics/memorypool.c \
	$(LIBS_DIR)/Generics/ringbuf.c \
	$(LIBS_DIR)/Generics/averager.c \
	$(LIBS_DIR)/Generics/statemachinemanager.c \
	$(LIBS_DIR)/Generics/statemachine.c \

OBJDEPS=$(CFILES:.c=.o)

all:
	$(CC) $(FLAGS) $(INCLUDE_DIRS) $(OPTS) $(CFILES) -o $(NAME).exe
	$(NAME).exe

clean:
	$(RM) $(NAME).exe
	$(RM) $(OBJDEPS)