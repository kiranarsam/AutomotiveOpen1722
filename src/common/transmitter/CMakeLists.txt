project(Transmitter_Project)

add_library(transmitter STATIC
  "src/Transmitter.cpp"
)

target_include_directories(transmitter
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(transmitter
  comm_if
  avtp_util
  common_utils
  canreader
)

add_executable(test_transmitter
  "src/main_tx.cpp"
)

target_include_directories(test_transmitter
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(test_transmitter
  transmitter
)

install(TARGETS
  test_transmitter
  DESTINATION "${CMAKE_INSTALL_PREFIX}"
)
