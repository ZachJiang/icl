SET(CMAKE_MODULE_PATH "${ICL_CMAKE_MODULE_PATH}")
MESSAGE(STATUS "CMake modules will be installed to: ${CMAKE_MODULE_PATH}")

INSTALL(FILES FindLibDC.cmake
              FindMesaSR.cmake
              FindOpenNI.cmake
              FindOpenCL.cmake
              FindPylon.cmake
              FindV4L.cmake
              FindWinProtobuf.cmake
              FindSSE.cmake
              InstallFilesRecursive.cmake
       DESTINATION ${CMAKE_MODULE_PATH}
       COMPONENT development)
