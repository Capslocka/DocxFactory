cmake_minimum_required(VERSION 3.16)

project(DocxFactory VERSION 1.2.18 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS
	"Build shared libraries instead of static libraries" OFF)

option(DOCXFACTORY_BUILD_TOOLS    "Build the command line tools"             ON)
option(DOCXFACTORY_BUILD_GUI      "Build the Qt test GUI (needs Qt5 Widgets)" OFF)
option(DOCXFACTORY_ENABLE_RAPIDJSON   "Enable merging from JSON data"        ON)
option(DOCXFACTORY_ENABLE_ICU         "Use ICU for character set conversion" OFF)
option(DOCXFACTORY_ENABLE_IMAGEMAGICK "Use ImageMagick for image conversion" OFF)
option(DOCXFACTORY_ENABLE_BARCODE     "Enable barcode support (needs the DocxFactory patched zint)" OFF)
option(DOCXFACTORY_FETCH_DEPS     "Download missing dependencies with FetchContent" ON)

set(DOCXFACTORY_INCLUDE_DIR	${CMAKE_CURRENT_SOURCE_DIR}/include)
set(DOCXFACTORY_MINIZIP_DIR	${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip)
set(DOCXFACTORY_ZINT_COMPAT_DIR	${CMAKE_CURRENT_SOURCE_DIR}/third_party/zint_compat)

# ---------------------------------------------------------------------------
# dependencies
# ---------------------------------------------------------------------------

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
endif()

# Fetches a dependency without adding it to the build with add_subdirectory().
function(docxfactory_fetch p_name p_url)
	FetchContent_Declare(${p_name}
		URL ${p_url}
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
	FetchContent_GetProperties(${p_name})
	if(NOT ${p_name}_POPULATED)
		if(POLICY CMP0169)
			cmake_policy(PUSH)
			cmake_policy(SET CMP0169 OLD)
		endif()
		FetchContent_Populate(${p_name})
		if(POLICY CMP0169)
			cmake_policy(POP)
		endif()
	endif()

	set(${p_name}_SOURCE_DIR ${${p_name}_SOURCE_DIR} PARENT_SCOPE)
	set(${p_name}_BINARY_DIR ${${p_name}_BINARY_DIR} PARENT_SCOPE)
endfunction()

# Xerces-C (required). Always built from source: an installed Xerces is an
# imported target and cannot be exported with the package, while this build is
# installed and exported as DocxFactory::xerces-c.
message(STATUS "Fetching Xerces-C")
docxfactory_fetch(xercesc https://github.com/apache/xerces-c/archive/refs/tags/v3.3.0.tar.gz)

# On Windows the project's _X() macro expands string literals to L"..."
# (i.e. wchar_t) and therefore requires Xerces' XMLCh to be wchar_t.
# Xerces defaults XMLCh to char16_t, which breaks that assumption
# under MSVC (cannot convert const wchar_t* to const XMLCh*). Force the
# Windows ABI to wchar_t to match the source.
if(WIN32)
	set(xmlch-type "wchar_t" CACHE STRING "XMLCh type" FORCE)
endif()

# BUILD_SHARED_LIBS also drives Xerces, so a shared package installs the
# Xerces DLL next to docxfactory.dll and a static package installs the static
# archive.
add_subdirectory(${xercesc_SOURCE_DIR} ${xercesc_BINARY_DIR} EXCLUDE_FROM_ALL)

set(DOCXFACTORY_XERCES_TARGET xerces-c)
set(DOCXFACTORY_XERCES_INCLUDE_DIRS
	${xercesc_SOURCE_DIR}/src
	${xercesc_BINARY_DIR}/src)

# zlib (required by minizip). Built from source as an independent library of
# the same type and installed with the package.
message(STATUS "Fetching zlib")
docxfactory_fetch(zlib https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz)

add_library(docxfactory_zlib
	${zlib_SOURCE_DIR}/adler32.c
	${zlib_SOURCE_DIR}/compress.c
	${zlib_SOURCE_DIR}/crc32.c
	${zlib_SOURCE_DIR}/deflate.c
	${zlib_SOURCE_DIR}/gzclose.c
	${zlib_SOURCE_DIR}/gzlib.c
	${zlib_SOURCE_DIR}/gzread.c
	${zlib_SOURCE_DIR}/gzwrite.c
	${zlib_SOURCE_DIR}/infback.c
	${zlib_SOURCE_DIR}/inffast.c
	${zlib_SOURCE_DIR}/inflate.c
	${zlib_SOURCE_DIR}/inftrees.c
	${zlib_SOURCE_DIR}/trees.c
	${zlib_SOURCE_DIR}/uncompr.c
	${zlib_SOURCE_DIR}/zutil.c)

target_include_directories(docxfactory_zlib SYSTEM PUBLIC
	$<BUILD_INTERFACE:${zlib_SOURCE_DIR}>)

set_target_properties(docxfactory_zlib PROPERTIES
	OUTPUT_NAME zlib
	OUTPUT_NAME_DEBUG zlibd
	POSITION_INDEPENDENT_CODE ON)

if(BUILD_SHARED_LIBS)
	# ZLIB_DLL turns the ZEXTERN declarations into dllexport while compiling
	# the DLL; consumers import through the generated import library.
	set_target_properties(docxfactory_zlib PROPERTIES DEFINE_SYMBOL ZLIB_DLL)
endif()

set(DOCXFACTORY_ZLIB_TARGET docxfactory_zlib)

# minizip (vendored), also an independent library of the same type.
add_library(docxfactory_minizip
	${DOCXFACTORY_MINIZIP_DIR}/zlib/zip.c
	${DOCXFACTORY_MINIZIP_DIR}/zlib/unzip.c
	${DOCXFACTORY_MINIZIP_DIR}/zlib/ioapi.c)

target_include_directories(docxfactory_minizip PUBLIC
	$<BUILD_INTERFACE:${DOCXFACTORY_MINIZIP_DIR}>)

set_target_properties(docxfactory_minizip PROPERTIES
	OUTPUT_NAME minizip
	OUTPUT_NAME_DEBUG minizipd)

target_link_libraries(docxfactory_minizip PUBLIC ${DOCXFACTORY_ZLIB_TARGET})

if(BUILD_SHARED_LIBS)
	# minizip has no export annotations of its own.
	set_target_properties(docxfactory_minizip PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

	if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_link_options(docxfactory_minizip PRIVATE -Wl,--export-all-symbols)
	endif()
endif()

set(DOCXFACTORY_MINIZIP_TARGET docxfactory_minizip)

# RapidJSON (optional)
if(DOCXFACTORY_ENABLE_RAPIDJSON)
	find_package(rapidjson QUIET)

	if(TARGET rapidjson::rapidjson)
		set(DOCXFACTORY_RAPIDJSON_TARGET rapidjson::rapidjson)
	elseif(TARGET RapidJSON::RapidJSON)
		set(DOCXFACTORY_RAPIDJSON_TARGET RapidJSON::RapidJSON)
	else()
		if(NOT DOCXFACTORY_FETCH_DEPS)
			message(FATAL_ERROR "RapidJSON was not found. Install it or set DOCXFACTORY_FETCH_DEPS=ON.")
		endif()

		message(STATUS "RapidJSON not found, fetching it")
		docxfactory_fetch(rapidjson https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz)
		set(DOCXFACTORY_RAPIDJSON_INCLUDE ${rapidjson_SOURCE_DIR}/include)
	endif()
endif()

# ICU (optional)
if(DOCXFACTORY_ENABLE_ICU)
	find_package(ICU REQUIRED COMPONENTS uc)
endif()

# ImageMagick (optional)
if(DOCXFACTORY_ENABLE_IMAGEMAGICK)
	find_package(ImageMagick REQUIRED COMPONENTS Magick++)
endif()

# zint (optional)
if(DOCXFACTORY_ENABLE_BARCODE)
	find_path(DOCXFACTORY_ZINT_INCLUDE_DIR zint/zint.h)
	find_library(DOCXFACTORY_ZINT_LIBRARY zint)

	if(NOT DOCXFACTORY_ZINT_INCLUDE_DIR OR NOT DOCXFACTORY_ZINT_LIBRARY)
		message(FATAL_ERROR
			"Barcode support requires the DocxFactory patched zint library "
			"(the one providing the 5 argument ZBarcode_Export()). "
			"Set DOCXFACTORY_ENABLE_BARCODE=OFF to build without barcodes.")
	endif()
endif()

# ---------------------------------------------------------------------------
# sources
# ---------------------------------------------------------------------------

file(GLOB_RECURSE DOCXFACTORY_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/(WordProcessingCompiler/word-processing-compiler|WordProcessingMerger/word-processing-merger)\\.cpp$")
list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/UnoUtil/")
list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/WordProcessingTestGui/")

if(WIN32)
	list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/unix/")
else()
	list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/win/")
	list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/ConverterPrinter/Ms[A-Za-z]*Util\\.cpp$")
endif()

if(NOT DOCXFACTORY_ENABLE_RAPIDJSON)
	list(FILTER DOCXFACTORY_SOURCES EXCLUDE REGEX "/src/json/JsonFunc\\.cpp$")
endif()

set(DOCXFACTORY_PUBLIC_HEADERS
	${DOCXFACTORY_INCLUDE_DIR}/DocxFactory/WordProcessingCompiler/dfwc.h
	${DOCXFACTORY_INCLUDE_DIR}/DocxFactory/WordProcessingCompiler/WordProcessingCompiler.h
	${DOCXFACTORY_INCLUDE_DIR}/DocxFactory/WordProcessingMerger/dfw.h
	${DOCXFACTORY_INCLUDE_DIR}/DocxFactory/WordProcessingMerger/WordProcessingMerger.h)

# ---------------------------------------------------------------------------
# object library
# ---------------------------------------------------------------------------

add_library(docxfactory_objects OBJECT ${DOCXFACTORY_SOURCES})

target_include_directories(docxfactory_objects PRIVATE
	${DOCXFACTORY_INCLUDE_DIR}
	${DOCXFACTORY_INCLUDE_DIR}/DocxFactory/WordProcessingCompiler
	${DOCXFACTORY_INCLUDE_DIR}/DocxFactory/WordProcessingMerger
	${DOCXFACTORY_MINIZIP_DIR})

if(DOCXFACTORY_XERCES_INCLUDE_DIRS)
	target_include_directories(docxfactory_objects SYSTEM PRIVATE
		${DOCXFACTORY_XERCES_INCLUDE_DIRS})
endif()

target_link_libraries(docxfactory_objects PRIVATE
	${DOCXFACTORY_XERCES_TARGET}
	${DOCXFACTORY_MINIZIP_TARGET}
	${DOCXFACTORY_ZLIB_TARGET})

if(DOCXFACTORY_ENABLE_RAPIDJSON)
	target_compile_definitions(docxfactory_objects PRIVATE DOCXFACTORY_HAVE_RAPIDJSON)
	if(DOCXFACTORY_RAPIDJSON_TARGET)
		target_link_libraries(docxfactory_objects PRIVATE ${DOCXFACTORY_RAPIDJSON_TARGET})
	elseif(DOCXFACTORY_RAPIDJSON_INCLUDE)
		target_include_directories(docxfactory_objects SYSTEM PRIVATE ${DOCXFACTORY_RAPIDJSON_INCLUDE})
	endif()
endif()

if(DOCXFACTORY_ENABLE_ICU)
	target_compile_definitions(docxfactory_objects PRIVATE DOCXFACTORY_USE_ICU)
	target_link_libraries(docxfactory_objects PRIVATE ICU::uc)
endif()

if(DOCXFACTORY_ENABLE_IMAGEMAGICK)
	target_compile_definitions(docxfactory_objects PRIVATE DOCXFACTORY_HAVE_IMAGEMAGICK)
	target_link_libraries(docxfactory_objects PRIVATE ImageMagick::Magick++)
endif()

if(DOCXFACTORY_ENABLE_BARCODE)
	target_compile_definitions(docxfactory_objects PRIVATE DOCXFACTORY_HAVE_ZINT)
	target_include_directories(docxfactory_objects PRIVATE ${DOCXFACTORY_ZINT_INCLUDE_DIR})
	target_link_libraries(docxfactory_objects PRIVATE ${DOCXFACTORY_ZINT_LIBRARY})
else()
	target_include_directories(docxfactory_objects PRIVATE ${DOCXFACTORY_ZINT_COMPAT_DIR})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(docxfactory_objects PRIVATE -fpermissive)
endif()

if(BUILD_SHARED_LIBS)
	target_compile_definitions(docxfactory_objects PRIVATE DOCXFACTORY_DLL_EXPORT)
else()
	target_compile_definitions(docxfactory_objects PRIVATE DOCXFACTORY_STATIC)
endif()

if(MSVC)
	target_compile_options(docxfactory_objects PRIVATE /utf-8 /bigobj)
	target_compile_definitions(docxfactory_objects PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
	target_compile_definitions(docxfactory_objects PRIVATE NOMINMAX)
endif()

# ---------------------------------------------------------------------------
# libraries
# ---------------------------------------------------------------------------

# Xerces-C, minizip and zlib are installed into the same export set, so the
# installed package resolves them as DocxFactory:: targets without extra
# find_package() calls.
set(DOCXFACTORY_LINK_LIBRARIES
	${DOCXFACTORY_XERCES_TARGET}
	${DOCXFACTORY_MINIZIP_TARGET}
	${DOCXFACTORY_ZLIB_TARGET})

# Imported targets cannot be exported; they stay a build time dependency and
# DocxFactoryConfig.cmake restores them for static consumers.
if(DOCXFACTORY_ENABLE_ICU)
	list(APPEND DOCXFACTORY_LINK_LIBRARIES "$<BUILD_INTERFACE:ICU::uc>")
endif()

if(DOCXFACTORY_ENABLE_IMAGEMAGICK)
	list(APPEND DOCXFACTORY_LINK_LIBRARIES "$<BUILD_INTERFACE:ImageMagick::Magick++>")
endif()

if(DOCXFACTORY_ENABLE_BARCODE)
	list(APPEND DOCXFACTORY_LINK_LIBRARIES "$<BUILD_INTERFACE:${DOCXFACTORY_ZINT_LIBRARY}>")
endif()

if(WIN32)
	list(APPEND DOCXFACTORY_LINK_LIBRARIES ole32 oleaut32 uuid version)
else()
	list(APPEND DOCXFACTORY_LINK_LIBRARIES dl)
endif()

add_library(docxfactory
	$<TARGET_OBJECTS:docxfactory_objects>)

target_include_directories(docxfactory PUBLIC
	$<BUILD_INTERFACE:${DOCXFACTORY_INCLUDE_DIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(docxfactory PROPERTIES
	OUTPUT_NAME docxfactory
	OUTPUT_NAME_DEBUG docxfactoryd)

if(BUILD_SHARED_LIBS)
	target_link_libraries(docxfactory PRIVATE ${DOCXFACTORY_LINK_LIBRARIES})

	set_target_properties(docxfactory PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

	if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_link_options(docxfactory PRIVATE -Wl,--export-all-symbols)
	endif()
else()
	# xerces-c is part of the same export set, so the installed interface
	# references DocxFactory::xerces-c; the plain system libraries are
	# exported as names.
	target_link_libraries(docxfactory PUBLIC ${DOCXFACTORY_LINK_LIBRARIES})
	target_compile_definitions(docxfactory INTERFACE DOCXFACTORY_STATIC)
endif()

# ---------------------------------------------------------------------------
# command line tools
# ---------------------------------------------------------------------------

if(DOCXFACTORY_BUILD_TOOLS)
	add_executable(word-processing-compiler
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingCompiler/word-processing-compiler.cpp)

	add_executable(word-processing-merger
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingMerger/word-processing-merger.cpp)

	# The tools link the docxfactory target, so they follow the built library
	# type: a static build compiles them with DOCXFACTORY_STATIC and a shared
	# build resolves the symbols through the import library.
	foreach(tool IN ITEMS word-processing-compiler word-processing-merger)
		target_link_libraries(${tool} PRIVATE docxfactory)

		if(BUILD_SHARED_LIBS AND WIN32)
			# Shared builds must carry their runtime DLLs, otherwise the tools
			# only run when the build output directory is on PATH.
			add_custom_command(TARGET ${tool} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
					$<TARGET_FILE:docxfactory>
					$<TARGET_FILE:${DOCXFACTORY_XERCES_TARGET}>
					$<TARGET_FILE:${DOCXFACTORY_MINIZIP_TARGET}>
					$<TARGET_FILE:${DOCXFACTORY_ZLIB_TARGET}>
					$<TARGET_FILE_DIR:${tool}>
				VERBATIM)
		endif()

		if(UNIX)
			set_target_properties(${tool} PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
		endif()
	endforeach()
endif()

# ---------------------------------------------------------------------------
# Qt test GUI
# ---------------------------------------------------------------------------

if(DOCXFACTORY_BUILD_GUI)
	# Qt must match the compiler used for the library: with the default MSVC
	# generator point CMAKE_PREFIX_PATH at the Qt msvc kit, not the MinGW one.
	find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)

	add_executable(word-processing-test-gui
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/main.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/TestWindow.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/TestWindow.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/DfWorker.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/DfWorker.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/LibStr.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/CompilerApi.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/WordProcessingTestGui/CompilerApi.cpp)

	set_target_properties(word-processing-test-gui PROPERTIES
		AUTOMOC ON
		WIN32_EXECUTABLE ON)

	target_link_libraries(word-processing-test-gui PRIVATE docxfactory Qt5::Widgets)

	if(MSVC)
		target_compile_options(word-processing-test-gui PRIVATE /utf-8)
	endif()

	if(BUILD_SHARED_LIBS AND WIN32)
		add_custom_command(TARGET word-processing-test-gui POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_FILE:docxfactory>
				$<TARGET_FILE:${DOCXFACTORY_XERCES_TARGET}>
				$<TARGET_FILE:${DOCXFACTORY_MINIZIP_TARGET}>
				$<TARGET_FILE:${DOCXFACTORY_ZLIB_TARGET}>
				$<TARGET_FILE_DIR:word-processing-test-gui>
			VERBATIM)
	endif()
endif()

# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------

install(FILES ${DOCXFACTORY_PUBLIC_HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# All libraries share one export set, so the installed package wires the
# dependencies itself: DocxFactory::docxfactory references
# DocxFactory::xerces-c, DocxFactory::docxfactory_minizip and
# DocxFactory::docxfactory_zlib.
install(TARGETS docxfactory ${DOCXFACTORY_MINIZIP_TARGET} ${DOCXFACTORY_ZLIB_TARGET}
	EXPORT DocxFactoryTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Installed and exported so the package is self contained. No headers: the
# public DocxFactory API does not expose Xerces types.
install(TARGETS ${DOCXFACTORY_XERCES_TARGET}
	EXPORT DocxFactoryTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(DOCXFACTORY_BUILD_TOOLS)
	install(TARGETS word-processing-compiler word-processing-merger
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(DOCXFACTORY_BUILD_GUI)
	install(TARGETS word-processing-test-gui
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(NOT BUILD_SHARED_LIBS)
	set(DOCXFACTORY_CONFIG_STATIC TRUE)
	set(DOCXFACTORY_CONFIG_USE_ICU ${DOCXFACTORY_ENABLE_ICU})
	set(DOCXFACTORY_CONFIG_USE_IMAGEMAGICK ${DOCXFACTORY_ENABLE_IMAGEMAGICK})

	if(DOCXFACTORY_ENABLE_BARCODE)
		file(TO_CMAKE_PATH "${DOCXFACTORY_ZINT_LIBRARY}" DOCXFACTORY_CONFIG_ZINT_LIBRARY)
	else()
		set(DOCXFACTORY_CONFIG_ZINT_LIBRARY "")
	endif()
else()
	set(DOCXFACTORY_CONFIG_STATIC FALSE)
	set(DOCXFACTORY_CONFIG_USE_ICU FALSE)
	set(DOCXFACTORY_CONFIG_USE_IMAGEMAGICK FALSE)
	set(DOCXFACTORY_CONFIG_ZINT_LIBRARY "")
endif()

install(EXPORT DocxFactoryTargets
	FILE DocxFactoryTargets.cmake
	NAMESPACE DocxFactory::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DocxFactory)

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/DocxFactoryConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/DocxFactoryConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DocxFactory)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/DocxFactoryConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/DocxFactoryConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/DocxFactoryConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DocxFactory)
