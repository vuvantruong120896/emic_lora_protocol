################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/EMIC_LORA_PROTOCOL.c 

COMPILER_OBJS += \
src/EMIC_LORA_PROTOCOL.obj 

C_DEPS += \
src/EMIC_LORA_PROTOCOL.d 

# Each subdirectory must supply rules for building sources it contributes
src/%.obj: ../src/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


