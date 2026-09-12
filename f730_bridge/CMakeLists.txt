cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

project(F730Bridge LANGUAGES C ASM)

add_executable(F730Bridge)

target_compile_definitions(F730Bridge PRIVATE
    USE_HAL_DRIVER
    STM32F730xx
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(F730Bridge PRIVATE
    Core/Inc
    BSP/Inc
    Bridge/Inc
    USB_DEVICE/App
    USB_DEVICE/Target
    Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    Drivers/STM32F7xx_HAL_Driver/Inc
    Drivers/STM32F7xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F7xx/Include
    Drivers/CMSIS/Include
)

target_sources(F730Bridge PRIVATE
    startup_stm32f730xx.s

    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/fmc.c
    Core/Src/i2c.c
    Core/Src/iwdg.c
    Core/Src/tim.c
    Core/Src/usart.c
    Core/Src/usb_otg.c
    Core/Src/stm32f7xx_it.c
    Core/Src/stm32f7xx_hal_msp.c
    Core/Src/system_stm32f7xx.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c

    BSP/Src/bsp.c
    BSP/Src/bsp_bridge.c
    BSP/Src/bsp_debug.c
    BSP/Src/bsp_fmc.c
    BSP/Src/bsp_fpga_ctrl.c
    BSP/Src/bsp_i2c.c
    BSP/Src/bsp_led.c
    BSP/Src/bsp_time.c
    BSP/Src/bsp_usb.c

    Bridge/Src/bridge_protocol.c

    USB_DEVICE/App/bridge_usb_identity.c
    USB_DEVICE/App/usb_device.c
    USB_DEVICE/App/usbd_desc.c
    USB_DEVICE/Target/usbd_conf.c
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c

    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cortex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_exti.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_gpio.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_iwdg.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_nand.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_nor.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pcd.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pcd_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sdram.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sram.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_usb.c
)

target_link_libraries(F730Bridge PRIVATE ${TOOLCHAIN_LINK_LIBRARIES})

add_custom_command(TARGET F730Bridge POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:F730Bridge>
            ${CMAKE_CURRENT_BINARY_DIR}/F730Bridge.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:F730Bridge>
    VERBATIM
)

set_target_properties(F730Bridge PROPERTIES
    ADDITIONAL_CLEAN_FILES "F730Bridge.map;F730Bridge.hex"
)
