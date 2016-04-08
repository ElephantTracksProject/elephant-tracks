################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../AllocationRecord.cpp \
../CallBackHandler.cpp \
../ClassInstrumenter.cpp \
../DeathRecord.cpp \
../ExternalClassInstrumenter.cpp \
../FlatTraceOutputter.cpp \
../MearthCallBackHandler.cpp \
../MethodEntryRecord.cpp \
../MethodExitRecord.cpp \
../MethodNameRecord.cpp \
../PointerUpdateRecord.cpp \
../Record.cpp \
../TraceOutputer.cpp \
../main.cpp 

OBJS += \
./AllocationRecord.o \
./CallBackHandler.o \
./ClassInstrumenter.o \
./DeathRecord.o \
./ExternalClassInstrumenter.o \
./FlatTraceOutputter.o \
./MearthCallBackHandler.o \
./MethodEntryRecord.o \
./MethodExitRecord.o \
./MethodNameRecord.o \
./PointerUpdateRecord.o \
./Record.o \
./TraceOutputer.o \
./main.o 

CPP_DEPS += \
./AllocationRecord.d \
./CallBackHandler.d \
./ClassInstrumenter.d \
./DeathRecord.d \
./ExternalClassInstrumenter.d \
./FlatTraceOutputter.d \
./MearthCallBackHandler.d \
./MethodEntryRecord.d \
./MethodExitRecord.d \
./MethodNameRecord.d \
./PointerUpdateRecord.d \
./Record.d \
./TraceOutputer.d \
./main.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/lib/jvm/java-6-sun-1.6.0.15/include/ -I/usr/lib/jvm/java-6-sun-1.6.0.15/include/linux -I/usr/include -I"/home/nricci01/workspace/mearthAgent2/java_crw_demo" -I"/home/nricci01/workspace/mearthAgent2/agent_util" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


