################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/utils/aes128.c \
../src/utils/crc16.c \
../src/utils/log_control.c \
../src/utils/util_system.c 

COMPILER_OBJS += \
src/utils/aes128.obj \
src/utils/crc16.obj \
src/utils/log_control.obj \
src/utils/util_system.obj 

C_DEPS += \
src/utils/aes128.d \
src/utils/crc16.d \
src/utils/log_control.d \
src/utils/util_system.d 

# Each subdirectory must supply rules for building sources it contributes
src/utils/%.obj: ../src/utils/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\utils\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\utils\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


