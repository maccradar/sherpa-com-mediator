add_executable(fsmloader fsmloader.c)
target_link_libraries(fsmloader rfsmbinding lua5.1)
set_target_properties(fsmloader PROPERTIES PREFIX "")

INSTALL(TARGETS fsmloader
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)