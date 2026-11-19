# Retro ghost race as a compiled QML module: qmlcachegen turns RetroRace.qml
# into C++ at build time, so the scene neither parses QML at startup nor
# interprets its bindings per frame.
qt_add_library(mt_game STATIC)
qt_add_qml_module(mt_game
    URI MaximumTrainer.Game
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/MaximumTrainer/Game"
    QML_FILES qml/RetroRace.qml
    SOURCES
        cyclingphysics.h
        ghostreplay.cpp ghostreplay.h
        retroracecontroller.cpp retroracecontroller.h
        racestyle.cpp racestyle.h
        roadsurface.cpp roadsurface.h)
target_include_directories(mt_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mt_game PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml mt_fit)
set_target_properties(mt_game mt_gameplugin PROPERTIES POSITION_INDEPENDENT_CODE ON)
mt_set_warnings(mt_game)
