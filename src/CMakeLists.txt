add_library(core
    barrier.c
    filter.c
    image.c
    threadpool.c
    list.c
    processing.c
    utils.c

    barrier.h
    filter.h
    image.h
    threadpool.h
    list.h
    processing.h
    utils.h
)
target_link_libraries(core PUBLIC png)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ieffect ieffect.c)
target_link_libraries(ieffect PRIVATE core)
