cmake_minimum_required (VERSION 2.8)
set(CMAKE_BUILD_TYPE Debug)
project(xel_miner)

message(STATUS "CMAKE - xel_miner")

FIND_PACKAGE(PackageHandleStandardArgs)

SET (OPENCL_VERSION_STRING "0.1.0")
SET (OPENCL_VERSION_MAJOR 0)
SET (OPENCL_VERSION_MINOR 1)
SET (OPENCL_VERSION_PATCH 0)

IF (APPLE)

	# IF OpenCL_LIBPATH is given use it and don't use default path
	IF (DEFINED ENV{OpenCL_LIBPATH})
		FIND_LIBRARY(OPENCL_LIBRARIES OpenCL PATHS ENV OpenCL_LIBPATH NO_DEFAULT_PATH)
	ELSE ()
		FIND_LIBRARY(OPENCL_LIBRARIES OpenCL DOC "OpenCL lib for OSX")
	ENDIF ()

	# IF OpenCL_INCPATH is given use it and find for CL/cl.h and OpenCL/cl.h do not try to find default paths
	IF (DEFINED ENV{OpenCL_INCPATH})
		FIND_PATH(OPENCL_INCLUDE_DIRS CL/cl.h OpenCL/cl.h PATHS ENV OpenCL_INCPATH NO_DEFAULT_PATH)
		FIND_PATH(_OPENCL_CPP_INCLUDE_DIRS CL/cl.hpp OpenCL/cl.hpp PATHS ${OPENCL_INCLUDE_DIRS} NO_DEFAULT_PATH)
	ELSE ()
		FIND_PATH(OPENCL_INCLUDE_DIRS OpenCL/cl.h DOC "Include for OpenCL on OSX")
		FIND_PATH(_OPENCL_CPP_INCLUDE_DIRS OpenCL/cl.hpp DOC "Include for OpenCL CPP bindings on OSX")
	ENDIF ()

ELSE (APPLE)

	IF (WIN32)

		# Find OpenCL includes and libraries from environment variables provided by vendor
		SET(OPENCL_INCLUDE_SEARCH_PATHS)
		SET(OPENCL_LIBRARY_SEARCH_PATHS)
		SET(OPENCL_LIBRARY_64_SEARCH_PATHS)

		# Nvidia
		IF (DEFINED ENV{CUDA_INC_PATH})
			SET(OPENCL_INCLUDE_SEARCH_PATHS ${OPENCL_INCLUDE_SEARCH_PATHS} $ENV{CUDA_INC_PATH})
			SET(OPENCL_LIBRARY_64_SEARCH_PATHS ${OPENCL_LIBRARY_64_SEARCH_PATHS} $ENV{CUDA_LIB_PATH}/../lib64)
			SET(OPENCL_LIBRARY_SEARCH_PATHS ${OPENCL_LIBRARY_SEARCH_PATHS} $ENV{CUDA_LIB_PATH}/../lib)
		ENDIF()

		# Intel SDK
		IF (DEFINED ENV{INTELOCSDKROOT})
			SET(OPENCL_INCLUDE_SEARCH_PATHS ${OPENCL_INCLUDE_SEARCH_PATHS} $ENV{INTELOCSDKROOT}/include)
			SET(OPENCL_LIBRARY_64_SEARCH_PATHS ${OPENCL_LIBRARY_64_SEARCH_PATHS} $ENV{INTELOCSDKROOT}/lib/x64)
			SET(OPENCL_LIBRARY_SEARCH_PATHS ${OPENCL_LIBRARY_SEARCH_PATHS} $ENV{INTELOCSDKROOT}/lib/x86)
		ENDIF()

		# AMD SDK
		IF (DEFINED ENV{AMDAPPSDKROOT})
			SET(OPENCL_INCLUDE_SEARCH_PATHS ${OPENCL_INCLUDE_SEARCH_PATHS} $ENV{AMDAPPSDKROOT}/include)
			SET(OPENCL_LIBRARY_64_SEARCH_PATHS ${OPENCL_LIBRARY_64_SEARCH_PATHS} $ENV{AMDAPPSDKROOT}/lib/x86_64)
			SET(OPENCL_LIBRARY_SEARCH_PATHS ${OPENCL_LIBRARY_SEARCH_PATHS} $ENV{AMDAPPSDKROOT}/lib/x86)
		ENDIF()

		# Override search paths with OpenCL_INCPATH env variable
		IF (DEFINED ENV{OpenCL_INCPATH})
			SET(OPENCL_INCLUDE_SEARCH_PATHS $ENV{OpenCL_INCPATH})
		ENDIF ()

		# Override search paths with OpenCL_LIBPATH env variable
		IF (DEFINED ENV{OpenCL_INCPATH})
			SET(OPENCL_LIBRARY_SEARCH_PATHS $ENV{OpenCL_LIBPATH})
			SET(OPENCL_LIBRARY_64_SEARCH_PATHS $ENV{OpenCL_LIBPATH})
		ENDIF ()

		FIND_PATH(OPENCL_INCLUDE_DIRS CL/cl.h PATHS ${OPENCL_INCLUDE_SEARCH_PATHS})
		FIND_PATH(_OPENCL_CPP_INCLUDE_DIRS CL/cl.hpp PATHS ${OPENCL_INCLUDE_SEARCH_PATHS})
		
		FIND_LIBRARY(_OPENCL_32_LIBRARIES OpenCL.lib HINTS ${OPENCL_LIBRARY_SEARCH_PATHS} PATHS ${OPENCL_LIB_DIR} ENV PATH)
		FIND_LIBRARY(_OPENCL_64_LIBRARIES OpenCL.lib HINTS ${OPENCL_LIBRARY_64_SEARCH_PATHS} PATHS ${OPENCL_LIB_DIR} ENV PATH)

		# Check if 64bit or 32bit versions links fine
  		SET (_OPENCL_VERSION_SOURCE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/openclversion.c")
  		#SET (_OPENCL_VERSION_SOURCE "${CMAKE_BINARY_DIR}/test.c")
		FILE (WRITE "${_OPENCL_VERSION_SOURCE}"
			"
			#if __APPLE__
			#include <OpenCL/cl.h>
			#else /* !__APPLE__ */
			#include <CL/cl.h>
			#endif /* __APPLE__ */
			int main()
			{	
			    cl_int result;
			    cl_platform_id id;
			    result = clGetPlatformIDs(1, &id, NULL);
			    return result != CL_SUCCESS;
			}
			")

  		TRY_COMPILE(_OPENCL_64_COMPILE_SUCCESS ${CMAKE_BINARY_DIR} "${_OPENCL_VERSION_SOURCE}"
			CMAKE_FLAGS
			"-DINCLUDE_DIRECTORIES:STRING=${OPENCL_INCLUDE_DIRS}"
			CMAKE_FLAGS
			"-DLINK_LIBRARIES:STRING=${_OPENCL_64_LIBRARIES}"
  		)

		IF(_OPENCL_64_COMPILE_SUCCESS)
			message(STATUS "  OpenCL 64bit lib found.")
			SET(OPENCL_LIBRARIES ${_OPENCL_64_LIBRARIES})
  		ELSE()
	  		TRY_COMPILE(_OPENCL_32_COMPILE_SUCCESS ${CMAKE_BINARY_DIR} "${_OPENCL_VERSION_SOURCE}"
				CMAKE_FLAGS
				"-DINCLUDE_DIRECTORIES:STRING=${OPENCL_INCLUDE_DIRS}"
				CMAKE_FLAGS
				"-DLINK_LIBRARIES:STRING=${_OPENCL_32_LIBRARIES}"
	  		)
			IF(_OPENCL_32_COMPILE_SUCCESS)
				message(STATUS "  OpenCL 32bit lib found.")
				SET(OPENCL_LIBRARIES ${_OPENCL_32_LIBRARIES})
			ELSE()
				message(STATUS "  Couldn't link opencl..")
			ENDIF()
		ENDIF()


	ELSE (WIN32)
  
  		IF (CYGWIN)
    		SET (CMAKE_FIND_LIBRARY_SUFFIXES .lib)
    		SET (OCL_LIB_SUFFIX .lib)
  		ENDIF (CYGWIN)

		# Unix style platforms
		FIND_LIBRARY(OPENCL_LIBRARIES OpenCL${OCL_LIB_SUFFIX}
			PATHS ENV LD_LIBRARY_PATH ENV OpenCL_LIBPATH
		)

		GET_FILENAME_COMPONENT(OPENCL_LIB_DIR ${OPENCL_LIBRARIES} PATH)
		GET_FILENAME_COMPONENT(_OPENCL_INC_CAND ${OPENCL_LIB_DIR}/../../include ABSOLUTE)

		# The AMD SDK currently does not place its headers
		# in /usr/include, therefore also search relative
		# to the library
		FIND_PATH(OPENCL_INCLUDE_DIRS CL/cl.h PATHS ${_OPENCL_INC_CAND} "/usr/local/cuda/include" "/opt/AMDAPP/include" ENV OpenCL_INCPATH)
		FIND_PATH(_OPENCL_CPP_INCLUDE_DIRS CL/cl.hpp PATHS ${_OPENCL_INC_CAND} "/usr/local/cuda/include" "/opt/AMDAPP/include" ENV OpenCL_INCPATH)

	ENDIF (WIN32)

ENDIF (APPLE)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenCL DEFAULT_MSG OPENCL_LIBRARIES OPENCL_INCLUDE_DIRS)

IF(_OPENCL_CPP_INCLUDE_DIRS)
	SET( OPENCL_HAS_CPP_BINDINGS TRUE )
	LIST( APPEND OPENCL_INCLUDE_DIRS ${_OPENCL_CPP_INCLUDE_DIRS} )
	# This is often the same, so clean up
	LIST( REMOVE_DUPLICATES OPENCL_INCLUDE_DIRS )
ENDIF(_OPENCL_CPP_INCLUDE_DIRS)

MARK_AS_ADVANCED(
  OPENCL_INCLUDE_DIRS
)

message(STATUS "  OpenCL found: ${OPENCL_FOUND}")
message(STATUS "  OpenCL includes: ${OPENCL_INCLUDE_DIRS}")
message(STATUS "  OpenCL CXX includes: ${OPENCL_HAS_CPP_BINDINGS}")
message(STATUS "  OpenCL libraries: ${OPENCL_LIBRARIES}")

# Confirm CURL Has Been Installed
find_path( CURL_INCLUDE_DIR curl/curl.h  PATHS ${PC_CURL_INCLUDEDIR} ${PC_CURL_INCLUDE_DIRS} PATH_SUFFIXES include )
find_library( CURL_LIBRARIES NAMES curl PATHS ${PC_CURL_LIBDIR} ${PC_CURL_LIBRARY_DIRS} PATH_SUFFIXES lib )
find_package(OpenSSL REQUIRED)

IF (CURL_INCLUDE_DIR AND CURL_LIBRARIES)
	SET(CURL_FOUND TRUE)
ELSE (CURL_INCLUDE_DIR AND CURL_LIBRARIES)
	SET(CURL_FOUND FALSE)
ENDIF (CURL_INCLUDE_DIR AND CURL_LIBRARIES)

IF (CURL_FOUND)
	message(STATUS "  CURL:   Found")
ELSE (CURL_FOUND)
	message(STATUS "  CURL:   Not Found!")
ENDIF (CURL_FOUND)

# Confirm CRYPTO Has Been Installed
find_library(CRYPTO_LIB libcrypto.a REQUIRED)

IF (CRYPTO_LIB)
	message(STATUS "  Crypto: Found")
ELSE (CRYPTO_LIB)
	message(STATUS "  Crypto: Not Found!")
ENDIF (CRYPTO_LIB)

# Confirm SSL Has Been Installed
find_library(SSL_LIBRARY libssl.a REQUIRED)

IF (SSL_LIBRARY)
	message(STATUS "  SSL:    Found")
ELSE (SSL_LIBRARY)
	message(STATUS "  SSL:    Not Found!")
ENDIF (SSL_LIBRARY)

add_subdirectory(compat/jansson)
add_subdirectory(ElasticPL)

IF (OPENCL_FOUND)
	add_definitions(-DUSE_OPENCL)
	include_directories(${OPENCL_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/algo ${PROJECT_SOURCE_DIR}/compat/jansson ${CURL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
ELSE (OPENCL_FOUND)
	include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/algo ${PROJECT_SOURCE_DIR}/compat/jansson ${CURL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
ENDIF(OPENCL_FOUND)

set(SRC_LIST	xel_miner.c
				xel_supernode.c
				xel_compiler.c
				util.c
				ocl.c
				./ElasticPL/ElasticPL.c
				./ElasticPL/ElasticPLTokenManager.c
				./ElasticPL/ElasticPLParser.c
				./ElasticPL/ElasticPLInterpreter.c
				./ElasticPL/ElasticPLMath.c
				./ElasticPL/ElasticPLConvert.c
				./crypto/curve25519-donna.c
				./crypto/sha2.c
)
			
set(TARGET_NAME xel_miner)

IF ( WIN32 )
	add_executable(${TARGET_NAME} ${SRC_LIST} ./compat/winansi.c)
ELSE ( WIN32 )
	add_executable(${TARGET_NAME} ${SRC_LIST})
ENDIF ( WIN32 )

IF (OPENCL_FOUND)
	target_link_libraries(${TARGET_NAME} crypto pthread ${CURL_LIBRARIES} ${OPENCL_LIBRARIES} jansson)
ELSE (OPENCL_FOUND)
	target_link_libraries(${TARGET_NAME} crypto pthread ${CURL_LIBRARIES} jansson)
ENDIF(OPENCL_FOUND)

IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
	target_link_libraries(${TARGET_NAME} m dl)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -fPIC -g")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -fPIC -g")
ELSEIF( WIN32 )
	target_link_libraries(${TARGET_NAME} libcurl Ws2_32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -msse -msse2 -msse3 -mmmx -m3dnow -DHAVE_STRUCT_TIMESPEC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -msse -msse2 -msse3 -mmmx -m3dnow -DHAVE_STRUCT_TIMESPEC")
ELSEIF ( CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)" )
	target_link_libraries(${TARGET_NAME} m dl)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -msse -msse2 -msse3 -mmmx -m3dnow -fPIC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -fPIC")
ELSEIF ( UNIX )
	target_link_libraries(${TARGET_NAME} m dl)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -fPIC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -fPIC")
ENDIF()

set_target_properties( ${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR} )