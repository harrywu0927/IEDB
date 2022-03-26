PROJECT(STDFB)
SET(CMAKE_CXX_STANDARD 11)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) #设置库文件的输出目录
AUX_SOURCE_DIRECTORY(./src SOURCES)
AUX_SOURCE_DIRECTORY(./DB_FS/sources FS_SOURCES)
INCLUDE_DIRECTORIES(./include)
ADD_LIBRARY(db_fs SHARED ${FS_SOURCES})
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/lib)
LINK_LIBRARIES(${PROJECT_SOURCE_DIR}/lib/libdb_fs.so)
ADD_LIBRARY(std_fb SHARED ${SOURCES})
ADD_EXECUTABLE(main src/main.cpp)

ADD_EXECUTABLE(test test.cpp)
ADD_EXECUTABLE(test2 test2.c)

TARGET_LINK_LIBRARIES(main std_fb)
TARGET_LINK_LIBRARIES(test std_fb)
TARGET_LINK_LIBRARIES(test2 std_fb)