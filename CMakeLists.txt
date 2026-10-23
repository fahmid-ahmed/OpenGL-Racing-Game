# CMakeLists.txt: the "recipe" that tells CMake how to build our game.

cmake_minimum_required(VERSION 3.20)
project(RacingGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake to also look where Homebrew installed GLFW and GLM.
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/glfw")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/glm")   # NEW: where GLM lives

# GLFW: creates the window and reads the keyboard.
find_package(glfw3 REQUIRED)

# OpenGL: draws the graphics (provided by Apple on a Mac).
find_package(OpenGL REQUIRED)

# NEW: GLM: the math library for 3D (matrices, vectors, rotations).
# REQUIRED means CMake stops with an error if it can't find it.
find_package(glm REQUIRED)

add_executable(RacingGame main.cpp)

# Link everything into our program.
# NEW: glm::glm gives our code access to the GLM math tools.
target_link_libraries(RacingGame PRIVATE glfw OpenGL::GL glm::glm)