#!/usr/bin/env python3
"""Preallocate hot USB requests. Applied last to the pinned Circle patch set."""
from pathlib import Path
import sys,shutil
root=Path(sys.argv[1]); marker='GROOVE_REQUEST_REUSE'
paths=['include/circle/usb/usbrequest.h','lib/usb/usbrequest.cpp','include/circle/usb/usbhiddevice.h','lib/usb/usbhiddevice.cpp','include/circle/usb/usbaudiostreaming.h','lib/usb/usbaudiostreaming.cpp']
text={p:(root/p).read_text() for p in paths}
pool_source=Path(__file__).resolve().parents[1]/'src/usb_request_pool.h'
pool_target=root/'include/circle/usb/groove_request_pool.h'
if not pool_target.exists() or pool_target.read_bytes()!=pool_source.read_bytes():shutil.copyfile(pool_source,pool_target)
if all(marker in s for s in text.values()):
 print('Circle fixed USB request reuse already applied');raise SystemExit(0)
if any(marker in s for s in text.values()):raise SystemExit('Incomplete request reuse patch')
def once(p,a,b):
 if text[p].count(a)!=1:raise SystemExit('Unexpected '+p+' anchor '+repr(a[:100]))
 text[p]=text[p].replace(a,b,1)
rh,rc,hh,hc,ah,ac=paths
once(rh,'\tCUSBEndpoint *GetEndpoint (void) const;', '\t// '+marker+': caller owns a completed, dequeued request.\n\tvoid Reuse (void *pBuffer, u32 nBufLen);\n\tCUSBEndpoint *GetEndpoint (void) const;')
once(rc,'CUSBRequest::~CUSBRequest (void)', '''// GROOVE_REQUEST_REUSE: endpoint stays bound; all per-submission state resets.
void CUSBRequest::Reuse (void *pBuffer, u32 nBufLen)
{
 assert (pBuffer || nBufLen == 0);
 m_pBuffer = pBuffer; m_nBufLen = nBufLen; m_pSetupData = 0;
 m_bStatus = 0; m_nResultLen = 0; m_USBError = USBErrorUnknown;
 m_nNumIsoPackets = 0; m_pCompletionRoutine = 0;
 m_pCompletionParam = 0; m_pCompletionContext = 0;
 m_bCompleteOnNAK = FALSE; m_bCompleteImmediately = FALSE;
}

CUSBRequest::~CUSBRequest (void)''')
# HID input owns one stable request. An orphan context retains DMA resources
# across removal; a late callback only retires it, never touches the old device.
once(hh,'\tboolean StartRequest (void);','''\tboolean StartRequest (void);
 // GROOVE_REQUEST_REUSE: transfer output endpoint ownership to the MK3 queue.
 CUSBEndpoint *TakeOutputEndpoint (void) { auto *p = m_pEndpointOut; m_pEndpointOut = 0; return p; }''')
once(hh,'\tunsigned m_nMaxReportSize;', ''' // GROOVE_REQUEST_REUSE: only configuration/task teardown allocates/frees.
 struct TPoll { CUSBHIDDevice *owner; CUSBEndpoint *endpoint; u8 *buffer;
  CUSBRequest *request; unsigned pending; TPoll *next; };
 TPoll *m_pPoll = nullptr;
 static TPoll *s_pRetired;
 static void ReapRetired (void);
 unsigned m_nMaxReportSize;''')
once(hc,'static const char FromUSBHID[] = "usbhid";','''static const char FromUSBHID[] = "usbhid";
// GROOVE_REQUEST_REUSE: task-side reclamation only.
CUSBHIDDevice::TPoll *CUSBHIDDevice::s_pRetired = nullptr;
void CUSBHIDDevice::ReapRetired (void)
{
 auto **link = &s_pRetired;
 while (*link) { auto *p = *link;
  if (__atomic_load_n (&p->pending, __ATOMIC_ACQUIRE)) { link = &p->next; continue; }
  *link = p->next; delete p->request; delete [] p->buffer; delete p->endpoint; delete p;
 }
}''')
once(hc,'\tdelete [] m_pReportBuffer;', ''' ReapRetired ();
 if (m_pPoll) {
  m_pPoll->owner = nullptr;
  m_pPoll->next = s_pRetired; s_pRetired = m_pPoll; m_pPoll = nullptr;
  m_pReportBuffer = nullptr; m_pReportEndpoint = nullptr;
  ReapRetired ();
 }
 delete [] m_pReportBuffer;''')
start=text[hc].index('boolean CUSBHIDDevice::ConfigureHID (');end=text[hc].index('\nboolean CUSBHIDDevice::SendToEndpointOut (',start)
p=text[hc][start:end];a='\treturn TRUE;';assert p.count(a)==1
p=p.replace(a,''' ReapRetired ();
 m_pPoll = new TPoll{this, m_pReportEndpoint, m_pReportBuffer,
  new CUSBRequest(m_pReportEndpoint, m_pReportBuffer, m_nMaxReportSize), 0, nullptr};
 return m_pPoll && m_pPoll->request;''');text[hc]=text[hc][:start]+p+text[hc][end:]
start=text[hc].index('boolean CUSBHIDDevice::StartRequest (');end=text[hc].index('\nvoid CUSBHIDDevice::CompletionRoutine (',start)
text[hc]=text[hc][:start]+'''boolean CUSBHIDDevice::StartRequest (void)
{
 // GROOVE_HID_POLL_STOPPED: rejected requests never remain pending.
 if (!m_pPoll || __atomic_load_n(&m_pPoll->pending, __ATOMIC_ACQUIRE)) return FALSE;
 auto *pURB = m_pPoll->request;
 pURB->Reuse(m_pReportBuffer, m_nMaxReportSize);
 pURB->SetCompletionRoutine(CompletionStub, nullptr, m_pPoll);
 __atomic_store_n(&m_pPoll->pending, 1u, __ATOMIC_RELEASE);
 boolean ok = GetHost()->SubmitAsyncRequest(pURB);
 if (!ok) __atomic_store_n(&m_pPoll->pending, 0u, __ATOMIC_RELEASE);
 return ok;
}
''' +text[hc][end:]
start=text[hc].index('void CUSBHIDDevice::CompletionRoutine (');end=text[hc].index('\nvoid CUSBHIDDevice::CompletionStub (',start)
p=text[hc][start:end].replace('\tdelete pURB;', '\t__atomic_store_n (&m_pPoll->pending, 0u, __ATOMIC_RELEASE);');text[hc]=text[hc][:start]+p+text[hc][end:]
start=text[hc].index('void CUSBHIDDevice::CompletionStub (');end=text[hc].find('\n}',start)+2
text[hc]=text[hc][:start]+'''void CUSBHIDDevice::CompletionStub (CUSBRequest *pURB, void *, void *pContext)
{
 auto *poll = static_cast<TPoll *>(pContext);
 if (poll->owner) poll->owner->CompletionRoutine(pURB);
 else __atomic_store_n(&poll->pending, 0u, __ATOMIC_RELEASE);
}'''+text[hc][end:]
# Audio: eight requests per stream match the existing eight-buffer cadence.
once(ah,'#include <circle/types.h>','#include <circle/types.h>\n#include <circle/usb/groove_request_pool.h>')
once(ah,'\tunsigned m_nBitResolution;', ''' // GROOVE_REQUEST_REUSE: fixed pool, release before client resubmission.
 struct TDataRequest { CUSBRequest *request=nullptr; TCompletionRoutine *routine=nullptr; void *param=nullptr; };
 groove::UsbRequestPool<TDataRequest,8> m_Requests;
 CUSBRequest *m_pSyncRequest=nullptr;
 unsigned m_nBitResolution;''')
once(ac,'\tdelete m_pEndpointSync;', ''' // GROOVE_REQUEST_REUSE: NotifyRemoved drained sound callbacks first.
 assert (!m_Requests.busy ());
 for (unsigned i=0;i<8;++i) delete m_Requests.at(i).request;
 delete m_pSyncRequest;
 delete m_pEndpointSync;''')
start=text[ac].index('boolean CUSBAudioStreamingDevice::Configure (');end=text[ac].index('\n',text[ac].index('\n}',start)+2)
p=text[ac][start:end];idx=p.rfind('\treturn TRUE;');assert idx>=0
p=p[:idx]+''' // GROOVE_REQUEST_REUSE: no allocation in Send/Receive/Completion.
 for (unsigned i=0;i<8;++i) m_Requests.at(i).request=new CUSBRequest(m_pEndpointData,nullptr,0);
 if (m_pEndpointSync) m_pSyncRequest=new CUSBRequest(m_pEndpointSync,nullptr,0);
'''+p[idx:];text[ac]=text[ac][:start]+p+text[ac][end:]
for buf in ('(void *) pBuffer','pBuffer'):
 a='\tCUSBRequest *pURB = new CUSBRequest (m_pEndpointData, '+buf+', nChunkSizeBytes);'
 b=''' auto *slot = m_Requests.acquire();
 if (!slot) return FALSE;
 CUSBRequest *pURB=slot->request;
 pURB->Reuse('''+buf+''',nChunkSizeBytes);
 slot->routine=pCompletionRoutine; slot->param=pParam;'''
 once(ac,a,b)
assert text[ac].count('pURB->SetCompletionRoutine (CompletionHandler, pParam, (void *) pCompletionRoutine);')==2
text[ac]=text[ac].replace('pURB->SetCompletionRoutine (CompletionHandler, pParam, (void *) pCompletionRoutine);','pURB->SetCompletionRoutine (CompletionHandler, slot, this);')
# Existing one-TRB rejected submission has no callback and releases its slot.
assert text[ac].count('\t\tdelete pURB;')==2
text[ac]=text[ac].replace('\t\tdelete pURB;','\t\tm_Requests.release(slot);')
once(ac,'''\t\tCUSBRequest *pURBSync = new CUSBRequest (m_pEndpointSync, &m_SyncEPBuffer,
\t\t\t\t\t\t\t usPacketSize);''','''  CUSBRequest *pURBSync = m_pSyncRequest;
  pURBSync->Reuse(&m_SyncEPBuffer, usPacketSize);''')
once(ac,'''\tdelete pURB;

\tTCompletionRoutine *pCompletionRoutine = (TCompletionRoutine *) pContext;
\tif (pCompletionRoutine)
\t{
\t\t(*pCompletionRoutine) (nBytesTransferred, pParam);
\t}''',''' auto *self=static_cast<CUSBAudioStreamingDevice *>(pContext);
 auto *slot=static_cast<TDataRequest *>(pParam);
 auto *routine=slot->routine; void *param=slot->param;
 self->m_Requests.release(slot);
 if (routine) (*routine)(nBytesTransferred,param);''')
# Sync request is private and reused only after m_bSyncEPActive becomes false.
start=text[ac].index('void CUSBAudioStreamingDevice::SyncCompletionHandler (');end=text[ac].index('\nvoid CUSBAudioStreamingDevice::UpdateChunkSize (',start)
p=text[ac][start:end];assert p.count('\tdelete pURB;')==1;p=p.replace('\tdelete pURB;','\t// Persistent feedback request; no destruction on completion.');text[ac]=text[ac][:start]+p+text[ac][end:]
for p,s in text.items():
 if marker not in s:raise SystemExit('Missing patch marker '+p)
 (root/p).write_text(s)
print('Circle requests reuse fixed storage; capture cadence unchanged')
