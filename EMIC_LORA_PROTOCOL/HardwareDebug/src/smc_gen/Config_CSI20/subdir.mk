################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/smc_gen/Config_CSI20/Config_CSI20.c \
../src/smc_gen/Config_CSI20/Config_CSI20_user.c 

COMPILER_OBJS += \
src/smc_gen/Config_CSI20/Config_CSI20.obj \
src/smc_gen/Config_CSI20/Config_CSI20_user.obj 

C_DEPS += \
src/smc_gen/Config_CSI20/Config_CSI20.d \
src/smc_gen/Config_CSI20/Config_CSI20_user.d 

# Each subdirectory must supply rules for building sources it contributes
src/smc_gen/Config_CSI20/%.obj: ../src/smc_gen/Config_CSI20/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\smc_gen\Config_CSI20\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\smc_gen\Config_CSI20\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


