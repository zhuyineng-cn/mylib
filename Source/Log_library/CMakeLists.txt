#cmake file for project association                     #表示注释 
#author:大T
#created:2013/4/9

#相关头文件的目录
INCLUDE_DIRECTORIES(
    "../public"
    "./Interface"
)

SET(LIBLOG_INTERFACE_FILES
    "Interface/ILog.h"
)

#设置变量，表示所有的源文件
SET(SOURCE_FILES
	"LogQueue.h"
	"LogSystem.h"
	"LogSystem.cpp"
    "main.cpp"
)

SOURCE_GROUP(\\ FILES ${SOURCE_FILES})
SOURCE_GROUP(Interface FILES ${LIBLOG_INTERFACE_FILES})
SOURCE_GROUP(public FILES ${public_FILES})

MESSAGE(STATUS "public : ${public_FILES}")

#相关库文件的目录
#LINK_DIRECTORIES(
#/usr/local/lib
#)

#设置输出路径,并且生成动态库
SET(LIBRARY_OUTPUT_PATH "${OUTPUT_PATH}/liblog")
ADD_LIBRARY("liblog" SHARED 
    ${SOURCE_FILES} 
    ${public_FILES}
    ${LIBLOG_INTERFACE_FILES}
)

#依赖的库文件
#TARGET_LINK_LIBRARY(association core)

#表示生成的执行文件所在路径
#SET_TARGET_PROPERTIES(utility PROPERTIES
#RUNTIME_OUTPUT_DIRECTORY> "${OUTPUT_PATH}/libnet")
