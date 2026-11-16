# capstone 5.0.7 (VERSION, NOTICE): the x86 decoder, and the PowerPC decoder behind SHSM_CAPSTONE_PPC (default OFF).
option(SHSM_CAPSTONE_PPC "Compile capstone's PowerPC decoder (sources vendored, unused by the tools so far)" OFF)

file(GLOB SHSM_CS_ARCH_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/arch/X86/*.c)
set(SHSM_CS_ARCH_DEFS CAPSTONE_HAS_X86)
if(SHSM_CAPSTONE_PPC)
    file(GLOB SHSM_CS_PPC_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/arch/PowerPC/*.c)
    list(APPEND SHSM_CS_ARCH_SOURCES ${SHSM_CS_PPC_SOURCES})
    list(APPEND SHSM_CS_ARCH_DEFS CAPSTONE_HAS_POWERPC)
endif()

add_library(shsm_capstone STATIC cs.c Mapping.c MCInst.c MCInstrDesc.c MCRegisterInfo.c SStream.c utils.c
    ${SHSM_CS_ARCH_SOURCES})
target_include_directories(shsm_capstone PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(shsm_capstone PRIVATE ${SHSM_CS_ARCH_DEFS} CAPSTONE_USE_SYS_DYN_MEM _CRT_SECURE_NO_WARNINGS)
shsm_register_third_party(shsm_capstone)
