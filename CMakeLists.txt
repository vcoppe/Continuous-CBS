cmake_minimum_required( VERSION 2.8 )

project( CCBS )

find_package(Boost)
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories( "." )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O3 -DNDEBUG -Wall -Wextra" )

set( SOURCE_FILES
     main.cpp
	 config.cpp
     tinyxml2.cpp
     xml_logger.cpp
     map.cpp
     heuristic.cpp
     sipp.cpp
     task.cpp
     cbs.cpp
	 simplex/columnset.cpp
	 simplex/constraint.cpp
	 simplex/matrix.cpp
	 simplex/objectivefunction.cpp
	 simplex/simplex.cpp
	 simplex/variable.cpp)

set( HEADER_FILES
     const.h
     tinyxml2.h
	 config.h
     xml_logger.h
     map.h
     heuristic.h
     sipp.h
     task.h
     cbs.h
	 structs.h
	 simplex/columnset.h
	 simplex/constraint.h
	 simplex/datamismatchexception.h
	 simplex/divisionbyzeroexception.h
	 simplex/indexoutofboundexception.h
	 simplex/matrix.h
	 simplex/matrixissingularexception.h
	 simplex/matrixnotsquareexception.h
     simplex/memoryreachedoutexception.h
     simplex/notavectorexception.h
     simplex/objectivefunction.h
     simplex/pilal.h
     simplex/pilalexceptions.h
	 simplex/simplex.h
	 simplex/simplexexceptions.h
	 simplex/sizemismatchexception.h
	 simplex/variable.h)

add_executable( CCBS ${SOURCE_FILES} ${HEADER_FILES} )
