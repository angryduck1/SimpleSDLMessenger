cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0167 NEW)

project(BoostServerClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути к SDL2 (установлен через Homebrew)
find_package(SDL2 REQUIRED)

# Поиск SDL2_ttf
find_path(SDL2_TTF_INCLUDE_DIR SDL2/SDL_ttf.h
    PATHS /usr/local/Cellar/sdl2_ttf/2.24.0/include/SDL2
)
find_library(SDL2_TTF_LIBRARY SDL2_ttf
    PATHS /usr/local/Cellar/sdl2_ttf/2.24.0/lib
)

# Поиск SDL2_image
find_path(SDL2_IMAGE_INCLUDE_DIR SDL2/SDL_image.h
    PATHS /usr/local/Cellar/sdl2_image/2.8.8/include/SDL2
)
find_library(SDL2_IMAGE_LIBRARY SDL2_image
    PATHS /usr/local/Cellar/sdl2_image/2.8.8/lib
)

# Поиск SDL2_mixer
find_path(SDL2_MIXER_INCLUDE_DIR SDL2/SDL_mixer.h
    PATHS /usr/local/Cellar/sdl2_mixer/2.8.1_1/include/SDL2
)
find_library(SDL2_MIXER_LIBRARY SDL2_mixer
    PATHS /usr/local/Cellar/sdl2_mixer/2.8.1_1/lib
)

find_package(PNG REQUIRED)

if(SDL2_TTF_INCLUDE_DIR AND SDL2_TTF_LIBRARY AND SDL2_IMAGE_INCLUDE_DIR AND SDL2_IMAGE_LIBRARY AND SDL2_MIXER_INCLUDE_DIR AND SDL2_MIXER_LIBRARY)
    set(SDL2_TTF_INCLUDE_DIRS ${SDL2_TTF_INCLUDE_DIR})
    set(SDL2_TTF_LIBRARIES ${SDL2_TTF_LIBRARY})
    set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
    set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
    set(SDL2_MIXER_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})
    set(SDL2_MIXER_LIBRARIES ${SDL2_MIXER_LIBRARY})
else()
    message(FATAL_ERROR "One or more SDL2 components not found")
endif()

# Поиск Boost
find_package(Boost 1.73 REQUIRED COMPONENTS system thread)

add_executable(BoostServerClient
    client.cpp
    cipher.h functions.h
)

target_include_directories(BoostServerClient PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PNG_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

target_link_libraries(BoostServerClient PRIVATE
    ${PNG_LIBRARIES}
    ${Boost_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    pthread
)
