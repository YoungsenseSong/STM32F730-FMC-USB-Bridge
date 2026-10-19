cmake_minimum_required(VERSION 3.22)
project(WchBridgeDemo LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
option(WCH_ENABLE_EXTBUS "Enable physical FSMC access (requires reviewed schematic)" OFF)
option(WCH_USB_TRACE "Print USB endpoint state on USART1 for board diagnosis" OFF)
option(WCH_SIM_SOURCE "Stream deterministic protocol frames over USBFS CDC" OFF)
option(WCH_FMC_BRIDGE "Read validated FPGA blocks over FSMC and stream UBR1 over USBFS CDC" OFF)

set(WCH_COMMON_FLAGS
  -march=rv32imacxw -mabi=ilp32 -msave-restore -msmall-data-limit=8
  -ffunction-sections -fdata-sections -fno-common -fsigned-char
  -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers)

add_executable(WchBridgeDemo
  app/main.c
  app/platform_port.c
  app/extbus_port.c
  app/fmc_bridge.c
  app/usb_port.c
  app/bridge_storage.c
  app/bridge_state.c
  app/bridge_sim.c
  protocol/bridge_protocol.c
  protocol/bridge_crc_combine.c
  vendor/Core/core_riscv.c
  vendor/Debug/debug.c
  vendor/Startup/startup_ch32v30x_D8C.S
  vendor_usb/system_ch32v30x.c
  vendor_usb/ch32v30x_it.c
  vendor_usb/UART/UART.c
  vendor_usb/USB_Device/ch32v30x_usbfs_device.c
  vendor_usb/USB_Device/usb_desc.c
  vendor/Peripheral/src/ch32v30x_dbgmcu.c
  vendor/Peripheral/src/ch32v30x_dma.c
  vendor/Peripheral/src/ch32v30x_fsmc.c
  vendor/Peripheral/src/ch32v30x_gpio.c
  vendor/Peripheral/src/ch32v30x_misc.c
  vendor/Peripheral/src/ch32v30x_rcc.c
  vendor/Peripheral/src/ch32v30x_tim.c
  vendor/Peripheral/src/ch32v30x_usart.c)

target_include_directories(WchBridgeDemo PRIVATE
  app protocol
  vendor/Core vendor/Debug vendor/Peripheral/inc
  vendor_usb vendor_usb/UART vendor_usb/USB_Device)

target_compile_definitions(WchBridgeDemo PRIVATE
  CH32V30x_D8C=1
  WCH_ENABLE_EXTBUS=$<BOOL:${WCH_ENABLE_EXTBUS}>
  WCH_USB_TRACE=$<BOOL:${WCH_USB_TRACE}>
  WCH_SIM_SOURCE=$<BOOL:${WCH_SIM_SOURCE}>
  WCH_FMC_BRIDGE=$<BOOL:${WCH_FMC_BRIDGE}>
  WCH_TEST_USB_IDENTITY=1)
target_compile_options(WchBridgeDemo PRIVATE ${WCH_COMMON_FLAGS})
target_link_options(WchBridgeDemo PRIVATE
  ${WCH_COMMON_FLAGS}
  -nostartfiles
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/WchBridgeDemo.map
  -T${CMAKE_CURRENT_SOURCE_DIR}/vendor/Ld/Link.ld)

add_custom_command(TARGET WchBridgeDemo POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:WchBridgeDemo> ${CMAKE_CURRENT_BINARY_DIR}/WchBridgeDemo.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:WchBridgeDemo> ${CMAKE_CURRENT_BINARY_DIR}/WchBridgeDemo.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:WchBridgeDemo>
  VERBATIM)

set_target_properties(WchBridgeDemo PROPERTIES
  SUFFIX ".elf"
  ADDITIONAL_CLEAN_FILES "WchBridgeDemo.hex;WchBridgeDemo.bin;WchBridgeDemo.map")
