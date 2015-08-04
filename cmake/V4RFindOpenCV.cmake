if(WITH_OpenCV)
  find_package(OpenCV "${MIN_VER_OpenCV}")
  if(OpenCV_FOUND)
    set(OPENCV_LIBRARIES "${OpenCV_LIBS}")
    set(OPENCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}")
    set(HAVE_OPENCV TRUE)
  endif()
endif()
