WOLFSSL_ROOT = ../../../wolfssl
WOLFSSH_ROOT = ../..
MQX_ROOT = /mnt/c/Freescale/Freescale_MQX_4_1
CC       = arm-none-eabi-gcc
AR		 = arm-none-eabi-ar

MQXLIB     =
WOLFSSL_MKDIR = $(WOLFSSL_ROOT)/IDE/MQX
WOLFLIBS   = $(WOLFSSHLIB) $(WOLFSSL_MKDIR)/libwolfssl.a
WOLFSSHLIB = ./libwolfssh.a

OPTIMIZE = -O2

WORNING  = \
 -Wno-int-to-pointer-cast\
 -Wno-incompatible-pointer-types\
 -Wno-discarded-qualifiers\
 -Wno-int-conversion\
 -Wno-implicit-function-declaration\
 -Wno-pointer-to-int-cast\
 -Wno-format

SETTINGS_H = -DWOLFSSL_USER_SETTINGS  -DWOLFSSL_TEST_H -I$(WOLFSSL_MKDIR) 
CONFIG_H   = -DHAVE_CONFIG_H
CFLAGS   = $(OPTIMIZE) -I$(WOLFSSH_ROOT) -I$(WOLFSSL_ROOT) $(CONFIG_H) $(SETTINGS_H) -I. $(MQX_INCLUDE) $(WORNING)

MQX_INCLUDE = -I$(MQX_ROOT)/mqx/source/include\
-I$(MQX_ROOT)/mqx/source/psp/cortex_m\
-I$(MQX_ROOT)/mqx/source/psp/cortex_a/compiler/gcc_arm\
-I$(MQX_ROOT)/mqx/source/psp/cortex_m/cpu\
-I$(MQX_ROOT)/mfs/source/include\
-I$(MQX_ROOT)/mqx/source/bsp/twrk70f120m\
-I$(MQX_ROOT)/mqx/source/io/cm\
-I$(MQX_ROOT)/mqx/source/io/lpm\
-I$(MQX_ROOT)/mqx/source/io/serial\
-I$(MQX_ROOT)/mqx/source/io/dma\
-I$(MQX_ROOT)/mqx/source/io/spi\
-I$(MQX_ROOT)/mqx/source/io/i2c\
-I$(MQX_ROOT)/mqx/source/io/io_mem\
-I$(MQX_ROOT)/mqx/source/io/io_null\
-I$(MQX_ROOT)/mqx/source/io/enet\
-I$(MQX_ROOT)/mqx/source/io/enet/macnet\
-I$(MQX_ROOT)/mqx/source/io/lwgpio\
-I$(MQX_ROOT)/mqx/source/io/gpio\
-I$(MQX_ROOT)/mqx/source/io/gpio/kgpio\
-I$(MQX_ROOT)/mqx/source/io/esdhc\
-I$(MQX_ROOT)/mqx/source/io/sdcard\
-I$(MQX_ROOT)/mqx/source/io/sdcard/sdcard_spi\
-I$(MQX_ROOT)/mqx/source/io/sdcard/sdcard_esdhc\
-I$(MQX_ROOT)/mqx/source/io/pccard\
-I$(MQX_ROOT)/mqx/source/io/pcflash\
-I$(MQX_ROOT)/mqx/source/io/adc/kadc\
-I$(MQX_ROOT)/mqx/source/io/adc\
-I$(MQX_ROOT)/mqx/source/io/lwadc\
-I$(MQX_ROOT)/mqx/source/io/flashx\
-I$(MQX_ROOT)/mqx/source/io/flashx/freescale\
-I$(MQX_ROOT)/mqx/source/io/hwtimer\
-I$(MQX_ROOT)/mqx/source/io/rtc\
-I$(MQX_ROOT)/mqx/source/io/debug\
-I$(MQX_ROOT)/mqx/source/io/tchres\
-I$(MQX_ROOT)/mqx/source/io/nandflash\
-I$(MQX_ROOT)/mqx/source/io/timer\
-I$(MQX_ROOT)/mqx/source/io/usb\
-I$(MQX_ROOT)/mqx/source/io/hmi\
-I$(MQX_ROOT)/mqx/source/io/sai\
-I$(MQX_ROOT)/mqx/source/io/rtcs\
-I$(MQX_ROOT)/rtcs/source/include

OBJ =\
$(WOLFSSH_ROOT)/src/agent.o\
$(WOLFSSH_ROOT)/src/internal.o\
$(WOLFSSH_ROOT)/src/io.o\
$(WOLFSSH_ROOT)/src/keygen.o\
$(WOLFSSH_ROOT)/src/log.o\
$(WOLFSSH_ROOT)/src/port.o\
$(WOLFSSH_ROOT)/src/ssh.o\
$(WOLFSSH_ROOT)/src/wolfscp.o\
$(WOLFSSH_ROOT)/src/wolfsftp.o\
$(WOLFSSH_ROOT)/src/wolfterm.o\


all	: wolfsshlib echoserver

wolfsshlib	: $(OBJ)
	$(AR) r $(WOLFSSHLIB) $(OBJ)
echoserver	: $(WOLFLIBS) $(WOLFSSL_MKDIR)/user_settings.h
	$(CC) -o echoserver $(CFLAGS) $(WOLFSSH_ROOT)/examples/echoserver/echoserver.c $(WOLFLIBS) $(MQXLIB)
clean	: 
	rm $(OBJ) $(WOLFSSHLIB)
