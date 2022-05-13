PROJECT(STDFB)
SET(CMAKE_CXX_STANDARD 11)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) #设置库文件的输出目录
AUX_SOURCE_DIRECTORY(./src SOURCES)
INCLUDE_DIRECTORIES(./include)
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/lib)
FIND_PACKAGE(PythonLibs REQUIRED) 
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_DIRS}) 

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
 link_libraries(/usr/lib/x86_64-linux-gnu/libpython3.8.so)
 ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES})
 ADD_EXECUTABLE(test.o test.cpp)
 TARGET_LINK_LIBRARIES(test.o  CassFactoryDB pthread python3.8)
ELSE()
 link_libraries(/opt/homebrew/Cellar/python@3.9/3.9.10/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib)
 INCLUDE_DIRECTORIES(/opt/homebrew/Cellar/numpy/1.22.2/lib/python3.9/site-packages/numpy/core/include)
 ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES})
 ADD_EXECUTABLE(test.o test.cpp)
 TARGET_LINK_LIBRARIES(test.o  CassFactoryDB pthread python3.9)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

# ADD_LIBRARY(CassFactoryDB SHARED ${SOURCES})

