add_executable(tc_mine_gpu tc_mine_gpu.cpp tc_mine.cu)
add_dependencies(apps tc_mine_gpu)
target_link_libraries(tc_mine_gpu PRIVATE Galois::pangolin_gpu LLVMSupport)
target_compile_definitions(tc_mine_gpu PRIVATE GALOIS_ENABLE_GPU=1)
set_property(TARGET tc_mine_gpu PROPERTY CUDA_STANDARD 14)
install(TARGETS tc_mine_gpu DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT apps EXCLUDE_FROM_ALL)
#add_test_mine(small1 tc_mine "${BASEINPUT}/Mining/citeseer.csgr")
