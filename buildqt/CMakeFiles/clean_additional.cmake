# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/wxpegged_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/wxpegged_autogen.dir/ParseCache.txt"
  "wxpegged_autogen"
  )
endif()
