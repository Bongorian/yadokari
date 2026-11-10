import os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0,str(root/'tools/testing'))
from circle_source import require_circle
circle=require_circle(root)
original=subprocess.check_output(['git','-C',str(circle),'show','HEAD:lib/usb/usbdevice.cpp'])
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); subprocess.run(['git','init','-q',str(path)],check=True)
    target=path/'lib/usb/usbdevice.cpp'; target.parent.mkdir(parents=True); target.write_bytes(original)
    subprocess.run(['git','-C',str(path),'add','lib/usb/usbdevice.cpp'],check=True)
    cmd=['python3',str(root/'scripts/patch-circle.py'),str(path)]
    subprocess.run(cmd,check=True); result=target.read_text(); subprocess.run(cmd,check=True)
    assert result==target.read_text() and result.count('GROOVE_OPTIONAL_INTERFACE_HOOK')==1
print('Circle optional hook is idempotent')

# The priming patch must be reproducible and idempotent against pinned source.
original_audio=subprocess.check_output(['git','-C',str(circle),'show','HEAD:lib/sound/usbsoundbasedevice.cpp'])
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/sound/usbsoundbasedevice.cpp'
    target.parent.mkdir(parents=True);target.write_bytes(original_audio)
    cmd=['python3',str(root/'scripts/patch-circle-audio.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_AUDIO_PRIMING_GUARD')==1
    assert first.count('m_State = m_nOutstanding ? StateCanceled : StateIdle;')==2
    assert first.count('nDrainStart < 2000000')==2
print('Circle audio priming/failure patch is pinned and idempotent; physical transfers unverified')

# Partial HID reports must remain pending just like failed USB transfers.
original_hid=subprocess.check_output(['git','-C',str(circle),'show','HEAD:lib/usb/usbhiddevice.cpp'])
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/usb/usbhiddevice.cpp'
    target.parent.mkdir(parents=True);target.write_bytes(original_hid)
    cmd=['python3',str(root/'scripts/patch-circle-hid.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_HID_OUT_EXACT_LENGTH')==1
    start=first.index('boolean CUSBHIDDevice::SendToEndpointOut (')
    end=first.index('boolean CUSBHIDDevice::SendToEndpointOutAsync (',start)
    assert '== (int) nBufSize' in first[start:end]
print('Circle HID short-write guard is pinned and idempotent; physical transfers unverified')

# The MK3 subclass must be notified even when plug-and-play is enabled and
# Circle otherwise suppresses failed interrupt-IN callbacks.
original_hid_header=subprocess.check_output(['git','-C',str(circle),'show','HEAD:include/circle/usb/usbhiddevice.h'])
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/usb/usbhiddevice.cpp';header=path/'include/circle/usb/usbhiddevice.h'
    target.parent.mkdir(parents=True);header.parent.mkdir(parents=True);target.write_bytes(original_hid);header.write_bytes(original_hid_header)
    cmd=['python3',str(root/'scripts/patch-circle-hid-error.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();first_h=header.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first_h==header.read_text()
    assert first.count('GROOVE_HID_INPUT_ERROR')==1 and first_h.count('GROOVE_HID_INPUT_ERROR')==1
    start=first.index('void CUSBHIDDevice::CompletionRoutine (');end=first.index('void CUSBHIDDevice::CompletionStub (',start)
    body=first[start:end];assert body.index('ReportTransferError (pURB->GetUSBError ())')<body.index('if (!GetHost ()->IsPlugAndPlay ())')
print('Circle HID error notification covers plug-and-play and non-plug-and-play; physical transfers unverified')

# Stopped polling is reported and rejected Pi 4 requests are returned to the
# allocator. The subclass retry state must not overwrite a concurrent stop.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/usb/usbhiddevice.cpp';header=path/'include/circle/usb/usbhiddevice.h'
    target.parent.mkdir(parents=True);header.parent.mkdir(parents=True);target.write_bytes(original_hid);header.write_bytes(original_hid_header)
    subprocess.run(['python3',str(root/'scripts/patch-circle-hid-error.py'),str(path)],check=True)
    cmd=['python3',str(root/'scripts/patch-circle-hid-restart.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();first_h=header.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first_h==header.read_text() and first.count('GROOVE_HID_POLL_STOPPED')==2
    start=first.index('boolean CUSBHIDDevice::StartRequest (');end=first.index('void CUSBHIDDevice::CompletionRoutine (',start)
    method=first[start:end];end2=first.index('void CUSBHIDDevice::CompletionStub (',end);completion=first[end:end2]
    assert completion.count('ReportPollingStopped ();')==2
    test=path/'hid-start.cpp';test.write_text('''#include <cassert>
#define RASPPI 4
using boolean=bool;
struct Endpoint{};struct CUSBRequest {static int live;CUSBRequest(Endpoint*,void*,unsigned){++live;}~CUSBRequest(){--live;}void SetCompletionRoutine(void(*)(CUSBRequest*,void*,void*),void*,void*){}};int CUSBRequest::live=0;
struct Host {bool ok=false;CUSBRequest* pending=nullptr;bool SubmitAsyncRequest(CUSBRequest* p){if(ok)pending=p;return ok;}};
struct CUSBHIDDevice {Endpoint ep,*m_pReportEndpoint=&ep;char buffer[64],*m_pReportBuffer=buffer;unsigned m_nMaxReportSize=64;Host host;Host* GetHost(){return &host;}static void CompletionStub(CUSBRequest*,void*,void*){}boolean StartRequest();};
'''+method+'''int main(){CUSBHIDDevice d;for(int i=0;i<100;++i){assert(!d.StartRequest());assert(CUSBRequest::live==0);}d.host.ok=true;assert(d.StartRequest());assert(CUSBRequest::live==1);delete d.host.pending;assert(CUSBRequest::live==0);}
''')
    exe=path/'hid-start';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
    firmware=(root/'firmware/mk3_usb.cpp').read_text();start=firmware.index('bool CMk3Hid::RestartInput(u32& resync)');end=firmware.index('\nboolean CMk3Hid::Configure()',start)
    test=path/'hid-retry.cpp';test.write_text('''#include <cassert>
using u32=unsigned;
class CMk3Hid {public:unsigned m_PollState=1,m_InputRecoveryFailures=0,m_InputFailureProgress=0;struct Events{unsigned generation=0;unsigned request_resync(){return ++generation;}}m_Events;bool ok=false,endpoint_ok=true,stop_during_start=false;unsigned InputProgress()const{return 0;}bool RecoverInputEndpoint(){return endpoint_ok;}bool StartRequest(){if(stop_during_start)__atomic_store_n(&m_PollState,1,__ATOMIC_RELEASE);return ok;}bool RestartInput(u32&);};
'''+firmware[start:end]+'''\nint main(){CMk3Hid d;u32 resync=0;assert(!d.RestartInput(resync) && d.m_PollState==1 && resync==1);d.ok=true;assert(d.RestartInput(resync) && d.m_PollState==0 && resync==2);d.m_PollState=1;d.stop_during_start=true;assert(!d.RestartInput(resync) && d.m_PollState==1 && resync==3);d.endpoint_ok=false;assert(!d.RestartInput(resync) && d.m_PollState==1 && resync==3);assert(d.m_InputRecoveryFailures==4);}
''')
    exe=path/'hid-retry';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle Pi 4 HID stopped-poll retry and request ownership host tested; physical behavior unverified')

# Compile the actual inserted predicate, including discrete and stepped ranges.
original_stream=subprocess.check_output(['git','-C',str(circle),'show','HEAD:lib/usb/usbaudiostreaming.cpp'])
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/usb/usbaudiostreaming.cpp'
    target.parent.mkdir(parents=True);target.write_bytes(original_stream)
    cmd=['python3',str(root/'scripts/patch-circle-rate.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_AUDIO_RATE_RANGE')==1
    start=first.index('static boolean GrooveAudioRateSupported (')
    end=first.index('boolean CUSBAudioStreamingDevice::Setup (',start)
    helper=first[start:end]
    test=path/'rate-test.cpp'
    test.write_text('''#include <cassert>
using boolean=bool;
'''+helper+'''int main() {
 assert(GrooveAudioRateSupported(48000,48000,48000,0));
 assert(!GrooveAudioRateSupported(44100,48000,48000,0));
 assert(GrooveAudioRateSupported(48000,44100,96000,0));
 assert(GrooveAudioRateSupported(48000,32000,96000,8000));
 assert(!GrooveAudioRateSupported(44100,32000,96000,8000));
 assert(!GrooveAudioRateSupported(16000,32000,96000,8000));
 assert(!GrooveAudioRateSupported(96001,32000,96000,0));
 assert(!GrooveAudioRateSupported(48000,96000,32000,0));
}
''')
    executable=path/'rate-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print('Circle USB audio advertised-rate matching tested; physical formats unverified')

# Device-supplied counts must not determine an unbounded stack allocation.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/usb/usbaudiostreaming.cpp'
    target.parent.mkdir(parents=True);target.write_bytes(original_stream)
    cmd=['python3',str(root/'scripts/patch-circle-range-data.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_AUDIO_RANGE_RESPONSE')==1
    start=first.index('// GROOVE_AUDIO_RANGE_RESPONSE')
    end=first.index('uchTerminalLink = pGeneralDesc->Ver200.bTerminalLink;',start)
    block=first[start:end]
    assert block.index('nSampleRates = MaxSampleRatesRanges') < block.index('DMA_BUFFER (u8, RangesBuffer')
    assert 'RangesBuffer, nBufferSize) != (int) nBufferSize' in block
    assert 'nReported < nSampleRates' in block and '(u32 *) (RangesBuffer+2)' not in block
    begin=first.index('static u32 GrooveAudioReadLE32 (')
    finish=first.index('CUSBAudioStreamingDevice::CUSBAudioStreamingDevice (',begin)
    test=path/'le-test.cpp'
    test.write_text('''#include <cassert>
#include <cstdint>
using u8=uint8_t; using u32=uint32_t;
'''+first[begin:finish]+'''int main() {
 const u8 bytes[]={0xff,0x80,0xbb,0x00,0x00,0x00};
 assert(GrooveAudioReadLE32(bytes+1)==48000);
 assert(GrooveAudioReadLE32(bytes)==0x00bb80ffu);
}
''')
    executable=path/'le-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print('Circle UAC2 range responses are bounded and unaligned decoding tested')

# USB MIDI host interfaces preserve their exact physical parent, even when
# string descriptors are absent or another identical controller is present.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=['include/circle/usb/usbmidi.h','lib/usb/usbmidi.cpp','lib/usb/usbmidihost.cpp']
    for name in names:
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    cmd=['python3',str(root/'scripts/patch-circle-midi.py'),str(path)]
    subprocess.run(cmd,check=True);first=[(path/name).read_text() for name in names]
    subprocess.run(cmd,check=True);assert first==[(path/name).read_text() for name in names]
    assert 'GetUSBDevice (void) const { return m_pUSBDevice; }' in first[0]
    assert 'm_pUSBDevice (pUSBDevice)' in first[1]
    assert 'new CUSBMIDIDevice (this, GetDevice ())' in first[2]
    assert '== (int) nLength' in first[2] and first[2].count('GROOVE_MIDI_EXACT_LENGTH')==1
print('Circle MIDI parent identity and short-write guard are pinned and idempotent')

# Exercise the patched callbacks themselves with deterministic transfer results.
with tempfile.TemporaryDirectory() as temp:
    path = pathlib.Path(temp)
    files = ['lib/usb/usbaudiostreaming.cpp', 'lib/sound/usbsoundbasedevice.cpp',
             'include/circle/sound/usbsoundbasedevice.h']
    for name in files:
        dest = path/name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    subprocess.run(['python3',str(root/'scripts/patch-circle-audio.py'),str(path)],check=True)
    cmd = ['python3',str(root/'scripts/patch-circle-audio-completion.py'),str(path)]
    subprocess.run(cmd,check=True)
    patched = [(path/name).read_text() for name in files]
    subprocess.run(cmd,check=True)
    assert patched == [(path/name).read_text() for name in files]
    source = patched[1]
    tx = source[source.index('void CUSBSoundBaseDevice::TXCompletionRoutine'):source.index('void CUSBSoundBaseDevice::TXCompletionStub')]
    rx = source[source.index('void CUSBSoundBaseDevice::RXCompletionRoutine'):source.index('void CUSBSoundBaseDevice::RXCompletionStub')]
    test = path/'completion-test.cpp'
    test.write_text('''#include <cassert>
#include <cstdint>
using boolean = bool; using s16 = int16_t; using u32 = uint32_t;
#define TRUE true
#define FALSE false
#define RASPPI 4
#define LOGWARN(...) ((void)0)
int AtomicDecrement(int* value) { return --*value; }
struct Lock { void Acquire() {} void Release() {} };
class CUSBSoundBaseDevice {
public:
 enum { StateCreated, StateIdle, StateRunning, StateCanceled } m_State = StateRunning;
 int m_nOutstanding=1; Lock m_SpinLock;
 unsigned m_nTXExpected[2]={24, 24}, m_nTXCompletedBuffer=0;
 unsigned m_nSubframeSize=2, m_nRXChunkSizeBytes=24, m_nRXCurrentBuffer=0;
 s16 data[48]={}; void* m_pRXBuffer[2]={data,data};
 bool send_ok=true, receive_ok=true; int sends=0, receives=0, puts=0;
 bool SendChunk() { ++sends; if(send_ok) ++m_nOutstanding; return send_ok; }
 bool ReceiveChunk() { ++receives; if(receive_ok) ++m_nOutstanding; return receive_ok; }
 void PutChunk(const s16*, unsigned) { ++puts; }
 void PutChunk(const u32*, unsigned) { ++puts; }
 void TXCompletionRoutine(unsigned); void RXCompletionRoutine(unsigned);
};
'''+tx+rx+'''int main() {
 CUSBSoundBaseDevice tx; tx.TXCompletionRoutine(24); assert(tx.sends==1 && tx.m_State==tx.StateRunning);
 CUSBSoundBaseDevice short_tx; short_tx.TXCompletionRoutine(12); assert(short_tx.sends==0 && short_tx.m_State==short_tx.StateIdle);
 CUSBSoundBaseDevice failed_tx; failed_tx.m_nOutstanding=2; failed_tx.TXCompletionRoutine(~0u); assert(failed_tx.m_State==failed_tx.StateCanceled); failed_tx.RXCompletionRoutine(~0u); assert(failed_tx.m_State==failed_tx.StateIdle);
 CUSBSoundBaseDevice empty_rx; empty_rx.RXCompletionRoutine(0); assert(empty_rx.receives==1 && empty_rx.puts==0 && empty_rx.m_State==empty_rx.StateRunning);
 CUSBSoundBaseDevice bad_rx; bad_rx.RXCompletionRoutine(~0u); assert(bad_rx.receives==0 && bad_rx.m_State==bad_rx.StateIdle);
 CUSBSoundBaseDevice malformed_rx; malformed_rx.RXCompletionRoutine(49); assert(malformed_rx.receives==0 && malformed_rx.m_State==malformed_rx.StateIdle);
 CUSBSoundBaseDevice submission; submission.send_ok=false; submission.TXCompletionRoutine(24); assert(submission.m_State==submission.StateIdle);
}
''')
    exe = path/'completion-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle audio completion errors, short output and empty capture handled; real transfers unverified')

# UAC2 implicit feedback usage is valid for capture, never for output or UAC1.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); target=path/'lib/usb/usbaudiostreaming.cpp'
    target.parent.mkdir(parents=True); target.write_bytes(original_stream)
    cmd=['python3',str(root/'scripts/patch-circle-audio-input.py'),str(path)]
    subprocess.run(cmd,check=True); first=target.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_AUDIO_IMPLICIT_CAPTURE')==1
    start=first.index('\tif (   !pEndpointDesc',first.index('GROOVE_AUDIO_IMPLICIT_CAPTURE'))
    end=first.index('\n\t{',start)
    condition=first[start:end].replace('if (', 'return !(', 1)+';'
    test=path/'capture-test.cpp'
    test.write_text('''#include <cassert>
struct Endpoint { unsigned bmAttributes, bEndpointAddress; };
bool accepted(bool m_bVer200, const Endpoint* pEndpointDesc) {
'''+condition+'''
}
int main() {
 Endpoint normal{0x05,0x81}, implicit{0x25,0x81}, out{0x25,0x01}, feedback{0x11,0x81};
 assert(accepted(true,&normal)); assert(accepted(true,&implicit));
 assert(!accepted(false,&implicit)); assert(!accepted(true,&out));
 assert(!accepted(true,&feedback)); assert(!accepted(true,nullptr));
}
''')
    exe=path/'capture-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle UAC2 implicit capture descriptor recognized without pairing outputs')

# Device-provided sizes must not overflow buffers or split audio frames.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); target=path/'lib/sound/usbsoundbasedevice.cpp'
    target.parent.mkdir(parents=True); target.write_bytes(original_audio)
    stream=path/'lib/usb/usbaudiostreaming.cpp'; stream.parent.mkdir(parents=True); stream.write_bytes(original_stream)
    cmd=['python3',str(root/'scripts/patch-circle-audio-chunk.py'),str(path)]
    subprocess.run(cmd,check=True); first=target.read_text(); stream_first=stream.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_AUDIO_CHUNK_BOUNDS')==1
    assert stream_first==stream.read_text() and stream_first.count('GROOVE_AUDIO_CAPTURE_FRAME')==1
    assert stream_first.count('GROOVE_AUDIO_ZERO_FEEDBACK')==1
    start=first.index('if (!nFrameSize || !nChunkSizeBytes',first.index('GROOVE_AUDIO_CHUNK_BOUNDS'))
    end=first.index('\n\t{',start)
    predicate=first[start:end].replace('if (','return !(',1)+';'
    test=path/'chunk-test.cpp'
    test.write_text('''#include <cassert>
bool valid(unsigned nFrameSize, unsigned nChunkSizeBytes, unsigned m_nTXChunkSizeBytes) {
'''+predicate+'''
}
int main() {
 assert(valid(12,72,72)); assert(valid(12,144,72));
 assert(!valid(0,72,72)); assert(!valid(12,0,72));
 assert(!valid(12,73,72)); assert(!valid(12,156,72));
}
''')
    exe=path/'chunk-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle audio feedback chunk bounds checked before DSP buffer access')

# Exercise the exact inserted buffer sizing using the connected MK3's
# 48-kHz nominal chunk and advertised endpoint maximum.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); target=path/'lib/sound/usbsoundbasedevice.cpp'
    target.parent.mkdir(parents=True); target.write_bytes(original_audio)
    cmd=['python3',str(root/'scripts/patch-circle-implicit-capacity.py'),str(path)]
    subprocess.run(cmd,check=True); first=target.read_text(); subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_IMPLICIT_TX_CAPACITY')==1
    start=first.index('if (m_bImplicitFeedback)',first.index('GROOVE_IMPLICIT_TX_CAPACITY'))
    end=first.index('\n\t\t\t}',start)+len('\n\t\t\t}')
    test=path/'capacity-test.cpp'
    test.write_text('''#include <cassert>
struct Stream { unsigned max; unsigned GetDataMaxPacketSize()const {return max;} };
unsigned capacity(unsigned nominal,unsigned max,bool implicit){
 unsigned m_nTXChunkSizeBytes=nominal; Stream stream{max}; Stream* m_pTXUSBDevice=&stream;bool m_bImplicitFeedback=implicit;
'''+first[start:end]+'''
 return m_nTXChunkSizeBytes*2;
}
int main(){assert(capacity(96,208,true)==208);assert(capacity(96,209,true)==210);
 assert(capacity(96,208,false)==192);assert(capacity(128,208,true)==256);}
''')
    exe=path/'capacity-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle implicit output capacity includes observed MK3 maximum; streaming unverified')

# Exercise the actual patched streaming methods with a rejecting host. Only
# single-packet implicit Pi 4 transfers have proven rejected-request ownership.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);target=path/'lib/usb/usbaudiostreaming.cpp'
    target.parent.mkdir(parents=True);target.write_bytes(original_stream)
    cmd=['python3',str(root/'scripts/patch-circle-implicit-submit.py'),str(path)]
    subprocess.run(cmd,check=True);first=target.read_text();subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_IMPLICIT_SUBMIT_OWNERSHIP')==2
    start=first.index('boolean CUSBAudioStreamingDevice::SendChunk (')
    end=first.index('boolean CUSBAudioStreamingDevice::SelectInputTerminal (',start)
    methods=first[start:end].replace('if (UseSynchronousChunks ())','if (m_bSynchronousSync && !m_bSinglePacketImplicit)').replace('&& UseSynchronousChunks ())','&& (m_bSynchronousSync && !m_bSinglePacketImplicit))')
    test=path/'submit-test.cpp';test.write_text('''#include <cassert>
using boolean=bool;using u16=unsigned short;
#define TRUE true
#define FALSE false
#define RASPPI 4
#define USBSpeedFull 1
struct Endpoint {};
struct CUSBRequest { static int live;CUSBRequest(Endpoint*,void*,unsigned){++live;}~CUSBRequest(){--live;}
 void AddIsoPacket(unsigned){};void SetCompletionRoutine(void(*)(CUSBRequest*,void*,void*),void*,void*){} };
int CUSBRequest::live=0;
struct Host {bool accept=false;CUSBRequest* pending=nullptr;bool SubmitAsyncRequest(CUSBRequest* r){if(accept)pending=r;return accept;}};
struct Device {unsigned GetSpeed(){return 2;}};
class CUSBAudioStreamingDevice { public:
 using TCompletionRoutine=void(unsigned,void*);Endpoint ep,*m_pEndpointData=&ep,*m_pEndpointSync=nullptr;Host host;Device dev;
 bool m_bSynchronousSync=false,m_bDeferredImplicit=true,m_bSinglePacketImplicit=true,m_bSyncEPActive=false;
 unsigned m_nPacketsPerChunk=1,m_usPacketSizeBytes[1]={16},m_SyncEPBuffer[1]={};
 Host* GetHost(){return &host;}Device* GetDevice(){return &dev;}void UpdateChunkSize(){}
 static void CompletionHandler(CUSBRequest*,void*,void*){}static void SyncCompletionHandler(CUSBRequest*,void*,void*){}
 boolean SendChunk(const void*,unsigned,TCompletionRoutine*,void*);boolean ReceiveChunk(void*,unsigned,TCompletionRoutine*,void*);
};
'''+methods+'''int main(){char data[16]{};CUSBAudioStreamingDevice d;
 for(int i=0;i<100;++i){assert(!d.SendChunk(data,16,nullptr,nullptr));assert(!d.ReceiveChunk(data,16,nullptr,nullptr));assert(CUSBRequest::live==0);}
 d.host.accept=true;assert(d.SendChunk(data,16,nullptr,nullptr));assert(CUSBRequest::live==1);delete d.host.pending;d.host.pending=nullptr;
 assert(d.ReceiveChunk(data,16,nullptr,nullptr));assert(CUSBRequest::live==1);delete d.host.pending;assert(CUSBRequest::live==0);
}
''')
    exe=path/'submit-test';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle Pi 4 implicit request rejection releases ownership; physical disconnect unverified')

# Request completion is not the end of the callback: exercise the actual
# patched Circle state transitions with cancellation during render/submission.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=['lib/usb/usbaudiostreaming.cpp','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h']
    for name in names:
        target=path/name; target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for script in ('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-lifetime.py'):
        subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    target=path/names[1]; header=path/names[2]; first=target.read_text(); hfirst=header.read_text()
    subprocess.run(['python3',str(root/'scripts/patch-circle-audio-lifetime.py'),str(path)],check=True)
    assert first==target.read_text() and hfirst==header.read_text()
    cancel=first[first.index('void CUSBSoundBaseDevice::Cancel'):first.index('CSoundController *CUSBSoundBaseDevice::GetController')]
    finished=first[first.index('void CUSBSoundBaseDevice::CallbackFinished'):first.index('void CUSBSoundBaseDevice::TXCompletionRoutine')]
    tx=first[first.index('void CUSBSoundBaseDevice::TXCompletionRoutine'):first.index('void CUSBSoundBaseDevice::TXCompletionStub')]
    rx=first[first.index('void CUSBSoundBaseDevice::RXCompletionRoutine'):first.index('void CUSBSoundBaseDevice::RXCompletionStub')]
    test=path/'lifetime-test.cpp'
    test.write_text('''#include <cassert>
#include <cstdint>
using boolean=bool; using s16=int16_t; using u32=uint32_t;
#define TRUE true
#define FALSE false
#define RASPPI 4
#define LOGWARN(...) ((void)0)
int AtomicDecrement(int* value) { return --*value; }
struct Lock { void Acquire() {} void Release() {} };
class CUSBSoundBaseDevice {
public:
 enum { StateCreated, StateIdle, StateRunning, StateCanceled } m_State=StateRunning;
 int m_nOutstanding=1; mutable Lock m_SpinLock; unsigned m_nCallbacksActive=0;
 unsigned m_nTXExpected[2]={24,24},m_nTXCompletedBuffer=0;
 unsigned m_nSubframeSize=2,m_nRXChunkSizeBytes=24,m_nRXCurrentBuffer=0;
 s16 data[48]={}; void* m_pRXBuffer[2]={data,data};
 bool send_ok=true,receive_ok=true,cancel_during_send=false,active_during_send=false;
 bool SendChunk() { active_during_send=IsActive(); if(cancel_during_send) Cancel(); if(send_ok) ++m_nOutstanding; return send_ok; }
 bool ReceiveChunk() { if(receive_ok) ++m_nOutstanding; return receive_ok; }
 void PutChunk(const s16*,unsigned) {} void PutChunk(const u32*,unsigned) {}
 void Cancel(); boolean IsActive() const; void CallbackFinished();
 void TXCompletionRoutine(unsigned); void RXCompletionRoutine(unsigned);
};
'''+cancel+finished+tx+rx+'''int main() {
 CUSBSoundBaseDevice a; a.cancel_during_send=true; a.send_ok=false;
 a.TXCompletionRoutine(24); assert(a.active_during_send); assert(!a.IsActive());
 CUSBSoundBaseDevice b; b.cancel_during_send=true; b.TXCompletionRoutine(24);
 assert(b.IsActive() && b.m_State==b.StateCanceled && b.m_nOutstanding==1);
 b.TXCompletionRoutine(24); assert(!b.IsActive());
 CUSBSoundBaseDevice c; c.m_nCallbacksActive=1; c.TXCompletionRoutine(~0u);
 assert(c.IsActive() && c.m_State==c.StateCanceled && c.m_nCallbacksActive==1);
 c.CallbackFinished(); assert(!c.IsActive());
 CUSBSoundBaseDevice d; d.m_nCallbacksActive=1; d.m_State=d.StateCanceled;
 d.RXCompletionRoutine(0); assert(d.IsActive() && d.m_State==d.StateCanceled);
 d.CallbackFinished(); assert(!d.IsActive());
}
''')
    exe=path/'lifetime-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle USB audio callbacks stay alive through cancellation and request draining')

# Cancellation during GetChunk must never submit to a removed endpoint; a
# timeout must retain buffers while requests or callbacks are still active.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=['lib/usb/usbaudiostreaming.cpp','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h']
    for name in names:
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for script in ('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py'):
        subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    target=path/names[1];first=target.read_text();header=(path/names[2]).read_text()
    subprocess.run(['python3',str(root/'scripts/patch-circle-audio-disconnect.py'),str(path)],check=True)
    assert first==target.read_text() and header==(path/names[2]).read_text()
    removed=first[first.index('void CUSBSoundBaseDevice::DeviceRemovedHandler'):first.index('void CUSBSoundBaseDevice::DeviceRemovedStub')]
    timeout=removed[removed.index('if (CTimer::Get ()->GetTicks () - nStartTicks'):removed.index('#ifdef NO_BUSY_WAIT')]
    assert 'return;' in timeout and 'StateIdle;' not in timeout and 'delete' not in timeout.replace('can delete it','')
    send=first[first.index('boolean CUSBSoundBaseDevice::SendChunk'):first.index('void CUSBSoundBaseDevice::CallbackFinished')]
    test=path/'disconnect-test.cpp'
    test.write_text('''#include <cassert>
#include <cstdint>
using boolean=bool; using s16=int16_t; using u8=uint8_t; using u32=uint32_t;
#define TRUE true
#define FALSE false
#define LOGWARN(...) ((void)0)
int AtomicIncrement(int* p) { return ++*p; } int AtomicDecrement(int* p) { return --*p; }
struct Lock { int depth=0; void Acquire(){assert(depth==0);++depth;} void Release(){assert(depth==1);--depth;} };
struct Stream { unsigned requests=0; unsigned GetChunkSizeBytes(){return 24;} bool SendChunk(void*,unsigned,void(*)(unsigned,void*),void*){++requests;return true;} bool ReceiveChunk(void*,unsigned,void(*)(unsigned,void*),void*){++requests;return true;} };
class CUSBSoundBaseDevice {
public:
 enum { StateRunning, StateCanceled } m_State=StateRunning; Lock m_SpinLock;
 Stream stream; Stream *m_pTXUSBDevice=&stream, *m_pRXUSBDevice=&stream;
 unsigned m_nTXChunkSizeBytes=24,m_nRXChunkSizeBytes=24,m_nSubframeSize=2;
 unsigned m_nTXCurrentBuffer=0,m_nRXCurrentBuffer=0,m_nTXExpected[2]={};
 s16 data[48]={}; u8* m_pTXBuffer[2]={(u8*)data,(u8*)data}; u8* m_pRXBuffer[2]={(u8*)data,(u8*)data};
 int m_nOutstanding=0; bool cancel_inside_render=false;
 unsigned GetHWTXChannels(){return 2;} unsigned GetHWRXChannels(){return 2;}
 unsigned GetChunk(s16*,unsigned n) { assert(!m_SpinLock.depth); if(cancel_inside_render)m_State=StateCanceled; return n; }
 unsigned GetChunk(u32*,unsigned n) { return n; }
 static void TXCompletionStub(unsigned,void*){} static void RXCompletionStub(unsigned,void*){}
 bool SendChunk(boolean bAlreadyLocked=false); bool ReceiveChunk(boolean bAlreadyLocked=false);
};
'''+send+'''int main() {
 CUSBSoundBaseDevice normal; assert(normal.SendChunk()); assert(normal.stream.requests==1);
 CUSBSoundBaseDevice stopped; stopped.cancel_inside_render=true; assert(!stopped.SendChunk());
 assert(stopped.stream.requests==0 && stopped.m_nOutstanding==0 && stopped.m_SpinLock.depth==0);
 CUSBSoundBaseDevice removed; removed.m_State=removed.StateCanceled;
 assert(!removed.SendChunk() && !removed.ReceiveChunk() && removed.stream.requests==0);
 CUSBSoundBaseDevice input; assert(input.ReceiveChunk()); assert(input.stream.requests==1 && input.m_nOutstanding==1);
}
''')
    exe=path/'disconnect-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle audio cancellation blocks new submissions and retains buffers on removal timeout')

# A draining (still alive) device must not be presented as playing.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=['lib/usb/usbaudiostreaming.cpp','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for script in ('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-lifetime.py','patch-circle-audio-status.py'):
        subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    target=path/names[1];first=target.read_text();header=(path/names[2]).read_text()
    subprocess.run(['python3',str(root/'scripts/patch-circle-audio-status.py'),str(path)],check=True)
    assert first==target.read_text() and header==(path/names[2]).read_text()
    code=first[first.index('boolean CUSBSoundBaseDevice::IsRunning'):first.index('CSoundController *CUSBSoundBaseDevice::GetController')]
    test=path/'running-test.cpp';test.write_text('''#include <cassert>
using boolean=bool;
struct Lock{ void Acquire(){} void Release(){} };
class CUSBSoundBaseDevice { public: enum {StateIdle,StateRunning,StateCanceled} m_State=StateIdle; mutable Lock m_SpinLock; boolean IsRunning() const; };
'''+code+'''int main(){CUSBSoundBaseDevice d; assert(!d.IsRunning()); d.m_State=d.StateRunning; assert(d.IsRunning()); d.m_State=d.StateCanceled; assert(!d.IsRunning());}
''')
    exe=path/'running-test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle audio running status excludes canceled/draining state')

# Exercise the actual patched capture/tx/drain callbacks against a synthetic
# one-packet UAC2 stream; these fixture timings are not MK3 captures.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); names=['lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h','lib/usb/usbrequest.cpp','lib/usb/xhciendpoint.cpp']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-implicit-zero.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    source=path/names[2];first=source.read_text();header=(path/names[3]).read_text();stream=(path/names[0]).read_text()
    for script in ('patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-implicit-zero.py'):subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    assert first==source.read_text() and header==(path/names[3]).read_text() and stream==(path/names[0]).read_text()
    send=first[first.index('boolean CUSBSoundBaseDevice::SendChunk'):first.index('void CUSBSoundBaseDevice::DrainImplicit')]
    drain=first[first.index('void CUSBSoundBaseDevice::DrainImplicit'):first.index('void CUSBSoundBaseDevice::TXCompletionStub')]
    # Remove the TX stub from the extraction; its test double is defined below.
    rx=first[first.index('void CUSBSoundBaseDevice::RXCompletionRoutine'):first.index('void CUSBSoundBaseDevice::RXCompletionStub')]
    test=path/'implicit-integration.cpp'
    test.write_text('''#include <cassert>
#include <cstdint>
#include "usb_implicit_cadence.h"
using boolean=bool; using s16=int16_t; using u8=uint8_t; using u32=uint32_t;
#define TRUE true
#define FALSE false
#define RASPPI 4
#define LOGWARN(...) ((void)0)
int AtomicIncrement(int* p){return ++*p;} int AtomicDecrement(int* p){return --*p;}
struct Lock { int depth=0; void Acquire(){assert(!depth);++depth;} void Release(){assert(depth==1);--depth;} };
struct Stream {
 unsigned requests=0, sizes[32]={};
 unsigned GetChunkSizeBytes(){return 24;} unsigned GetDataMaxPacketSize(){return 96;}
 bool SendChunk(void*,unsigned n,void(*)(unsigned,void*),void*){sizes[requests++]=n;return true;}
 bool ReceiveChunk(void*,unsigned n,void(*)(unsigned,void*),void*){sizes[requests++]=n;return true;}
};
class CUSBSoundBaseDevice {
public:
 enum { StateCreated,StateIdle,StateRunning,StateCanceled } m_State=StateRunning;
 Lock m_SpinLock; Stream tx,rx; Stream *m_pTXUSBDevice=&tx,*m_pRXUSBDevice=&rx;
 bool m_bImplicitFeedback=true,m_bImplicitDraining=false; groove::UsbImplicitCadence m_ImplicitCadence;
 unsigned m_nTXChunkSizeBytes=24,m_nRXChunkSizeBytes=24,m_nSubframeSize=2;
 unsigned m_nTXCurrentBuffer=0,m_nRXCurrentBuffer=0,m_nTXExpected[2]={},m_nTXCompletedBuffer=0,m_nCallbacksActive=0;
 s16 data[96]={};u8* m_pTXBuffer[2]={(u8*)data,(u8*)data};u8* m_pRXBuffer[2]={(u8*)data,(u8*)data};
 int m_nOutstanding=2; unsigned rendered=0,consumed=0;
 CUSBSoundBaseDevice(){m_ImplicitCadence.reset(2);}
 unsigned GetHWTXChannels(){return 4;} unsigned GetHWRXChannels(){return 2;}
 unsigned GetChunk(s16*,unsigned n){assert(!m_SpinLock.depth); rendered+=n/4;return n;}
 unsigned GetChunk(u32*,unsigned n){return n;}
 void PutChunk(const s16*,unsigned n){consumed+=n/2;} void PutChunk(const u32*,unsigned){}
 static void TXCompletionStub(unsigned,void*){} static void RXCompletionStub(unsigned,void*){}
 bool SendChunk(bool bAlreadyLocked=false,unsigned nFrames=~0u); bool ReceiveChunk(bool bAlreadyLocked=false);
 void DrainImplicit();void CallbackFinished();void TXCompletionRoutine(unsigned);void RXCompletionRoutine(unsigned);
};
'''+send+drain+rx+'''int main(){
 CUSBSoundBaseDevice d; assert(d.tx.requests==0);
 d.RXCompletionRoutine(12); assert(d.tx.requests==1 && d.tx.sizes[0]==24 && d.rendered==3);
 d.RXCompletionRoutine(16); assert(d.tx.requests==2 && d.tx.sizes[1]==32 && d.rendered==7);
 d.RXCompletionRoutine(20); assert(d.tx.requests==2 && d.m_ImplicitCadence.queued()==1);
 d.TXCompletionRoutine(24); assert(d.tx.requests==3 && d.tx.sizes[2]==40 && d.rendered==12);
 assert(d.m_ImplicitCadence.queued()==0 && d.consumed==12);
 CUSBSoundBaseDevice early; early.RXCompletionRoutine(12); early.TXCompletionRoutine(24);
 assert(early.tx.requests==1 && early.m_ImplicitCadence.ready()==2);
 early.RXCompletionRoutine(16); assert(early.tx.requests==2 && early.tx.sizes[1]==32);
 CUSBSoundBaseDevice empty; empty.RXCompletionRoutine(0); assert(empty.tx.requests==1 && empty.tx.sizes[0]==0 && empty.rendered==0 && empty.m_State==empty.StateRunning); empty.TXCompletionRoutine(0); assert(empty.m_State==empty.StateRunning); empty.RXCompletionRoutine(12); assert(empty.tx.requests==2 && empty.tx.sizes[1]==24 && empty.rendered==3);
 CUSBSoundBaseDevice huge; huge.RXCompletionRoutine(48); assert(huge.tx.requests==0 && huge.m_State==huge.StateCanceled);
}
''')
    exe=path/'implicit-integration'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Circle implicit scheduler integration: variable packet cadence, bounded queue, no priming or invented feedback')

# Test the actual explicit-profile predicate, rather than inferring a pair from
# VID/PID or the existence of an input endpoint.
fixture=(root/'tests/fixtures/mk3-actual-config-20261030.bin').read_bytes()
assert len(fixture)>=9 and fixture[:2]==b'\x09\x02' and int.from_bytes(fixture[2:4],'little')==len(fixture)
clock_for_terminal={};streams={};interface=None;pos=0
while pos<len(fixture):
    length=fixture[pos]
    assert length>=2 and pos+length<=len(fixture)
    desc=fixture[pos:pos+length];pos+=length
    if desc[1]==4:
        assert length>=9
        interface=(desc[2],desc[3],desc[5],desc[6])
    elif interface and interface[2:]==(1,1) and desc[1]==0x24 and length>=9:
        if desc[2]==2: clock_for_terminal[desc[3]]=desc[7]
        elif desc[2]==3: clock_for_terminal[desc[3]]=desc[8]
    elif interface and interface[2:]==(1,2) and interface[1]==1:
        stream=streams.setdefault(interface[0],{})
        if desc[1]==0x24 and desc[2]==1 and length>=16:
            stream.update(terminal=desc[3],channels=desc[10],formats=int.from_bytes(desc[6:10],'little'))
        elif desc[1]==0x24 and desc[2]==2 and length>=6:
            stream.update(subslot=desc[4],bits=desc[5])
        elif desc[1]==5 and length>=7:
            stream.update(address=desc[2],attributes=desc[3],interval=desc[6],packet=int.from_bytes(desc[4:6],'little'))
tx_actual,rx_actual=streams[1],streams[2]
assert tx_actual==dict(terminal=20,channels=4,formats=1,subslot=4,bits=24,address=1,attributes=5,interval=1,packet=208)
assert rx_actual==dict(terminal=21,channels=2,formats=1,subslot=4,bits=24,address=0x81,attributes=0x25,interval=1,packet=104)
assert clock_for_terminal[tx_actual['terminal']]==clock_for_terminal[rx_actual['terminal']]==2
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    for name in ('lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h'):
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    subprocess.run(['python3',str(root/'scripts/patch-circle-implicit-stream.py'),str(path)],check=True)
    source=(path/'lib/usb/usbaudiostreaming.cpp').read_text()
    code=source[source.index('boolean CUSBAudioStreamingDevice::EnableImplicitFeedback'):source.index('boolean CUSBAudioStreamingDevice::Setup')]
    test=path/'pair-test.cpp';test.write_text('''#include <cassert>
using boolean=bool;
#define TRUE true
#define FALSE false
#define USB_AUDIO_UNDEFINED_UNIT_ID 0
struct Endpoint { unsigned number; unsigned GetNumber(){return number;} };
class CUSBAudioStreamingDevice { public:
 bool m_bDeferredImplicit=false,m_bVer200=true,m_bIsOutput=false,m_bImplicitInput=false,m_bImplicitEnabled=false,m_bSynchronousSync=false;
 unsigned m_uchClockSourceID=5,m_nDataIntervalFactor=1,m_nSubframeSize=3,iface=0; void* device=(void*)1; Endpoint ep{1}; Endpoint* m_pEndpointData=&ep;
 void* GetDevice(){return device;} unsigned GetInterfaceNumber(){return iface;}
 bool EnableImplicitFeedback(CUSBAudioStreamingDevice*,unsigned,unsigned);
};
'''+code+'''int main(){
 CUSBAudioStreamingDevice tx,rx; tx.m_bDeferredImplicit=true;tx.m_bIsOutput=true;
 rx.m_bImplicitInput=true;rx.iface=2;rx.ep.number=3;
 assert(!tx.EnableImplicitFeedback(&rx,2,0x83));assert(!tx.m_bImplicitEnabled);
 assert(!tx.EnableImplicitFeedback(&rx,1,0x84));assert(!tx.EnableImplicitFeedback(nullptr,1,0x83));
 rx.m_uchClockSourceID=6;assert(!tx.EnableImplicitFeedback(&rx,1,0x83));rx.m_uchClockSourceID=5;
 rx.m_nDataIntervalFactor=2;assert(!tx.EnableImplicitFeedback(&rx,1,0x83));rx.m_nDataIntervalFactor=1;
 rx.m_bImplicitInput=false;assert(!tx.EnableImplicitFeedback(&rx,1,0x83));rx.m_bImplicitInput=true;
 rx.m_bSynchronousSync=true;assert(!tx.EnableImplicitFeedback(&rx,1,0x83));rx.m_bSynchronousSync=false;
 assert(tx.EnableImplicitFeedback(&rx,1,0x83));assert(tx.m_bImplicitEnabled);
}
''')
    # Values below come from the captured real descriptor. This tests Circle's
    # selection predicate only, not physical Circle enumeration or streaming.
    actual=''' CUSBAudioStreamingDevice observed_tx,observed_rx;
 observed_tx.m_bDeferredImplicit=true;observed_tx.m_bIsOutput=true;
 observed_tx.iface=%d;observed_rx.iface=%d;observed_rx.m_bImplicitInput=true;
 observed_tx.m_uchClockSourceID=%d;observed_rx.m_uchClockSourceID=%d;
 observed_tx.m_nSubframeSize=%d;observed_rx.m_nSubframeSize=%d;
 observed_tx.m_nDataIntervalFactor=%d;observed_rx.m_nDataIntervalFactor=%d;
 observed_tx.ep.number=%d;observed_rx.ep.number=%d;
 assert(observed_tx.EnableImplicitFeedback(&observed_rx,%d,%d));
 assert(!observed_tx.EnableImplicitFeedback(&observed_rx,%d,%d));
'''%(1,2,clock_for_terminal[20],clock_for_terminal[21],tx_actual['subslot'],rx_actual['subslot'],1<<(tx_actual['interval']-1),1<<(rx_actual['interval']-1),tx_actual['address'],rx_actual['address']&0x7f,tx_actual['address'],rx_actual['address'],tx_actual['address'],0x83)
    text=test.read_text();at=text.rfind('\n}');assert at>=0;test.write_text(text[:at]+ '\n'+actual+text[at:])
    exe=path/'pair-test';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle implicit profile: synthetic cases and observed MK3 descriptor values accepted; Circle streaming unverified')

# Verify the real subslot conversion loops and idempotent patch on pinned code.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=['lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h','lib/usb/usbrequest.cpp','lib/usb/xhciendpoint.cpp']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    before=[(path/name).read_text() for name in names]
    subprocess.run(['python3',str(root/'scripts/patch-circle-audio-subslot.py'),str(path)],check=True)
    assert before==[(path/name).read_text() for name in names]
    sound=before[2]; assert 'Input and output subslot widths differ' in sound
    assert 'Output subslot width changed' in sound
    stream=before[0]
    selected=stream[stream.index('\tif (m_nBitResolution == 24)',stream.index('GROOVE_AUDIO_SUBSLOT')):stream.index('\n\t// We take the first alternate interface',stream.index('GROOVE_AUDIO_SUBSLOT'))]
    select_test=path/'select-test.cpp';select_test.write_text('''#include <cassert>
#define USB_PROTO_AUDIO_VER_200 32
struct Format { struct { unsigned bSubslotSize; } Ver200; struct { unsigned bSubframeSize; } Ver100; };
unsigned select(unsigned m_nBitResolution,unsigned protocol,unsigned width) {
 auto GetInterfaceProtocol=[&](){return protocol;}; Format f{{width},{width}}; auto* pFormatTypeDesc=&f;
 unsigned m_nSubframeSize=m_nBitResolution/8;
'''+selected+'''return m_nSubframeSize;
}
int main(){assert(select(24,32,3)==3);assert(select(24,32,4)==4);assert(select(24,0,4)==4);assert(select(24,32,2)==3);assert(select(16,32,4)==2);}
''')
    select_exe=path/'select-test';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(select_test),'-o',str(select_exe)],check=True);subprocess.run([str(select_exe)],check=True)
    output=sound[sound.index('for (unsigned i = 0; i < nChunkSize; ++i) pSamples'):].split('\n',1)[0]
    input_loop=sound[sound.index('for (unsigned i = 0; i < nBytesTransferred / 4; ++i) pSamples'):].split('\n',1)[0]
    test=path/'subslot-test.cpp';test.write_text('''#include <cassert>
#include <cstdint>
using u32=uint32_t;
void output(u32* pSamples,unsigned nChunkSize){
'''+output+'''
}
void input(u32* pSamples,unsigned nBytesTransferred){
'''+input_loop+'''
}
int main(){
 u32 samples[]={0x007fffff,0x00800000,0x00000001,0x00ffffff};
 output(samples,4);assert(samples[0]==0x7fffff00 && samples[1]==0x80000000);
 assert(samples[2]==0x00000100 && samples[3]==0xffffff00);
 input(samples,16);assert(samples[0]==0x007fffff && samples[1]==0x00800000);
 assert(samples[2]==0x00000001 && samples[3]==0x00ffffff);
}
''')
    exe=path/'subslot-test';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle descriptor-selected 24-bit subslots use left-justified USB PCM')

# The advertised encoding must actually include PCM, not merely Type I.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=['lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h','lib/usb/usbrequest.cpp','lib/usb/xhciendpoint.cpp']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for script in ('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-pcm.py'):
        subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    target=path/names[0];first=target.read_text();subprocess.run(['python3',str(root/'scripts/patch-circle-audio-pcm.py'),str(path)],check=True)
    assert first==target.read_text()
    code=first[first.index('\tif (GetInterfaceProtocol () == USB_PROTO_AUDIO_VER_200)',first.index('GROOVE_AUDIO_PCM_FORMAT')):first.index('\n\t// GROOVE_AUDIO_SUBSLOT')]
    test=path/'pcm-test.cpp';test.write_text('''#include <cassert>
#include <cstdint>
using u8=uint8_t;
#define FALSE false
#define USB_PROTO_AUDIO_VER_200 32
struct Desc { struct { uint16_t wFormatTag; } Ver100; struct { uint32_t bmFormats; } Ver200; };
bool accepted(unsigned protocol, const Desc* pAudioInterfaceDesc){
 auto GetInterfaceProtocol=[&](){return protocol;};
'''+code+'''return true;
}
int main(){Desc d{};d.Ver200.bmFormats=1;assert(accepted(32,&d));d.Ver200.bmFormats=4;assert(!accepted(32,&d));d.Ver100.wFormatTag=1;assert(accepted(0,&d));d.Ver100.wFormatTag=3;assert(!accepted(0,&d));}
''')
    exe=path/'pcm-test';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle audio rejects non-PCM Type I descriptors')

# Actual patched callbacks: 3-byte capture / 4-byte playback and the reverse.
# Both cases use identical frame counts for implicit cadence, not byte counts.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=['lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h','lib/usb/usbrequest.cpp','lib/usb/xhciendpoint.cpp']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-pcm.py','patch-circle-audio-mixed.py','patch-circle-implicit-zero.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    before=[(path/name).read_text() for name in names]
    subprocess.run(['python3',str(root/'scripts/patch-circle-audio-mixed.py'),str(path)],check=True)
    assert before==[(path/name).read_text() for name in names]
    stream,sound=before[0],before[2];pcm_sound_fixture=sound
    assert 'm_nSubframeSize != pInput->m_nSubframeSize' not in stream
    assert 'GetHWRXChannels () * m_nRXSubframeSize' in sound
    pair=stream[stream.index('boolean CUSBAudioStreamingDevice::EnableImplicitFeedback'):stream.index('boolean CUSBAudioStreamingDevice::Setup')]
    pair_test=path/'mixed-pair.cpp';pair_test.write_text('''#include <cassert>
using boolean=bool;
#define TRUE true
#define FALSE false
#define USB_AUDIO_UNDEFINED_UNIT_ID 0
struct Endpoint{unsigned number;unsigned GetNumber(){return number;}};
class CUSBAudioStreamingDevice{public:
 bool m_bDeferredImplicit=false,m_bVer200=true,m_bIsOutput=false,m_bImplicitInput=false,m_bImplicitEnabled=false,m_bSynchronousSync=false;
 unsigned m_uchClockSourceID=5,m_nDataIntervalFactor=1,m_nSubframeSize=3,m_nBitResolution=24,iface=0;void* device=(void*)1;Endpoint ep{1};Endpoint* m_pEndpointData=&ep;
 void* GetDevice(){return device;}unsigned GetInterfaceNumber(){return iface;}bool EnableImplicitFeedback(CUSBAudioStreamingDevice*,unsigned,unsigned);
};
'''+pair+'''int main(){CUSBAudioStreamingDevice tx,rx;tx.m_bDeferredImplicit=true;tx.m_bIsOutput=true;tx.m_nSubframeSize=4;rx.m_bImplicitInput=true;rx.iface=2;rx.ep.number=3;rx.m_nBitResolution=16;assert(!tx.EnableImplicitFeedback(&rx,1,0x83));rx.m_nBitResolution=24;assert(tx.EnableImplicitFeedback(&rx,1,0x83));}
''')
    pair_exe=path/'mixed-pair';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(pair_test),'-o',str(pair_exe)],check=True);subprocess.run([str(pair_exe)],check=True)
    send=sound[sound.index('boolean CUSBSoundBaseDevice::SendChunk'):sound.index('void CUSBSoundBaseDevice::DrainImplicit')]
    drain=sound[sound.index('void CUSBSoundBaseDevice::DrainImplicit'):sound.index('void CUSBSoundBaseDevice::TXCompletionStub')]
    rx=sound[sound.index('void CUSBSoundBaseDevice::RXCompletionRoutine'):sound.index('void CUSBSoundBaseDevice::RXCompletionStub')]
    test=path/'mixed-integration.cpp';test.write_text('''#include <cassert>
#include <cstdint>
#include <cstring>
#include "usb_implicit_cadence.h"
#include "usb_pcm_convert.h"
using boolean=bool;using s16=int16_t;using u8=uint8_t;using u32=uint32_t;
#define TRUE true
#define FALSE false
#define RASPPI 4
#define LOGWARN(...) ((void)0)
int AtomicIncrement(int* p){return ++*p;} int AtomicDecrement(int* p){return --*p;}
struct Lock{int depth=0;void Acquire(){assert(!depth);++depth;}void Release(){assert(depth==1);--depth;}};
struct Stream{unsigned requests=0,sizes[8]={};unsigned GetChunkSizeBytes(){return 24;}unsigned GetDataMaxPacketSize(){return 96;}
 bool SendChunk(void*,unsigned n,void(*)(unsigned,void*),void*){sizes[requests++]=n;return true;}
 bool ReceiveChunk(void*,unsigned n,void(*)(unsigned,void*),void*){sizes[requests++]=n;return true;}};
class CUSBSoundBaseDevice {public:
 enum{StateCreated,StateIdle,StateRunning,StateCanceled}m_State=StateRunning;Lock m_SpinLock;
 Stream tx,rx;Stream *m_pTXUSBDevice=&tx,*m_pRXUSBDevice=&rx;bool m_bImplicitFeedback=true,m_bImplicitDraining=false;
 groove::UsbImplicitCadence m_ImplicitCadence;unsigned m_nTXChunkSizeBytes=24,m_nRXChunkSizeBytes=24,m_nSubframeSize,m_nRXSubframeSize;
 unsigned m_nTXCurrentBuffer=0,m_nRXCurrentBuffer=0,m_nTXExpected[2]={},m_nTXCompletedBuffer=0,m_nCallbacksActive=0;
 u8 txdata[2][96]{},rxdata[2][96]{};u8* m_pTXBuffer[2]={txdata[0],txdata[1]};u8* m_pRXBuffer[2]={rxdata[0],rxdata[1]};
 int m_nOutstanding=2;unsigned samples=0;u8 received[32]{};
 CUSBSoundBaseDevice(unsigned output,unsigned input):m_nSubframeSize(output),m_nRXSubframeSize(input){m_ImplicitCadence.reset(2);}
 unsigned GetHWTXChannels(){return 4;}unsigned GetHWRXChannels(){return 2;}
 unsigned GetChunk(s16*,unsigned n){return n;}unsigned GetChunk(u32* p,unsigned n){std::memset(p,0,n*m_nSubframeSize);return n;}
 void PutChunk(const s16*,unsigned){}void PutChunk(const u32* p,unsigned n){samples=n;std::memcpy(received,p,n*m_nSubframeSize);}
 static void TXCompletionStub(unsigned,void*){}static void RXCompletionStub(unsigned,void*){}
 bool SendChunk(bool bAlreadyLocked=false,unsigned nFrames=~0u);bool ReceiveChunk(bool bAlreadyLocked=false);
 void DrainImplicit();void CallbackFinished();void TXCompletionRoutine(unsigned);void RXCompletionRoutine(unsigned);
};
'''+send+drain+rx+'''int main(){
 const u8 packed[]={0,0,0x80, 0xff,0xff,0x7f, 1,2,3, 0xff,0xff,0xff};
 const u8 right[]={0,0,0x80,0, 0xff,0xff,0x7f,0, 1,2,3,0, 0xff,0xff,0xff,0};
 const u8 wire4[]={0,0,0,0x80, 0,0xff,0xff,0x7f, 0,1,2,3, 0,0xff,0xff,0xff};
 CUSBSoundBaseDevice expand(4,3);std::memcpy(expand.rxdata[0],packed,12);expand.RXCompletionRoutine(12);
 assert(expand.samples==4 && !std::memcmp(expand.received,right,16));assert(expand.tx.requests==1 && expand.tx.sizes[0]==32);
 CUSBSoundBaseDevice compact(3,4);std::memcpy(compact.rxdata[0],wire4,16);compact.RXCompletionRoutine(16);
 assert(compact.samples==4 && !std::memcmp(compact.received,packed,12));assert(compact.tx.requests==1 && compact.tx.sizes[0]==24);
}
''')
    exe=path/'mixed-integration';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src'),str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle mixed 24-bit duplex converts input while feedback stays in frames')

# Check the patched CUSBRequest and xHCI enqueue code for a single real
# zero-length isochronous OUT TD and for ordinary non-empty transfers.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); names=['lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h','lib/usb/usbrequest.cpp','lib/usb/xhciendpoint.cpp']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-pcm.py','patch-circle-audio-mixed.py','patch-circle-implicit-zero.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    before=[(path/name).read_text() for name in names]
    subprocess.run(['python3',str(root/'scripts/patch-circle-implicit-zero.py'),str(path)],check=True)
    assert before==[(path/name).read_text() for name in names]
    req,xhci=before[4],before[5]
    add=req[req.index('void CUSBRequest::AddIsoPacket'):req.index('u32 CUSBRequest::GetNumIsoPackets')]
    code=xhci[xhci.index('\t\tassert ((m_uchEndpointType & 3) == 1);'):xhci.index('\n\tDataSyncBarrier ();')]
    # Exclude the original closing brace of the enclosing else block.
    code=code[:code.rfind('\n\t}')]
    test=path/'zero-td.cpp';test.write_text('''#include <cassert>
#include <cstdint>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using uintptr=uintptr_t;
#define MEM_KERNEL_END 0
#define XHCI_TRB_TYPE_ISOCH 1
#define XHCI_TRB_CONTROL_TRB_TYPE__SHIFT 0
#define XHCI_TRANSFER_TRB_CONTROL_IOC 2
#define XHCI_TRANSFER_TRB_CONTROL_SIA 4
#define XHCI_TRANSFER_TRB_STATUS_TD_SIZE__SHIFT 16
#define XHCI_TO_DMA_LO(p) ((u32)(uintptr)(p))
#define XHCI_TO_DMA_HI(p) ((u32)((uintptr)(p)>>32))
enum {EndpointTypeIsochronous};
struct Endpoint{int GetType(){return EndpointTypeIsochronous;}bool IsDirectionIn(){return false;}};
class CUSBRequest {public:static const unsigned MaxIsoPackets=32;Endpoint endpoint,*m_pEndpoint=&endpoint;unsigned m_nNumIsoPackets=0,m_nBufLen=0;u16 m_usIsoPacketSize[32]{};
 void AddIsoPacket(u16);unsigned GetNumIsoPackets(){return m_nNumIsoPackets;}u16 GetIsoPacketSize(unsigned i){return m_usIsoPacketSize[i];}Endpoint* GetEndpoint(){return m_pEndpoint;}};
'''+add+'''
int cache_calls=0;void CleanAndInvalidateDataCacheRange(uintptr,unsigned){++cache_calls;}
unsigned last_length=999;bool EnqueueTRB(unsigned,unsigned length,unsigned,unsigned){last_length=length & 0xffff;return true;}
bool enqueue(CUSBRequest* pURB,void* pBuffer,u32 nBufLen){unsigned m_uchEndpointType=1;
'''+code+'''
 return true;
EnqueueError:return false;
}
int main(){
 u8 buffer[16]{};CUSBRequest zero;zero.m_nBufLen=0;zero.AddIsoPacket(0);
 assert(enqueue(&zero,buffer,0));assert(last_length==0 && cache_calls==0);
 CUSBRequest full;full.m_nBufLen=12;full.AddIsoPacket(12);
 assert(enqueue(&full,buffer,12));assert(last_length==12 && cache_calls==1);
}
''')
    exe=path/'zero-td';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle xHCI emits a single zero-length isochronous OUT TRB without a cache operation')

# A synchronous UAC2 input that normally batches packets must switch to one
# packet per URB only when selected as the implicit feedback source.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=['lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h','lib/usb/usbrequest.cpp','lib/usb/xhciendpoint.cpp']
    for name in names:
        dest=path/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-pcm.py','patch-circle-audio-mixed.py','patch-circle-implicit-zero.py','patch-circle-implicit-sync.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    before=[(path/name).read_text() for name in names]
    subprocess.run(['python3',str(root/'scripts/patch-circle-implicit-sync.py'),str(path)],check=True);assert before==[(path/name).read_text() for name in names]
    source,header,sound=before[0],before[1],before[2]
    assert 'SetImplicitCaptureMode (m_bImplicitFeedback)' in sound
    method=header[header.index('\tboolean UseSynchronousChunks'):].split('\n',1)[0]
    setter=header[header.index('\tvoid SetImplicitCaptureMode'):].split('\n',1)[0]
    pair=source[source.index('boolean CUSBAudioStreamingDevice::EnableImplicitFeedback'):source.index('boolean CUSBAudioStreamingDevice::Setup')]
    pair_test=path/'sync-pair.cpp';pair_test.write_text('''#include <cassert>
using boolean=bool;
#define TRUE true
#define FALSE false
#define USB_AUDIO_UNDEFINED_UNIT_ID 0
struct Endpoint{unsigned n;unsigned GetNumber(){return n;}};
class CUSBAudioStreamingDevice {public:
 bool m_bDeferredImplicit=false,m_bVer200=true,m_bIsOutput=false,m_bImplicitInput=false,m_bImplicitEnabled=false,m_bSynchronousSync=false;
 unsigned m_uchClockSourceID=5,m_nDataIntervalFactor=1,m_nBitResolution=24,iface=0;void* device=(void*)1;Endpoint ep{1};Endpoint* m_pEndpointData=&ep;
 void* GetDevice(){return device;}unsigned GetInterfaceNumber(){return iface;}bool EnableImplicitFeedback(CUSBAudioStreamingDevice*,unsigned,unsigned);
};
'''+pair+'''int main(){CUSBAudioStreamingDevice tx,rx;tx.m_bDeferredImplicit=true;tx.m_bIsOutput=true;rx.m_bImplicitInput=true;rx.m_bSynchronousSync=true;rx.iface=2;rx.ep.n=3;assert(tx.EnableImplicitFeedback(&rx,1,0x83));}
''')
    pair_exe=path/'sync-pair';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(pair_test),'-o',str(pair_exe)],check=True);subprocess.run([str(pair_exe)],check=True)
    setup=source[source.index('\t// GROOVE_IMPLICIT_SYNC_CAPTURE: preserve'):source.index('\nunsigned CUSBAudioStreamingDevice::GetChunkSizeBytes')]
    # Strip the trailing brace of the real Setup method; keep its return value.
    setup=setup[:setup.rfind('\n}')]
    receive=source[source.index('boolean CUSBAudioStreamingDevice::ReceiveChunk'):source.index('boolean CUSBAudioStreamingDevice::SelectInputTerminal')]
    test=path/'sync-capture.cpp';test.write_text('''#include <cassert>
using boolean=bool;
#define TRUE true
#define FALSE false
#define USBSpeedFull 1
struct Endpoint { unsigned GetMaxPacketSize(){return 50;} };
struct Device { unsigned GetSpeed(){return 2;} };
struct CUSBRequest {unsigned count=0,lengths[8]={};CUSBRequest(Endpoint*,void*,unsigned){}
 void AddIsoPacket(unsigned n){lengths[count++]=n;}
 void SetCompletionRoutine(void(*)(CUSBRequest*,void*,void*),void*,void*){}
};
struct Host {unsigned count=0,lengths[8]={};bool SubmitAsyncRequest(CUSBRequest* r){count=r->count;for(unsigned i=0;i<count;++i)lengths[i]=r->lengths[i];delete r;return true;}};
class CUSBAudioStreamingDevice {public:
 using TCompletionRoutine=void(unsigned,void*);bool m_bSynchronousSync=true,m_bSinglePacketImplicit=false,m_bIsOutput=false;
 unsigned m_nSampleRate=0,m_nChunkSizeBytes=0,m_nChannels=2,m_nSubframeSize=3,m_nDataIntervalFactor=1,m_nPacketsPerChunk=2,m_usPacketSizeBytes[2]={24,24},updates=0;
 Endpoint endpoint,*m_pEndpointData=&endpoint,*m_pEndpointSync=nullptr;Device device;Host host;
 Device* GetDevice(){return &device;}Host* GetHost(){return &host;}
 void UpdateChunkSize(){++updates;m_nChunkSizeBytes=48;}
 static void CompletionHandler(CUSBRequest*,void*,void*){}
'''+method+setter+'''
 boolean apply(unsigned nSampleRate){m_nSampleRate=nSampleRate;
'''+setup+'''
 }
 boolean ReceiveChunk(void*,unsigned,TCompletionRoutine*=nullptr,void*=nullptr);
};
'''.replace('TCompletionRoutine*=','TCompletionRoutine* =').replace('void*=','void* =')+receive+'''int main(){
 char buffer[64]{};CUSBAudioStreamingDevice normal;assert(normal.apply(48000));assert(normal.updates==1);
 assert(normal.ReceiveChunk(buffer,48));assert(normal.host.count==2 && normal.host.lengths[0]==24 && normal.host.lengths[1]==24 && normal.updates==2);
 CUSBAudioStreamingDevice implicit;implicit.SetImplicitCaptureMode(true);assert(implicit.apply(48000));
 assert(implicit.updates==0 && implicit.m_nChunkSizeBytes==48);assert(implicit.ReceiveChunk(buffer,48));
 assert(implicit.host.count==1 && implicit.host.lengths[0]==48 && implicit.updates==0);
 implicit.SetImplicitCaptureMode(false);assert(implicit.apply(48000));assert(implicit.ReceiveChunk(buffer,48));assert(implicit.host.count==2 && implicit.updates==2);
}
''')
    exe=path/'sync-capture';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle synchronous feedback capture uses exactly one packet per request; normal batching remains intact')

# Sparse subdevice numbers can appear after individual stream removal. Exercise
# the patched Circle method itself, including the bounded not-found path.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); target=path/'lib/sound/usbsoundbasedevice.cpp'
    target.parent.mkdir(parents=True); target.write_bytes(original_audio)
    cmd=['python3',str(root/'scripts/patch-circle-audio-sparse.py'),str(path)]
    subprocess.run(cmd,check=True); first=target.read_text(); subprocess.run(cmd,check=True)
    assert first==target.read_text() and first.count('GROOVE_AUDIO_SPARSE_SUBDEVICES')==1
    start=first.index('CUSBAudioStreamingDevice *CUSBSoundBaseDevice::GetStreamingDevice (')
    end=first.index('\nboolean CUSBSoundBaseDevice::SendChunk',start)
    body=first[start:end]
    test=path/'sparse.cpp';test.write_text('''#include <cassert>
#include <cstdio>
using boolean=bool;
#define FALSE false
class CString {public: unsigned device=0,sub=0; void Format(const char*,unsigned d,unsigned s){device=d;sub=s;}};
struct CDevice {virtual ~CDevice()=default;};
struct CUSBAudioStreamingDevice: CDevice {struct TDeviceInfo {bool IsOutput;}; bool output=false; TDeviceInfo GetDeviceInfo(){return {output};}};
struct CDeviceNameService {CDevice* slots[16]{};unsigned calls=0;static CDeviceNameService* Get(){static CDeviceNameService s;return &s;}CDevice* GetDevice(const CString& n,bool){++calls;return n.sub<=16?slots[n.sub-1]:nullptr;}};
struct CUSBSoundBaseDevice {unsigned m_nDevice=0;CUSBAudioStreamingDevice* GetStreamingDevice(boolean,unsigned);};
'''+body+'''
int main(){auto* names=CDeviceNameService::Get();CUSBAudioStreamingDevice tx,rx;tx.output=true;names->slots[2]=&tx;names->slots[6]=&rx;CUSBSoundBaseDevice sound;assert(sound.GetStreamingDevice(true,0)==&tx);assert(sound.GetStreamingDevice(false,0)==&rx);names->calls=0;assert(!sound.GetStreamingDevice(true,1));assert(names->calls==16);}
''')
    exe=path/'sparse';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
    kernel=(root/'firmware/kernel.cpp').read_text()
    start=kernel.index('static int FindMk3Audio(');end=kernel.index('class CPrepareUpdate',start)
    scan=kernel[start:end];assert 'if(!stream) break;' not in scan and 'if(!stream)break;' not in scan
print('Circle sparse audio discovery bounded and host-tested; physical reconnect unverified')

# The selected receive ordinal must match both rate and requested endpoint,
# including when an earlier input is unsuitable and interface numbers have gaps.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp); kernel=(root/'firmware/kernel.cpp').read_text()
    start=kernel.index('static int FindMk3Input(');end=kernel.index('\n// A canceled stream',start)
    body=kernel[start:end];test=path/'rx-select.cpp';test.write_text('''#include <cassert>
#include <cstdint>
#define FALSE false
enum {LogNotice};
struct CString {unsigned sub=0;void Format(const char*,unsigned,unsigned s){sub=s;}};
struct Desc {unsigned idVendor=0x17cc,idProduct=0x1600;};struct Dev {Desc desc;Desc* GetDeviceDescriptor(){return &desc;}};
using CUSBDevice=Dev;
struct CUSBAudioStreamingDevice {struct Range {unsigned Min,Max,Resolution;};struct Info {bool IsOutput;unsigned NumChannels,SampleRateRanges;Range SampleRateRange[2];};Dev dev;Dev* parent=&dev;Info info{};unsigned endpoint=0;Dev* GetDevice(){return parent;}Info GetDeviceInfo(){return info;}unsigned GetDataEndpointAddress(){return endpoint;}};
struct CDeviceNameService {CUSBAudioStreamingDevice* slots[16]{};static CDeviceNameService* Get(){static CDeviceNameService s;return &s;}void* GetDevice(const CString& n,bool){return slots[n.sub-1];}};
struct CLogger {static CLogger* Get(){static CLogger l;return &l;}template<class... T>void Write(const char*,int,const char*,T...) {}};
namespace groove {struct HardwareDiagnostics {unsigned tx_channels=0,rx_channels=0;};}
'''+body+'''
int main(){auto* names=CDeviceNameService::Get();CUSBAudioStreamingDevice bad,good,tx;bad.info={false,2,1,{{44100,44100,0}}};bad.endpoint=0x82;good.info={false,2,1,{{48000,48000,0}}};good.endpoint=0x81;bad.parent=&good.dev;tx.parent=&good.dev;tx.info={true,4,0,{}};names->slots[1]=&bad;names->slots[4]=&tx;names->slots[8]=&good;groove::HardwareDiagnostics d;assert(FindMk3Input(&good.dev,0,0x81,d)==1);assert(FindMk3Input(&good.dev,0,0x82,d)==-1);assert(FindMk3Input(&good.dev,0,0,d)==1);assert(d.tx_channels==4 && d.rx_channels==2);good.parent=&bad.dev;assert(FindMk3Input(&good.dev,0,0x81,d)==-1);assert(FindMk3Input(nullptr,0,0,d)==-1);}
''')
    exe=path/'rx-select';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('MK3 input ordinal and provisional endpoint selection host-tested; physical audio unverified')

with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=('lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    cmd=['python3',str(root/'scripts/patch-circle-audio-rx-select.py'),str(path)]
    subprocess.run(cmd,check=True);before=[(path/n).read_text() for n in names];subprocess.run(cmd,check=True)
    assert before==[(path/n).read_text() for n in names]
    assert before[2].count('GetStreamingDevice (FALSE, m_nRXInterface)')==3
    assert 'unsigned nRXInterface  = 0' in before[3] and 'GetDataEndpointAddress' in before[1]
print('Circle input ordinal patch pinned and idempotent; physical audio unverified')

# Duplex input and output can be removed in either order. Exercise the exact
# generated removal-registration handling with a synthetic pair.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=('lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-rx-select.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    cmd=['python3',str(root/'scripts/patch-circle-audio-duplex-removal.py'),str(path)]
    subprocess.run(cmd,check=True);before=[(path/n).read_text() for n in names];subprocess.run(cmd,check=True);assert before==[(path/n).read_text() for n in names]
    sound=before[2];start=sound.index('void CUSBSoundBaseDevice::DeviceRemovedHandler');stop=sound.index('\n\tCancel ();',start);part=sound[start:stop]+'\n}\n'
    assert 'm_pRXUSBDevice->RegisterRemovedHandler' in sound and 'm_hRXRemoveRegistration' in before[3]
    test=path/'duplex-remove.cpp';test.write_text('''#include <cassert>
struct CDevice {virtual ~CDevice()=default;};
struct Stream:CDevice {int removed=0;void UnregisterRemovedHandler(int h){assert(h);++removed;}};
struct CUSBSoundBaseDevice {Stream* m_pTXUSBDevice=nullptr;Stream* m_pRXUSBDevice=nullptr;int m_hRemoveRegistration=0,m_hRXRemoveRegistration=0;void DeviceRemovedHandler(CDevice*);};
'''+part+'''
int main(){Stream tx,rx;CUSBSoundBaseDevice a{&tx,&rx,1,2};a.DeviceRemovedHandler(&rx);assert(tx.removed==1 && rx.removed==0 && !a.m_hRemoveRegistration && !a.m_hRXRemoveRegistration);CUSBSoundBaseDevice b{&tx,&rx,1,2};b.DeviceRemovedHandler(&tx);assert(tx.removed==1 && rx.removed==1 && !b.m_hRemoveRegistration && !b.m_hRXRemoveRegistration);CUSBSoundBaseDevice c{nullptr,&rx,1,0};c.DeviceRemovedHandler(&rx);assert(rx.removed==1 && !c.m_hRemoveRegistration);}
''')
    exe=path/'duplex-remove';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle duplex removal registration host-tested in either order; physical behavior unverified')

# Audio endpoint teardown must notify consumers before endpoint destruction;
# the later base destructor must not deliver the same notification twice.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=('include/circle/device.h','lib/device.cpp','lib/usb/usbaudiostreaming.cpp')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    cmd=['python3',str(root/'scripts/patch-circle-audio-early-removal.py'),str(path)]
    subprocess.run(cmd,check=True);before=[(path/n).read_text() for n in names];subprocess.run(cmd,check=True);assert before==[(path/n).read_text() for n in names]
    device,stream=before[1],before[2]
    base=device[device.index('CDevice::~CDevice'):device.index('\nint CDevice::Read')]
    sub=stream[stream.index('CUSBAudioStreamingDevice::~CUSBAudioStreamingDevice'):stream.index('\nboolean CUSBAudioStreamingDevice::Initialize')]
    test=path/'early.cpp';test.write_text('''#include <cassert>
#define FALSE false
int notices=0, destroyed=0;class CDevice;struct TRemovedHandlerEntry {void (*pHandler)(CDevice*,void*);void* pContext;};struct TPtrListElement {TRemovedHandlerEntry* p;};
struct List {TPtrListElement* item=nullptr;TPtrListElement* GetFirst(){return item;}void* GetPtr(TPtrListElement* e){return e->p;}void Remove(TPtrListElement*){delete item;item=nullptr;}};
class CDevice {public:List m_RemovedHandlerList;virtual ~CDevice();protected:void NotifyRemoved();};
struct Name {unsigned GetLength(){return 0;}};struct CDeviceNameService {static CDeviceNameService* Get(){static CDeviceNameService s;return &s;}void RemoveDevice(Name,bool){}};
struct Endpoint {~Endpoint(){assert(notices==1);++destroyed;}};
class CUSBAudioStreamingDevice:public CDevice {public:Name m_DeviceName;Endpoint* m_pEndpointSync=new Endpoint;Endpoint* m_pEndpointData=new Endpoint;~CUSBAudioStreamingDevice();};
'''+base+sub+'''
void removed(CDevice*,void*){assert(destroyed==0);++notices;}
int main(){auto* d=new CUSBAudioStreamingDevice;d->m_RemovedHandlerList.item=new TPtrListElement{new TRemovedHandlerEntry{removed,nullptr}};delete d;assert(notices==1 && destroyed==2);}
''')
    exe=path/'early';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle audio removal notifies before endpoint teardown exactly once; physical disconnect unverified')

# Output selection must preserve Circle's ordinal among all output streams and
# match the provisional output endpoint and an actually advertised 48 kHz rate.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);kernel=(root/'firmware/kernel.cpp').read_text()
    start=kernel.index('static int FindMk3Audio(');end=kernel.index('\nstatic int FindMk3Input',start)
    body=kernel[start:end];test=path/'tx-select.cpp';test.write_text('''#include <cassert>
#define FALSE false
struct CString {unsigned device=0,sub=0;void Format(const char*,unsigned d,unsigned s){device=d;sub=s;}};
struct Desc {unsigned idVendor=0x17cc,idProduct=0x1600;};struct Dev {Desc desc;Desc* GetDeviceDescriptor(){return &desc;}};
using CUSBDevice=Dev;
struct CUSBAudioStreamingDevice {struct Range {unsigned Min,Max,Resolution;};struct Info {bool IsOutput;unsigned NumChannels,SampleRateRanges;Range SampleRateRange[2];};Dev dev;Dev* parent=&dev;Info info{};unsigned endpoint=0;Dev* GetDevice(){return parent;}Info GetDeviceInfo(){return info;}unsigned GetDataEndpointAddress(){return endpoint;}};
struct CDeviceNameService {CUSBAudioStreamingDevice* slots[16][16]{};static CDeviceNameService* Get(){static CDeviceNameService s;return &s;}void* GetDevice(const CString& n,bool){return slots[n.device-1][n.sub-1];}};
'''+body+'''
int main(){auto* names=CDeviceNameService::Get();CUSBAudioStreamingDevice wrong,bad,good,in;wrong.dev.desc.idVendor=1;wrong.info={true,2,1,{{48000,48000,0}}};wrong.endpoint=1;bad.info={true,4,1,{{44100,44100,0}}};bad.endpoint=2;good.info={true,4,1,{{48000,48000,0}}};good.endpoint=1;bad.parent=&good.dev;in.parent=&good.dev;in.info={false,2,1,{{48000,48000,0}}};names->slots[1][0]=&wrong;names->slots[1][2]=&bad;names->slots[1][4]=&in;names->slots[1][7]=&good;unsigned ordinal=99;assert(FindMk3Audio(&good.dev,1,ordinal)==1 && ordinal==2);assert(FindMk3Audio(&good.dev,2,ordinal)==-1);assert(FindMk3Audio(&good.dev,0,ordinal)==1 && ordinal==2);good.parent=&in.dev;assert(FindMk3Audio(&good.dev,1,ordinal)==-1);assert(FindMk3Audio(nullptr,1,ordinal)==-1);}
''')
    exe=path/'tx-select';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('MK3 output ordinal, 48 kHz range and provisional endpoint host-tested; physical audio unverified')

with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=('lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-rx-select.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    cmd=['python3',str(root/'scripts/patch-circle-audio-tx-select.py'),str(path)]
    subprocess.run(cmd,check=True);before=[(path/n).read_text() for n in names];subprocess.run(cmd,check=True);assert before==[(path/n).read_text() for n in names]
    assert 'm_nTXInterface (nTXInterface)' in before[2] and 'unsigned nTXInterface  = 0' in before[3]
print('Circle output ordinal patch pinned and idempotent; physical audio unverified')

# Each input completion must be bounded by its corresponding submitted packet.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=('lib/usb/usbaudiostreaming.cpp','include/circle/usb/usbaudiostreaming.h','lib/sound/usbsoundbasedevice.cpp','include/circle/sound/usbsoundbasedevice.h')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    scripts=('patch-circle-audio.py','patch-circle-audio-completion.py','patch-circle-audio-input.py','patch-circle-audio-chunk.py','patch-circle-audio-lifetime.py','patch-circle-audio-disconnect.py','patch-circle-audio-status.py','patch-circle-implicit-stream.py','patch-circle-implicit-sound.py','patch-circle-audio-subslot.py','patch-circle-audio-mixed.py','patch-circle-audio-rx-length.py')
    for script in scripts:subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    before=[(path/n).read_text() for n in names];subprocess.run(['python3',str(root/'scripts/patch-circle-audio-rx-length.py'),str(path)],check=True);assert before==[(path/n).read_text() for n in names]
    source=before[2];receive=source[source.index('boolean CUSBSoundBaseDevice::ReceiveChunk'):source.index('void CUSBSoundBaseDevice::DrainImplicit')];complete=source[source.index('void CUSBSoundBaseDevice::RXCompletionRoutine'):source.index('void CUSBSoundBaseDevice::RXCompletionStub')]
    assert receive.index('m_RXLengths.submitted') < receive.index('m_pRXUSBDevice->ReceiveChunk')
    assert complete.index('m_RXLengths.completed') < complete.index('PutChunk') and complete.index('m_SpinLock.Acquire') < complete.index('m_RXLengths.completed')
print('Circle input completion checked against submitted request lengths; Pi streaming unverified')

# Exercise the patched MIDI DMA lifetime for success, short failure and timeout.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=('include/circle/usb/usbmidi.h','lib/usb/usbmidi.cpp','lib/usb/usbmidihost.cpp')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for script in ('patch-circle-midi.py','patch-circle-midi-dma.py'):subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    source=(path/names[2]).read_text();subprocess.run(['python3',str(root/'scripts/patch-circle-midi-dma.py'),str(path)],check=True);assert source==(path/names[2]).read_text()
    body=source[source.index('boolean CUSBMIDIHostDevice::SendEventsHandler'):source.index('boolean CUSBMIDIHostDevice::StartRequest')]
    test=path/'midi-dma.cpp';test.write_text('''#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
using u8=uint8_t;using uintptr=uintptr_t;using boolean=bool;
#define FALSE false
#define DATA_CACHE_LINE_LENGTH_MAX 64
int allocated=0,freed=0;void* last=nullptr;
void* operator new[](size_t n){++allocated;last=std::malloc(n);if(!last)throw std::bad_alloc();return last;}
void operator delete[](void* p)noexcept{++freed;std::free(p);}
void operator delete[](void* p,size_t)noexcept{++freed;std::free(p);}
unsigned ticks=0,delay=0;int result=0;struct CTimer {static unsigned GetClockTicks(){return ticks;}};
struct CLogger {static CLogger* Get(){static CLogger logger;return &logger;}void Write(const char*,int,const char*){}};
const char* FromMIDI="midi";const int LogWarning=1;
struct Host {int Transfer(void*,void* buffer,unsigned){assert((uintptr(buffer)&63)==0);ticks+=delay;return result;}};
struct CUSBMIDIHostDevice {void* m_pEndpointOut=this;Host host;Host* GetHost(){return &host;}static bool SendEventsHandler(const u8*,unsigned,void*);};
'''+body+'''int main(){CUSBMIDIHostDevice d;const u8 bytes[4]={1,2,3,4};result=4;delay=20;assert(d.SendEventsHandler(bytes,4,&d) && allocated==1 && freed==1);result=2;assert(!d.SendEventsHandler(bytes,4,&d) && allocated==2 && freed==2);result=-1;delay=3000000;assert(!d.SendEventsHandler(bytes,4,&d) && allocated==3 && freed==2);operator delete[](last);assert(freed==3);}
''')
    exe=path/'midi-dma';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle MIDI timeout retains DMA buffer; success and short results release it; Pi timeout unverified')

# The final Circle patch reuses MIDI DMA storage and quarantines it on timeout.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);names=('include/circle/usb/usbmidi.h','lib/usb/usbmidi.cpp','lib/usb/usbmidihost.cpp','include/circle/usb/usbmidihost.h')
    for name in names:
        dst=path/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for script in ('patch-circle-midi.py','patch-circle-midi-dma.py','patch-circle-midi-cache.py'):subprocess.run(['python3',str(root/'scripts'/script),str(path)],check=True)
    source=(path/names[2]).read_text();header=(path/names[3]).read_text();subprocess.run(['python3',str(root/'scripts/patch-circle-midi-cache.py'),str(path)],check=True);assert source==(path/names[2]).read_text() and header==(path/names[3]).read_text()
    assert 'groove::MidiDmaBuffer m_OutputBuffer' in header and 'GROOVE_MIDI_TIMEOUT_DMA' in source
    body=source[source.index('boolean CUSBMIDIHostDevice::SendEventsHandler'):source.index('boolean CUSBMIDIHostDevice::StartRequest')]
    test=path/'midi-cache.cpp';test.write_text('''#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
using u8=uint8_t;using uintptr=uintptr_t;using boolean=bool;
#define FALSE false
#define DATA_CACHE_LINE_LENGTH_MAX 64
int allocated=0,freed=0;void* last=nullptr;
void* operator new[](size_t n){++allocated;last=std::malloc(n);if(!last)throw std::bad_alloc();return last;}
void operator delete[](void* p)noexcept{if(p){++freed;std::free(p);}}
void operator delete[](void* p,size_t)noexcept{if(p){++freed;std::free(p);}}
#include "midi_dma_buffer.h"
unsigned ticks=0,delay=0,calls=0;int result=0;struct CTimer {static unsigned GetClockTicks(){return ticks;}};
struct CLogger {static CLogger* Get(){static CLogger logger;return &logger;}void Write(const char*,int,const char*){}};
const char* FromMIDI="midi";const int LogWarning=1;
struct Host {int Transfer(void*,void* buffer,unsigned){++calls;assert((uintptr(buffer)&63)==0);ticks+=delay;return result;}};
struct CUSBMIDIHostDevice {groove::MidiDmaBuffer m_OutputBuffer;void* m_pEndpointOut=this;Host host;Host* GetHost(){return &host;}static bool SendEventsHandler(const u8*,unsigned,void*);};
'''+body+'''int main(){const u8 bytes[128]={1,2,3,4};{CUSBMIDIHostDevice d;result=4;delay=20;assert(d.SendEventsHandler(bytes,4,&d) && allocated==1 && freed==0);assert(d.SendEventsHandler(bytes,4,&d) && allocated==1 && freed==0);result=128;assert(d.SendEventsHandler(bytes,128,&d) && allocated==2 && freed==1);result=-1;delay=3000000;assert(!d.SendEventsHandler(bytes,128,&d) && allocated==2 && freed==1);unsigned old=calls;assert(!d.SendEventsHandler(bytes,128,&d) && calls==old);}assert(freed==1);operator delete[](last);assert(freed==2);}
''')
    exe=path/'midi-cache';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle MIDI DMA buffer reused and quarantined after timeout; Pi behavior unverified')

# Binding changes cancel the old sound device but do not destroy it while
# callbacks are still active. The kernel's normal drain path owns destruction.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);kernel=(root/'firmware/kernel.cpp').read_text()
    start=kernel.index('static bool ReconcileAudioBinding(');end=kernel.index('\n// Never draw',start)
    body=kernel[start:end];test=path/'audio-binding.cpp';test.write_text('''#include <cassert>
struct CUSBDevice {};
struct CMk3Audio {bool running=true,active=true;unsigned canceled=0;bool operational(){return running;}void Cancel(){++canceled;running=false;}};
'''+body+'''
int main(){CUSBDevice a,b;CMk3Audio audio;assert(!ReconcileAudioBinding(nullptr,&a,&b));assert(!ReconcileAudioBinding(&audio,nullptr,&b));assert(!ReconcileAudioBinding(&audio,&a,&a));assert(ReconcileAudioBinding(&audio,&a,&b));assert(audio.canceled==1 && audio.active);assert(!ReconcileAudioBinding(&audio,&a,&b));audio.running=true;assert(ReconcileAudioBinding(&audio,&a,nullptr));assert(audio.canceled==2 && audio.active);}
''')
    exe=path/'binding';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('MK3 audio binding changes cancel without destroying a draining stream; physical rebind unverified')

# A still-registered MIDI function from a previous controller must never be
# used for the newly selected HID. Rebinding waits for Circle's removal event.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);source=(root/'firmware/mk3_midi.cpp').read_text()
    start=source.index('bool CMk3Midi::Matches(');end=source.index('\nvoid CMk3Midi::Packet',start)
    test=path/'midi-binding.cpp';test.write_text('''#include <cassert>
struct Usb {};
struct CMk3Hid {Usb* device;Usb* GetDevice(){return device;}};
struct Midi {Usb* device;Usb* GetUSBDevice(){return device;}};
struct CMk3Midi {Midi* m_Device=nullptr;bool Matches(CMk3Hid*)const;};
'''+source[start:end]+'''
int main(){Usb a,b;CMk3Hid selected{&a};Midi midi{&a};CMk3Midi owner;assert(!owner.Matches(nullptr));assert(!owner.Matches(&selected));owner.m_Device=&midi;assert(owner.Matches(&selected));selected.device=&b;assert(!owner.Matches(&selected));}
''')
    exe=path/'binding';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
    kernel=(root/'firmware/kernel.cpp').read_text();assert 'midi_connected && midi->Send' in kernel and 'if(!midi_connected)continue;' in kernel
print('MK3 MIDI input and output are gated by selected HID parent; rebind waits for removal')

# Real queue/fence and Pop implementation: pre-rebind events and packets from
# another function are discarded, while packets produced after the fence remain.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);source=(root/'firmware/mk3_midi.cpp').read_text()
    start=source.index('bool CMk3Midi::Pop(');end=source.index('\nbool CMk3Midi::Matches',start)
    test=path/'midi-fence.cpp';test.write_text('''#include <cassert>
#include "'''+str(root/'src/spsc_queue.h')+'''"
struct Mk3MidiEvent {unsigned device,value,generation;};
struct CMk3Midi {void* m_Device=nullptr;groove::SpscQueue<Mk3MidiEvent,4> m_Events;unsigned m_DeviceNumber=0,m_Fence=0,m_ActiveGeneration=1;bool m_FenceActive=false;bool Pop(Mk3MidiEvent&);};
'''+source[start:end]+'''
int main(){CMk3Midi m;Mk3MidiEvent e{};int device;m.m_Device=&device;m.m_DeviceNumber=2;
 assert(m.m_Events.push({2,1,1}));m.m_Fence=m.m_Events.consumer_boundary();m.m_FenceActive=true;
 assert(m.m_Events.push({1,2,1}));assert(m.m_Events.push({2,3,1}));assert(m.Pop(e) && e.value==3);assert(!m.Pop(e));
 assert(m.m_Events.push({2,4,1}));m.m_Fence=m.m_Events.consumer_boundary();m.m_FenceActive=true;assert(m.m_Events.push({2,5,1}));assert(m.Pop(e) && e.value==5);m.m_ActiveGeneration=2;assert(m.m_Events.push({2,6,1}));assert(m.m_Events.push({2,7,2}));assert(m.Pop(e) && e.value==7);}
''')
    exe=path/'fence';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('MIDI rebind discards pre-fence input and packets from different USB MIDI functions')

# Retained immutable callback contexts reject a late packet and removal from an
# earlier binding, even when Circle has reused the same MIDI device number.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);source=(root/'firmware/mk3_midi.cpp').read_text()
    start=source.index('void CMk3Midi::Packet(');end=source.index('\nbool CMk3Midi::Send',start)
    test=path/'midi-generation.cpp';test.write_text('''#include <cassert>
#include <cstdint>
#include "'''+str(root/'src/spsc_queue.h')+'''"
using u8=uint8_t;
struct CDevice {};
struct Mk3MidiEvent {u8 cable,length,bytes[3];unsigned device,generation;};
struct CMk3Midi {struct Binding {CMk3Midi* owner;unsigned generation;};CDevice* m_Device=nullptr;unsigned m_ActiveGeneration=0,m_Dropped=0;groove::SpscQueue<Mk3MidiEvent,8> m_Events;static void Packet(unsigned,u8*,unsigned,unsigned,void*);static void Removed(CDevice*,void*);};
'''+source[start:end]+'''
int main(){CMk3Midi m;CDevice d;CMk3Midi::Binding old{&m,1},current{&m,2};m.m_Device=&d;m.m_ActiveGeneration=2;u8 note[]={0x90,60,100};Mk3MidiEvent e{};
 CMk3Midi::Packet(0,note,3,4,&old);assert(!m.m_Events.pop(e));CMk3Midi::Removed(&d,&old);assert(m.m_Device==&d);
 CMk3Midi::Packet(0,note,3,4,&current);assert(m.m_Events.pop(e) && e.generation==2 && e.device==4);CMk3Midi::Removed(&d,&current);assert(!m.m_Device);}
''')
    exe=path/'generation';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('MIDI callback generations reject stale packets and stale removals even after number reuse')

# Exercise real LED/display code, independent ACKs, dual-LCD pipeline and DMA
# retention across failure/removal with an asynchronous host.
subprocess.run(['python3',str(root/'tests/usb_hot_paths_test.py')],check=True)

with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);source=(root/'firmware/kernel.cpp').read_text()
    start=source.index('static CMk3Display* SelectMk3Display(');end=source.index('\n// Shared task-owned handoff.',start)
    test=path/'display-parent.cpp';test.write_text('''#include <cassert>
struct Usb{};struct CMk3Hid{Usb* device;Usb* GetDevice(){return device;}};struct CMk3Display{Usb* device;Usb* GetDevice(){return device;}};
'''+source[start:end]+'''
int main(){Usb a,b;CMk3Hid hid{&a};CMk3Display same{&a},other{&b};assert(!SelectMk3Display(nullptr,&same));assert(!SelectMk3Display(&hid,nullptr));assert(!SelectMk3Display(&hid,&other));assert(SelectMk3Display(&hid,&same)==&same);}
''')
    exe=path/'display-parent';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Display association requires the selected HID USB parent')

# Exercise the real early initialization paths without Pi hardware: failures
# after UART setup must leave a useful stage marker and must not start Run().
# Interrupt setup now precedes UART registration, so an IRQ setup failure has no UART yet.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    app=(root/'firmware/kernel.cpp').read_text()
    app_header=(root/'firmware/kernel.h').read_text()
    assert 'm_Serial(&m_Interrupt)' in app and app_header.index('CInterruptSystem\tm_Interrupt')<app_header.index('CSerialDevice\t\tm_Serial')
    recovery=(root/'firmware/recovery/kernel.cpp').read_text()
    app_init=app[app.index('boolean CKernel::Initialize()'):app.index('\n// Find a Circle audio-control',app.index('boolean CKernel::Initialize()'))]
    recovery_init=recovery[recovery.index('bool CRecoveryKernel::Initialize()'):recovery.index('\nvoid CRecoveryKernel::Run()',recovery.index('bool CRecoveryKernel::Initialize()'))]
    test=path/'boot-diagnostics.cpp';test.write_text(r'''#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <cstdint>
constexpr unsigned ARM_PM_RSTS=0;unsigned read32(unsigned){return 0;}
using boolean=bool;constexpr bool TRUE=true,FALSE=false;unsigned GrooveFirmwareVersion(){return 1234;}const char* FromKernel="groove";enum {LogNotice,LogError,LogWarning};
struct Serial{bool ok=true;std::string text;bool Initialize(int){return ok;}void Write(const char* s,unsigned n){text.append(s,n);}};
struct Part{bool ok=true;bool Initialize(){return ok;}void MsDelay(unsigned){}void GetUptime(unsigned* s,unsigned* us){*s=*us=0;}};
struct Logger{Serial* s=nullptr;bool ok=true;bool Initialize(Serial* value){s=value;return ok;}void Write(const char*,int,const char* fmt,...){char b[256];va_list args;va_start(args,fmt);vsnprintf(b,sizeof b,fmt,args);va_end(args);s->text+=b;}};
struct Watchdog{void Start(unsigned){}};
// This fixture tests startup error reports without a connected display.
// Production asynchronous splash ACK/shadow handoff runs in usb_hot_paths_test.
using u8=uint8_t;struct CTimer{static unsigned GetClockTicks(){return 0;}};
struct CMk3Hid{};struct CMk3Display{};
static CMk3Display* SelectMk3Display(CMk3Hid*,CMk3Display*){return nullptr;}
void* palloc(){return new u8[1];}
struct CircleDisplay{explicit CircleDisplay(u8* p){delete[] p;}bool BootSplash(CMk3Display*){return false;}bool BootSplashComplete(){return false;}};
struct Devices{void* GetDevice(const char*,bool){return nullptr;}};
struct CKernel{Serial m_Serial;Logger m_Logger;Part m_Interrupt,m_Timer,m_USBHCI;Watchdog m_Watchdog;Devices m_DeviceNameService;CircleDisplay* m_Display=nullptr;~CKernel(){delete m_Display;}void BootTrace(const char*){}boolean Initialize();};
struct CRecoveryKernel{Serial m_Serial;Logger m_Logger;Part m_Interrupt,m_Timer,m_SD,m_USB;Watchdog m_Watchdog;bool Initialize();};
'''+app_init+'\n'+recovery_init+r'''
int main(){CKernel a;assert(a.Initialize());assert(a.m_Serial.text.find("Application firmware 1234")!=std::string::npos);CKernel b;b.m_Interrupt.ok=false;assert(!b.Initialize());assert(b.m_Serial.text.empty());CKernel c;c.m_Timer.ok=false;assert(!c.Initialize());assert(c.m_Serial.text.find("timer initialization failed")!=std::string::npos);CKernel d;d.m_USBHCI.ok=false;assert(!d.Initialize());assert(d.m_Serial.text.find("USB host initialization failed")!=std::string::npos);
 CRecoveryKernel e;assert(e.Initialize());assert(e.m_Serial.text.find("Recovery firmware 1234")!=std::string::npos);CRecoveryKernel f;f.m_SD.ok=false;assert(!f.Initialize());assert(f.m_Serial.text.find("SD controller initialization failed")!=std::string::npos);CRecoveryKernel g;g.m_USB.ok=false;assert(!g.Initialize());assert(g.m_Serial.text.find("USB host initialization failed")!=std::string::npos);}
''')
    exe=path/'boot-diagnostics';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Application and recovery startup report initialization failures; Pi UART unverified')

# A late completion from a timed-out blocking xHCI request must never be
# paired with a newly submitted request on the same endpoint.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    for name in ('include/circle/usb/xhciendpoint.h','lib/usb/xhciendpoint.cpp'):
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    cmd=['python3',str(root/'scripts/patch-circle-xhci-timeout.py'),str(path)]
    subprocess.run(cmd,check=True);header=(path/'include/circle/usb/xhciendpoint.h').read_text();code=(path/'lib/usb/xhciendpoint.cpp').read_text()
    subprocess.run(cmd,check=True);assert header==(path/'include/circle/usb/xhciendpoint.h').read_text() and code==(path/'lib/usb/xhciendpoint.cpp').read_text()
    start=code.index('boolean CXHCIEndpoint::Transfer (');end=code.index('\nboolean CXHCIEndpoint::TransferAsync',start);blocking=code[start:end]
    start=code.index('\tm_SpinLock.Acquire ();',end);end=code.index('\n\tif (   (m_uchEndpointType',start);queue=code[start:end]
    test=path/'xhci-timeout.cpp';test.write_text('''#include <cassert>
using boolean=bool;using u8=unsigned char;constexpr bool TRUE=true,FALSE=false;constexpr unsigned HZ=1,USB_TIMEOUT_NONE=0;const char* From="test";constexpr int LogDebug=0;
struct Endpoint {unsigned GetNumber(){return 1;}};struct CUSBRequest {Endpoint ep;void SetCompletionRoutine(void(*)(CUSBRequest*,void*,void*),void*,void*){} Endpoint* GetEndpoint(){return &ep;}bool GetStatus(){return true;}};
struct CTimer {unsigned ticks=0;static CTimer* Get(){static CTimer timer;return &timer;}unsigned GetTicks(){return ticks++;}};
struct CLogger {static CLogger* Get(){static CLogger logger;return &logger;}void Write(const char*,int,const char*,unsigned){}};void DataMemBarrier(){}
struct Lock {void Acquire(){}void Release(){}};
struct CXHCIEndpoint {Lock m_SpinLock;bool m_bTransferCompleted=true,m_bValid=true,m_bGrooveTimedOut=false,reject=false;CUSBRequest* m_pURB[2]={};static void CompletionRoutine(CUSBRequest*,void*,void*){};boolean Transfer(CUSBRequest*,unsigned);boolean TransferAsync(CUSBRequest* pURB,unsigned){if(reject)return false;
'''+queue+'''
 return true;}};
'''+blocking+'''
int main(){CXHCIEndpoint e;CUSBRequest old,next;e.reject=true;assert(!e.Transfer(&old,0) && e.m_bTransferCompleted && !e.m_bGrooveTimedOut);e.reject=false;assert(!e.Transfer(&old,0));assert(e.m_bTransferCompleted && e.m_bGrooveTimedOut && !e.m_pURB[0]);assert(!e.TransferAsync(&next,0) && !e.m_pURB[0]);assert(!e.Transfer(&next,0) && e.m_bTransferCompleted && !e.m_pURB[0]);}
''')
    exe=path/'xhci-timeout';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle xHCI timeout rejects later endpoint submissions; failed submit leaves blocking state reusable')

# Exercise the patched blocking-owner and completion paths at the timeout
# boundary. A callback made during the timeout warning must win safely, while
# a callback after timeout cannot touch the expired blocking request.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    for name in ('include/circle/usb/xhciendpoint.h','lib/usb/xhciendpoint.cpp'):
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for patch in ('patch-circle-xhci-timeout.py','patch-circle-xhci-blocking-race.py','patch-circle-xhci-async-critical.py','patch-circle-xhci-queue.py'):
        cmd=['python3',str(root/'scripts'/patch),str(path)];subprocess.run(cmd,check=True);subprocess.run(cmd,check=True)
    code=(path/'lib/usb/xhciendpoint.cpp').read_text();start=code.index('boolean CXHCIEndpoint::Transfer (');end=code.index('\nboolean CXHCIEndpoint::TransferAsync',start);blocking=code[start:end]
    start=code.index('\tm_SpinLock.Acquire ();',end);end=code.index('\n\tif (   (m_uchEndpointType',start);queue=code[start:end]
    start=code.index('void CXHCIEndpoint::TransferEvent (');end=code.index('\nboolean CXHCIEndpoint::ResetFromHalted',start);event=code[start:end]
    test=path/'blocking-race.cpp';test.write_text('''#include <cassert>
#include <cstdint>
using boolean=bool;using u8=uint8_t;using u32=uint32_t;using uintptr=uintptr_t;constexpr bool TRUE=true,FALSE=false;constexpr unsigned HZ=1,USB_TIMEOUT_NONE=0;const char* From="test";constexpr int LogDebug=0,LogWarning=1;constexpr unsigned XHCI_TRB_COMPLETION_CODE_SHORT_PACKET=2,XHCI_TRB_COMPLETION_CODE_RING_UNDERRUN=3,XHCI_TRB_COMPLETION_CODE_RING_OVERRUN=4,EndpointTypeIsochronous=5;
#define XHCI_TRB_SUCCESS(x) ((x)==1)
struct Endpoint{unsigned GetNumber(){return 1;}unsigned GetType(){return 0;}};
bool* active_lock=nullptr;bool expect_locked=true;
struct CUSBRequest{Endpoint ep;bool status=false;unsigned result=99,callbacks=0;void SetCompletionRoutine(void(*)(CUSBRequest*,void*,void*),void*,void*){}Endpoint* GetEndpoint(){return &ep;}void* GetBuffer(){if(active_lock)assert(*active_lock==expect_locked);return nullptr;}u32 GetBufLen(){return 10;}void SetResultLen(unsigned v){result=v;}void SetStatus(unsigned v){status=v;}bool GetStatus(){return status;}void CallCompletionRoutine(){++callbacks;}};
struct CTimer{unsigned ticks=0;static CTimer* Get(){static CTimer timer;return &timer;}unsigned GetTicks(){return ticks++;}};void (*warning_hook)()=nullptr;
struct CLogger{static CLogger* Get(){static CLogger logger;return &logger;}void Write(const char*,int,const char*,unsigned){if(warning_hook){auto hook=warning_hook;warning_hook=nullptr;hook();}}void Write(const char*,int,const char*,unsigned,unsigned){}};
void DataMemBarrier(){}void CleanAndInvalidateDataCacheRange(uintptr,unsigned){}struct Lock{bool held=false;void Acquire(){assert(!held);held=true;}void Release(){assert(held);held=false;}};
struct CXHCIEndpoint{Lock m_SpinLock;bool m_bTransferCompleted=true,m_bGrooveTimedOut=false,reject=false;CUSBRequest* m_pURB[2]={};CUSBRequest* m_pGrooveBlocking=nullptr;unsigned m_uchEndpointID=1;static void CompletionRoutine(CUSBRequest*,void*,void*){}boolean Transfer(CUSBRequest*,unsigned);void TransferEvent(u8,u32);boolean TransferAsync(CUSBRequest* pURB,unsigned){if(reject)return false;
'''+queue+'''
 return true;}};
'''+blocking+'\n'+event+'''
CXHCIEndpoint* current=nullptr;void complete_now(){current->TransferEvent(1,0);}
int main(){CUSBRequest a,b;CXHCIEndpoint e;current=&e;active_lock=&e.m_SpinLock.held;warning_hook=complete_now;assert(e.Transfer(&a,0));assert(a.status && a.result==10 && a.callbacks==0 && !e.m_bGrooveTimedOut && !e.m_pGrooveBlocking && !e.m_pURB[0]);
 CXHCIEndpoint timeout;assert(!timeout.Transfer(&b,0));assert(timeout.m_bGrooveTimedOut && !timeout.m_pURB[0] && !timeout.m_pGrooveBlocking);timeout.TransferEvent(1,0);assert(!b.status && !timeout.TransferAsync(&a,0));
 CXHCIEndpoint occupied;active_lock=&occupied.m_SpinLock.held;expect_locked=false;occupied.m_pURB[0]=&a;assert(!occupied.Transfer(&b,0) && occupied.m_pURB[0]==&a);occupied.TransferEvent(1,0);assert(a.callbacks==1 && !occupied.m_pURB[0]);
 CXHCIEndpoint rejected;rejected.reject=true;assert(!rejected.Transfer(&b,0) && rejected.m_bTransferCompleted && !rejected.m_pGrooveBlocking);
}
''')
    exe=path/'blocking-race';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle xHCI blocking completion and timeout arbitrate the same request under lock; physical behavior unverified')

# Exercise the real MK3 audio adapter with actual PCM helpers and Circle's
# wire-alignment statement; this is not a physical USB transfer test.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp);adapter=(root/'firmware/mk3_audio.cpp').read_text();start=adapter.index('unsigned CMk3Audio::GetChunk(s16*');methods=adapter[start:adapter.index('\nvoid CMk3Audio::TracePhases',start)];helpers=adapter[adapter.index('static void ProbeMaximum'):adapter.index('void CMk3Audio::ResetProbeStats')]
    sound=pcm_sound_fixture;start=sound.index('\tif (m_nSubframeSize == 4)',sound.index('boolean CUSBSoundBaseDevice::SendChunk'));end=sound.index('\n\t// The device can disappear',start);align=sound[start:end]
    test=path/'mk3-audio.cpp';test.write_text('''#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include "'''+str(root/'src/pcm_output.h')+'''"
#include "'''+str(root/'src/usb_pcm_convert.h')+'''"
#include "'''+str(root/'src/audio_phase_timing.h')+'''"
#include "'''+str(root/'src/usb_progress.h')+'''"
using u8=uint8_t;using u32=uint32_t;using s16=int16_t;
struct CTimer{static unsigned GetClockTicks(){return 1;}};
struct Engine{unsigned calls=0,frames=0,received=0;float left=0,right=0;void render_ports(float* main,float* cue,void*,unsigned n){++calls;frames+=n;for(unsigned i=0;i<n;++i){main[2*i]=-1;main[2*i+1]=0.5f;cue[2*i]=0.25f;cue[2*i+1]=-0.5f;}}void receive_audio(float a,float b){++received;left=a;right=b;}};
struct CMk3Audio{struct Gaps{void record(unsigned){}} m_Gaps;groove::AudioPhaseTiming m_PhaseTiming;Engine m_Engine;struct Slot{void read(float*,float*,unsigned){assert(false);}};bool m_WorkerStarted=false;Slot* m_RenderSlot=nullptr;unsigned channels=4,rxchannels=2,slot=4,bad=0,m_BadChunks=0;u32 m_MainPeak=0,m_PhonesPeak=0,m_InputRightPeakL=0,m_InputRightPeakR=0,m_ProbeEnabled=0;unsigned GetHWTXChannels(){return channels;}unsigned GetHWRXChannels(){return rxchannels;}unsigned GetUSBSubslotBytes(){return slot;}void Record(unsigned,unsigned,bool invalid){bad+=invalid;}unsigned GetChunk(s16*,unsigned);unsigned GetChunk(u32*,unsigned);void PutChunk(const s16*,unsigned);void PutChunk(const u32*,unsigned);};
'''+helpers+methods+'''
void wire_align(u32* buffer,unsigned nChunkSize,unsigned m_nSubframeSize){u8* m_pTXBuffer[1]={reinterpret_cast<u8*>(buffer)};unsigned m_nTXCurrentBuffer=0;
'''+align+'''
}
int main(){CMk3Audio a;alignas(4) u32 words[260]{};assert(a.GetChunk(words,260)==260 && a.m_Engine.calls==2 && a.m_Engine.frames==65);wire_align(words,260,4);const u8* b=reinterpret_cast<const u8*>(words);const u8 expected[]={0x00,0x01,0x00,0x80,0x00,0xff,0xff,0x3f,0x00,0xff,0xff,0x1f,0x00,0x01,0x00,0xc0};assert(!std::memcmp(b,expected,sizeof expected));
 a.slot=3;std::memset(words,0,sizeof words);assert(a.GetChunk(words,4)==4);const u8 packed[]={1,0,0x80,0xff,0xff,0x3f,0xff,0xff,0x1f,1,0,0xc0};assert(!std::memcmp(words,packed,sizeof packed));
 a.slot=4;alignas(4) u8 input[8]={0,0,0x80,0xff,0,0,0x40,0};assert(groove::usb_pcm24_capture_inplace(input,8,4,4,sizeof input));a.PutChunk(reinterpret_cast<u32*>(input),2);assert(a.m_Engine.received==1 && a.m_Engine.left==-1.0f && a.m_Engine.right==0.5f);
 s16 out[4]{};assert(a.GetChunk(out,4)==4 && out[0]==-32767 && out[1]==16383 && out[2]==8191 && out[3]==-16383);s16 in[2]={-32768,16384};a.PutChunk(in,2);assert(a.m_Engine.received==2 && a.m_Engine.left==-1 && a.m_Engine.right==0.5f);
 a.channels=3;assert(a.GetChunk(out,4)==4 && a.bad==1 && out[0]==0 && out[3]==0);
}
''')
    exe=path/'mk3-audio';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
print('MK3 audio adapter: four-channel Main/Cue PCM, 16/24-bit wire alignment, 65-frame render and stereo input host-tested')

# Exercise the actual request registration and rejection paths, including a
# completion and a later enqueue between the failed request's two lock uses.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    for name in ('include/circle/usb/xhciendpoint.h','lib/usb/xhciendpoint.cpp'):
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for patch in ('patch-circle-xhci-timeout.py','patch-circle-xhci-blocking-race.py','patch-circle-xhci-async-critical.py','patch-circle-xhci-queue.py'):
        cmd=['python3',str(root/'scripts'/patch),str(path)];subprocess.run(cmd,check=True);subprocess.run(cmd,check=True)
    code=(path/'lib/usb/xhciendpoint.cpp').read_text();start=code.index('boolean CXHCIEndpoint::TransferAsync');qstart=code.index('\tm_SpinLock.Acquire ();',start);qend=code.index('\n\tif (   (m_uchEndpointType',qstart);queue=code[qstart:qend];start=code.index('EnqueueError:',qend);end=code.index('\nvoid CXHCIEndpoint::TransferEvent',start);failure=code[start:end]
    test=path/'queue.cpp';test.write_text('''#include <cassert>
using boolean=bool;constexpr bool TRUE=true,FALSE=false;struct CUSBRequest{};struct Lock{bool held=false;void Acquire(){assert(!held);held=true;}void Release(){assert(held);held=false;}};
struct CXHCIEndpoint{Lock m_SpinLock;bool m_bGrooveTimedOut=false;CUSBRequest* m_pGrooveBlocking=nullptr;CUSBRequest* m_pURB[2]={};void(*interleave)(CXHCIEndpoint*)=nullptr;boolean submit(CUSBRequest* pURB){
'''+queue+'''
 if(interleave){interleave(this);}
 goto EnqueueError;
'''+failure+'''
};
CUSBRequest *first=nullptr,*second=nullptr,*third=nullptr;
void complete_then_enqueue(CXHCIEndpoint* e){assert(e->m_pURB[0]==first && e->m_pURB[1]==second);e->m_pURB[0]=e->m_pURB[1];e->m_pURB[1]=third;}
int main(){CUSBRequest a,b,c;first=&a;second=&b;third=&c;CXHCIEndpoint full;full.m_pURB[0]=&a;full.m_pURB[1]=&b;assert(!full.submit(&c) && full.m_pURB[0]==&a && full.m_pURB[1]==&b && !full.m_SpinLock.held);
 CXHCIEndpoint tail;tail.m_pURB[0]=&a;assert(!tail.submit(&b) && tail.m_pURB[0]==&a && !tail.m_pURB[1]);
 CXHCIEndpoint moved;moved.m_pURB[0]=&a;moved.interleave=complete_then_enqueue;assert(!moved.submit(&b) && moved.m_pURB[0]==&c && !moved.m_pURB[1]);
 CXHCIEndpoint empty;assert(!empty.submit(&a) && !empty.m_pURB[0] && !empty.m_pURB[1]);}
''')
    exe=path/'queue';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle xHCI full queue is rejected and failed request removal follows identity after queue shifts')

# Pinned ring cycle-bit preflight: no part of a multi-TRB transfer should be
# published when any required slot is unavailable. This harness does not model
# hardware consumption or DMA cancellation.
with tempfile.TemporaryDirectory() as temp:
    path=pathlib.Path(temp)
    names=('include/circle/usb/xhciring.h','lib/usb/xhciring.cpp','include/circle/usb/xhciendpoint.h','lib/usb/xhciendpoint.cpp')
    for name in names:
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(subprocess.check_output(['git','-C',str(circle),'show','HEAD:'+name]))
    for patch in ('patch-circle-xhci-timeout.py','patch-circle-xhci-blocking-race.py','patch-circle-xhci-async-critical.py','patch-circle-xhci-queue.py','patch-circle-xhci-preflight.py','patch-circle-xhci-cache-before-lock.py'):
        cmd=['python3',str(root/'scripts'/patch),str(path)];subprocess.run(cmd,check=True);subprocess.run(cmd,check=True)
    ring=(path/'lib/usb/xhciring.cpp').read_text();start=ring.index('boolean CXHCIRing::CanEnqueueTransfer');end=ring.index('\nTXHCITRB *CXHCIRing::IncrementDequeue',start);method=ring[start:end]
    endpoint=(path/'lib/usb/xhciendpoint.cpp').read_text();start=endpoint.index('\tvoid *pBuffer = pURB->GetBuffer ();',endpoint.index('boolean CXHCIEndpoint::TransferAsync'));end=endpoint.index('\n\tif (   (m_uchEndpointType',start);admission=endpoint[start:end]
    test=path/'preflight.cpp';test.write_text('''#include <cassert>
#include <cstdint>
using boolean=bool;using u32=uint32_t;using uintptr=uintptr_t;constexpr uintptr MEM_KERNEL_END=0;constexpr bool TRUE=true,FALSE=false;constexpr unsigned XHCI_TRB_CONTROL_C=1,XHCI_LINK_TRB_CONTROL_TC=2,XHCIRingTypeTransfer=0;bool* cache_lock=nullptr;unsigned cache_calls=0;void CleanAndInvalidateDataCacheRange(uintptr,unsigned){assert(cache_lock && !*cache_lock);++cache_calls;}
struct TXHCITRB{u32 Control=0;};struct CXHCIRing{TXHCITRB* m_pFirstTRB;unsigned m_Type=0,m_nTRBCount=16,m_nEnqueueIndex=0;u32 m_nCycleState=1;boolean CanEnqueueTransfer(unsigned)const;};
'''+method+'''
struct CUSBRequest{unsigned length=0,packets=1;char buffer=0;void* GetBuffer(){return &buffer;}u32 GetBufLen(){return length;}unsigned GetNumIsoPackets(){return packets;}};
struct Lock{bool held=false;void Acquire(){assert(!held);held=true;}void Release(){assert(held);held=false;}};
struct CXHCIEndpoint{Lock m_EnqueueLock,m_SpinLock;CXHCIRing* m_pTransferRing;unsigned m_uchEndpointType=4;bool m_bGrooveTimedOut=false;CUSBRequest* m_pGrooveBlocking=nullptr;CUSBRequest* m_pURB[2]={};unsigned published=0;boolean submit(CUSBRequest* pURB){cache_lock=&m_EnqueueLock.held;
'''+admission+'''
 (void)pBuffer;assert(m_EnqueueLock.held);++published;m_EnqueueLock.Release();return TRUE;}};
int main(){TXHCITRB trb[16]{};trb[15].Control=2;CXHCIRing ring{trb};assert(ring.CanEnqueueTransfer(3));trb[2].Control=1;assert(!ring.CanEnqueueTransfer(3));assert(!ring.CanEnqueueTransfer(16) && !ring.CanEnqueueTransfer(0));CUSBRequest control;control.length=1;CXHCIEndpoint ep{{},{},&ring};assert(!ep.submit(&control) && ep.published==0 && !ep.m_pURB[0] && !ep.m_EnqueueLock.held);trb[2].Control=0;assert(ep.submit(&control) && ep.published==1 && ep.m_pURB[0]==&control && !ep.m_EnqueueLock.held);
 ring.m_nEnqueueIndex=14;trb[0].Control=0;assert(!ring.CanEnqueueTransfer(2));trb[0].Control=1;assert(ring.CanEnqueueTransfer(2));
 CXHCIEndpoint iso{{},{},&ring};iso.m_uchEndpointType=1;CUSBRequest audio;audio.packets=2;unsigned before_zero=cache_calls;assert(iso.submit(&audio) && iso.published==1 && cache_calls==before_zero);ring.m_nEnqueueIndex=0;trb[1].Control=1;CXHCIEndpoint full{{},{},&ring};full.m_uchEndpointType=1;assert(!full.submit(&audio) && !full.published && !full.m_pURB[0]);trb[0].Control=0;trb[1].Control=0;CXHCIEndpoint slots{{},{},&ring};CUSBRequest a,b;slots.m_pURB[0]=&a;slots.m_pURB[1]=&b;assert(!slots.submit(&audio) && slots.m_pURB[0]==&a && slots.m_pURB[1]==&b && !slots.m_SpinLock.held && !slots.m_EnqueueLock.held);CXHCIEndpoint fenced{{},{},&ring};fenced.m_bGrooveTimedOut=true;assert(!fenced.submit(&audio) && !fenced.m_pURB[0] && !fenced.m_SpinLock.held && !fenced.m_EnqueueLock.held);}
''')
    exe=path/'preflight';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',str(test),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Circle xHCI whole-transfer cycle-bit preflight rejects insufficient slots before queue admission, including ring wrap')
