include(${CMAKE_CURRENT_LIST_DIR}/tdm_find_package.cmake)

set(libpng_FOUND 1)
set(libpng_INCLUDE_DIRS "${ARTEFACTS_DIR}/libpng/include")
set(libpng_LIBRARY_DIR "${ARTEFACTS_DIR}/libpng/lib/${PACKAGE_PLATFORM}")
if(MSVC)
	set(libpng_LIBRARIES "${libpng_LIBRARY_DIR}/libpng16.lib")
else()
	set(libpng_LIBRARIES "${libpng_LIBRARY_DIR}/libpng16.a")
endif()
