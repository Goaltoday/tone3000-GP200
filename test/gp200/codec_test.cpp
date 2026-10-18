#include "../../plugin/ui/views/gp200/GP200NAM.h"
#include "../../plugin/ui/views/gp200/GP200ModSyncProtocol.h"
#include "../../plugin/ui/views/gp200/Eligible.h"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
 assert(argc==2);juce::File file{juce::String(argv[1])};std::vector<juce::uint8> a,b;
 assert(gp200::GP200NAM::readWeights(file,a).wasOk());
 assert(gp200::GP200NAM::readJson(file.loadFileAsString(),b).wasOk());assert(a==b&&b.size()==7484);
 assert(gp200::GP200NAM::readJson("{}",b).failed()&&b.empty());
 auto query=gp200::GP200NAM::query(1,17,true,true);auto upload=gp200::GP200NAM::upload(a,1,18,"TEST",true);
 assert(query.chunks.size()==1&&upload.chunks.size()==42);
 // The allowed destination table must contain exactly 62 distinct stock IDs.
 int eligible=0;for(int i=0;i<71;++i)eligible+=lite::isEligible(i);assert(eligible==62);
 assert(!lite::isEligible(-1)&&!lite::isEligible(71));
 std::array<juce::uint8,28> raw{};
 gp200::modsync::write32(raw.data(),0x0018104b);gp200::modsync::write16(raw.data()+4,18);
 gp200::modsync::write16(raw.data()+6,1);gp200::modsync::write32(raw.data()+8,0x3430484e);
 gp200::modsync::write32(raw.data()+12,gp200::GP200NAM::binding);
 gp200::modsync::write32(raw.data()+16,gp200::modsync::ampIds[1]);
 gp200::modsync::write32(raw.data()+20,1);gp200::modsync::write32(raw.data()+24,7484);
 std::vector<juce::uint8> ack{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,28,0,0,0};
 for(auto v:raw){ack.push_back(v>>4);ack.push_back(v&15);}ack.push_back(0xf7);
 gp200::GP200NAM::Ack reply;assert(gp200::GP200NAM::decodeAck(ack.data(),int(ack.size()),reply));
 assert(reply.inplace&&reply.status==1&&reply.nonce==18&&reply.physical==1);
 ack[1]^=1;assert(!gp200::GP200NAM::decodeAck(ack.data(),int(ack.size()),reply));ack[1]^=1;
 ack.back()=0;assert(gp200::GP200NAM::decodeAck(ack.data(),int(ack.size()),reply)&&!reply.inplace&&reply.nonce==0);
 std::cout<<"Memory/file reader identical; invalid model rejected; NH04 query/upload frame counts PASS\n";
}
