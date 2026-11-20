"""Execute the production CDC channel / remote gate expressions from
firmware/update_runtime.cpp (CUpdateRuntime::Initialize) over their full
truth table.

Gadget enumeration must follow the channel decision alone (the opt-in
marker, or the Pi 5 default-on configuration), never update storage
readiness -- the gadget also serves read-only status on
boards whose update layout is not prepared. m_Remote, which gates every
mutating and diagnostic command, is the production predicate
remote_diagnostics_enabled: prepared storage AND the marker, on both
boards. The Pi 5 default-on enumerates the gadget but does not by
itself enable remote diagnostics.
"""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'firmware/update_runtime.cpp').read_text()
def rhs(anchor):
 a=s.index(anchor)+len(anchor);b=s.index(';',a);return s[a:b].strip()
channel_expr=rhs('const bool cdc_channel=')
remote_expr=rhs('m_Remote=')
# Take the diagnostics predicate itself from the implementation, so the
# truth table below judges the shipped formula, not a copy of it.
m=re.search(r'constexpr bool (remote_diagnostics_enabled\(bool prepared,bool marker\)\{return [^;]+;\})',s)
assert m, 'remote_diagnostics_enabled predicate not found in update_runtime.cpp'
predicate='bool '+m.group(1)
assert 'if(cdc_channel && !StartGadget())' in s, 'gadget start must be gated on cdc_channel alone'
runtime=(root/'firmware/update_runtime.h').read_text()
usb_poll=runtime.split('void PollUsbControl(',1)[1].split('void PollNetworkControl(',1)[0]
assert 'files->remote_ready(remote_ready())' in usb_poll
ready=re.search(r'bool remote_ready\(\)const\{return ([^;]+);\}', runtime).group(1)
source=r'''
#include <cassert>
bool storage_ready=false,cdc_marker=false,cdc_default_on=false;
PREDICATE
bool channel(){return CHANNEL_EXPR;}
bool remote(){bool cdc_channel=channel();(void)cdc_channel;return REMOTE_EXPR;}
bool file_ready(bool m_Remote,bool m_Ready,bool m_Identity,bool m_DiagnosticBusy){return READY_EXPR;}
int main(){
 for(int i=0;i<8;++i){
  storage_ready=i&1;cdc_marker=i&2;cdc_default_on=i&4;
  // Enumeration: the channel decision, independent of storage readiness.
  assert(channel()==(cdc_marker||cdc_default_on));
  // Remote gate: prepared storage AND the marker, on both boards
  // (the default-on configuration alone never enables diagnostics).
  assert(remote()==(storage_ready&&cdc_marker));
  for(int flags=0;flags<8;++flags){
   bool prepared=flags&1,identity=flags&2,busy=flags&4;
   assert(file_ready(remote(),prepared,identity,busy)==
          (storage_ready && cdc_marker && prepared && identity && !busy));
  }
 }
}
'''.replace('READY_EXPR',ready).replace('PREDICATE',predicate).replace('CHANNEL_EXPR',channel_expr).replace('REMOTE_EXPR',remote_expr)
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'cdc_gate.cpp';exe=Path(d)/'cdc_gate';cpp.write_text(source)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('CDC gate: enumeration follows the channel decision alone; remote diagnostics require prepared storage and the marker over all 8 input rows')
