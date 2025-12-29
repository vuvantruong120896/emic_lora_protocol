################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/app/EXAMPLE_PERIODIC_TX.c \
../src/app/app_periodic_tx.c 

COMPILER_OBJS += \
src/app/EXAMPLE_PERIODIC_TX.obj \
src/app/app_periodic_tx.obj 

C_DEPS += \
src/app/EXAMPLE_PERIODIC_TX.d \
src/app/app_periodic_tx.d 

# Each subdirectory must supply rules for building sources it contributes
src/app/%.obj: ../src/app/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\app\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\app\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


