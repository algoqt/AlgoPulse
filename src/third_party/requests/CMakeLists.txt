INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

set(SRCS
    Utils.cpp
    Exception.cpp
    Url.cpp
    Response.cpp
    Context.cpp
    Request.cpp
    AsyncRequest.cpp)

add_library (requests STATIC ${SRCS})
