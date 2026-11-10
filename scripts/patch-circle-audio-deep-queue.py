#!/usr/bin/env python3
"""Provide eight serialized xHCI requests and eight implicit-audio slots."""
from pathlib import Path
import sys
root=Path(sys.argv[1]); repo=Path(__file__).resolve().parent.parent
helper=root/'include/circle/usb/groove_request_fifo.h'
content=(repo/'src/usb_request_fifo.h').read_text()
if not helper.exists() or helper.read_text()!=content: helper.write_text(content)
eh=root/'include/circle/usb/xhciendpoint.h';ep=root/'lib/usb/xhciendpoint.cpp'
sh=root/'include/circle/sound/usbsoundbasedevice.h';sp=root/'lib/sound/usbsoundbasedevice.cpp'
h=eh.read_text();e=ep.read_text();a=sh.read_text();s=sp.read_text()
marker='GROOVE_XHCI_DEEP_QUEUE'
if marker in e:
 if marker not in h or 'GROOVE_AUDIO_DEEP_QUEUE' not in s: raise SystemExit('Incomplete deep queue patch')
 raise SystemExit(0)
def once(text,old,new):
 if text.count(old)!=1: raise SystemExit('Unexpected deep queue anchor: '+repr(old[:100])+f' count={text.count(old)}')
 return text.replace(old,new,1)
h=once(h,'#include <circle/usb/xhci.h>','#include <circle/usb/xhci.h>\n#include <circle/usb/groove_request_fifo.h>')
h=once(h,'\tCUSBRequest\t*m_pURB[2];','\tgroove::UsbRequestFifo<CUSBRequest, 8> m_Requests; // GROOVE_XHCI_DEEP_QUEUE')
if e.count('\tm_pURB {0, 0},')!=2: raise SystemExit('Unexpected xHCI constructor anchors')
e=e.replace('\tm_pURB {0, 0},\n','')
e=once(e,'m_bGrooveTimedOut || m_pGrooveBlocking || m_pURB[0] || m_pURB[1]','m_bGrooveTimedOut || m_pGrooveBlocking || !m_Requests.empty ()')
e=once(e,'assert (m_pGrooveBlocking == pURB && m_pURB[0] == pURB && !m_pURB[1]);\n\t\t\tm_pURB[0] = 0;','assert (m_pGrooveBlocking == pURB && m_Requests.front () == pURB && m_Requests.size () == 1);\n\t\t\tm_Requests.erase (pURB);')
start=e.index('\tif (m_pURB[0] == 0)',e.index('boolean CXHCIEndpoint::TransferAsync'))
end=e.index('\tm_SpinLock.Release ();',e.index('\t\tm_pURB[1] = pURB;',start))
old=e[start:end]
new='''\t// GROOVE_XHCI_DEEP_QUEUE: FIFO slots match endpoint completion order.
\tif (!m_Requests.push (pURB))
\t{
\t\tm_SpinLock.Release ();
\t\tm_EnqueueLock.Release ();
\t\treturn FALSE;
\t}
'''
e=e[:start]+new+e[end:]
start=e.index('\t// GROOVE_XHCI_QUEUE_IDENTITY:',e.index('EnqueueError:'));end=e.index('\tm_SpinLock.Release ();',start)
e=e[:start]+'''\t// GROOVE_XHCI_QUEUE_IDENTITY: remove only the rejected request.
\tm_Requests.erase (pURB);
'''+e[end:]
e=once(e,'CUSBRequest *pURB = m_pURB[0];','CUSBRequest *pURB = m_Requests.front ();')
e=once(e,'\tm_pURB[0] = m_pURB[1];\n\tm_pURB[1] = 0;','\tassert (m_Requests.front () == pURB);\n\tm_Requests.erase (pURB);')
if 'm_pURB[' in e: raise SystemExit('Unhandled xHCI request array access')
a=once(a,'u8 *m_pTXBuffer[2];','u8 *m_pTXBuffer[8]; // GROOVE_AUDIO_DEEP_QUEUE')
a=once(a,'unsigned m_nTXExpected[2];','unsigned m_nTXExpected[8];')
a=once(a,'u8 *m_pRXBuffer[2];','u8 *m_pRXBuffer[8];')
s=once(s,'\tm_pTXBuffer {nullptr, nullptr},','\tm_pTXBuffer {},')
s=once(s,'\tm_pRXBuffer {nullptr, nullptr},','\tm_pRXBuffer {},')
cleanup='''\tdelete [] m_pTXBuffer[0];
\tdelete [] m_pTXBuffer[1];
\tm_pTXBuffer[0] = nullptr;
\tm_pTXBuffer[1] = nullptr;

\tdelete [] m_pRXBuffer[0];
\tdelete [] m_pRXBuffer[1];
\tm_pRXBuffer[0] = nullptr;
\tm_pRXBuffer[1] = nullptr;'''
if s.count(cleanup)!=3: raise SystemExit('Unexpected audio cleanup anchors')
s=s.replace(cleanup,'''\tfor (unsigned i = 0; i < 8; ++i)
\t{
\t\tdelete [] m_pTXBuffer[i]; m_pTXBuffer[i] = nullptr;
\t\tdelete [] m_pRXBuffer[i]; m_pRXBuffer[i] = nullptr;
\t}''')
s=once(s,'''\t\t\tassert (!m_pTXBuffer[0]);
\t\t\tassert (!m_pTXBuffer[1]);
\t\t\tm_pTXBuffer[0] = new u8[m_nTXChunkSizeBytes * 2];
\t\t\tm_pTXBuffer[1] = new u8[m_nTXChunkSizeBytes * 2];''','''\t\t\tfor (unsigned i = 0; i < (m_bImplicitFeedback ? 8u : 2u); ++i)
\t\t\t{
\t\t\t\tassert (!m_pTXBuffer[i]);
\t\t\t\tm_pTXBuffer[i] = new u8[m_nTXChunkSizeBytes * 2];
\t\t\t}''')
s=once(s,'''\t\t\tassert (!m_pRXBuffer[0]);
\t\t\tassert (!m_pRXBuffer[1]);
\t\t\t// Up to 2x nominal USB bytes; expanding 3-byte samples to 4
\t\t\t// requires at most 8/3x the nominal capacity.
\t\t\tm_pRXBuffer[0] = new u8[m_nRXChunkSizeBytes * 3];
\t\t\tm_pRXBuffer[1] = new u8[m_nRXChunkSizeBytes * 3];''','''\t\t\tfor (unsigned i = 0; i < (m_bImplicitFeedback ? 8u : 2u); ++i)
\t\t\t{
\t\t\t\tassert (!m_pRXBuffer[i]);
\t\t\t\tm_pRXBuffer[i] = new u8[m_nRXChunkSizeBytes * 3];
\t\t\t}''')
s=once(s,'\t\tm_nTXExpected[0] = m_nTXExpected[1] = 0;','\t\tfor (unsigned i = 0; i < 8; ++i) m_nTXExpected[i] = 0; // GROOVE_AUDIO_DEEP_QUEUE')
s=once(s,'m_ImplicitCadence.reset (2);','m_ImplicitCadence.reset (m_bImplicitFeedback ? 8 : 2);')
s=once(s,'m_RXLengths.reset ();','m_RXLengths.reset (m_bImplicitFeedback ? 8 : 2);')
old='''\t\tif (   !ReceiveChunk (TRUE)
#if RASPPI >= 4
\t\t    || !ReceiveChunk (TRUE)
#endif
\t\t   )'''
new='''\t\tboolean bReceiveOK = TRUE;
#if RASPPI >= 4
\t\tconst unsigned nReceiveSlots = m_bImplicitFeedback ? 8 : 2;
#else
\t\tconst unsigned nReceiveSlots = 1;
#endif
\t\tfor (unsigned i = 0; i < nReceiveSlots && bReceiveOK; ++i)
\t\t\tbReceiveOK = ReceiveChunk (TRUE);
\t\tif (!bReceiveOK)'''
s=once(s,old,new)
s=once(s,'assert (m_nTXCurrentBuffer < 2);','assert (m_nTXCurrentBuffer < (m_bImplicitFeedback ? 8u : 2u));')
s=once(s,'m_nTXCurrentBuffer ^= 1;','m_nTXCurrentBuffer = (m_nTXCurrentBuffer + 1) % (m_bImplicitFeedback ? 8 : 2);')
s=once(s,'m_nTXCompletedBuffer ^= 1;','m_nTXCompletedBuffer = (m_nTXCompletedBuffer + 1) % (m_bImplicitFeedback ? 8 : 2);')
s=once(s,'assert (m_nRXCurrentBuffer < 2);','assert (m_nRXCurrentBuffer < (m_bImplicitFeedback ? 8u : 2u));')
s=once(s,'m_nRXCurrentBuffer ^= 1;','m_nRXCurrentBuffer = (m_nRXCurrentBuffer + 1) % (m_bImplicitFeedback ? 8 : 2);')
s=once(s,'assert (nRXCurrentBuffer < 2);','assert (nRXCurrentBuffer < (m_bImplicitFeedback ? 8u : 2u));')
for path,value in [(eh,h),(ep,e),(sh,a),(sp,s)]:path.write_text(value)
