#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace gp200::ampnames {
constexpr int pageCount=9,totalNames=71,namesPerPage=8,nameBytes=16,responseBytes=152;
inline unsigned read16(const std::uint8_t*p){return p[0]|(unsigned(p[1])<<8);}
inline std::uint32_t read32(const std::uint8_t*p){return read16(p)|(std::uint32_t(read16(p+2))<<16);}
inline void put16(std::uint8_t*p,unsigned v){p[0]=std::uint8_t(v);p[1]=std::uint8_t(v>>8);}
inline void put32(std::uint8_t*p,std::uint32_t v){put16(p,v);put16(p+2,v>>16);}
inline std::vector<std::uint8_t> request(int page,std::uint32_t nonce){
 if(page<0||page>=pageCount||!nonce)return {};
 std::array<std::uint8_t,28>b{};put16(b.data(),0x1009);put16(b.data()+2,24);put16(b.data()+4,0x414e);put16(b.data()+6,unsigned(page));put32(b.data()+8,0x31304e41);put32(b.data()+12,nonce);b[16]=1;
 std::vector<std::uint8_t>out{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,28,0,0,0};
 for(auto v:b){out.push_back(v>>4);out.push_back(v&15);}out.push_back(0xf7);return out;
}
struct Page{std::array<std::array<char,nameBytes>,namesPerPage>names{};int count=0;};
enum class Decode{unrelated,invalid,unavailable,valid};
inline Decode decode(const std::uint8_t*data,int size,int page,std::uint32_t nonce,Page& out){
 if(!data||size<46||data[0]!=0xf0||data[8]!=0x12)return Decode::unrelated;
 constexpr std::uint8_t head[]{0x21,0x25,0x7e,0x47,0x50,0x2d,0x32};
 for(int i=0;i<7;i++)if(data[i+1]!=head[i])return Decode::unrelated;
 std::array<std::uint8_t,responseBytes>b{};
 for(int i=0;i<16;i++){if(data[13+i*2]>15||data[14+i*2]>15)return Decode::unrelated;b[i]=std::uint8_t((data[13+i*2]<<4)|data[14+i*2]);}
 if(read16(b.data()+4)!=0x414e||read32(b.data()+8)!=0x31304e41)return Decode::unrelated;
 if(size!=14+2*responseBytes||data[9]!=(responseBytes&127)||data[10]!=(responseBytes>>7)||data[11]||data[12]||data[size-1]!=0xf7||page<0||page>=pageCount||!nonce)return Decode::invalid;
 unsigned sum=0;for(int i=0;i<responseBytes;i++){if(data[13+i*2]>15||data[14+i*2]>15)return Decode::invalid;b[i]=std::uint8_t((data[13+i*2]<<4)|data[14+i*2]);sum+=b[i];}
 if((sum&255)||read16(b.data())!=0x1009||read16(b.data()+2)!=148||read16(b.data()+6)!=unsigned(page)||read32(b.data()+12)!=nonce||b[16]!=1||b[17]>1||b[18]!=(page==8?7:8)||b[19]!=71||b[20]!=16||b[21]!=9||b[22])return Decode::invalid;
 Page next;next.count=b[18];
 for(int i=0;i<8;i++){
  bool ended=false;for(int j=0;j<16;j++){
   auto v=b[24+i*16+j];if(i>=next.count){if(v)return Decode::invalid;continue;}
   if(!v)ended=true;else if(ended||v<32||v>126||j==15)return Decode::invalid;
   next.names[std::size_t(i)][std::size_t(j)]=char(v);
  }
 }
 if(b[17])return Decode::unavailable;
 out=next;return Decode::valid;
}
} // namespace gp200::ampnames
