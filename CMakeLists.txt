cmake_minimum_required(VERSION 3.21)
project(lumo VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 Network Multimedia DBus)

# sherpa-onnx (offline speech recognition), fetched by scripts/fetch-sherpa.sh.
set(SHERPA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sherpa-onnx-v1.13.8-linux-x64-shared)
if(NOT EXISTS ${SHERPA_DIR}/lib/libsherpa-onnx-c-api.so)
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch-sherpa.sh RESULT_VARIABLE fetched)
    if(NOT fetched EQUAL 0)
        message(FATAL_ERROR "Couldn't fetch sherpa-onnx; run scripts/fetch-sherpa.sh")
    endif()
endif()
add_library(sherpa SHARED IMPORTED)
set_target_properties(sherpa PROPERTIES
    IMPORTED_LOCATION ${SHERPA_DIR}/lib/libsherpa-onnx-c-api.so
    INTERFACE_INCLUDE_DIRECTORIES ${SHERPA_DIR}/include)
find_package(LayerShellQt REQUIRED)
find_package(KF6GlobalAccel REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)
include(GNUInstallDirs)

# The relay Claude Code runs on every hook: plain C, starts in about a millisecond.
add_executable(lumo-hook hook/lumo-hook.c)
target_compile_options(lumo-hook PRIVATE -O2 -Wall -Wextra)

qt_add_executable(lumo
    src/main.cpp
    src/AgentHub.cpp src/AgentHub.h
    src/ChatBackend.cpp src/ChatBackend.h
    src/Diff.cpp src/Diff.h
    src/HookInstaller.cpp src/HookInstaller.h
    src/HookServer.cpp src/HookServer.h
    src/MapListModel.cpp src/MapListModel.h
    src/Shell.cpp src/Shell.h
    src/Sounds.cpp src/Sounds.h
    src/ActiveWindow.cpp src/ActiveWindow.h
    src/Dictation.cpp src/Dictation.h
    src/EvdevKey.cpp src/EvdevKey.h
    src/Portal.cpp src/Portal.h
    src/Speech.cpp src/Speech.h
    src/Vocabulary.cpp src/Vocabulary.h
    src/PlanUsage.cpp src/PlanUsage.h
    src/Music.cpp src/Music.h
)
# Finds sherpa's libraries next to the build, and in lib/lumo once installed.
set_target_properties(lumo PROPERTIES
    BUILD_RPATH ${SHERPA_DIR}/lib
    INSTALL_RPATH "$ORIGIN/../lib/lumo")

set(LUMO_QML Island Mascot Ui SessionCard RequestCard ChatPane DiffView UsageRing PillButton SettingsWindow DictationBar MusicCard Outfit Wardrobe InstinctButton)
set(LUMO_QML_FILES)
foreach(name IN LISTS LUMO_QML)
    # Served from the module root so they all see the module's own qmldir (and the Ui singleton).
    set_source_files_properties(qml/${name}.qml PROPERTIES QT_RESOURCE_ALIAS ${name}.qml)
    list(APPEND LUMO_QML_FILES qml/${name}.qml)
endforeach()
set_source_files_properties(qml/Ui.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(lumo
    URI Lumo
    VERSION 1.0
    QML_FILES ${LUMO_QML_FILES}
)

target_link_libraries(lumo PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Multimedia
    LayerShellQt::Interface KF6::GlobalAccel Qt6::DBus sherpa)
add_dependencies(lumo lumo-hook)

install(TARGETS lumo lumo-hook RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${SHERPA_DIR}/lib/libsherpa-onnx-c-api.so ${SHERPA_DIR}/lib/libonnxruntime.so
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/lumo)
configure_file(packaging/io.github.mobenlamine.lumo.desktop.in io.github.mobenlamine.lumo.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.mobenlamine.lumo.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# End-to-end tests: the real hook against the real server.
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
qt_add_executable(test_hub tests/test_hub.cpp
    src/AgentHub.cpp src/HookServer.cpp src/MapListModel.cpp src/Diff.cpp)
target_include_directories(test_hub PRIVATE src)
target_compile_definitions(test_hub PRIVATE LUMO_HOOK_PATH="$<TARGET_FILE:lumo-hook>")
target_link_libraries(test_hub PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_dependencies(test_hub lumo-hook)
add_test(NAME hub COMMAND test_hub)
install(FILES packaging/io.github.mobenlamine.lumo.svg DESTINATION share/icons/hicolor/scalable/apps)

qt_add_executable(test_speech tests/test_speech.cpp src/Speech.cpp)
target_include_directories(test_speech PRIVATE src)
target_link_libraries(test_speech PRIVATE Qt6::Core Qt6::Network Qt6::Test sherpa)
set_target_properties(test_speech PROPERTIES BUILD_RPATH ${SHERPA_DIR}/lib)
add_test(NAME speech COMMAND test_speech)

qt_add_executable(test_vocabulary tests/test_vocabulary.cpp src/Vocabulary.cpp)
target_include_directories(test_vocabulary PRIVATE src)
target_link_libraries(test_vocabulary PRIVATE Qt6::Core Qt6::Test)
add_test(NAME vocabulary COMMAND test_vocabulary)

# Needs an MPRIS player named org.mpris.MediaPlayer2.lumotest (skipped otherwise).
qt_add_executable(test_music tests/test_music.cpp src/Music.cpp)
target_include_directories(test_music PRIVATE src)
target_link_libraries(test_music PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
