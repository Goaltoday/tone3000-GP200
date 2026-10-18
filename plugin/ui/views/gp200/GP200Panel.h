#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "services/Services.h"
#include "widgets/PillButton.h"
#include "GP200NAM.h"
#include "GP200ModSyncProtocol.h"
#include "Eligible.h"
#include "PresetFrames.h"
#include <memory>
#include <vector>
#include <array>
#include <algorithm>
#include <atomic>

namespace t3k::ui {
class GP200Panel final : public juce::Component, private juce::Timer, private juce::MidiInputCallback {
public:
 explicit GP200Panel(Services& services) : services_(services) {
  addAndMakeVisible(input); addAndMakeVisible(output); addAndMakeVisible(refresh); addAndMakeVisible(fileButton);
  addAndMakeVisible(models);addAndMakeVisible(note);
  models.setTextWhenNothingSelected("Choose a loaded NAM");models.setName("Loaded NAM model");models.onChange=[this]{selectLoaded();};
  note.setText("Transfers the NAM only. Tone3000 EQ, IR, gain and chain settings are not transferred. Requires NH04 firmware. Select another AMP on the pedal before uploading; keep the preset unchanged during upload.",juce::dontSendNotification);
  note.setColour(juce::Label::textColourId,juce::Colours::lightgrey);
  addAndMakeVisible(destination);addAndMakeVisible(name);addAndMakeVisible(send);addAndMakeVisible(status);
  refresh.setButtonText("Refresh MIDI"); fileButton.setButtonText("Refresh loaded NAMs");send.setButtonText("Upload NAM HOT");
  input.setTextWhenNothingSelected("MIDI input");output.setTextWhenNothingSelected("MIDI output");
  for(auto d:lite::eligible) destination.addItem(juce::String(d.number)+" - "+d.name,d.number);
  name.setTextToShowWhenEmpty("Model name (max 12 ASCII)",juce::Colours::grey);
  status.setColour(juce::Label::textColourId,juce::Colours::white);
  send.setEnabled(false);
  refresh.onClick=[this]{scan();};fileButton.onClick=[this]{choose();};send.onClick=[this]{begin();};
  setSize(1024,460);scan();choose();startTimer(20);
 }
 ~GP200Panel() override { stopTimer(); if(in) in->stop(); in.reset(); out.reset(); }
 void refreshLoaded() { if(!busy()) choose(); }
 bool busy() const { return stage!=Stage::idle; }
 void paint(juce::Graphics&g) override {g.fillAll(juce::Colour(0xff161c26));g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(19.0f));g.drawText("GP200 | Loaded NAMs → pedal",20,12,getWidth()-40,30,juce::Justification::centredLeft);}
 void resized() override {
  auto r=getLocalBounds().reduced(20);r.removeFromTop(50);auto a=r.removeFromTop(35);input.setBounds(a.removeFromLeft(245));a.removeFromLeft(8);output.setBounds(a.removeFromLeft(245));a.removeFromLeft(8);refresh.setBounds(a);
  r.removeFromTop(10);a=r.removeFromTop(36);fileButton.setBounds(a.removeFromLeft(160));a.removeFromLeft(10);destination.setBounds(a.removeFromLeft(240));a.removeFromLeft(10);name.setBounds(a);
  r.removeFromTop(12);models.setBounds(r.removeFromTop(36));
  r.removeFromTop(10);note.setBounds(r.removeFromTop(55));
  r.removeFromTop(15);send.setBounds(r.removeFromTop(40));r.removeFromTop(10);status.setBounds(r.removeFromTop(95));
 }
private:
 Services& services_;
 struct Source { std::string blockId; int modelId; juce::String title; };
 std::vector<Source> sources_;
 juce::Array<juce::MidiDeviceInfo> midiInputs_,midiOutputs_;
 juce::ComboBox models;juce::Label note;
 juce::ComboBox input,output,destination;PillButton refresh{"Refresh MIDI", PillButton::Style::outline},fileButton{"Refresh loaded NAMs", PillButton::Style::outline},send{"Upload NAM HOT", PillButton::Style::filled};juce::TextEditor name;juce::Label status;
 std::unique_ptr<juce::MidiInput> in;std::unique_ptr<juce::MidiOutput> out;
 std::vector<juce::uint8> weights;gp200::GP200IRUpload active,pending;
 enum class Stage {idle,preset,livePreset,query,write};Stage stage=Stage::idle;std::atomic<unsigned> generation{0};
 int packet=0,target=-1;unsigned nonce=0;double due=0;bool waiting=false;
 juce::String selectedName;
 gp200::PresetFrames stateFrames{5,true}, liveFrames{7};
 bool presetRequestSent=false;int presetHandshakeStep=0;int activeSlot=-1;
 static juce::MidiMessage sysex(const std::vector<juce::uint8>& b){return juce::MidiMessage::createSysExMessage(b.data()+1,int(b.size()-2));}
 void startPresetRead(){
  // Original VST initial handshake: identity -> editor mode -> current state dump.
  static const std::vector<juce::uint8> identity{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x04,0,0,0,0,0x01,0x02,0,0,0,0,0,0xf7};
  stateFrames.clear();liveFrames.clear();presetRequestSent=false;presetHandshakeStep=0;
  stage=Stage::preset;waiting=false;out->sendMessageNow(sysex(identity));
  due=juce::Time::getMillisecondCounterHiRes()+150;
  log("Reading the LIVE preset before enabling any flash write...");
 }
 bool consumePreset(const juce::MidiMessage& m){
  if(!presetRequestSent||!m.isSysEx())return false;
  auto result=stateFrames.accept(m.getRawData(),m.getRawDataSize());
  if(result==gp200::PresetFrames::Result::unrelated)return false;
  if(result==gp200::PresetFrames::Result::invalid){reset("Invalid state dump. Upload blocked.");return true;}
  if(result!=gp200::PresetFrames::Result::complete)return true;
  const auto& all=stateFrames.bytes;
  activeSlot=int(all[8]) | (int(all[9])<<8);
  if(activeSlot<0||activeSlot>=256){reset("Invalid current preset slot. Upload blocked.");return true;}
  presetRequestSent=false;liveFrames.clear();stage=Stage::livePreset;
  const juce::uint8 sh=juce::uint8((activeSlot>>4)&15),sl=juce::uint8(activeSlot&15);
  std::vector<juce::uint8> req{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,
    0x11,0x10,0,0,0,0,0,0,0,0,0x04,0,0,0,0,0,0,sh,sl,0,0,0,1,0,0,0,4,0,0,15,15,15,15,sh,sl,0,0,0xf7};
  out->sendMessageNow(sysex(req));due=juce::Time::getMillisecondCounterHiRes()+6000;
  log("Active preset identified; reading full live edit buffer (7 chunks)...");return true;
 }
 bool consumeLivePreset(const juce::MidiMessage& m){
  if(!m.isSysEx())return false;
  auto result=liveFrames.accept(m.getRawData(),m.getRawDataSize());
  if(result==gp200::PresetFrames::Result::unrelated)return false;
  if(result==gp200::PresetFrames::Result::invalid){reset("Invalid live preset. Upload blocked.");return true;}
  if(result!=gp200::PresetFrames::Result::complete)return true;
  const auto& all=liveFrames.bytes;
  const auto expected=gp200::modsync::ampIds[static_cast<std::size_t>(target)];
  for(int k=0;k<11;k++)if(gp200::modsync::read32(all.data()+120+k*72+8)==expected){reset("TARGET AMP IS ACTIVE in live preset. Select a different AMP and retry.");return true;}
  startNh04Query();return true;
 }
 void startNh04Query(){
  stage=Stage::query;packet=0;waiting=false;send.setEnabled(false);
  out->sendMessageNow(active.prepareMessage);due=juce::Time::getMillisecondCounterHiRes()+250;
  log("Live preset verified. NH04 capability query in progress...");
 }

 void log(const juce::String&s){status.setText(s,juce::dontSendNotification);}
 void scan(){midiInputs_=juce::MidiInput::getAvailableDevices();midiOutputs_=juce::MidiOutput::getAvailableDevices();const auto& ins=midiInputs_;const auto& outs=midiOutputs_;input.clear();output.clear();for(int i=0;i<ins.size();++i)input.addItem(ins[i].name,i+1);for(int i=0;i<outs.size();++i)output.addItem(outs[i].name,i+1);log("Select GP-200 MIDI ports, a loaded NAM and an eligible Factory AMP.");}
 void choose(){
  if(busy())return;
  models.clear(juce::dontSendNotification);sources_.clear();weights.clear();send.setEnabled(false);
  services_.chain.refresh(true);
  for(const auto* block:services_.chain.state().toneBlocks()){
   if(!block->tone.isNam()||!block->loaded||block->modelLoading||block->loadFailed)continue;
   juce::String title=block->tone.title;
   for(const auto& model:block->tone.models)if(model.id==block->activeModelId)title+=" / "+model.name;
   sources_.push_back({block->blockId,block->activeModelId,title});models.addItem(title,int(sources_.size()));
  }
  log(sources_.empty()?"Load a NAM in Tone3000 first, then refresh this list.":"Choose a loaded NAM. Compatibility is checked from its cached bytes.");
 }
 void selectLoaded(){
  if(busy())return;
  weights.clear();send.setEnabled(false);const int index=models.getSelectedId()-1;
  if(index<0||index>=int(sources_.size()))return;
  const auto& source=sources_[size_t(index)];
  const auto bytes=services_.backend.getGp200NamBytes(source.blockId,source.modelId);
  if(bytes.empty()){log("Model changed, is loading or unavailable. Refresh loaded NAMs.");return;}
  auto result=gp200::GP200NAM::readJson(juce::String::fromUTF8(reinterpret_cast<const char*>(bytes.data()),int(bytes.size())),weights);
  if(result.failed()){log("Not GP200 compatible: "+result.getErrorMessage());return;}
  juce::String ascii;for(auto c:source.title)if(c>=32&&c<=126)ascii+=juce::String::charToString(c);
  name.setText(ascii.substring(0,12));send.setEnabled(true);log("Compatible A2 Lite, 48 kHz. Ready to select the destination AMP.");
 }
 void setBusyControls(bool enabled){for(auto* c:std::initializer_list<juce::Component*>{&models,&fileButton,&refresh,&input,&output,&destination,&name})c->setEnabled(enabled);}
 void reset(const juce::String& s){++generation;stage=Stage::idle;setBusyControls(true);waiting=false;packet=0;presetRequestSent=false; presetHandshakeStep=0;activeSlot=-1;liveFrames.clear();active={};pending={};if(in)in->stop();in.reset();out.reset();send.setEnabled(!weights.empty());log(s);}
 void begin(){if(stage!=Stage::idle||weights.empty())return;target=destination.getSelectedId()-1;
  if(!lite::isEligible(target)){log("Select an eligible destination.");return;}
  selectedName=name.getText().trim();if(selectedName.isEmpty()||selectedName.length()>12){log("Name must be 1-12 ASCII characters.");return;}for(auto c:selectedName)if(c<32||c>126){log("Name must be printable ASCII.");return;}
  const auto& ins=midiInputs_;const auto& outs=midiOutputs_;int i=input.getSelectedId()-1,o=output.getSelectedId()-1;
  if(i<0||i>=ins.size()||o<0||o>=outs.size()){log("Choose both MIDI ports.");return;}
  in=juce::MidiInput::openDevice(ins[i].identifier,this);out=juce::MidiOutput::openDevice(outs[o].identifier);
  if(!in||!out){reset("Could not open MIDI ports. Close other loaders and disable this GP200 MIDI input in Tone3000 audio/MIDI settings, then refresh ports.");return;}in->start();
  nonce=(unsigned(juce::Time::getMillisecondCounter())%65000u)+1u;unsigned n2=nonce==65535u?1u:nonce+1u;
  active=gp200::GP200NAM::query(target,nonce,true,true);pending=gp200::GP200NAM::upload(weights,target,n2,selectedName,true);
  if(active.chunks.empty()||pending.chunks.empty()){reset("Unable to build NH04 transfer.");return;}
  send.setEnabled(false);setBusyControls(false);startPresetRead();
 }
 void timerCallback() override {if(stage==Stage::idle||!out)return;double now=juce::Time::getMillisecondCounterHiRes();if(now<due)return;
  if(stage==Stage::livePreset){reset("Timed out reading complete LIVE preset. Upload blocked; no flash write sent.");return;}
  if(stage==Stage::preset){
   if(presetHandshakeStep==0){
    static const std::vector<juce::uint8> editor{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x12,0,0,0,0xf7};
    out->sendMessageNow(sysex(editor));presetHandshakeStep=1;due=now+150;
   }else if(presetHandshakeStep==1){
    static const std::vector<juce::uint8> request{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x04,0,0,0,0,0x06,0x01,0,0,0,0,0,0xf7};
    out->sendMessageNow(sysex(request));presetRequestSent=true;presetHandshakeStep=2;due=now+6000;
   }else reset("Timed out reading LIVE preset. Upload blocked; no flash write sent.");
   return;
  }
  if(waiting){reset(stage==Stage::query?"No valid NH04 capability ACK; no model written.":"No verified flash ACK. Do not retry until device state is checked.");return;}
  if(packet>=static_cast<int>(active.chunks.size())){reset("Unexpected packet state.");return;}
  bool last=packet+1==static_cast<int>(active.chunks.size());
  // The final data packet triggers native execution. No stock IR commit message is sent.
  if(last){waiting=true;due=now+30000;}else due=now+40;
  out->sendMessageNow(active.chunks[static_cast<size_t>(packet++)]);
  if(!last)log((stage==Stage::query?"Query ":"Writing ")+juce::String(packet)+"/"+juce::String(active.chunks.size()));
  else log(stage==Stage::query?"Waiting for NH04 query ACK...":"Waiting for NH04 flash readback ACK...");
 }
 void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& message) override {
  if(!message.isSysEx())return;
  const auto epoch=generation.load();
  const auto bytes=std::vector<juce::uint8>(message.getRawData(),message.getRawData()+message.getRawDataSize());
  juce::Component::SafePointer<GP200Panel> safe(this);
  juce::MessageManager::callAsync([safe,bytes,epoch]{
   if(!safe||safe->generation.load()!=epoch)return;
   juce::MidiMessage incoming(bytes.data(),int(bytes.size()));
   if(safe->stage==Stage::preset){safe->consumePreset(incoming);return;}
   if(safe->stage==Stage::livePreset){safe->consumeLivePreset(incoming);return;}
   gp200::GP200NAM::Ack reply;
   if(gp200::GP200NAM::decodeAck(bytes.data(),int(bytes.size()),reply))safe->onAck(reply);
  });
 }
 void onAck(const gp200::GP200NAM::Ack& ack){
  if(stage==Stage::idle||!waiting||!out)return;
  const unsigned expected=gp200::modsync::ampIds[static_cast<size_t>(target)];
  if(ack.nonce!=nonce||!ack.inplace)return;
  if(ack.id!=expected||ack.physical!=unsigned(target))return;
  if(ack.status!=1){reset("Device refused request, status "+juce::String(ack.status)+". Do not retry automatically.");return;}
  if(stage==Stage::query){
   stage=Stage::write;nonce=nonce==65535u?1u:nonce+1u;active=std::move(pending);packet=0;waiting=false;
   out->sendMessageNow(active.prepareMessage);due=juce::Time::getMillisecondCounterHiRes()+250;
   log("Firmware confirmed. Preparing NAM transfer...");
  }else reset("Flash readback VERIFIED by NH04. Select destination AMP to hear NAM.");
 }

};

} // namespace t3k::ui
