# The Strange Drift engine. Plain C++17, no JUCE, no plugin code.
# Chaos.h holds one circuit, Engine.h the three circuits, blend, the two clocks and the saved position,
# Outputs.h the CC numbers, CcStream the smooth 14-bit CC sender (copied from Ogdoad).

add_library(strangedrift_engine STATIC
    src/Engine.cpp
    src/CcStream.cpp
)

target_include_directories(strangedrift_engine PUBLIC include)
target_compile_features(strangedrift_engine PUBLIC cxx_std_17)

if(STRANGEDRIFT_BUILD_TESTS)
    add_executable(strangedrift_engine_test tests/engine_test.cpp)
    target_link_libraries(strangedrift_engine_test PRIVATE strangedrift_engine)
    add_test(NAME engine COMMAND strangedrift_engine_test)

    # Writes the engine's real CC stream to a file; the mapper test (Node.js) feeds it through the Scripter mapper.
    add_executable(strangedrift_ccdump tests/ccdump.cpp)
    target_link_libraries(strangedrift_ccdump PRIVATE strangedrift_engine)

    find_program(NODE_EXE node)
    if(NODE_EXE)
        add_test(NAME mapper
                 COMMAND ${CMAKE_COMMAND} -E env
                         STRANGEDRIFT_CCDUMP=$<TARGET_FILE:strangedrift_ccdump>
                         ${NODE_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/mapper_test.js
                         ${CMAKE_SOURCE_DIR}/tools/mapper/StrangeDrift-Mapper.js
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Ogdoad-Mapper.js)
    endif()
endif()
