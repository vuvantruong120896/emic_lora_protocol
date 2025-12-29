################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/phy/sx1262_config.c \
../src/phy/sx1262_rx.c \
../src/phy/sx1262_tx.c \
../src/phy/sx126x.c \
../src/phy/sx126x_board.c 

COMPILER_OBJS += \
src/phy/sx1262_config.obj \
src/phy/sx1262_rx.obj \
src/phy/sx1262_tx.obj \
src/phy/sx126x.obj \
src/phy/sx126x_board.obj 

C_DEPS += \
src/phy/sx1262_config.d \
src/phy/sx1262_rx.d \
src/phy/sx1262_tx.d \
src/phy/sx126x.d \
src/phy/sx126x_board.d 

# Each subdirectory must supply rules for building sources it contributes
src/phy/%.obj: ../src/phy/%.c 
	@echo 'Scanning and building file: $<'
	ccrl -subcommand="src\phy\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)" -MT="$(@:%.obj=%.obj)" -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	ccrl -subcommand="src\phy\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"


