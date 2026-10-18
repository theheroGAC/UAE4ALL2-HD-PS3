cmake_minimum_required(VERSION 3.10)
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(VERSION_MAJOR 2)
set(VERSION_MINOR 12)
option(BUILD_CDROM_TESTS "Build CD-ROM backend tests" OFF)

option(BUILD_PS3 "Build for Sony PlayStation 3 (PSL1GHT)" OFF)
# PS3 TITLE_ID: 4 uppercase letters + 5 digits. Must match pkgbuild/sfo.xml
# and the CONTENT_ID used by tools/make_ps3_release.sh.
set(PS3_TITLE_ID "UAEH00001" CACHE STRING "PS3 TITLE_ID (AAAA00000)")

if(NOT BUILD_PS3)
    if(vita OR DEFINED ENV{VITASDK})
        set(BUILD_PSP2 ON)
    endif()
endif()

if(BUILD_PSP2)
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else ()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif ()
endif ()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_DEBUG true CACHE BOOL "Build with psp2shell support")
endif()
endif(BUILD_PSP2)

if(BUILD_PS3)
  add_compile_definitions(__POSIX_THREADS=200112L)
    if(NOT DEFINED PS3DEV)
        if(DEFINED ENV{PS3DEV})
            set(PS3DEV "$ENV{PS3DEV}")
        else()
            set(PS3DEV "/usr/local/ps3dev")
        endif()
    endif()
    if(NOT DEFINED ENV{PSL1GHT})
        set(ENV{PSL1GHT} "${PS3DEV}")
    endif()
    set(CMAKE_SYSTEM_NAME "Generic")
    set(CMAKE_SYSTEM_PROCESSOR "powerpc64")
    set(CMAKE_C_COMPILER_WORKS 1)
    set(CMAKE_CXX_COMPILER_WORKS 1)
    set(CMAKE_C_COMPILER "${PS3DEV}/ppu/bin/powerpc64-ps3-elf-gcc")
    set(CMAKE_CXX_COMPILER "${PS3DEV}/ppu/bin/powerpc64-ps3-elf-g++")
    set(CMAKE_ASM_COMPILER "${PS3DEV}/ppu/bin/powerpc64-ps3-elf-gcc")
    set(CMAKE_AR "${PS3DEV}/ppu/bin/powerpc64-ps3-elf-ar" CACHE STRING "" FORCE)
    set(CMAKE_RANLIB "${PS3DEV}/ppu/bin/powerpc64-ps3-elf-ranlib" CACHE STRING "" FORCE)
    set(CMAKE_FIND_ROOT_PATH ${PS3DEV} ${PS3DEV}/ppu ${PS3DEV}/portlibs/ppu)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available")
    link_directories(${PS3DEV}/ppu/lib ${PS3DEV}/portlibs/ppu/lib)
endif(BUILD_PS3)

execute_process(COMMAND "date" "+%Y.%m.%d"
        OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

project(uae4all2)

if(BUILD_PSP2)
  enable_language(ASM)

  include("${VITASDK}/share/vita.cmake" REQUIRED)
endif(BUILD_PSP2)

set(TITLEID "UAE4ALLHD")

set(SRC_FILES
        src/vkbd/vkbd.cpp
        src/audio.cpp
        src/autoconf.cpp
        src/blitfunc.cpp
        src/blittable.cpp
        src/blitter.cpp
        src/cfgfile.cpp
        src/rdb.cpp
        src/cia.cpp
        src/savedisk.cpp
        src/savestate.cpp
        src/custom.cpp
        src/disk.cpp
        src/drawing.cpp
        src/ersatz.cpp
        src/expansion.cpp
        src/filesys.cpp
        src/fsdb.cpp
        src/fsdb_unix.cpp
        src/fsusage.cpp
        src/gfxutil.cpp
        src/hardfile.cpp
        src/keybuf.cpp
        src/main.cpp
        src/memory.cpp
        src/missing.cpp
        src/native2amiga.cpp
        src/picasso96.cpp
        src/gui.cpp
        src/od-joy.cpp
        src/scsi-none.cpp
        src/sdlgfx.cpp
        src/writelog.cpp
        src/unzip.c
        src/ioapi.c
        src/zfile.cpp
        src/menu/fade.cpp
        src/gp2x/gp2x.cpp
        src/gp2x/inputmode.cpp
        src/gp2x/menu/menu_helper.cpp
        src/gp2x/menu/menu_config.cpp
        src/gp2x/menu/menu.cpp
        src/gp2x/menu/menu_fileinfo.cpp
        src/gp2x/menu/menu_load.cpp
        src/gp2x/menu/menu_main.cpp
        src/gp2x/menu/menu_savestates.cpp
        src/gp2x/menu/menu_misc.cpp
        src/gp2x/menu/menu_controls.cpp
        src/gp2x/menu/menu_display.cpp
        src/gp2x/menu/menu_memory_disk.cpp
        src/sound_sdl_new.cpp
        src/disk_sound.cpp
        src/midi_synth.cpp
        src/cdrom.cpp
        src/osd.cpp
        src/akiko.cpp
        src/capsimg/CAPSImg/CapsAPI.cpp
        src/capsimg/CAPSImg/CapsFDCEmulator.cpp
        src/capsimg/CAPSImg/CapsFile.cpp
        src/capsimg/CAPSImg/CapsFormatMFM.cpp
        src/capsimg/CAPSImg/CapsImage.cpp
        src/capsimg/CAPSImg/CapsImageStd.cpp
        src/capsimg/CAPSImg/CapsLoader.cpp
        src/capsimg/CAPSImg/DiskImage.cpp
        src/capsimg/CAPSImg/DiskImageFactory.cpp
        src/capsimg/CAPSImg/StreamCueImage.cpp
        src/capsimg/CAPSImg/StreamImage.cpp
        src/capsimg/Codec/CTRawCodec.cpp
        src/capsimg/Codec/CTRawCodecDecompressor.cpp
        src/capsimg/Codec/DiskEncoding.cpp
        src/capsimg/Core/BaseFile.cpp
        src/capsimg/Core/BitBuffer.cpp
        src/capsimg/Core/CRC.cpp
        src/capsimg/Core/DiskFile.cpp
        src/capsimg/Core/MemoryFile.cpp
        src/m68k/fame/famec.cpp
        src/m68k/fame/m68k_intrf.cpp
        src/m68k/m68k_cmn_intrf.cpp
)

include_directories(
        ../common/include
        include
        src
        src/ps3
        src/gp2x
        src/menu
        src/include
        src/gp2x/menu
        src/vkbd
        src/capsimg/CAPSImg
        src/capsimg/Codec
        src/capsimg/Core
        src/capsimg/Device
        src/capsimg/LibIPF
)

if(BUILD_PSP2)
  set(PSP2_SRC_FILES
    src/psp2/uae_gui_vita.cpp
    src/psp2/ipf_disk.cpp
    src/psp2/uae_gui_views.cpp
    src/psp2/uae_gui_browser.cpp
    src/psp2/hdf_manager.cpp
    src/psp2/whdload_manager.cpp
    src/psp2/cover_downloader.cpp
    src/psp2/ftp_server.cpp
    src/psp2/psp2_shader.cpp
    src/psp2/psp2-dirent.c
    src/psp2/psp2_touch.c
    src/psp2/psp2_kbdvita.c
    src/neon_helper.s
    src/gp2x/memset.s
    src/gp2x/memcpy.s
  )  
  set(SRC_FILES ${PSP2_SRC_FILES} ${SRC_FILES})

  include_directories(
    src/psp2
    ${VITASDK}/arm-vita-eabi/include/SDL
  )

  set(FLAGS
          -fno-lto
          -Wno-write-strings
          -fno-strict-aliasing
          -DVERSION=\"${BUILD_DATE}\"
          -DGP2X -DPANDORA -D__PSP2__ -DUSE_AUTOCONFIG -DUSE_ZFILE -DUSE_FAME_CORE -DWITH_TESTMODE
          -DUSE_SDL -DGCCCONSTFUNC="__attribute__\(\(const\)\)" -DUSE_UNDERSCORE
          -DSHM_SUPPORT_LINKS=0 -DUNALIGNED_PROFITABLE -DOPTIMIZED_FLAGS -DOS_WITHOUT_MEMORY_MANAGEMENT
          -DROM_PATH_PREFIX=\"ux0:/data/uae4all/roms/\" -DDATA_PREFIX=\"app0:/data/\" -DSAVE_PREFIX=\"ux0:/data/uae4all/saves/\" -DTHUMB_PREFIX=\"ux0:/data/uae4all/thumbs/\" -DTMP_PREFIX=\"ux0:/data/uae4all/tmp/\"
          -DUSE_UAE4ALL_VKBD
          -DUSE_ARMV7
          -DUSE_ARMNEON
          -DVERSION_MAJOR=\"${VERSION_MAJOR}\"
          -DVERSION_MINOR=\"${VERSION_MINOR}\"
          -marm -mfpu=neon -mtune=cortex-a9 -mfloat-abi=hard -ffast-math
          -fno-asynchronous-unwind-tables
          -mword-relocations -fno-unwind-tables -fno-optimize-sibling-calls
          -mlittle-endian 
  )

  set(LDFLAGS
          -fno-lto
          SDL SDL_image SDL_ttf freetype bz2 harfbuzz vorbisfile vorbis ogg
          ${CMAKE_SOURCE_DIR}/src/psp2/vita2d_fbo/lib/libvita2d.a
          ${CMAKE_SOURCE_DIR}/src/psp2/vita-shader-collection/lib/libvitashaders.a
          SceSysmodule_stub SceDisplay_stub SceGxm_stub
          SceCtrl_stub ScePgf_stub ScePower_stub SceCommonDialog_stub
          SceAudio_stub SceShellSvc_stub SceHid_stub SceTouch_stub png jpeg webp archive pthread z m c
          SceAppUtil_stub SceAppMgr_stub
          SceHttp_stub SceNet_stub SceNetCtl_stub SceSsl_stub ftpvita
  )

  if (BUILD_DEBUG)
      list(APPEND FLAGS -DDEBUG -DDEBUG_UAE4ALL -DUAE_CONSOLE)
      list(APPEND LDFLAGS psp2shell SceSysmodule_stub SceNetCtl_stub
              SceNet_stub SceAppUtil_stub SceAppMgr_stub
      )
      set(UNSAFE_FLAG UNSAFE)
  else ()
      list(APPEND FLAGS -O2)
  endif (BUILD_DEBUG)

  add_executable(${PROJECT_NAME} ${SRC_FILES})
  set_source_files_properties(src/m68k/fame/famec.cpp PROPERTIES COMPILE_FLAGS "-O1 -fno-unroll-loops -fno-inline")
  target_compile_options(${PROJECT_NAME} PRIVATE ${FLAGS})
  target_link_libraries(${PROJECT_NAME} ${LDFLAGS})

  vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
  add_custom_target(${PROJECT_NAME}.vpk
          DEPENDS ${PROJECT_NAME}.self-self
          COMMAND ${CMAKE_COMMAND} -E echo "[VPK] Generating param.sfo"
          COMMAND ${VITASDK}/bin/vita-mksfoex -d ATTRIBUTE2=12 -s TITLE_ID=${TITLEID} -s APP_VER=01.07 "UAE4All2 HD" param.sfo
          COMMAND ${CMAKE_COMMAND} -E echo "[VPK] Packaging uae4all2hd.vpk"
          COMMAND ${VITASDK}/bin/vita-pack-vpk -s param.sfo -b ${PROJECT_NAME}.self -a ${CMAKE_SOURCE_DIR}/psp2data/data=data -a ${CMAKE_SOURCE_DIR}/psp2data/sce_sys=sce_sys uae4all2hd.vpk
  )
  add_custom_target(AmigaGame.vpk DEPENDS ${PROJECT_NAME}.vpk)
endif(BUILD_PSP2)

if(BUILD_PS3)
  set(PS3_SRC_FILES
    src/ps3/ps3_input.c
    src/ps3/ps3_touch.c
    src/ps3/sdl2_to_sdl1.cpp
    src/ps3/ps3_kbd.c
    src/ps3/ps3_compat.c
    src/ps3/ps3_paths.c
    src/ps3/ps3_log.c
    src/ps3/ps3_diag.c
    src/ps3/ps3_netstub.c
    src/ps3/ps3_libarchive_stub.c
    src/ps3/uae_gui_ps3.cpp
    src/ps3/uae_gui_views_ps3.cpp
    src/ps3/uae_gui_browser_ps3.cpp
    src/ps3/whdload_manager_ps3.cpp
    src/ps3/hdf_manager_ps3.cpp
    src/ps3/m3u_manager.cpp
    src/ps3/cover_manager_ps3.cpp
    src/ps3/game_metadata_ps3.cpp
    src/ps3/ipf_disk.cpp
    src/stb_vorbis.c
  )
  set(SRC_FILES ${PS3_SRC_FILES} ${SRC_FILES})

  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(CHDR_WANT_TESTS OFF CACHE BOOL "" FORCE)
  set(INSTALL_STATIC_LIBS OFF CACHE BOOL "" FORCE)
  add_subdirectory(src/libchdr EXCLUDE_FROM_ALL)
  target_compile_options(chdr-static PRIVATE -O2)
  target_compile_options(chdr-lzma PRIVATE -O2)
  target_compile_options(miniz PRIVATE -O2)
  target_compile_options(zstd PRIVATE -O2)

  include_directories(
    src/libchdr/include
    src/libchdr/include/libchdr
    ${PS3DEV}/portlibs/ppu/include/SDL2
    ${PS3DEV}/ppu/include
    ${PS3DEV}/portlibs/ppu/include
    ${PS3DEV}/portlibs/ppu/include/curl
    ${PS3DEV}/portlibs/ppu/include
  )
  
  add_definitions(
    -DVERSION=\"${BUILD_DATE}\"
    -DGP2X
    -DPANDORA
    -D__PS3__
    -D__BIG_ENDIAN__
    -DPS3_TITLE_ID=\"${PS3_TITLE_ID}\"
    -D__64BIT__
    -DUSE_AUTOCONFIG
    -DUSE_ZFILE
    -DUSE_FAME_CORE
    -DWITH_TESTMODE
    -DUSE_SDL
    -DUSE_SDL2
    -DGCCCONSTFUNC=\"__attribute__\(\(const\)\)\"
    -DUSE_UNDERSCORE
    -DSHM_SUPPORT_LINKS=0
    -DUNALIGNED_PROFITABLE
    -DOPTIMIZED_FLAGS
    -DOS_WITHOUT_MEMORY_MANAGEMENT
    -DROM_PATH_PREFIX=\"./roms/\"
    -DDATA_PREFIX=\"./data/\"
    -DSAVE_PREFIX=\"./saves/\"
    -DTHUMB_PREFIX=\"./thumbs/\"
    -DTMP_PREFIX=\"./tmp/\"
    -DUSE_UAE4ALL_VKBD
    -DPICASSO96
    -DVERSION_MAJOR=\"${VERSION_MAJOR}\"
    -DVERSION_MINOR=\"${VERSION_MINOR}\"
  )

  set(FLAGS
    -Wno-write-strings
    -O2
    -fno-strict-aliasing
    -fno-tree-vectorize
    -fno-tree-slp-vectorize
    -ffast-math
    -fno-asynchronous-unwind-tables
    -fno-unwind-tables
    -fno-optimize-sibling-calls
    -funsafe-math-optimizations
    -ffunction-sections
    -fno-var-tracking
    -fno-var-tracking-assignments
  )
  
  set(LIBS
    SDL2
    SDL2main
    SDL2_image
    SDL2_gfx
    SDL2_ttf
    SDL2_mixer
    freetype
    png
    jpeg
    curl
    pthread
    -Wl,--allow-multiple-definition
    polarssl
    sysmodule
    bzip2

    z
    m
    c
    stdc++
    chdr-static
    rsx
    sysutil
    netctl
    io
    audio

    lv2
    rt
    gcm_sys
  )

  foreach(flag ${FLAGS})
    set(FLAGS_GENERAL "${FLAGS_GENERAL} ${flag}")
  endforeach(flag ${FLAGS})

  #set(CMAKE_C_FLAGS "${FLAGS_GENERAL}" CACHE STRING "C Flags" FORCE)
  #set(CMAKE_CXX_FLAGS "${FLAGS_GENERAL}" CACHE STRING "C++ Flags" FORCE)

  set_source_files_properties(src/m68k/fame/famec.cpp PROPERTIES COMPILE_FLAGS "-O1 -fno-inline -fno-inline-functions-called-once -fno-inline-small-functions -fno-ipa-cp -fno-ipa-sra -fno-ipa-pure-const -fno-ipa-reference -fno-toplevel-reorder -fno-strict-aliasing -fno-unroll-loops -fno-var-tracking -fno-var-tracking-assignments --param ggc-min-expand=10 --param ggc-min-heapsize=8192")
  set_source_files_properties(src/ps3/uae_gui_views_ps3.cpp PROPERTIES COMPILE_FLAGS "-O0 -fno-inline -fno-var-tracking -fno-var-tracking-assignments")
  add_executable(${PROJECT_NAME}.elf ${SRC_FILES})
  target_compile_options(${PROJECT_NAME}.elf PRIVATE ${FLAGS})
  target_link_directories(${PROJECT_NAME}.elf PRIVATE ${DEVKITPRO}/libnx/lib ${DEVKITPRO}/portlibs/switch/lib)

  target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${LIBS})
  # Same pipeline as PSL1GHT's ppu_rules: strip -> sprxlinker -> make_self.
  # sprxlinker is mandatory: it fills the PRX import stubs, without it the
  # executable crashes as soon as it calls a system library.
  add_custom_target(${PROJECT_NAME}.self
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND ${PS3DEV}/ppu/bin/powerpc64-ps3-elf-strip ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf -o ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.stripped.elf
    COMMAND ${PS3DEV}/bin/sprxlinker ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.stripped.elf
    COMMAND ${PS3DEV}/bin/make_self ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.stripped.elf ${CMAKE_BINARY_DIR}/uae4all2hd.self
  )
  add_custom_target(${PROJECT_NAME}_ps3
    DEPENDS ${PROJECT_NAME}.self
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/roms
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/kickstarts
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/saves
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/uae4all2hd.self ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/uae4all2hd.self
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/ps3data/data ${CMAKE_BINARY_DIR}/release/${PROJECT_NAME}/data
    COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/uae4all2hd_ps3.tar.gz --format=gnu -C ${CMAKE_BINARY_DIR}/release ${PROJECT_NAME}
  )
endif(BUILD_PS3)

if(BUILD_CDROM_TESTS AND NOT BUILD_PSP2 AND NOT BUILD_PS3)
  enable_testing()
  add_executable(cdrom_tests tests/cdrom_tests.cpp src/cdrom.cpp)
  target_include_directories(cdrom_tests PRIVATE src src/include)
  add_test(NAME cdrom_backend COMMAND cdrom_tests)
endif()

