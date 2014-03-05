NAME=BandDNicdCharger

CC=avr-gcc

RM = rm -f
CAT = cat

MCU_TARGET=attiny85
LIBS_DIR = $(PROJECTS_PATH)/Libs

OPT_LEVEL=s

INCLUDE_DIRS = \
	-I..\Common \
	-I$(LIBS_DIR)/AVR \
	-I$(LIBS_DIR)/Common \
	-I$(LIBS_DIR)/Devices \
	-I$(LIBS_DIR)/Generics \
	-I$(LIBS_DIR)/Protocols \
	-I$(LIBS_DIR)/Utility \

CFILES = \
	main.c \
	$(LIBS_DIR)/AVR/lib_clk.c \
	$(LIBS_DIR)/AVR/lib_sleep.c \
	$(LIBS_DIR)/AVR/lib_wdt.c \
	$(LIBS_DIR)/AVR/lib_io.c \
	$(LIBS_DIR)/AVR/lib_fuses.c \
	$(LIBS_DIR)/AVR/lib_adc.c \
	$(LIBS_DIR)/AVR/lib_tmr8.c \
	$(LIBS_DIR)/AVR/lib_tmr8_tick.c \
	$(LIBS_DIR)/Generics/memorypool.c \
	$(LIBS_DIR)/Generics/averager.c \
	$(LIBS_DIR)/Generics/ringbuf.c \
	$(LIBS_DIR)/Generics/statemachinemanager.c \
	$(LIBS_DIR)/Generics/statemachine.c

OPTS = \
	-Wall \
	-Wextra \
	-DF_CPU=8000000 \
	-DMEMORY_POOL_BYTES=64 \
	-ffunction-sections \
	-std=c99

LDFLAGS = \
	-Wl,-Map=$(MAPFILE),-gc-sections

OBJDEPS=$(CFILES:.c=.o)

MAPFILE = $(NAME).map

all: $(NAME).elf

	
$(NAME).elf: $(OBJDEPS)
	$(CC) $(INCLUDE_DIRS) $(OPTS) $(LDFLAGS) -O$(OPT_LEVEL) -mmcu=$(MCU_TARGET) -o $@ $^ $(LD_SUFFIX)

%.o:%.c
	$(CC) $(INCLUDE_DIRS) $(OPTS) -O$(OPT_LEVEL) -mmcu=$(MCU_TARGET) -c $< -o $@

upload:
	avr-objcopy -R .eeprom -O ihex $(NAME).elf  $(NAME).hex
	avrdude -pt85 -cusbtiny -Uflash:w:$(NAME).hex:a

clean:
	$(RM) $(NAME).elf
	$(RM) $(NAME).hex
	$(RM) $(OBJDEPS)