add_library(rfsmbinding SHARED rfsmbinding.c)
target_link_libraries(rfsmbinding rfsmbinding lua5.1)

INSTALL(TARGETS  rfsmbinding#StaticLibraryTarget SharedLibraryTarget
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)