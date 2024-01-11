# adding for mpi version of threaded_io_test
add_executable(mpi_threaded_io_test
  ../DeserializeStrategy.cc
  ../EmptySource.cc
  ../SerializeOutputer.cc
  ../PHDFBatchEventsOutputer.cc
  ../PHDFBatchEventsV2Outputer.cc
  ../HDFSource.cc
  ../Lane.cc
  ../PDSSource.cc
  ../RepeatingRootSource.cc
  ../RootSource.cc
  ../SerialRootSource.cc
  ../SharedRootEventSource.cc
  ../SharedRootBatchEventsSource.cc
  ../SerialTaskQueue.cc
  ../SerializeStrategy.cc
  ../SharedPDSSource.cc
  ../TestProductsOutputer.cc
  ../TestProductsSource.cc
  ../TextDumpOutputer.cc
  ../UnrolledDeserializer.cc
  ../UnrolledSerializer.cc
  ../common_unrolling.cc
  ../ConfigurationParameters.cc
  ../OutputerFactory.cc
  ../outputerFactoryGenerator.cc
  ../WaiterFactory.cc
  ../waiterFactoryGenerator.cc
  ../ScaleWaiter.cc
  ../EventSleepWaiter.cc
  ../EventUnevenSleepWaiter.cc
  ../pds_reading.cc
  ../pds_writer.cc
  ../pds_common.cc
  ../SourceFactory.cc
  ../sourceFactoryGenerator.cc
  mpi_threaded_io_test.cc)

target_include_directories(mpi_threaded_io_test PRIVATE "${PROJECT_BINARY_DIR}" ${HDF5_DIR}/include ${MPI_DIR}/include ${CLI11_DIR})
target_link_directories(mpi_threaded_io_test PRIVATE ${HDF5_DIR}/lib ${MPI_DIR}/lib)

target_link_libraries(mpi_threaded_io_test
                      PRIVATE LZ4::lz4
                              ROOT::Core
                              ROOT::RIO
                              ROOT::Tree
                              TBB::tbb
                              Threads::Threads
                              configKeys
                              sequence_classes_dict
			      PkgConfig::ZSTD
			      stdc++
			      hdf5_hl_parallel
			      hdf5_parallel
			      mpich
			      )
		      set_target_properties(mpi_threaded_io_test  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

