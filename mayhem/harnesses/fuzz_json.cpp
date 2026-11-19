// libFuzzer harness: cereal::JSONInputArchive (include/cereal/archives/json.hpp).
// The fuzz input is the archive stream; one archive(root) call drives the archive's parser plus
// every type loader in include/cereal/types (see fuzz_types.hpp).
#include <cereal/archives/json.hpp>
#include "fuzz_types.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  fz::g_live = 0;  // allocation budget is per input
  std::istringstream is(std::string(reinterpret_cast<const char*>(data), size));
  // cereal reports malformed input by throwing (cereal::Exception, RapidJSONException, and the
  // std::bad_alloc of the bounded containers): rejected input, not a defect.
  try {
    cereal::JSONInputArchive ar(is);
    fz::Root root;
    ar(root);
  } catch (const std::exception&) {
  }
  return 0;
}
