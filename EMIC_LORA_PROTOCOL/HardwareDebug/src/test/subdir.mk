################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/test/test_gpio.c \
../src/test/test_mac.c \
../src/test/test_main.c \
../src/test/test_protocol.c \
../src/test/test_rtc.c \
../src/test/test_spi.c \
../src/test/test_sx126x.c \
../src/test/test_systick.c \
../src/test/test_timer.c \
../src/test/test_uart_debug.c \
../src/test/test_utils.c 

COMPILER_OBJS += \
src/test/test_gpio.obj \
src/test/test_mac.obj \
src/test/test_main.obj \
src/test/test_protocol.obj \
src/test/test_rtc.obj \
src/test/test_spi.obj \
src/test/test_sx126x.obj \
src/test/test_systick.obj \
src/test/test_timer.obj \
src/test/test_uart_debug.obj \
src/test/test_utils.obj 

C_DEPS += \
src/test/test_gpio.d \
src/test/test_mac.d \
src/test/test_main.d \
src/test/test_protocol.d \
src/test/test_rtc.d \
src/test/test_spi.d \
src/test/test_sx126x.d \
src/test/test_systick.d \
src/test/test_timer.d \
src/test/test_uart_debug.d \
src/test/test_utils.d 

# Each subdirectory must supply rules for building sources it contributes
src/test/%.obj: ../src/test/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\test\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\test\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


