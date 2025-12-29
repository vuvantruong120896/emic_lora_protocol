################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/hal/hal_gpio.c \
../src/hal/hal_rtc.c \
../src/hal/hal_spi.c \
../src/hal/hal_systick.c \
../src/hal/hal_timer.c \
../src/hal/hal_uart.c 

COMPILER_OBJS += \
src/hal/hal_gpio.obj \
src/hal/hal_rtc.obj \
src/hal/hal_spi.obj \
src/hal/hal_systick.obj \
src/hal/hal_timer.obj \
src/hal/hal_uart.obj 

C_DEPS += \
src/hal/hal_gpio.d \
src/hal/hal_rtc.d \
src/hal/hal_spi.d \
src/hal/hal_systick.d \
src/hal/hal_timer.d \
src/hal/hal_uart.d 

# Each subdirectory must supply rules for building sources it contributes
src/hal/%.obj: ../src/hal/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\hal\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\hal\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


