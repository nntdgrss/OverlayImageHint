cmake_minimum_required(VERSION 3.10)
project(OverlayImageHint)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем поддержку Unicode
add_definitions(-DUNICODE -D_UNICODE)

# Создаем исполняемый файл
add_executable(OverlayImageHint WIN32 main.cpp)

# Добавляем библиотеки Windows
target_link_libraries(OverlayImageHint
    gdiplus
    comctl32
    shell32
    user32
) 