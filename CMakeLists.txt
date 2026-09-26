cmake_minimum_required(VERSION 3.16)
project(lingmo-spotlight VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick DBus Concurrent Network LinguistTools)
find_package(KF6Service REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(KF6WindowSystem REQUIRED)

add_executable(lingmo-spotlight
    src/main.cpp
    src/spotlight.cpp
    src/resultsmodel.cpp
    src/searchengine.cpp
    src/converter.cpp
    src/currencyrates.cpp
    src/contentsearch.cpp
    qml.qrc
)

target_link_libraries(lingmo-spotlight PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Concurrent Qt6::Network
    KF6::Service KF6::KIOGui KF6::WindowSystem
)

# Conversion parser self-check: cmake -DBUILD_TESTING=ON, then ctest
option(BUILD_TESTING "Build the self-check programs" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_executable(convertertest tests/convertertest.cpp src/converter.cpp)
    target_link_libraries(convertertest PRIVATE Qt6::Core)
    add_test(NAME converter COMMAND convertertest)
endif()

set(TRANSLATIONS_DIR ${CMAKE_INSTALL_FULL_DATADIR}/lingmo-spotlight/translations)
target_compile_definitions(lingmo-spotlight PRIVATE TRANSLATIONS_DIR="${TRANSLATIONS_DIR}")
# lrelease only: building never rewrites the .ts sources
qt_add_translation(QM_FILES translations/lingmo-spotlight_pt_BR.ts)
add_custom_target(translations ALL DEPENDS ${QM_FILES})
install(FILES ${QM_FILES} DESTINATION ${TRANSLATIONS_DIR})

install(TARGETS lingmo-spotlight RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# Started hidden with the Lingmo session so it opens instantly on Meta+Space
install(FILES lingmo-spotlight-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart
        RENAME lingmo-spotlight.desktop)
