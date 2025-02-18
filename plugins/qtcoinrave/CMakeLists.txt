############################
# qtcoinrave openrave plugin
############################
find_package(Coin)
find_package(SoQt4)
if (NOT SOQT_LIBRARY_FOUND)
  message(STATUS "WARNING: SoQt4 not found, disabling QtCoin GUI plugin. Please install SoQt (http://www.coin3d.org)")
endif ()

find_package(OpenGL)
if(NOT OPENGL_FOUND)
  message(STATUS "WARNING: OpenGL not found. This library is not necessary, but it is good to link with")
endif()

find_package(Qt4 COMPONENTS QtCore QtGui QtDeclarative)
if (NOT QT_FOUND)
  message(STATUS "WARNING: Qt4 not found, is your QTDIR enviornment variable set?")
endif()

if (QT_FOUND AND QT_QTCORE_FOUND AND QT_QTGUI_FOUND AND SOQT_LIBRARY_FOUND AND COIN_LIBRARY_FOUND)
  
  message(STATUS "Detected SoQt/Coin3D GUI, making plugin")
  include_directories(${OPENGL_INCLUDE_DIR} ${SOQT_INCLUDE_DIRS} ${COIN_INCLUDE_DIRS})
  link_directories(${OPENRAVE_LINK_DIRS} ${SOQT_LINK_DIRS} ${COIN_LINK_DIRS})

  # generate rules for building source files from the resources
  set(QTCOIN_MOCS qtcoinviewer.h)
  include(${QT_USE_FILE})
  qt4_wrap_cpp(MOC_OUTPUT_FILES ${QTCOIN_MOCS})

  if (Boost_MINOR_VERSION LESS 65)
    QT4_WRAP_CPP(MOC_OUTPUT_FILES ${QTCOIN_MOCS} OPTIONS -DBOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION) #for boost previous to 1.65.1
  else(Boost_MINOR_VERSION LESS 65)
    QT4_WRAP_CPP(MOC_OUTPUT_FILES ${QTCOIN_MOCS} OPTIONS -DBOOST_TT_HAS_OPERATOR_HPP_INCLUDED -DBOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION -DBOOST_LEXICAL_CAST_INCLUDED -DBOOST_NEXT_PRIOR_HPP_INCLUDED -DBOOST_TYPE_TRAITS_HPP -D_SYS_SYSMACROS_H_OUTER) #for boost 1.65.1
  endif(Boost_MINOR_VERSION LESS 65)


  check_include_file(X11/Xlib.h HAVE_X11_XLIB_H)

   # build sources, moc'd sources, and rcc'd sources
  add_library(qtcoinrave SHARED ivselector.cpp item.cpp qtcoinviewer.cpp qtcoinrave.cpp ${MOC_OUTPUT_FILES} item.h ivselector.h qtcoin.h qtcoinviewer.h qtcameraviewer.h ivmodelloader.cpp)

  set(QTCOIN_COMPILE_FLAGS "${PLUGIN_COMPILE_FLAGS}")
  if( HAVE_X11_XLIB_H )
    set(QTCOIN_COMPILE_FLAGS "${QTCOIN_COMPILE_FLAGS} -DHAVE_X11_XLIB_H")
  endif()
  set(QTCOIN_LINK_FLAGS "${PLUGIN_LINK_FLAGS} ${COIN_LINK_FLAGS} ${SOQT_LINK_FLAGS}")

  set (extLIBS ${QT_LIBRARIES} ${LIBXML2_LIBRARIES} ${COIN_LIBRARY} ${SOQT_LIBRARY} ${OPENGL_LIBRARIES})
  if( MSVC )
    set(extLIBS ${extLIBS} winmm imm32 ws2_32)
  endif()

  target_link_libraries(qtcoinrave ${extLIBS} libopenrave)
  target_link_libraries(qtcoinrave PRIVATE boost_assertion_failed)
  set_target_properties(qtcoinrave PROPERTIES COMPILE_FLAGS "${QTCOIN_COMPILE_FLAGS} ${SOQT_CXXFLAGS} ${COIN_CXXFLAGS}" LINK_FLAGS "${QTCOIN_LINK_FLAGS}")
  install(TARGETS qtcoinrave DESTINATION ${OPENRAVE_PLUGINS_INSTALL_DIR} COMPONENT ${COMPONENT_PREFIX}plugin-qtcoinrave)
  
  if( MSVC )
    # have to install the DLLs
    install(DIRECTORY "${SOQT_LINK_DIRS}/" DESTINATION bin COMPONENT ${COMPONENT_PREFIX}plugin-qtcoinrave FILES_MATCHING PATTERN "*-${MSVC_PREFIX}-*.dll")
  endif()

  set(CPACK_COMPONENT_${COMPONENT_PREFIX_UPPER}PLUGIN-QTCOINRAVE_DISPLAY_NAME "Plugin for a Qt/Coin3d/SoQt viewer" PARENT_SCOPE)
  set(PLUGIN_COMPONENT ${COMPONENT_PREFIX}plugin-qtcoinrave PARENT_SCOPE)
  
else()
  message(STATUS "WARNING: Could not find Coin3D/SoQt/Qt libraries, will not compile qtcoinrave viewer.")
endif()
