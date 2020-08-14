#include "TROOT.h"
#include "TVirtualStreamerInfo.h"
#include "TObject.h"
#include <iostream>
#include <vector>
#include <string>
#include <atomic>

#include "Outputer.h"
#include "Lane.h"

#include "tbb/task_group.h"
#include "tbb/global_control.h"
#include "tbb/task_scheduler_init.h"

#include "SerialTaskQueue.h"
#include "SerialTaskQueue.cc"

int main(int argc, char* argv[]) {

  if(not (argc > 1 and argc < 6) ) {
    std::cout <<"1 to 4 arguments required\n"
                "cms_read_threaded [# threads] [# conconcurrent events] [time scale factor]\n";
    return 1;
  }

  //Tell Root we want to be multi-threaded
  ROOT::EnableThreadSafety();
  
  //When threading, also have to keep ROOT from logging all TObjects into a list
  TObject::SetObjectStat(false);
  
  //Have to avoid having Streamers modify themselves after they have been used
  TVirtualStreamerInfo::Optimize(false);

  size_t parallelism = tbb::task_scheduler_init::default_num_threads();
  if(argc > 2) {
    parallelism = atoi(argv[2]);
  }
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, parallelism);

  std::vector<Lane> lanes;
  unsigned int nLanes = 4;
  if(argc > 3) {
    nLanes = atoi(argv[3]);
  }

  double scale = 0.;
  if(argc == 5) {
    scale = atof(argv[4]);
  }

  lanes.reserve(nLanes);
  for(unsigned int i = 0; i< nLanes; ++i) {
    lanes.emplace_back(argv[1],scale);
  }
  Outputer out;
  std::atomic<long> ievt{0};
  tbb::task_group group;
  
  auto start = std::chrono::high_resolution_clock::now();
  group.run([&]() {
      for(auto& lane : lanes) {
	lane.processEventsAsync(ievt, group, out);
      }
    });
    
  group.wait();
  std::chrono::microseconds eventTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()-start);

  std::cout <<"----------"<<std::endl;
  std::cout <<"Read file "<<argv[1]<<"\n"
	    <<"# threads "<<parallelism<<"\n"
	    <<"# concurrent events "<<nLanes <<"\n"
	    <<"time scale "<<scale<<"\n";
  std::cout <<"Event processing time: "<<eventTime.count()<<"us"<<std::endl;
  std::cout <<" number events: "<<ievt.load()<<std::endl;
  std::cout <<"----------"<<std::endl;

  std::chrono::microseconds sourceTime = std::chrono::microseconds::zero();

  std::vector<std::pair<const char*, std::chrono::microseconds>> serializerTimes;
  serializerTimes.reserve(lanes[0].serializers().size());
  bool isFirst = true;
  for(auto const& lane: lanes) {
    sourceTime += lane.sourceAccumulatedTime();
    if(isFirst) {
      isFirst = false;
      for(auto& s: lane.serializers()) {
	serializerTimes.emplace_back(s.name(), s.accumulatedTime());
      }
    } else {
      int i =0;
      for(auto& s: lane.serializers()) {
	serializerTimes[i++].second += s.accumulatedTime();
      }
    }
  }

  std::cout <<"\nSource time: "<<sourceTime.count()<<"us\n"<<std::endl;

  std::sort(serializerTimes.begin(),serializerTimes.end(), [](auto const& iLHS, auto const& iRHS) {
      return iLHS.second > iRHS.second;
    });
  
  std::cout <<"Serialization times"<<std::endl;
  for(auto const& p: serializerTimes) {
    std::cout <<p.first<<" time: "<<p.second.count()<<"us\n";
  }
}