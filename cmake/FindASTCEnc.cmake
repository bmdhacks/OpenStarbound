# Variables defined by this module:
#
#  ASTCENC_LIBRARY          The ASTC Encoder library
#  ASTCENC_INCLUDE_DIR      The ASTC Encoder header file

find_library(ASTCENC_LIBRARY
    NAMES libastcenc-neon-static libastcenc-static libascenc
)

find_path(ASTCENC_INCLUDE_DIR
    NAMES libastenc.h
    PATHS ${CMAKE_INCLUDE_PATH} /usr/local/include /usr/include
)

mark_as_advanced(
    ASTCENC_LIBRARY
    ASTCENC_INCLUDE_DIR
)
