// Seed generator (dev tool, not a target): writes fz::sample() through each output archive.
//   clang++ -std=c++17 -Iinclude -Imayhem/harnesses mayhem/harnesses/gen_seeds.cpp -o /tmp/gen_seeds
//   /tmp/gen_seeds mayhem
// writes mayhem/<target>/testsuite/sample.{json,xml,bin}.
#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/xml.hpp>
#include "fuzz_types.hpp"

#include <fstream>
#include <string>

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "mayhem";
  const fz::Root r = fz::sample();
  {
    std::ofstream os(dir + "/fuzz_json/testsuite/sample.json");
    cereal::JSONOutputArchive ar(os);
    ar(r);
  }
  {
    std::ofstream os(dir + "/fuzz_xml/testsuite/sample.xml");
    cereal::XMLOutputArchive ar(os);
    ar(r);
  }
  {
    std::ofstream os(dir + "/fuzz_pbin/testsuite/sample.bin", std::ios::binary);
    cereal::PortableBinaryOutputArchive ar(os);
    ar(r);
  }
  return 0;
}
