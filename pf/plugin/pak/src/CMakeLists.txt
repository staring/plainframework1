SET(TF_CURRENT_BINARY_PATH "../dll")

IF(CMAKE_SYSTEM MATCHES Linux)

ELSE(CMAKE_SYSTEM MATCHES Linux)
ENDIF(CMAKE_SYSTEM MATCHES Linux)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
LINK_DIRECTORIES(
  "./" #run dir
  "../../../dependencies/dll" #linux
  "../../../core/dll" #linux
)

SET(EXECUTABLE_OUTPUT_PATH ${TF_CURRENT_BINARY_PATH_BIN})


INCLUDE_DIRECTORIES("../../../core/include",
                    "../include",
                    "../../../dependencies/iconv/include",
                    "../../../dependencies/lua/src",
                    "../../../dependencies/",
                    "/usr/local/unixODBC/include",
                    "../misc")

##update_begin




SET (INCLUDE_PAK_LIST
	../include/pak/compress.h
	../include/pak/config.h
	../include/pak/encrypt.h
	../include/pak/file.h
	../include/pak/interface.h
	../include/pak/listfile.h
	../include/pak/util.h
)

SET (INCLUDE_LIST
)

SET (SRC_LIST
	../src/compress.cc
	../src/encrypt.cc
	../src/file.cc
	../src/interface.cc
	../src/listfile.cc
	../src/util.cc
)

SET (MISC_BZIP2_LIST
	../misc/bzip2/blocksort.c
	../misc/bzip2/bzlib.c
	../misc/bzip2/bzlib.h
	../misc/bzip2/bzlib_private.h
	../misc/bzip2/compress.c
	../misc/bzip2/crctable.c
	../misc/bzip2/decompress.c
	../misc/bzip2/huffman.c
	../misc/bzip2/randtable.c
)

SET (MISC_HUFFMAN_LIST
	../misc/huffman/huff.cpp
	../misc/huffman/huff.h
)

SET (MISC_PKLIB_LIST
	../misc/pklib/crc32.c
	../misc/pklib/explode.c
	../misc/pklib/implode.c
	../misc/pklib/pklib.h
)

SET (MISC_WAVE_LIST
	../misc/wave/wave.cpp
	../misc/wave/wave.h
)

SET (MISC_LIST
	../misc/crc32.cpp
	../misc/crc32.h
	../misc/md5.cpp
	../misc/md5.h
)



if (WIN32)
	source_group(include\\pak FILES ${INCLUDE_PAK_LIST})
	source_group(include FILES ${INCLUDE_LIST})
	source_group(src FILES ${SRC_LIST})
	source_group(misc\\bzip2 FILES ${MISC_BZIP2_LIST})
	source_group(misc\\huffman FILES ${MISC_HUFFMAN_LIST})
	source_group(misc\\pklib FILES ${MISC_PKLIB_LIST})
	source_group(misc\\wave FILES ${MISC_WAVE_LIST})
	source_group(misc FILES ${MISC_LIST})
endif()


SET(LIBRARY_OUTPUT_PATH ${TF_CURRENT_BINARY_PATH})
ADD_LIBRARY(plugin_pak SHARED
	${INCLUDE_PAK_LIST}
	${INCLUDE_LIST}
	${SRC_LIST}
	${MISC_BZIP2_LIST}
	${MISC_HUFFMAN_LIST}
	${MISC_PKLIB_LIST}
	${MISC_WAVE_LIST}
	${MISC_LIST}
)


##update_end

if (WIN32)
  TARGET_LINK_LIBRARIES(plugin_pak ws2_32.lib odbc32.lib odbccp32.lib lua.lib iconv.lib zlib.lib)
else()
  TARGET_LINK_LIBRARIES(plugin_pak pf_core iconv lua zlib)
  if(USE_32BITS)
    message(STATUS "using 32bits")
    SET(CMAKE_C_FLAGS "-Wall -ggdb -pipe -march=i386 -mtune=i686 -fPIC")
    SET(CMAKE_CXX_FLAGS "-Wall -ggdb -pipe -march=i386 -mtune=i686 -fPIC")
  else()
    message(STATUS "using 64bits")
    SET(CMAKE_C_FLAGS "-Wall -ggdb -pipe -march=x86-64 -mtune=i686 -fPIC")
    SET(CMAKE_CXX_FLAGS "-Wall -ggdb -pipe -march=x86-64 -mtune=i686 -fPIC")
  endif(USE_32BITS)
endif(WIN32)
