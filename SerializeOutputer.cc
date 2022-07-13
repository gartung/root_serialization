#include "SerializeOutputer.h"
#include "OutputerFactory.h"
#include <iostream>

namespace cce::tf {
namespace {
    class Maker : public OutputerMakerBase {
  public:
    Maker(): OutputerMakerBase("SerializeOutputer") {}
    std::unique_ptr<OutputerBase> create(unsigned int iNLanes, ConfigurationParameters const& params, int) const final {
      bool verbose = params.get<bool>("verbose",false);
      return std::make_unique<SerializeOutputer>(iNLanes, verbose);
    }
    };

  Maker s_maker;
}
}
