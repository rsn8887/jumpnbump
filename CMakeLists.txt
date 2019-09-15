cmake_minimum_required(VERSION 2.8)
set(VERSION_MAJOR 1)
set(VERSION_MINOR 01)

if(BUILD_PSP2)
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else ()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif ()
endif ()
endif ()

if(BUILD_NX)
    set(CMAKE_SYSTEM_NAME "Generic")
    set(DEVKITPRO $ENV{DEVKITPRO})
    set(CMAKE_SYSTEM_PROCESSOR "armv8-a")
    set(CMAKE_C_COMPILER "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-gcc")
    set(CMAKE_CXX_COMPILER "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-g++")
    set(CMAKE_ASM_COMPILER "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-as")
    set(CMAKE_AR "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-gcc-ar" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-gcc-ranlib" CACHE STRING "")
    set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO} ${DEVKITPRO}/devkitA64 ${DEVKITPRO}/libnx ${DEVKITPRO}/portlibs/switch)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available")
    link_directories(${DEVKITPRO}/libnx/lib ${DEVKITPRO}/portlibs/switch/lib)
endif(BUILD_NX)

project(jumpnbump C)

aux_source_directory(. SRC_LIST)
aux_source_directory(sdl SRC_LIST)
add_custom_command(OUTPUT version.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_version_h.sh
  DEPENDS VERSION)
add_custom_command(OUTPUT jumpbump_dat.c
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_jumpbump_dat.sh
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_jumpbump_dat_c.sh
)

if(BUILD_PSP2)
  include("${VITASDK}/share/vita.cmake" REQUIRED)
  set(TITLEID "JUMPNBUMP")
  include_directories(
    ${VITASDK}/arm-vita-eabi/include/
    ${VITASDK}/arm-vita-eabi/include/SDL2
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  set(LDFLAGS
        SDL2_mixer
        SDL2
#        SDL2_net
        vorbisfile 
        vorbis 
        ogg
        mikmod
        mpg123
        flac
        vita2d
        SceSysmodule_stub SceDisplay_stub SceGxm_stub SceAppUtil_stub
        SceCtrl_stub ScePgf_stub ScePower_stub SceCommonDialog_stub
        SceAudio_stub SceShellSvc_stub 
#        SceNet_stub
#        SceNetCtl_stub
        SceHid_stub
        SceTouch_stub
        png jpeg z m c
  )
  set(FLAGS
    -DNO_SDL_NET
    -D__PSP2__
    -std=c++11
    -std=c11
    -Wno-write-strings
#Note: using -Os, -O1, -O2, or -O3 all cause crash on Vita when jumping out of the top of the screen
#to the left (!?!)
    -marm -mfpu=neon -mcpu=cortex-a9 -march=armv7-a -mfloat-abi=hard -ffast-math
    -fno-asynchronous-unwind-tables -funroll-loops
    -mword-relocations -fno-unwind-tables -fno-optimize-sibling-calls
    -mvectorize-with-neon-quad
    -mlittle-endian -munaligned-access
  )
  add_executable(${PROJECT_NAME} ${SRC_LIST} jumpbump_dat.c version.h)
  target_compile_options(${PROJECT_NAME} PRIVATE ${FLAGS})
  target_link_libraries(${PROJECT_NAME} ${LDFLAGS})

  vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} ${UNSAFE_FLAG})
  add_custom_target(${PROJECT_NAME}.vpk
        DEPENDS ${PROJECT_NAME}.self
        COMMAND rm -rf vpk && mkdir -p vpk/sce_sys
        COMMAND ${VITASDK}/bin/vita-mksfoex -s TITLE_ID=${TITLEID} "${PROJECT_NAME}" vpk/sce_sys/param.sfo
        COMMAND cp ${PROJECT_NAME}.self vpk/eboot.bin
        COMMAND cp -r ${CMAKE_SOURCE_DIR}/psp2data/* vpk
#        COMMAND cp -r ${CMAKE_SOURCE_DIR}/data vpk/data
        COMMAND cd vpk && zip -r ../${PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}.vpk . && cd ..
        COMMAND zip -d ${PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}.vpk *.DS_Store
        COMMAND zip -d ${PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}.vpk *__MAC*
  )
elseif(BUILD_NX)
  include_directories(
    ${DEVKITPRO}/portlibs/switch/include/SDL2
    ${DEVKITPRO}/libnx/include
    ${DEVKITPRO}/portlibs/switch/include
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  
  set(FLAGS
    -DNO_SDL_NET
    -D__SWITCH__
    -std=gnu++11
    -std=gnu11
    -Wno-write-strings
    -march=armv8-a
    -mtune=cortex-a57
    -mtp=soft
#-fPIE is neccessary to prevent crash on startup
    -fPIE
    -fno-asynchronous-unwind-tables
    -funroll-loops
    -fno-unwind-tables
    -fno-optimize-sibling-calls
    -funsafe-math-optimizations
    -mlittle-endian
    -ffunction-sections
  )
  set(LIBS
    SDL2_mixer
#    SDL2_net
    SDL2 
    EGL
    GLESv2
    glapi
    drm_nouveau
    png
    jpeg
    vorbisidec
    ogg
    modplug
    opusfile
    opus
    mpg123
    flac
    z
    m
    c
    nx
    stdc++
  )
  foreach(flag ${FLAGS})
    set(FLAGS_GENERAL "${FLAGS_GENERAL} ${flag}")
  endforeach(flag ${FLAGS})

  #set(CMAKE_C_FLAGS "${FLAGS_GENERAL}" CACHE STRING "C Flags" FORCE)
  #set(CMAKE_CXX_FLAGS "${FLAGS_GENERAL}" CACHE STRING "C++ Flags" FORCE)

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FLAGS_GENERAL} -specs=${DEVKITPRO}/libnx/switch.specs --verbose")
  
  add_executable(${PROJECT_NAME}.elf ${SRC_LIST} jumpbump_dat.c version.h)
  target_compile_options(${PROJECT_NAME}.elf PRIVATE ${FLAGS})
  target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${LIBS})
  add_custom_target(${PROJECT_NAME}.nro
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND ${DEVKITPRO}/tools/bin/nacptool --create "${PROJECT_NAME}" "rsn8887" "${VERSION_MAJOR}.${VERSION_MINOR}" ${PROJECT_NAME}.nacp
    COMMAND ${DEVKITPRO}/tools/bin/elf2nro ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${CMAKE_SOURCE_DIR}/switchdata/icon0.jpg --nacp=${PROJECT_NAME}.nacp
  )
  add_custom_target(${PROJECT_NAME}_switch.zip
    DEPENDS ${PROJECT_NAME}.nro
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}
    COMMAND cp -f ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.nro ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/${PROJECT_NAME}.nro
 #   COMMAND unzip ${CMAKE_SOURCE_DIR}/switchdata/data.zip -d${CMAKE_BINARY_DIR}/release/
    COMMAND cd ${CMAKE_BINARY_DIR}/release && zip -r ../${PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}_switch.zip ${PROJECT_NAME} && cd ${CMAKE_BINARY_DIR}
    COMMAND zip -d ${PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}_switch.zip *.DS_Store
    COMMAND zip -d ${PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}_switch.zip *__MAC*
  )
else()
  include(FindPkgConfig)
  pkg_check_modules(SDL REQUIRED sdl2)
  pkg_check_modules(SDL_MIXER SDL2_mixer)
  pkg_check_modules(SDL_NET SDL2_net)
  if(NOT SDL_MIXER_FOUND)
  	add_definitions(-DNO_SDL_MIXER)
  endif()
  if(NOT SDL_NET_FOUND)
  	add_definitions(-DNO_SDL_NET)
  endif()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} SYSTEM ${SDL_INCLUDE_DIRS} ${SDL_MIXER_INCLUDE_DIRS})
  link_directories(${SDL_LIBRARY_DIRS} ${SDL_MIXER_LIBRARY_DIRS})
  string (REPLACE ";" " " SDL_STATIC_LDFLAGS_STR "${SDL_STATIC_LDFLAGS} ${SDL_MIXER_STATIC_LDFLAGS} ${SDL_NET_STATIC_LDFLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${SDL_STATIC_LDFLAGS_STR}")
  add_executable(${PROJECT_NAME} ${SRC_LIST} jumpbump_dat.c version.h)
  target_link_libraries(${PROJECT_NAME} m ${SDL_LIBRARIES} ${SDL_MIXER_LIBRARIES})
endif()

