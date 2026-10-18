#include "../../plugin/ui/views/gp200/PresetFrames.h"
#include <cassert>
#include <iostream>
using Frames=gp200::PresetFrames;
std::vector<uint8_t> packet(const std::vector<uint8_t>& b,int off,int n) {
 std::vector<uint8_t> x{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,uint8_t(b.size()&127),uint8_t(b.size()>>7),uint8_t(off&127),uint8_t(off>>7)};
 for(int i=off;i<off+n;++i){x.push_back(b[i]>>4);x.push_back(b[i]&15);}x.push_back(0xf7);return x;
}
int main(){
 for(int count:{5,7}){
  std::vector<uint8_t> data(count==5?925:1184);for(size_t i=0;i<data.size();++i)data[i]=uint8_t(i*17);
  std::vector<std::vector<uint8_t>> packets;
  for(size_t i=0;i<data.size();i+=185)packets.push_back(packet(data,int(i),int(std::min(size_t(185),data.size()-i))));
  Frames frames(count,count==5);
  // Out of order and exact duplicates are accepted; completion is byte exact.
  for(int i=count-1;i>=0;--i){auto& p=packets[i];auto result=frames.accept(p.data(),int(p.size()));assert(result==(i==0?Frames::Result::complete:Frames::Result::pending));if(i>0)assert(frames.accept(p.data(),int(p.size()))==Frames::Result::pending);}
  assert(frames.bytes==data);
  for(auto mutation:{0,1,2,3,4}){
   frames.clear();auto p=packets[0];
   if(mutation==0)p[13]=16; // invalid nibble
   if(mutation==1)p.pop_back(); // framing
   if(mutation==2)p[9]=0x80; // non MIDI header
   if(mutation==3){p[11]=127;p[12]=127;} // offset overflow
   if(mutation==4){p[9]=1;p[10]=0;} // declared size overflow
   assert(frames.accept(p.data(),int(p.size()))==Frames::Result::invalid);
  }
  frames.clear();auto p=packets[0];assert(frames.accept(p.data(),int(p.size()))==Frames::Result::pending);p[13]^=1;assert(frames.accept(p.data(),int(p.size()))==Frames::Result::invalid);
  frames.clear();p=packets[0];frames.accept(p.data(),int(p.size()));p=packets[1];p[9]^=1;assert(frames.accept(p.data(),int(p.size()))==Frames::Result::invalid);
  // Missing packets never complete; unrelated SysEx cannot satisfy the read.
  frames.clear();for(int i=0;i<count-1;++i){p=packets[i];assert(frames.accept(p.data(),int(p.size()))==Frames::Result::pending);}p=packets.back();p[8]=0x11;assert(frames.accept(p.data(),int(p.size()))==Frames::Result::unrelated);
 }
 std::cout<<"GP200 state/live frame regression tests PASS\n";
}
