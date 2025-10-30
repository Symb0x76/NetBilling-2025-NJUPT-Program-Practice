cmake_minimum_required(VERSION 3.16)
project(NetBilling LANGUAGES CXX)

# Qt settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_NO_PRIVATE_MODULE_WARNING ON)

# Try to auto-detect a Qt installation. Prefer Qt6, fall back to Qt5.
set(_found_qt FALSE)

# If user or environment provided a Qt dir, respect it (Qt6_DIR, Qt5_DIR or QTDIR)
if(NOT DEFINED Qt6_DIR AND DEFINED ENV{Qt6_DIR})
    set(Qt6_DIR $ENV{Qt6_DIR})
endif()
if(NOT DEFINED Qt5_DIR AND DEFINED ENV{Qt5_DIR})
    set(Qt5_DIR $ENV{Qt5_DIR})
endif()
if(NOT DEFINED QTDIR AND DEFINED ENV{QTDIR})
    set(QTDIR $ENV{QTDIR})
endif()

# Helper: probe common Windows Qt install locations when no prefix provided
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    if(EXISTS "C:/Qt")
        file(GLOB QT_POSSIBLE "C:/Qt/*")
        if(QT_POSSIBLE)
            list(SORT QT_POSSIBLE)
            list(GET QT_POSSIBLE 0 _firstQt)
            # prefer mingw_64 or msvc* variants if present
            if(EXISTS "${_firstQt}/mingw_64")
                set(CMAKE_PREFIX_PATH "${_firstQt}/mingw_64")
            else()
                set(CMAKE_PREFIX_PATH "${_firstQt}")
            endif()
        endif()
    elseif(DEFINED QTDIR)
        set(CMAKE_PREFIX_PATH "${QTDIR}")
    endif()
endif()

# Try Qt6 first
find_package(Qt6 COMPONENTS Core Gui Widgets Charts QUIET)
if(TARGET Qt6::Core)
    message(STATUS "Found Qt6: ${Qt6_VERSION}")
    set(_found_qt TRUE)
    set(QT_MAJOR 6)
    set(QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts)
else()
    # Try Qt5
    find_package(Qt5 COMPONENTS Core Gui Widgets Charts QUIET)
    if(TARGET Qt5::Core)
        message(STATUS "Found Qt5: ${Qt5_VERSION}")
        set(_found_qt TRUE)
        set(QT_MAJOR 5)
        set(QT_LIBS Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Charts)
    endif()
endif()

if(NOT _found_qt)
    message(WARNING "Could not automatically find Qt6 or Qt5. You can set CMAKE_PREFIX_PATH to your Qt kit (for example C:/Qt/6.4.2/mingw_64) or define Qt6_DIR/Qt5_DIR environment variables.")
    # Try again but require Qt6 to produce a clearer error during configure
    find_package(Qt6 COMPONENTS Core Gui Widgets Charts REQUIRED)
endif()

# Integrate ElaWidgetTools (widgets UI toolkit)
add_subdirectory(ElaWidgetTools/ElaWidgetTools)

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

# Specify MSVC UTF-8 encoding
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if(DEFINED QT_MAJOR AND QT_MAJOR EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        WIN32 # If you need a terminal for debug, please comment this statement
        ${SOURCES}
        ${HEADERS}
        resources/app.qrc
    )
else()
    add_executable(${PROJECT_NAME}
        WIN32
        ${SOURCES}
        ${HEADERS}
        resources/app.qrc
    )
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${QT_LIBS}
    ElaWidgetTools
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ElaWidgetTools>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
