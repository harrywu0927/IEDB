PROJECT(STDFB)
SET(CMAKE_CXX_STANDARD 17)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) # 设置库文件的输出目录
AUX_SOURCE_DIRECTORY(./src SOURCES)
AUX_SOURCE_DIRECTORY(./src/compress LZMA)
INCLUDE_DIRECTORIES(./include)
INCLUDE_DIRECTORIES(./include/compress)
INCLUDE_DIRECTORIES(./include/spdlog)
INCLUDE_DIRECTORIES(./include/spdlog/cfg)
INCLUDE_DIRECTORIES(./include/spdlog/details)
INCLUDE_DIRECTORIES(./include/spdlog/fmt)
INCLUDE_DIRECTORIES(./include/spdlog/sinks)

# find_package(LibLZMA)
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/lib)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    # FIND_PACKAGE(PythonLibs 3.8 REQUIRED)
    INCLUDE_DIRECTORIES(/usr/include/python3.8)
    INCLUDE_DIRECTORIES(/usr/include/numpy)
    LINK_LIBRARIES(/usr/lib/x86_64-linux-gnu/libpython3.8.so)
    ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES} ${LZMA})

    # IF(!LIBLZMA_FOUND)
    # AUX_SOURCE_DIRECTORY(./src/compress LZMA)

    # ELSE()
    # ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES})
    # target_link_libraries(CassFactoryDB ${LIBLZMA_LIBRARIES})
    # ENDIF(!LIBLZMA_FOUND)
    TARGET_LINK_LIBRARIES(CassFactoryDB stdc++fs)
    ADD_EXECUTABLE(test.o test.cpp)
    TARGET_LINK_LIBRARIES(test.o CassFactoryDB pthread python3.8)
ELSE()
    # LINK_LIBRARIES(/opt/homebrew/Cellar/python@3.9/3.9.10/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib)
    INCLUDE_DIRECTORIES(/opt/homebrew/Cellar/numpy/1.22.2/lib/python3.9/site-packages/numpy/core/include)
    INCLUDE_DIRECTORIES(/opt/homebrew/Cellar/python@3.9/3.9.10/Frameworks/Python.framework/Versions/3.9/include/python3.9)

    # link_libraries(${CMAKE_SOURCE_DIR}/lib/libspdlog.a)
    ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES} ${LZMA})

    # IF(!LIBLZMA_FOUND)

    # ELSE()
    # ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES})
    # target_link_libraries(CassFactoryDB ${LIBLZMA_LIBRARIES})
    # ENDIF(!LIBLZMA_FOUND)
    TARGET_LINK_LIBRARIES(CassFactoryDB /opt/homebrew/Cellar/python@3.9/3.9.10/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib)

    # ADD_EXECUTABLE(test.o test.cpp)
    # TARGET_LINK_LIBRARIES(test.o CassFactoryDB pthread python3.9)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

# ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES})
