#--------------------------------------------------------
# The CMakeLists.txt for:                      uSimNomoto
# Author(s):                  Mike Benjamin, Damian Manda
#--------------------------------------------------------

# Set System Specific Libraries
if (${WIN32})
  SET(SYSTEM_LIBS
    wsock32)
else (${WIN32})
  SET(SYSTEM_LIBS
    m
    pthread)
endif (${WIN32})

SET(SRC
   USM_MOOSApp.cpp
   USM_Model.cpp
   USM_Info.cpp
   SimEngine.cpp
   ThrustMap.cpp
   main.cpp
)

ADD_EXECUTABLE(uSimNomoto ${SRC})

TARGET_LINK_LIBRARIES(uSimNomoto 
  ${MOOS_LIBRARIES}
  ${MOOSGeodesy_LIBRARIES}
  contacts
  geometry 
  apputil
  mbutil
  ${SYSTEM_LIBS})


