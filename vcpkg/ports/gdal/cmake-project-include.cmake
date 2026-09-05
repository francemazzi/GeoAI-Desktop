# SWIG 4.5.0 (released 2026-08-21, two days after Strata 1.4.2) dropped the
# Python 2 PyInt_* compatibility macros that GDAL 3.12 typemaps still emit.
# 1.4.2 never rebuilt GDAL from source (NuGet cache hit). When the cache
# misses, restore the aliases so source builds succeed on current runners.
add_compile_definitions(
    PyInt_FromLong=PyLong_FromLong
    PyInt_AsLong=PyLong_AsLong
    PyInt_Check=PyLong_Check
)

if(GDAL_USE_KEA)
    find_package(Kealib CONFIG REQUIRED)
    add_library(KEA::KEA ALIAS Kealib::Kealib)
    set(GDAL_CHECK_PACKAGE_KEA_NAMES Kealib CACHE INTERNAL "vcpkg")
    set(GDAL_CHECK_PACKAGE_KEA_TARGETS Kealib::Kealib CACHE INTERNAL "vcpkg")
endif()

if(GDAL_USE_WEBP)
    find_package(WebP CONFIG REQUIRED)
    add_library(WEBP::WebP ALIAS WebP::webp)
    set(GDAL_CHECK_PACKAGE_WebP_NAMES WebP CACHE INTERNAL "vcpkg")
    set(GDAL_CHECK_PACKAGE_WebP_TARGETS WebP::webp CACHE INTERNAL "vcpkg")
endif()

if(GDAL_USE_ARROW)
    find_package(Arrow REQUIRED)
    set(ARROW_USE_STATIC_LIBRARIES "${ARROW_BUILD_STATIC}" CACHE INTERNAL "")
    set(GDAL_USE_ARROWDATASET "${ARROW_DATASET}" CACHE INTERNAL "")
    set(GDAL_USE_ARROWCOMPUTE "${ARROW_COMPUTE}" CACHE INTERNAL "")
endif()

if(GDAL_USE_SQLITE3)
    # CMake find module with vcpkg cmake wrapper
    find_package(SQLite3 REQUIRED)
    # .. and inject into GDAL's FindSQLite3.cmake
    set(SQLite3_LIBRARY "${SQLite3_LIBRARIES}")
    set(SQLite3_FOUND FALSE)
    set(SQLITE3_FOUND FALSE)
endif()
