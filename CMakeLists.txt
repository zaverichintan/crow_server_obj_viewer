cmake_minimum_required(VERSION 3.16)
project(CrowMeshViewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(mesh_server src/main.cpp)
target_link_libraries(mesh_server PRIVATE Threads::Threads)

# Include Crow headers
target_include_directories(mesh_server PRIVATE ${CMAKE_SOURCE_DIR}/crow_include/include)

# Copy public directory to build output directory
add_custom_command(TARGET mesh_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/public
        $<TARGET_FILE_DIR:mesh_server>/public
    COMMENT "Copying public files..."
)

# Copy static directory to build output directory
add_custom_command(TARGET mesh_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/static
        $<TARGET_FILE_DIR:mesh_server>/static
    COMMENT "Copying static files..."
)

# Create models directory and copy if it exists
add_custom_command(TARGET mesh_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:mesh_server>/models
    COMMENT "Creating models directory..."
)

if(EXISTS ${CMAKE_SOURCE_DIR}/models)
    add_custom_command(TARGET mesh_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/models
            $<TARGET_FILE_DIR:mesh_server>/models
        COMMENT "Copying models..."
    )
endif()
