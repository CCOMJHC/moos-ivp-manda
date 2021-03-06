#--------------------------------------------------------
# The CMakeLists.txt for:                 pSurveyPath
# Author(s):                              Damian Manda
#--------------------------------------------------------
SET(CMAKE_CXX_STANDARD 11)

# Find package is broken under < 3.3 on Homebrew
# And Debian Wheezy armhf
find_package(Eigen3)
if(NOT EIGEN3_FOUND)
  include_directories(/usr/local/Cellar/eigen/3.2.8/include/eigen3/)
  include_directories(/usr/include/eigen3/)
else()
  include_directories(${EIGEN3_INCLUDE_DIR})
endif()

# Find boost
# COMPONENTS geometry
FIND_PACKAGE(Boost REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIRS}  )
# This is needed for Boost 1.58 on Ubuntu 16.04git 
add_definitions(-DBOOST_MATH_DISABLE_FLOAT128)

SET(SRC
  SurveyPath.cpp
  SurveyPath_Info.cpp
  RecordSwath.cpp
  PathPlan.cpp
  main.cpp
)

ADD_EXECUTABLE(pSurveyPath ${SRC})

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} geos-config")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} geos-config")
#CFLAGS += `geos-config --cflags`
#LDFLAGS += `geos-config --ldflags`

TARGET_LINK_LIBRARIES(pSurveyPath
   ${MOOS_LIBRARIES}
   ${Boost_LIBRARIES}
   apputil
   geometry
   mbutil
   m
   pthread
   geos
)

# Add the tests at the end so they can find the eigen library
ADD_SUBDIRECTORY(tests)
