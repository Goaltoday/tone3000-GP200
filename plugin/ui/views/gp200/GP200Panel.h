#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "services/Services.h"
#include "widgets/PillButton.h"
#include "widgets/TextField.h"
#include "widgets/form/SelectField.h"
#include "widgets/form/FormStyle.h"
#include "core/Fonts.h"
#include "core/Theme.h"
#include "GP200NAM.h"
#include "GP200ModSyncProtocol.h"
#include "GP200AmpNamesProtocol.h"
#include "Eligible.h"
#include "PresetFrames.h"
#include <memory>
#include <vector>
#include <array>
#include <algorithm>
#include <atomic>
#include <optional>

namespace t3k::ui {
class GP200Panel final : public juce::Component, private juce::Timer, private juce::MidiInputCallback {
public:
 explicit GP200Panel(Services& services) : services_(services) {
  for(auto* c:std::initializer_list<juce::Component*>{&input,&output,&refresh,&fileButton,&models,&note,&destination,&name,&send,&status})addAndMakeVisible(c);
  models.setPlaceholder("Choose a loaded NAM");
  models.onChange=[this](const juce::String& value){models.setValue(value);selectLoaded();};
  input.onChange=[this](const juce::String& value){input.setValue(value);queueAmpNames();};
  output.onChange=[this](const juce::String& value){output.setValue(value);queueAmpNames();};
  destination.onChange=[this](const juce::String& value){destination.setValue(value);};
  input.setPlaceholder("Choose MIDI input");output.setPlaceholder("Choose MIDI output");destination.setPlaceholder("Choose destination AMP");
  rebuildDestinations();
  name.setPlaceholder("Model name (1-12 ASCII characters)");name.setName("GP200 model name");
  name.setFontSize(form::kFieldPx);name.setCornerRadius(form::kFieldRadius);name.setBackground(theme::kBlack);name.setBorder(form::kFieldBorder);
  note.setText("NAM weights and name only; Tone3000 EQ, IR, gain and chain settings stay in Tone3000. Requires NH04 firmware. Select a different AMP on the pedal before uploading and keep the preset unchanged during transfer.",juce::dontSendNotification);
  note.setFont(Fonts::sans(12));note.setColour(juce::Label::textColourId,theme::kMuted);note.setJustificationType(juce::Justification::topLeft);
  status.setFont(Fonts::sans(13));status.setColour(juce::Label::textColourId,theme::kWhite);status.setJustificationType(juce::Justification::topLeft);
  note.setBorderSize(juce::BorderSize<int>(0));status.setBorderSize(juce::BorderSize<int>(0));
  refresh.setLabel("Refresh MIDI");fileButton.setLabel("Refresh NAMs");send.setLabel("Upload NAM");send.setEnabled(false);
  refresh.onClick=[this]{scan();queueAmpNames(true);};fileButton.onClick=[this]{choose();};send.onClick=[this]{begin();};
  setSize(1024,460);scan();choose();startTimer(20);
 }
 ~GP200Panel() override {stopTimer();if(in)in->stop();in.reset();out.reset();}
 void refreshLoaded(){if(!busy()){scan();choose();}}
 bool busy()const{return stage!=Stage::idle;}
 void paint(juce::Graphics&g)override{
  g.fillAll(theme::kBlack);const auto content=contentBounds();const int x=content.getX(),w=content.getWidth();
  g.setColour(theme::kWhite);g.setFont(Fonts::sans(20,true));g.drawText("GP200",x,14,w,26,juce::Justification::centredLeft);
  g.setFont(Fonts::sans(13));g.setColour(theme::kMuted);g.drawText("Send a loaded NAM to your pedal",x,44,w,20,juce::Justification::centredLeft);
  g.setColour(theme::kBorder);g.drawHorizontalLine(76,float(x),float(x+w));
  caption(g,input,"MIDI INPUT");caption(g,output,"MIDI OUTPUT");caption(g,models,"LOADED NAM");caption(g,destination,"DESTINATION AMP");caption(g,name,"MODEL NAME");
  const auto box=status.getBounds().expanded(12,10).toFloat();g.setColour(theme::kSurface);g.fillRoundedRectangle(box,form::kCardRadius);g.setColour(theme::kBorder);g.drawRoundedRectangle(box,form::kCardRadius,1.0f);
 }
 void resized()override{
  auto r=contentBounds();const int x=r.getX(),w=r.getWidth(),gap=12,refreshWidth=132,fieldWidth=juce::jmax(80,(w-refreshWidth-2*gap)/2);
  input.setBounds(x,110,fieldWidth,40);output.setBounds(x+fieldWidth+gap,110,fieldWidth,40);refresh.setBounds(x+w-refreshWidth,112,refreshWidth,36);
  models.setBounds(x,190,juce::jmax(80,w-refreshWidth-gap),40);fileButton.setBounds(x+w-refreshWidth,192,refreshWidth,36);
  const int targetWidth=juce::jmax(100,(w-gap)/2);destination.setBounds(x,270,targetWidth,40);name.setBounds(x+targetWidth+gap,270,w-targetWidth-gap,40);
  note.setBounds(x,326,juce::jmax(100,w-180),58);send.setBounds(x+w-160,330,160,40);status.setBounds(x+12,410,w-24,juce::jmax(30,getHeight()-430));
 }
private:
 Services& services_;
 struct Source{std::string blockId;int modelId;juce::String title;};
 std::vector<Source>sources_;
 juce::Array<juce::MidiDeviceInfo>midiInputs_,midiOutputs_;
 SelectField models{"Loaded NAM model"},input{"GP200 MIDI input"},output{"GP200 MIDI output"},destination{"GP200 destination AMP"};
 juce::Label note,status;
 PillButton refresh{"Refresh MIDI",PillButton::Style::outline},fileButton{"Refresh NAMs",PillButton::Style::outline},send{"Upload NAM",PillButton::Style::filled};
 TextField name;
 double nextMidiScan_=0;
 std::unique_ptr<juce::MidiInput>in;std::unique_ptr<juce::MidiOutput>out;
 bool modelReady_=false;std::vector<juce::uint8>weights;gp200::GP200IRUpload active,pending;
 enum class Stage{idle,names,preset,livePreset,query,write};Stage stage=Stage::idle;std::atomic<unsigned>generation{0};
 int packet=0,target=-1;unsigned nonce=0;double due=0;bool waiting=false;juce::String selectedName;
 gp200::PresetFrames stateFrames{5,true},liveFrames{7};
 bool presetRequestSent=false;int presetHandshakeStep=0,activeSlot=-1;
 // Session-only names. A complete nine-page snapshot is published atomically.
 std::array<juce::String,71>ampNames_{},incomingNames_{};
 juce::String namesPortKey_,attemptedNamesKey_;
 bool namesPending_=false;int namesPage_=0;std::uint32_t namesNonce_=0;

 juce::Rectangle<int>contentBounds()const{const int w=juce::jmax(1,juce::jmin(1000,getWidth()-48));return{(getWidth()-w)/2,0,w,getHeight()};}
 static void caption(juce::Graphics&g,const juce::Component&field,const juce::String&text){g.setColour(theme::kMuted);g.setFont(Fonts::tracked(Fonts::mono(11),0.04f));g.drawText(text,field.getX(),field.getY()-24,field.getWidth(),18,juce::Justification::centredLeft);}
 static int selectedId(const SelectField&field){return field.value()?field.value()->getIntValue():0;}
 static bool isGp200(const juce::String&v){return v.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").contains("GP200");}
 static bool sameDevices(const juce::Array<juce::MidiDeviceInfo>&a,const juce::Array<juce::MidiDeviceInfo>&b){if(a.size()!=b.size())return false;for(int i=0;i<a.size();++i)if(a[i].identifier!=b[i].identifier||a[i].name!=b[i].name)return false;return true;}
 static void populatePorts(SelectField&field,const juce::Array<juce::MidiDeviceInfo>&devices){
  const auto previous=field.value();std::optional<juce::String>preferred;std::vector<SelectField::Option>options;
  for(const auto&d:devices){options.push_back({d.identifier,d.name,{}});if(!preferred&&isGp200(d.name))preferred=d.identifier;}
  field.close();field.setOptions(std::move(options));for(const auto&d:devices)if(previous&&d.identifier==*previous){field.setValue(previous);return;}field.setValue(preferred);
 }
 static juce::MidiMessage sysex(const std::vector<juce::uint8>&b){return juce::MidiMessage::createSysExMessage(b.data()+1,int(b.size()-2));}
 void log(const juce::String&s){status.setText(s,juce::dontSendNotification);}
 void rebuildDestinations(){
  const auto selected=destination.value();std::vector<SelectField::Option>options;
  for(auto d:lite::eligible){const auto&live=ampNames_[size_t(d.number-1)];options.push_back({juce::String(d.number),juce::String(d.number)+" - "+(live.isNotEmpty()?live:juce::String(d.name)),{}});}
  destination.close();destination.setOptions(std::move(options));destination.setValue(selected);
 }
 juce::String selectedPortsKey()const{
  if(!input.value()||!output.value())return{};
  return juce::String(input.value()->length())+":"+*input.value()+*output.value();
 }
 void queueAmpNames(bool force=false){
  const auto key=selectedPortsKey();
  if(key!=namesPortKey_){namesPortKey_=key;attemptedNamesKey_.clear();ampNames_={};rebuildDestinations();}
  if(key.isEmpty()){namesPending_=false;return;}
  if(force||attemptedNamesKey_!=key)namesPending_=true;
 }
 bool openPorts(){
  if(!input.value()||!output.value())return false;
  in=juce::MidiInput::openDevice(*input.value(),this);out=juce::MidiOutput::openDevice(*output.value());
  if(!in||!out){if(in)in->stop();in.reset();out.reset();return false;}in->start();return true;
 }
 void startAmpNames(){
  namesPending_=false;attemptedNamesKey_=namesPortKey_;
  if(!openPorts()){log("Could not read AMP names: MIDI ports unavailable. Close other editors, then Refresh MIDI.");return;}
  incomingNames_={};namesPage_=0;namesNonce_=std::uint32_t(juce::Random::getSystemRandom().nextInt());if(!namesNonce_)namesNonce_=1;
  stage=Stage::names;waiting=false;due=juce::Time::getMillisecondCounterHiRes();send.setEnabled(false);setBusyControls(false);
  log("Reading AMP names from GP200...");
 }
 void consumeAmpNames(const juce::MidiMessage&m){
  gp200::ampnames::Page page;const auto result=gp200::ampnames::decode(m.getRawData(),m.getRawDataSize(),namesPage_,namesNonce_,page);
  if(result==gp200::ampnames::Decode::unrelated||result==gp200::ampnames::Decode::invalid||!waiting)return;
  if(result==gp200::ampnames::Decode::unavailable){reset("GP200 names are not ready. Select Refresh MIDI to try again.");return;}
  for(int i=0;i<page.count;i++)incomingNames_[size_t(namesPage_*8+i)]=juce::String::fromUTF8(page.names[size_t(i)].data());
  ++namesPage_;waiting=false;
  if(namesPage_==gp200::ampnames::pageCount){ampNames_=incomingNames_;rebuildDestinations();reset("AMP names received from GP200. Choose the destination AMP.");}
  else{due=juce::Time::getMillisecondCounterHiRes()+150;log("Reading AMP names: "+juce::String(namesPage_)+"/9...");}
 }
 void scan(){
  if(busy())return;midiInputs_=juce::MidiInput::getAvailableDevices();midiOutputs_=juce::MidiOutput::getAvailableDevices();
  populatePorts(input,midiInputs_);populatePorts(output,midiOutputs_);nextMidiScan_=juce::Time::getMillisecondCounterHiRes()+1000;queueAmpNames();
 }
 void choose(){
  if(busy())return;const int previousIndex=selectedId(models)-1;std::optional<Source>previous;
  if(previousIndex>=0&&previousIndex<int(sources_.size()))previous=sources_[size_t(previousIndex)];
  models.close();sources_.clear();weights.clear();modelReady_=false;send.setEnabled(false);services_.chain.refresh(true);std::vector<SelectField::Option>options;int selection=1;
  for(const auto*block:services_.chain.state().toneBlocks()){
   if(!block->tone.isNam()||!block->loaded||block->modelLoading||block->loadFailed)continue;juce::String title=block->tone.title;
   for(const auto&model:block->tone.models)if(model.id==block->activeModelId)title+=" / "+model.name;
   sources_.push_back({block->blockId,block->activeModelId,title});const int id=int(sources_.size());options.push_back({juce::String(id),title,{}});
   if(previous&&previous->blockId==block->blockId&&previous->modelId==block->activeModelId)selection=id;
  }
  models.setOptions(std::move(options));if(sources_.empty()){models.setValue(std::nullopt);name.setText({});log("Load a NAM in Tone3000, then select Refresh NAMs.");}
  else{models.setValue(juce::String(selection));selectLoaded();}
 }
 void selectLoaded(){
  if(busy())return;weights.clear();modelReady_=false;send.setEnabled(false);name.setText({});const int index=selectedId(models)-1;if(index<0||index>=int(sources_.size()))return;
  const auto&source=sources_[size_t(index)];const auto bytes=services_.backend.getGp200NamBytes(source.blockId,source.modelId);
  if(bytes.empty()){log("Model changed, is loading or unavailable. Refresh loaded NAMs.");return;}
  auto result=gp200::GP200NAM::readJson(juce::String::fromUTF8(reinterpret_cast<const char*>(bytes.data()),int(bytes.size())),weights);
  if(result.failed()){weights.clear();log("Not GP200 compatible: "+result.getErrorMessage());return;}
  juce::String ascii;for(auto c:source.title)if(c>=32&&c<=126)ascii+=juce::String::charToString(c);name.setText(ascii.substring(0,12));modelReady_=true;send.setEnabled(true);log("Compatible A2 Lite, 48 kHz. Ready to select the destination AMP.");
 }
 void setBusyControls(bool enabled){for(auto*field:{&models,&input,&output,&destination})field->setDisabled(!enabled);for(auto*c:std::initializer_list<juce::Component*>{&models,&fileButton,&refresh,&input,&output,&destination,&name})c->setEnabled(enabled);}
 void reset(const juce::String&s){++generation;stage=Stage::idle;setBusyControls(true);waiting=false;packet=0;presetRequestSent=false;presetHandshakeStep=0;activeSlot=-1;liveFrames.clear();active={};pending={};if(in)in->stop();in.reset();out.reset();send.setEnabled(modelReady_&&!weights.empty());log(s);}
 void begin(){
  if(stage!=Stage::idle||!modelReady_||weights.empty())return;target=selectedId(destination)-1;
  if(!lite::isEligible(target)){log("Select an eligible destination.");return;}
  selectedName=name.text().trim();if(selectedName.isEmpty()||selectedName.length()>12){log("Name must be 1-12 ASCII characters.");return;}for(auto c:selectedName)if(c<32||c>126){log("Name must be printable ASCII.");return;}
  if(!openPorts()){reset("Could not open MIDI ports. Close other loaders and disable this GP200 MIDI input in Tone3000 audio/MIDI settings, then refresh ports.");return;}
  nonce=(unsigned(juce::Time::getMillisecondCounter())%65000u)+1u;unsigned n2=nonce==65535u?1u:nonce+1u;
  active=gp200::GP200NAM::query(target,nonce,true,true);pending=gp200::GP200NAM::upload(weights,target,n2,selectedName,true);
  if(active.chunks.empty()||pending.chunks.empty()){reset("Unable to build NH04 transfer.");return;}
  send.setEnabled(false);setBusyControls(false);startPresetRead();
 }
 void startPresetRead(){
  static const std::vector<juce::uint8>identity{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x04,0,0,0,0,0x01,0x02,0,0,0,0,0,0xf7};
  stateFrames.clear();liveFrames.clear();presetRequestSent=false;presetHandshakeStep=0;stage=Stage::preset;waiting=false;out->sendMessageNow(sysex(identity));due=juce::Time::getMillisecondCounterHiRes()+150;log("Reading the LIVE preset before enabling any flash write...");
 }
 bool consumePreset(const juce::MidiMessage&m){
  if(!presetRequestSent||!m.isSysEx())return false;auto result=stateFrames.accept(m.getRawData(),m.getRawDataSize());
  if(result==gp200::PresetFrames::Result::unrelated)return false;
  if(result==gp200::PresetFrames::Result::invalid){reset("Invalid state dump. Upload blocked.");return true;}
  if(result!=gp200::PresetFrames::Result::complete)return true;
  const auto&all=stateFrames.bytes;activeSlot=int(all[8])|(int(all[9])<<8);
  if(activeSlot<0||activeSlot>=256){reset("Invalid current preset slot. Upload blocked.");return true;}
  presetRequestSent=false;liveFrames.clear();stage=Stage::livePreset;
  const juce::uint8 sh=juce::uint8((activeSlot>>4)&15),sl=juce::uint8(activeSlot&15);
  std::vector<juce::uint8>req{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x10,0,0,0,0,0,0,0,0,0x04,0,0,0,0,0,0,sh,sl,0,0,0,1,0,0,0,4,0,0,15,15,15,15,sh,sl,0,0,0xf7};
  out->sendMessageNow(sysex(req));due=juce::Time::getMillisecondCounterHiRes()+6000;log("Active preset identified; reading full live edit buffer (7 chunks)...");return true;
 }
 bool consumeLivePreset(const juce::MidiMessage&m){
  if(!m.isSysEx())return false;auto result=liveFrames.accept(m.getRawData(),m.getRawDataSize());
  if(result==gp200::PresetFrames::Result::unrelated)return false;
  if(result==gp200::PresetFrames::Result::invalid){reset("Invalid live preset. Upload blocked.");return true;}
  if(result!=gp200::PresetFrames::Result::complete)return true;
  const auto&all=liveFrames.bytes;const auto expected=gp200::modsync::ampIds[static_cast<std::size_t>(target)];
  for(int k=0;k<11;k++)if(gp200::modsync::read32(all.data()+120+k*72+8)==expected){reset("TARGET AMP IS ACTIVE in live preset. Select a different AMP and retry.");return true;}
  startNh04Query();return true;
 }
 void startNh04Query(){stage=Stage::query;packet=0;waiting=false;send.setEnabled(false);out->sendMessageNow(active.prepareMessage);due=juce::Time::getMillisecondCounterHiRes()+250;log("Live preset verified. NH04 capability query in progress...");}
 void timerCallback()override{
  const double now=juce::Time::getMillisecondCounterHiRes();
  if(stage==Stage::idle){
   if(!isShowing())return;
   if(now>=nextMidiScan_){nextMidiScan_=now+1000;if(!sameDevices(midiInputs_,juce::MidiInput::getAvailableDevices())||!sameDevices(midiOutputs_,juce::MidiOutput::getAvailableDevices()))scan();}
   if(namesPending_&&selectedPortsKey().isNotEmpty())startAmpNames();return;
  }
  if(!out||now<due)return;
  if(stage==Stage::names){
   if(waiting){reset("AMP names unavailable: AN01 firmware required. Upload remains available; Refresh MIDI retries the query.");return;}
   const auto bytes=gp200::ampnames::request(namesPage_,namesNonce_);waiting=true;due=now+1000;out->sendMessageNow(sysex(bytes));return;
  }
  if(stage==Stage::livePreset){reset("Timed out reading complete LIVE preset. Upload blocked; no flash write sent.");return;}
  if(stage==Stage::preset){
   if(presetHandshakeStep==0){static const std::vector<juce::uint8>editor{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x12,0,0,0,0xf7};out->sendMessageNow(sysex(editor));presetHandshakeStep=1;due=now+150;}
   else if(presetHandshakeStep==1){static const std::vector<juce::uint8>request{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,0x04,0,0,0,0,0x06,0x01,0,0,0,0,0,0xf7};out->sendMessageNow(sysex(request));presetRequestSent=true;presetHandshakeStep=2;due=now+6000;}
   else reset("Timed out reading LIVE preset. Upload blocked; no flash write sent.");return;
  }
  if(waiting){reset(stage==Stage::query?"No valid NH04 capability ACK; no model written.":"No verified flash ACK. Do not retry until device state is checked.");return;}
  if(packet>=static_cast<int>(active.chunks.size())){reset("Unexpected packet state.");return;}
  bool last=packet+1==static_cast<int>(active.chunks.size());if(last){waiting=true;due=now+30000;}else due=now+40;
  out->sendMessageNow(active.chunks[static_cast<size_t>(packet++)]);
  if(!last)log((stage==Stage::query?"Query ":"Writing ")+juce::String(packet)+"/"+juce::String(active.chunks.size()));else log(stage==Stage::query?"Waiting for NH04 query ACK...":"Waiting for NH04 flash readback ACK...");
 }
 void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage&message)override{
  if(!message.isSysEx())return;const auto epoch=generation.load();const auto bytes=std::vector<juce::uint8>(message.getRawData(),message.getRawData()+message.getRawDataSize());juce::Component::SafePointer<GP200Panel>safe(this);
  juce::MessageManager::callAsync([safe,bytes,epoch]{
   if(!safe||safe->generation.load()!=epoch)return;juce::MidiMessage incoming(bytes.data(),int(bytes.size()));
   if(safe->stage==Stage::names){safe->consumeAmpNames(incoming);return;}
   if(safe->stage==Stage::preset){safe->consumePreset(incoming);return;}if(safe->stage==Stage::livePreset){safe->consumeLivePreset(incoming);return;}
   gp200::GP200NAM::Ack reply;if(gp200::GP200NAM::decodeAck(bytes.data(),int(bytes.size()),reply))safe->onAck(reply);
  });
 }
 void onAck(const gp200::GP200NAM::Ack&ack){
  if(stage==Stage::idle||!waiting||!out)return;const unsigned expected=gp200::modsync::ampIds[static_cast<size_t>(target)];
  if(ack.nonce!=nonce||!ack.inplace)return;if(ack.id!=expected||ack.physical!=unsigned(target))return;
  if(ack.status!=1){reset("Device refused request, status "+juce::String(ack.status)+". Do not retry automatically.");return;}
  if(stage==Stage::query){stage=Stage::write;nonce=nonce==65535u?1u:nonce+1u;active=std::move(pending);packet=0;waiting=false;out->sendMessageNow(active.prepareMessage);due=juce::Time::getMillisecondCounterHiRes()+250;log("Firmware confirmed. Preparing NAM transfer...");}
  else if(stage==Stage::write){ampNames_[size_t(target)]=selectedName;rebuildDestinations();reset("Flash readback VERIFIED by NH04. Select destination AMP to hear NAM.");}
 }
};
} // namespace t3k::ui
