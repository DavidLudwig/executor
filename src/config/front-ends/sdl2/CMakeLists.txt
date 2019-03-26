find_package(SDL2)

if(SDL2_FOUND)
    add_library(front-end-sdl2
        default_vdriver.h
        sdl2.h sdl2.cpp
        keycode_map.cpp
    )

    # workaround for SDL2_LIBRARY containing trailing whitespace that is illegal in CMake 3.10
    if(SDL2_LIBRARY)
        string(STRIP "${SDL2_LIBRARY}" SDL2_LIBRARY)
    endif()

    target_link_libraries(front-end-sdl2 syn68k ${SDL2_LIBRARY})
    target_include_directories(front-end-sdl2
        PUBLIC .
        PRIVATE ${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIR}/SDL2)
endif()
