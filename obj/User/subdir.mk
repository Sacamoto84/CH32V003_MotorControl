################################################################################
# MRS Version: 2.3.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/EEPROM.c \
../User/buzzer.c \
../User/ch32v00x_it.c \
../User/system_ch32v00x.c 

C_DEPS += \
./User/EEPROM.d \
./User/buzzer.d \
./User/ch32v00x_it.d \
./User/system_ch32v00x.d 

CPP_SRCS += \
../User/main.cpp \
../User/pwm.cpp 

CPP_DEPS += \
./User/main.d \
./User/pwm.d 

OBJS += \
./User/EEPROM.o \
./User/buzzer.o \
./User/ch32v00x_it.o \
./User/main.o \
./User/pwm.o \
./User/system_ch32v00x.o 

DIR_OBJS += \
./User/*.o \

DIR_DEPS += \
./User/*.d \

DIR_EXPANDS += \
./User/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
User/%.o: ../User/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GNU RISC-V Cross C Compiler'
	riscv-none-embed-gcc -march=rv32ecxw -mabi=ilp32e -msmall-data-limit=0 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -I"f:/CH32V003_MotorControl/SRC/Debug" -I"f:/CH32V003_MotorControl/SRC/Core" -I"f:/CH32V003_MotorControl/User" -I"f:/CH32V003_MotorControl/SRC/Peripheral/inc" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@

User/%.o: ../User/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GNU RISC-V Cross C Compiler'
	riscv-none-embed-g++ -march=rv32ecxw -mabi=ilp32e -msmall-data-limit=0 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -I"f:/CH32V003_MotorControl/SRC/Core" -I"f:/CH32V003_MotorControl/SRC/Debug" -I"f:/CH32V003_MotorControl/SRC/Peripheral/inc" -I"f:/CH32V003_MotorControl/User" -std=gnu++11 -fabi-version=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@

