"""Execute pinned Circle FinishTransfer with a stale speculative DMA cache model."""
from pathlib import Path
import subprocess,tempfile,os,re
root=Path(__file__).resolve().parents[1];circle=Path(os.environ.get('CIRCLEHOME',root/'.deps/circle-build'))
original=subprocess.check_output(['git','-C',str(circle),'show','HEAD:lib/usb/gadget/dwusbgadgetendpoint.cpp'],text=True)
start=original.index('size_t CDWUSBGadgetEndpoint::FinishTransfer (void)');end=original.index('\nvoid CDWUSBGadgetEndpoint::InitTransfer',start)
def body(source):
 start=source.index('size_t CDWUSBGadgetEndpoint::FinishTransfer (void)');end=source.index('\nvoid CDWUSBGadgetEndpoint::InitTransfer',start);return source[start:end]
stub=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
using uintptr=uintptr_t;
#define AARCH 64
#define DWHCI_DEV_IN_EP_XFER_SIZ(n) (n)
#define DWHCI_DEV_OUT_EP_XFER_SIZ(n) (n)
#define DWHCI_DEV_EP0_XFER_SIZ_XFER_SIZ__MASK 0xffff
#define DWHCI_DEV_EP_XFER_SIZ_XFER_SIZ__MASK 0xffff
#define DWHCI_DEV_EP_XFER_SIZ_XFER_SIZ__SHIFT 0
static unsigned remaining=0,invalidations=0;static uint8_t memory[512],cached[512];static bool reset=false;
struct CDWHCIRegister {explicit CDWHCIRegister(unsigned){};unsigned Read(){return remaining;}};
void InvalidateDataCacheRange(uintptr address,size_t n){assert(!reset && address==uintptr(cached) && n<=512);++invalidations;std::memcpy(cached,memory,n);}
struct CDWUSBGadgetEndpoint {
 enum Mode{TransferDataIn,TransferDataOut,TransferSetup,TransferUnknown};Mode m_TransferMode=TransferUnknown;unsigned m_nEP=0;size_t m_nTransferLength=0;void* m_pTransferBuffer=nullptr;
 size_t FinishTransfer();void InitTransfer(){reset=true;m_TransferMode=TransferUnknown;m_pTransferBuffer=nullptr;}
};
'''
main=r'''
int main(){for(unsigned ep:{0u,2u})for(auto mode:{CDWUSBGadgetEndpoint::TransferDataIn,CDWUSBGadgetEndpoint::TransferDataOut,CDWUSBGadgetEndpoint::TransferSetup})for(unsigned length:{8u,24u,176u,512u}){
 for(unsigned i=0;i<512;++i){memory[i]=uint8_t(i*17+length);cached[i]=0xa5;}
 CDWUSBGadgetEndpoint endpoint;endpoint.m_TransferMode=mode;endpoint.m_nEP=ep;endpoint.m_nTransferLength=512;endpoint.m_pTransferBuffer=cached;remaining=512-length;invalidations=0;reset=false;
 assert(endpoint.FinishTransfer()==length && reset);bool receives=mode!=CDWUSBGadgetEndpoint::TransferDataIn;
 if(EXPECT_COHERENT && receives){assert(invalidations==1 && !std::memcmp(cached,memory,length));}else {assert(!invalidations && cached[0]==0xa5);}
 }std::puts(EXPECT_COHERENT?"PASS actual FinishTransfer: OUT/SETUP cache ownership restored before consumers, IN untouched":"PASS reproduced stale pre-DMA-only baseline");}
'''
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp);path=base/'lib/usb/gadget/dwusbgadgetendpoint.cpp';path.parent.mkdir(parents=True);path.write_text(original)
 subprocess.run(['python3',str(root/'scripts/patch-circle-gadget-rx-cache.py'),str(base)],check=True);patched=path.read_text();subprocess.run(['python3',str(root/'scripts/patch-circle-gadget-rx-cache.py'),str(base)],check=True);assert path.read_text()==patched
 for name,source,expect in [('before',original,0),('after',patched,1)]:
  cpp=base/(name+'.cpp');cpp.write_text('#include <initializer_list>\n'+stub+body(source)+main);binary=base/name
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-DEXPECT_COHERENT={expect}',str(cpp),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
print('Pinned source extracted; cache regression and patch idempotence passed')
