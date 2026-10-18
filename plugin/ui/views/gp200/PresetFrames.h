#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace gp200 {
// Strict decoder shared by state (5 packets) and live preset (7 packets).
// No MIDI I/O or writes: callers must wait for complete before acting.
class PresetFrames {
public:
 enum class Result { unrelated, invalid, pending, complete };
 explicit PresetFrames(int expectedPackets, bool stateDump=false)
     : expected_(expectedPackets), state_(stateDump) {}
 void clear() { chunks_.clear(); total_=-1; bytes.clear(); }
 Result accept(const uint8_t* data, int size) {
  static constexpr uint8_t prefix[]{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32};
  if(size<9||!std::equal(prefix,prefix+8,data)||data[8]!=0x12)return Result::unrelated;
  if(size<16||data[size-1]!=0xf7||((size-14)&1))return Result::invalid;
  for(int i=9;i<13;++i)if(data[i]>127)return Result::invalid;
  const int total=int(data[9])|(int(data[10])<<7),offset=int(data[11])|(int(data[12])<<7);
  if(total<=0||total>4096||(total_>=0&&total!=total_)||
     (state_&&(offset%185!=0||offset>740)))return Result::invalid;
  std::vector<uint8_t> decoded;
  for(int i=13;i<size-1;i+=2){
   if(data[i]>15||data[i+1]>15)return Result::invalid;
   decoded.push_back(uint8_t((data[i]<<4)|data[i+1]));
  }
  if(offset+int(decoded.size())>total)return Result::invalid;
  const auto found=chunks_.find(offset);
  if(found!=chunks_.end())return found->second==decoded?Result::pending:Result::invalid;
  total_=total;chunks_[offset]=std::move(decoded);
  if(int(chunks_.size())<expected_)return Result::pending;
  if(int(chunks_.size())!=expected_)return Result::invalid;
  bytes.clear();
  for(const auto& [at,chunk]:chunks_){
   if(at!=int(bytes.size()))return Result::invalid;
   bytes.insert(bytes.end(),chunk.begin(),chunk.end());
  }
  if(int(bytes.size())!=total_||bytes.size()<size_t(state_?10:912))return Result::invalid;
  return Result::complete;
 }
 std::vector<uint8_t> bytes;
private:
 int expected_, total_=-1;
 bool state_;
 std::map<int,std::vector<uint8_t>> chunks_;
};
}
