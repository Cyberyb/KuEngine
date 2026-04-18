add_executable(MclarenApp
    main.cpp
    MclarenPass.cpp
)

target_include_directories(MclarenApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(MclarenApp PRIVATE
    KUENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(MclarenApp PRIVATE
    KuEngine
    imgui::imgui
    nlohmann_json::nlohmann_json
)

set_target_properties(MclarenApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET MclarenApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:MclarenApp>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.bat
        $<TARGET_FILE_DIR:MclarenApp>/shaders/compile_shaders.bat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mclaren.vert
        $<TARGET_FILE_DIR:MclarenApp>/shaders/mclaren.vert
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mclaren.frag
        $<TARGET_FILE_DIR:MclarenApp>/shaders/mclaren.frag
)

if(Vulkan_GLSLC_EXECUTABLE)
    add_custom_command(TARGET MclarenApp POST_BUILD
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mclaren.vert
            -o $<TARGET_FILE_DIR:MclarenApp>/shaders/mclaren.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mclaren.frag
            -o $<TARGET_FILE_DIR:MclarenApp>/shaders/mclaren.frag.spv
        COMMENT "Compiling mclaren shaders with glslc"
    )
else()
    message(WARNING "Vulkan_GLSLC_EXECUTABLE not found. Compile shaders manually with shaders/compile_shaders.bat")
endif()

set(MCLAREN_MODEL_SOURCE ${CMAKE_SOURCE_DIR}/resources/models/props/mclaren_765lt.glb)
if(EXISTS ${MCLAREN_MODEL_SOURCE})
    add_custom_command(TARGET MclarenApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:MclarenApp>/resources/models/props
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MCLAREN_MODEL_SOURCE}
            $<TARGET_FILE_DIR:MclarenApp>/resources/models/props/mclaren_765lt.glb
        COMMENT "Copying mclaren model to runtime resources"
    )
else()
    message(WARNING "mclaren_765lt.glb not found under resources/models/props")
endif()

set(MCLAREN_SCENE_SOURCE ${CMAKE_SOURCE_DIR}/resources/scenes/sandbox/mclaren-sandbox.scene.json)
if(EXISTS ${MCLAREN_SCENE_SOURCE})
    add_custom_command(TARGET MclarenApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:MclarenApp>/resources/scenes/sandbox
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MCLAREN_SCENE_SOURCE}
            $<TARGET_FILE_DIR:MclarenApp>/resources/scenes/sandbox/mclaren-sandbox.scene.json
        COMMENT "Copying mclaren scene config to runtime resources"
    )
else()
    message(WARNING "mclaren-sandbox.scene.json not found under resources/scenes/sandbox")
endif()

set(MCLAREN_MATERIAL_SOURCE ${CMAKE_SOURCE_DIR}/resources/materials/pbr/mclaren-765lt.material.json)
if(EXISTS ${MCLAREN_MATERIAL_SOURCE})
    add_custom_command(TARGET MclarenApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:MclarenApp>/resources/materials/pbr
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MCLAREN_MATERIAL_SOURCE}
            $<TARGET_FILE_DIR:MclarenApp>/resources/materials/pbr/mclaren-765lt.material.json
        COMMENT "Copying mclaren material config to runtime resources"
    )
else()
    message(WARNING "mclaren-765lt.material.json not found under resources/materials/pbr")
endif()

set(MCLAREN_MANIFEST_SOURCE ${CMAKE_SOURCE_DIR}/resources/manifests/asset-registry.json)
if(EXISTS ${MCLAREN_MANIFEST_SOURCE})
    add_custom_command(TARGET MclarenApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:MclarenApp>/resources/manifests
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MCLAREN_MANIFEST_SOURCE}
            $<TARGET_FILE_DIR:MclarenApp>/resources/manifests/asset-registry.json
        COMMENT "Copying asset registry to runtime resources"
    )
else()
    message(WARNING "asset-registry.json not found under resources/manifests")
endif()

add_custom_command(TARGET MclarenApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:MclarenApp>/shaders
    COMMENT "Copying mclaren shader sources"
)
