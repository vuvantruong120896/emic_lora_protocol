################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/mac/lora_mac.c 

COMPILER_OBJS += \
src/mac/lora_mac.obj 

C_DEPS += \
src/mac/lora_mac.d 

# Each subdirectory must supply rules for building sources it contributes
src/mac/%.obj: ../src/mac/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\mac\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\mac\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


