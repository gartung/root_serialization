#if !defined(PHDFBatchEventsV2Outputer_h)
#define PHDFBatchEventsV2Outputer_h


#include <vector>
#include <string>
#include <cstdint>
#include <fstream>


#include "OutputerBase.h"
#include "EventIdentifier.h"
#include "SerializeStrategy.h"
#include "DataProductRetriever.h"
#include "pds_writer.h"

#include "SerialTaskQueue.h"

#include "HDFCxx.h"


namespace cce::tf {
  class PHDFBatchEventsV2Outputer : public OutputerBase {
    public:
    enum class CompressionChoice {
      kNone,
        kEvents,
        kBatch,
        kBoth
    };

    PHDFBatchEventsV2Outputer(std::string const& iFileName, unsigned int iNLanes, int iChunkSize, pds::Compression iCompression, int iCompressionLevel, CompressionChoice iChoice, pds::Serialization iSerialization, uint32_t iBatchSize, int numBatches);
    PHDFBatchEventsV2Outputer(PHDFBatchEventsV2Outputer&&) = default;
    PHDFBatchEventsV2Outputer(PHDFBatchEventsV2Outputer const&) = default;

  void setupForLane(unsigned int iLaneIndex, std::vector<DataProductRetriever> const& iDPs) final;

  void productReadyAsync(unsigned int iLaneIndex, DataProductRetriever const& iDataProduct, TaskHolder iCallback) const final;
  bool usesProductReadyAsync() const final {return true;}

  void outputAsync(unsigned int iLaneIndex, EventIdentifier const& iEventID, TaskHolder iCallback) const final;
  
  void printSummary() const final;

 private:

  void finishBatchAsync(unsigned int iBatchIndex, TaskHolder iCallback);

  void output(std::vector<EventIdentifier> iEventID, std::vector<char> iBuffer, std::vector<uint32_t> iOffset);
  void writeFileHeader(SerializeStrategy const& iSerializers);
  std::pair<std::vector<uint32_t>,std::vector<char>> writeDataProductsToOutputBuffer(SerializeStrategy const& iSerializers) const;

private:
  hdf5::File file_;
  hdf5::Group group_;
  mutable SerialTaskQueue queue_;
  int chunkSize_;
  mutable std::vector<SerializeStrategy> serializers_;

  //This is used as a circular buffer of length nLanes but only entries being used exist
  using EventInfo = std::tuple<EventIdentifier, std::vector<uint32_t>, std::vector<char>>;
  mutable std::vector<std::atomic<std::vector<EventInfo>*>> eventBatches_;
  mutable std::vector<std::atomic<uint32_t>> waitingEventsInBatch_;

  mutable std::atomic<uint64_t> presentEventEntry_;
  uint32_t batchSize_;
  mutable std::atomic<uint64_t> localEventcounter_;
  mutable bool writefirstEvent_ = true;
  pds::Compression compression_;
  int compressionLevel_;
  CompressionChoice compressionChoice_;
  pds::Serialization serialization_;
  mutable std::chrono::microseconds serialTime_;
  mutable std::atomic<std::chrono::microseconds::rep> parallelTime_;
  };    
}
#endif
