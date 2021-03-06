include(${CMAKE_CURRENT_LIST_DIR}/tdm_find_package.cmake)

set(openal_FOUND 1)
set(openal_INCLUDE_DIRS "${ARTEFACTS_DIR}/openal/include")
set(openal_LIBRARY_DIR "${ARTEFACTS_DIR}/openal/lib/${PACKAGE_PLATFORM}")
set(openal_LIBRARY_D_DIR "${ARTEFACTS_DIR}/openal/lib/${PACKAGE_PLATFORM_DEBUG}")
if(MSVC)
	set(openal_LIBRARIES "${openal_LIBRARY_DIR}/OpenAL32.lib")
    set(openal_LIBRARIES_D "${openal_LIBRARY_D_DIR}/OpenAL32.lib")
else()
	set(openal_LIBRARIES "${openal_LIBRARY_DIR}/libopenal.a")
    set(openal_LIBRARIES_D "${openal_LIBRARY_D_DIR}/libopenal.a")
endif()
