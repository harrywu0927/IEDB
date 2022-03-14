PROJECT(STDFB)
SET(CMAKE_CXX_STANDARD 11)
AUX_SOURCE_DIRECTORY(./src SOURCES)
link_directories(${CMAKE_SOURCE_DIR}/lib)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) #设置库文件的输出目录
link_libraries("/Users/harrywu/Documents/StdFunctions/lib/libdb_fs.dylib")

ADD_EXECUTABLE(main src/main.cpp)
ADD_LIBRARY(std_fb SHARED ${SOURCES})
ADD_EXECUTABLE(test test.cpp)
ADD_EXECUTABLE(test2 test2.c)

target_link_libraries(main std_fb)
target_link_libraries(test std_fb)
target_link_libraries(test2 std_fb)