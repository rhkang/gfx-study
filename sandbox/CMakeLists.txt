function(buildApplication APP_NAME)
    set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${APP_NAME})
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
    file(GLOB SHADERS
        "${SHADER_DIR}/*.hlsl"
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
    )
    file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
        "${APP_DIR}/*.h"
        "${APP_DIR}/*.hpp"
    )
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "${APP_DIR}/*.cpp"
    )

    add_executable(${APP_NAME} ${SOURCES} ${HEADERS})
    source_group("src" FILES ${SOURCES} ${HEADERS})
    source_group("shaders" FILES ${SHADERS})

    target_link_libraries(${APP_NAME} PRIVATE ${ENGINE_NAME})
endfunction(buildApplication)

# Build all
function(buildAll)
	foreach(APP ${APPS})
		buildApplication(${APP})
	endforeach(APP)
endfunction(buildAll)

set(APPS
    triangle
    model
    simpleshadow
)

buildAll()