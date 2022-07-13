#include "PDSOutputer.h"
#include "OutputerFactory.h"
#include "ConfigurationParameters.h"
#include "UnrolledSerializerWrapper.h"
#include "SerializerWrapper.h"
#include "summarize_serializers.h"
#include "pds_writer.h"
#include <iostream>
#include <cstring>
#include <set>

using namespace cce::tf;
using namespace cce::tf::pds;

void PDSOutputer::setupForLane(unsigned int iLaneIndex, std::vector<DataProductRetriever> const& iDPs) {
  auto& s = serializers_[iLaneIndex];
  switch(serialization_) {
  case Serialization::kRoot:
    {   s = SerializeStrategy::make<SerializeProxy<SerializerWrapper>>(); break; }
  case Serialization::kRootUnrolled:
    {   s = SerializeStrategy::make<SerializeProxy<UnrolledSerializerWrapper>>(); break; }
  }
  s.reserve(iDPs.size());
  for(auto const& dp: iDPs) {
    s.emplace_back(dp.name(), dp.classType());
  }
}

void PDSOutputer::productReadyAsync(unsigned int iLaneIndex, DataProductRetriever const& iDataProduct, TaskHolder iCallback) const {
  auto& laneSerializers = serializers_[iLaneIndex];
  auto group = iCallback.group();
  laneSerializers[iDataProduct.index()].doWorkAsync(*group, iDataProduct.address(), std::move(iCallback));
}

void PDSOutputer::outputAsync(unsigned int iLaneIndex, EventIdentifier const& iEventID, TaskHolder iCallback) const {
  auto start = std::chrono::high_resolution_clock::now();
  auto tempBuffer = std::make_unique<std::vector<uint32_t>>(writeDataProductsToOutputBuffer(serializers_[iLaneIndex]));
  queue_.push(*iCallback.group(), [this, iEventID, iLaneIndex, callback=std::move(iCallback), buffer=std::move(tempBuffer)]() mutable {
      auto start = std::chrono::high_resolution_clock::now();
      const_cast<PDSOutputer*>(this)->output(iEventID, serializers_[iLaneIndex],*buffer);
      buffer.reset();
        serialTime_ += std::chrono::duration_cast<decltype(serialTime_)>(std::chrono::high_resolution_clock::now() - start);
      callback.doneWaiting();
    });
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);
    parallelTime_ += time.count();
}

void PDSOutputer::printSummary() const  {
  std::cout <<"PDSOutputer\n  total serial time at end event: "<<serialTime_.count()<<"us\n"
    "  total parallel time at end event: "<<parallelTime_.load()<<"us\n";
  summarize_serializers(serializers_);
}



void PDSOutputer::output(EventIdentifier const& iEventID, SerializeStrategy const& iSerializers, std::vector<uint32_t>const& iBuffer) {
  if(firstTime_) {
    writeFileHeader(iSerializers);
    firstTime_ = false;
  }
  using namespace std::string_literals;
  
  //std::cout <<"   run:"s+std::to_string(iEventID.run)+" lumi:"s+std::to_string(iEventID.lumi)+" event:"s+std::to_string(iEventID.event)+"\n"<<std::flush;
  
  writeEventHeader(iEventID);
  file_.write(reinterpret_cast<char const*>(iBuffer.data()), (iBuffer.size())*4);
  /*
    for(auto& s: iSerializers) {
    std::cout<<"   "s+s.name()+" size "+std::to_string(s.blob().size())+"\n" <<std::flush;
    }
  */
}

void PDSOutputer::writeFileHeader(SerializeStrategy const& iSerializers) {
  std::set<std::string> typeNamesSet;
  for(auto const& w: iSerializers) {
    std::string n(w.className());
    n.push_back('\0');
    auto it = typeNamesSet.insert(n);
  }
  std::vector<std::string> typeNames(typeNamesSet.begin(), typeNamesSet.end());
  typeNamesSet.clear();
  size_t nCharactersInTypeNames = 0U;
  for(auto const& n: typeNames) {
    nCharactersInTypeNames += n.size();
  }
  
  std::vector<std::pair<uint32_t, std::string>> dataProducts;
  dataProducts.reserve(iSerializers.size());
  dataProductIndices_.reserve(iSerializers.size());
  size_t nCharactersInDataProducts = 0U;
  size_t index = 0;
  for(auto const& s: iSerializers) {
    auto itFind = std::lower_bound(typeNames.begin(), typeNames.end(), s.className());
    std::string name{s.name()};
    name.push_back('\0');
    dataProducts.emplace_back(itFind - typeNames.begin(), name);
    dataProductIndices_.emplace_back(name,index++);
    //pad to 32 bit size
    while( 0 != dataProducts.back().second.size() % 4) {
      dataProducts.back().second.push_back('\0');
    }
    nCharactersInDataProducts += 4 + dataProducts.back().second.size();
  }
  
  std::array<char, 8> transitions = {'E','v','e','n','t','\0','\0','\0'};
  
  size_t bufferPosition = 0;
  std::vector<uint32_t> buffer;
  const auto nWordsInTypeNames = bytesToWords(nCharactersInTypeNames);
  buffer.resize(1+transitions.size()/4+1+nWordsInTypeNames+1+1+nCharactersInDataProducts/4);
  
  //The different record types stored
  buffer[bufferPosition++] = transitions.size()/4;
  std::memcpy(reinterpret_cast<char*>(buffer.data()+bufferPosition), transitions.data(), transitions.size());
  bufferPosition += transitions.size()/4;
  
  //The 'top level' types stored in the file
  buffer[bufferPosition++] = nWordsInTypeNames;
  
  size_t bufferPositionInChars = bufferPosition*4;
  for(auto const& t: typeNames) {
    std::memcpy(reinterpret_cast<char*>(buffer.data())+bufferPositionInChars, t.data(), t.size());
    bufferPositionInChars+=t.size();
  }
  //std::cout <<bufferPositionInChars<<" "<<bufferPosition*4<<" "<<bufferPositionInChars-bufferPosition*4<<" "<<nCharactersInTypeNames<<std::endl;
  assert(bufferPositionInChars-bufferPosition*4 == nCharactersInTypeNames);
  
  bufferPosition += nWordsInTypeNames;
  
  //Information about types that are not at the 'top level' (none for now)
  buffer[bufferPosition++] = 0;
  
  //The different data products to be stored
  buffer[bufferPosition++] = dataProducts.size();
  for(auto const& dp : dataProducts) {
    buffer[bufferPosition++] = dp.first;
    std::memcpy(reinterpret_cast<char*>(buffer.data()+bufferPosition), dp.second.data(), dp.second.size());
    assert(0 == dp.second.size() % 4);
    bufferPosition += dp.second.size()/4;
  }
  
  {
    //The file type identifier
    uint32_t comp = 0;
    if(serialization_ == Serialization::kRootUnrolled) {
      comp = 1;
    }
    const uint32_t id = 3141592*256+1 + comp;
    file_.write(reinterpret_cast<char const*>(&id), 4);
  }
  {
    //The 'unique' file id, just dummy for now
    const uint32_t fileID = 0;
    file_.write(reinterpret_cast<char const*>(&fileID), 4);     
  }
  {
    //Compression type used
    // note want exactly 4 bytes so sometimes skip trailing \0
    file_.write(pds::name(compression_), 4);
  }
  
  //The size of the header buffer in words (excluding first 3 words)
  const uint32_t bufferSize = buffer.size();
  file_.write(reinterpret_cast<char const*>(&bufferSize), 4);
  
  file_.write(reinterpret_cast<char const*>(buffer.data()), bufferSize*4);
  //for(auto v: buffer) {
  //  file_.write(reinterpret_cast<char const*>(&v), sizeof(v));
  //}
  
  //The size of the header buffer in words (excluding first 3 words)
  file_.write(reinterpret_cast<char const*>(&bufferSize), 4);
}

void PDSOutputer::writeEventHeader(EventIdentifier const& iEventID) {
  constexpr unsigned int headerBufferSizeInWords = 5;
  std::array<uint32_t,headerBufferSizeInWords> buffer;
  buffer[0] = 0; //Record index for Event
  buffer[1] = iEventID.run;
  buffer[2] = iEventID.lumi;
  buffer[3] = (iEventID.event >> 32) & 0xFFFFFFFF;
  buffer[4] = iEventID.event & 0xFFFFFFFF;
  file_.write(reinterpret_cast<char const*>(buffer.data()), headerBufferSizeInWords*4);
}

std::vector<uint32_t> PDSOutputer::writeDataProductsToOutputBuffer(SerializeStrategy const& iSerializers) const{
  //Calculate buffer size needed
  uint32_t bufferSize = 0;
  for(auto const& s: iSerializers) {
    bufferSize +=1+1;
    auto const blobSize = s.blob().size();
    bufferSize += bytesToWords(blobSize); //handles padding
  }
  //initialize with 0
  std::vector<uint32_t> buffer(size_t(bufferSize), 0);
  
  {
    uint32_t bufferIndex = 0;
    uint32_t dataProductIndex = 0;
    for(auto const& s: iSerializers) {
      buffer[bufferIndex++]=dataProductIndex++;
      auto const blobSize = s.blob().size();
      uint32_t sizeInWords = bytesToWords(blobSize);
      buffer[bufferIndex++]=sizeInWords;
      std::copy(s.blob().begin(), s.blob().end(), reinterpret_cast<char*>( &(*(buffer.begin()+bufferIndex)) ) );
      bufferIndex += sizeInWords;
    }
    assert(buffer.size() == bufferIndex);
  }

  auto [cBuffer,cSize] = compressBuffer(2, 1, buffer);

  //std::cout <<"compressed "<<cSize<<" uncompressed "<<buffer.size()*4<<std::endl;
  //std::cout <<"compressed "<<(buffer.size()*4)/float(cSize)<<std::endl;
  uint32_t const recordSize = bytesToWords(cSize)+1;
  cBuffer[0] = recordSize;
  //Record the actual number of bytes used in the last word of the compression buffer in the lowest
  // 2 bits of the word
  cBuffer[1] = buffer.size()*4 + (cSize % 4);
  if(cBuffer.size() != recordSize+2) {
    std::cout <<"BAD BUFFER SIZE: want: "<<recordSize+2<<" got "<<cBuffer.size()<<std::endl;
  }
  assert(cBuffer.size() == recordSize+2);
  cBuffer[recordSize+1]=recordSize;
  return cBuffer;
}

std::pair<std::vector<uint32_t>,int> PDSOutputer::compressBuffer(unsigned int iLeadPadding, unsigned int iTrailingPadding, std::vector<uint32_t> const& iBuffer) const {
  return pds::compressBuffer(iLeadPadding, iTrailingPadding, compression_, compressionLevel_, iBuffer);
}

namespace {

  class PDSMaker : public OutputerMakerBase {
  public:
    PDSMaker(): OutputerMakerBase("PDSOutputer") {}

    std::unique_ptr<OutputerBase> create(unsigned int iNLanes, ConfigurationParameters const& params, int) const final {

      auto fileName = params.get<std::string>("fileName");
      if(not fileName) {
        std::cout <<"no file name given for PDSOutputer\n";
        return {};
      }

      int compressionLevel = params.get<int>("compressionLevel", 18);

      auto compressionName = params.get<std::string>("compressionAlgorithm", "ZSTD");
      auto serializationName = params.get<std::string>("serializationAlgorithm", "ROOT");

      auto compression = pds::toCompression(compressionName);
      if(not compression) {
        std::cout <<"unknown compression "<<compressionName<<std::endl;
        return {};
      }

      auto serialization = pds::toSerialization(serializationName);
      if(not serialization) {
        std::cout <<"unknown serialization "<<serializationName<<std::endl;
        return {};
      }
      
      return std::make_unique<PDSOutputer>(*fileName,iNLanes, *compression, compressionLevel, *serialization);
    }
    
  };

  PDSMaker s_maker;
}


