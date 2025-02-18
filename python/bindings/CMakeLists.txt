if( Boost_PYTHON_FOUND AND Boost_THREAD_FOUND )
  if( PYTHONLIBS_FOUND OR PYTHON_LIBRARIES )
  add_definitions(${Boost_CFLAGS})
    set(CMAKE_REQUIRED_INCLUDES ${PYTHON_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} ${REQUIRED_INCLUDES})
    # On MacOSX, boost_system also needs to be linked or will output the following errors:
    # Undefined symbols for architecture x86_64:
    #   "boost::system::system_category()", referenced from: __GLOBAL__I_a in src.cxx.o
    #   "boost::system::generic_category()", referenced from: __GLOBAL__I_a in src.cxx.o
    set(CMAKE_REQUIRED_LIBRARIES ${PYTHON_LIBRARIES} ${Boost_PYTHON_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})
    set(CMAKE_REQUIRED_FLAGS ${Boost_CFLAGS} "-DCMAKE_BUILD_TYPE:STRING=Release")

    # check if all header files can be included
    check_cxx_source_runs("
    #include <Python.h>
    #include <boost/thread/thread.hpp>
    #include <boost/thread/mutex.hpp>
    #include <boost/thread/condition.hpp>
    #include <boost/shared_ptr.hpp>
    #define PY_ARRAY_UNIQUE_SYMBOL PyArrayHandle
    #include <boost/python.hpp>
    #include <boost/python/numpy.hpp>
    #include <pyconfig.h>
    int main()
    {
      return 0;
    }"
    HAVE_ALL_PYTHON_HEADERS)
    set(HAVE_ALL_PYTHON_HEADERS ${HAVE_ALL_PYTHON_HEADERS} PARENT_SCOPE)
    if( HAVE_ALL_PYTHON_HEADERS )
      message(STATUS "python and boost-python found")
      include_directories(${PYTHON_INCLUDE_DIRS} ${OPENRAVE_INCLUDE_LOCAL_DIRS} ${OPENRAVE_CORE_INCLUDE_LOCAL_DIRS})
      
      set(extralibs)
      if( CLOCK_GETTIME_FOUND )
        set(extralibs ${extralibs} rt)
      endif()

      set(OPENRAVEPY_COMPILE_FLAGS "-DOPENRAVE_CORE_DLL")
      if( MSVC )
        check_cxx_compiler_flag("/bigobj" COMPILER_HAS_BIGOBJ)
        if( COMPILER_HAS_BIGOBJ )
          set(OPENRAVEPY_COMPILE_FLAGS "${OPENRAVEPY_COMPILE_FLAGS} /bigobj")
        endif()
      endif()
      ## openrave bindings
      add_library(openravepy_int SHARED openravepy_kinbody.cpp openravepy_int.cpp openravepy_robot.cpp openravepy_int.h openravepy_global.cpp openravepy_collisionchecker.cpp openravepy_controller.cpp openravepy_ikparameterization.cpp openravepy_iksolver.cpp openravepy_module.cpp openravepy_physicsengine.cpp openravepy_planner.cpp openravepy_planningutils.cpp openravepy_sensor.cpp openravepy_sensorsystem.cpp openravepy_spacesampler.cpp openravepy_trajectory.cpp openravepy_viewer.cpp bindings.cpp bindings.h docstrings.cpp docstrings.h)
      # stdc++ has to be included before opengl libraries due to some ATI bug (http://wiki.fifengine.de/Segfault_in_cxa_allocate_exception#Workaround)
      target_link_libraries(openravepy_int ${STDC_LIBRARY} libopenrave libopenrave-core ${Boost_THREAD_LIBRARY} ${extralibs})
      target_link_libraries(openravepy_int ${PYTHON_LIBRARIES} ${Boost_PYTHON_LIBRARY} ${Boost_NUMPY_LIBRARY})
      add_dependencies(openravepy_int libopenrave libopenrave-core)
      set_target_properties(openravepy_int PROPERTIES PREFIX "" COMPILE_FLAGS "${OPENRAVEPY_COMPILE_FLAGS}")
      if( WIN32 )
        set_target_properties(openravepy_int PROPERTIES SUFFIX ".pyd")
      elseif( APPLE OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        # on mac osx, python cannot import libraries with .dylib extension
        set_target_properties(openravepy_int PROPERTIES SUFFIX ".so")
      endif()
      install(TARGETS openravepy_int DESTINATION ${OPENRAVEPY_VER_INSTALL_DIR} COMPONENT ${COMPONENT_PREFIX}python)
      
      ## ann bindings
      if( ANN_FOUND )
        add_library(pyANN_int SHARED pyann.cpp bindings.cpp bindings.h)
        include_directories(${ANN_INCLUDE_DIR})
        # stdc++ has to be included before opengl libraries due to some ATI bug (http://wiki.fifengine.de/  Segfault_in_cxa_allocate_exception#Workaround)
        target_link_libraries(pyANN_int ${STDC_LIBRARY} ANN ${PYTHON_LIBRARIES} ${Boost_PYTHON_LIBRARY} ${Boost_NUMPY_LIBRARY} ${Boost_THREAD_LIBRARY} ${extralibs})
        set_target_properties(pyANN_int PROPERTIES PREFIX "" COMPILE_FLAGS "${ANN_CFLAGS}" )
        add_dependencies(pyANN_int ANN)
        if( WIN32 )
          set_target_properties(pyANN_int PROPERTIES SUFFIX ".pyd")
          if( MSVC )
            # set "link library dependencies" for visual studio in order to include symbols for other statically linked libs
            # this is such an unbelievable hack, that it's disgusting
            set_target_properties(pyANN_int PROPERTIES STATIC_LIBRARY_FLAGS "\" LinkLibraryDependencies=\"true")
          endif()
        elseif( APPLE OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
          # on mac osx, python cannot import libraries with .dylib extension
          set_target_properties(pyANN_int PROPERTIES SUFFIX ".so")
        endif()
        install(TARGETS pyANN_int DESTINATION ${OPENRAVEPY_VER_INSTALL_DIR} COMPONENT ${COMPONENT_PREFIX}python)
      endif()

      ## convexdecomposition bindings
      if( CONVEXDECOMPOSITION_FOUND )
        add_library(convexdecompositionpy SHARED convexdecompositionpy.cpp bindings.cpp bindings.h)
        include_directories(${CONVEXDECOMPOSITION_INCLUDE_DIR})
        # stdc++ has to be included before opengl libraries due to some ATI bug (http://wiki.fifengine.de/  Segfault_in_cxa_allocate_exception#Workaround)
        target_link_libraries(convexdecompositionpy ${STDC_LIBRARY} convexdecomposition ${PYTHON_LIBRARIES} ${Boost_PYTHON_LIBRARY} ${Boost_NUMPY_LIBRARY} ${Boost_THREAD_LIBRARY} ${extralibs})
        set_target_properties(convexdecompositionpy PROPERTIES PREFIX "" COMPILE_FLAGS "${CONVEXDECOMPOSITION_CFLAGS}" )
        add_dependencies(convexdecompositionpy convexdecomposition)
        if( WIN32 )
          set_target_properties(convexdecompositionpy PROPERTIES SUFFIX ".pyd")
          if( MSVC )
            # set "link library dependencies" for visual studio in order to include symbols for other statically linked libs
            # this is such an unbelievable hack, that it's disgusting
            set_target_properties(convexdecompositionpy PROPERTIES STATIC_LIBRARY_FLAGS "\" LinkLibraryDependencies=\"true")
          endif()
        elseif( APPLE OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
          # on mac osx, python cannot import libraries with .dylib extension
          set_target_properties(convexdecompositionpy PROPERTIES SUFFIX ".so")
        endif()
        install(TARGETS convexdecompositionpy DESTINATION ${OPENRAVEPY_VER_INSTALL_DIR} COMPONENT ${COMPONENT_PREFIX}python)
      endif()
    else()
      message(STATUS "failed to use boost python libraries, check if python-numpy is installed.")
    endif()
  else()
    message(STATUS "failed to find development python headers please install them")
  endif()
else()
  message(STATUS "failed to find boost-python (${Boost_PYTHON_FOUND}) or boost-thread (${Boost_THREAD_FOUND}), please install them")
endif()
