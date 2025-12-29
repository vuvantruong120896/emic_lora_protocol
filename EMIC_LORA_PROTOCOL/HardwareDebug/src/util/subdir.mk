################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/util/log_control.c 

COMPILER_OBJS += \
src/util/log_control.obj 

C_DEPS += \
src/util/log_control.d 

# Each subdirectory must supply rules for building sources it contributes
src/util/%.obj: ../src/util/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\util\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\util\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


