add_executable(betweennesscentrality-outer BetweennessCentralityOuter.cpp)
add_dependencies(apps betweennesscentrality-outer)
target_link_libraries(betweennesscentrality-outer PRIVATE Galois::shmem lonestar)
install(TARGETS betweennesscentrality-outer DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT apps EXCLUDE_FROM_ALL)
add_test_scale(small betweennesscentrality-outer "${BASEINPUT}/scalefree/rmat10.gr")

add_executable(bc-async BetweennessCentralityAsync.cpp)
add_dependencies(apps bc-async)
target_link_libraries(bc-async PRIVATE Galois::shmem lonestar)
install(TARGETS bc-async DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT apps EXCLUDE_FROM_ALL)

add_executable(bc-level BetweennessCentralityLevel.cpp)
add_dependencies(apps bc-level)
target_link_libraries(bc-level PRIVATE Galois::shmem lonestar)
install(TARGETS bc-level DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT apps EXCLUDE_FROM_ALL)
