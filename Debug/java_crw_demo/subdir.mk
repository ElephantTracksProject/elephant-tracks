################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../java_crw_demo/java_crw_demo.c 

OBJS += \
./java_crw_demo/java_crw_demo.o 

C_DEPS += \
./java_crw_demo/java_crw_demo.d 


# Each subdirectory must supply rules for building sources it contributes
java_crw_demo/%.o: ../java_crw_demo/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"/home/nricci01/workspace/mearthAgent2/agent_util" -I"/home/nricci01/workspace/mearthAgent2/java_crw_demo" -I/usr/lib/jvm/java-6-sun-1.6.0.15/include/ -I/usr/lib/jvm/java-6-sun-1.6.0.15/include/linux -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


