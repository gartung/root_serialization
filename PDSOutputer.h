#if !defined(PDSOutputer_h)
#define PDSOutputer_h

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

#include "OutputerBase.h"
#include "EventIdentifier.h"
#include "SerializerWrapper.h"
#include "DataProductRetriever.h"

#include "SerialTaskQueue.h"

namespace cce::tf {
class PDSOutputer :public OutputerBase {
 public:
  enum class Compression {kNone, kLZ4, kZSTD};
 PDSOutputer(std::string const& iFileName, unsigned int iNLanes, Compression iCompression, int iCompressionLevel ): 
  file_(iFileName, std::ios_base::out| std::ios_base::binary),
  serializers_{std::size_t(iNLanes)},
  compression_{iCompression},
  compressionLevel_{iCompressionLevel},
  serialTime_{std::chrono::microseconds::zero()},
  parallelTime_{0}
  {}

  void setupForLane(unsigned int iLaneIndex, std::vector<DataProductRetriever> const& iDPs) final;

  void productReadyAsync(unsigned int iLaneIndex, DataProductRetriever const& iDataProduct, TaskHolder iCallback) const final;
  bool usesProductReadyAsync() const final {return true;}

  void outputAsync(unsigned int iLaneIndex, EventIdentifier const& iEventID, TaskHolder iCallback) const final;
  
  void printSummary() const final;

 private:
  static inline size_t bytesToWords(size_t nBytes) {
    return nBytes/4 + ( (nBytes % 4) == 0 ? 0 : 1);
  }

  void output(EventIdentifier const& iEventID, std::vector<SerializerWrapper> const& iSerializers, std::vector<uint32_t> const& iBuffer);
  void writeFileHeader(std::vector<SerializerWrapper> const& iSerializers);

  void writeEventHeader(EventIdentifier const& iEventID);
  std::vector<uint32_t> writeDataProductsToOutputBuffer(std::vector<SerializerWrapper> const& iSerializers) const;

  std::vector<uint32_t> compressBuffer(unsigned int iReserveFirstNWords, unsigned int iPadding, std::vector<uint32_t> const& iBuffer, int& oCompressedSize) const;

  std::vector<uint32_t> lz4CompressBuffer(unsigned int iReserveFirstNWords, unsigned int iPadding, std::vector<uint32_t> const& iBuffer, int& oCompressedSize) const;
  std::vector<uint32_t> zstdCompressBuffer(unsigned int iReserveFirstNWords, unsigned int iPadding, std::vector<uint32_t> const& iBuffer, int& oCompressedSize) const;
  std::vector<uint32_t> noCompressBuffer(unsigned int iReserveFirstNWords, unsigned int iPadding, std::vector<uint32_t> const& iBuffer, int& oCompressedSize) const;

private:
  std::ofstream file_;

  mutable SerialTaskQueue queue_;
  std::vector<std::pair<std::string, uint32_t>> dataProductIndices_;
  mutable std::vector<std::vector<SerializerWrapper>> serializers_;
  Compression compression_;
  int compressionLevel_;
  bool firstTime_ = true;
  mutable std::chrono::microseconds serialTime_;
  mutable std::atomic<std::chrono::microseconds::rep> parallelTime_;
};
}
#endif
