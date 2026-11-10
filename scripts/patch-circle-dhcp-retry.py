#!/usr/bin/env python3
"""Keep DHCP retries bounded while exposing whether responses reach the client."""
from pathlib import Path
import sys
p = Path(sys.argv[1]) / 'lib/net/dhcpclient.cpp'
s = p.read_text()
marker = 'GROOVE_DHCP_RETRY_DIAGNOSTICS'
if marker in s:
    print('Circle DHCP retry diagnostics already applied')
    raise SystemExit(0)
changes = [
    ('#define RESTART_DELAY\t\t60\t\t// seconds', '#define RESTART_DELAY\t\t5\t\t// seconds; GROOVE_DHCP_RETRY_DIAGNOSTICS'),
    ('static const char FromDHCPClient[] = "dhcp";', 'static const char FromDHCPClient[] = "dhcp";\n// Circle has one network subsystem and one DHCP task. These counters are task-owned.\nstatic unsigned s_GrooveDHCPDatagrams = 0, s_GrooveDHCPShort = 0, s_GrooveDHCPInvalid = 0;'),
    ('boolean CDHCPClient::SendAndReceive (boolean bRequest, u32 nCIAddr)\n{\n', 'boolean CDHCPClient::SendAndReceive (boolean bRequest, u32 nCIAddr)\n{\n\ts_GrooveDHCPDatagrams = s_GrooveDHCPShort = s_GrooveDHCPInvalid = 0;\n'),
    ('\t\tCLogger::Get ()->Write (FromDHCPClient, LogWarning, "No response from server. Retrying.");', '\t\tCLogger::Get ()->Write (FromDHCPClient, LogWarning,\n\t\t\t"No usable %s; received %u short %u invalid %u; retrying",\n\t\t\tbRequest ? "ACK/NAK" : "OFFER", s_GrooveDHCPDatagrams,\n\t\t\ts_GrooveDHCPShort, s_GrooveDHCPInvalid);'),
    ('\tint nMessageSize = m_Socket.Receive (Buffer, sizeof Buffer, MSG_DONTWAIT);\n\tif (nMessageSize < (int) DHCP_HEADER_SIZE)\n\t{\n\t\treturn FALSE;\n\t}', '\tint nMessageSize = m_Socket.Receive (Buffer, sizeof Buffer, MSG_DONTWAIT);\n\tif (nMessageSize > 0) ++s_GrooveDHCPDatagrams;\n\tif (nMessageSize < (int) DHCP_HEADER_SIZE)\n\t{\n\t\tif (nMessageSize > 0) ++s_GrooveDHCPShort;\n\t\treturn FALSE;\n\t}'),
    ('\t    || pMessage->MagicCookie != DHCP_MAGIC_COOKIE)\n\t{\n\t\treturn FALSE;\n\t}', '\t    || pMessage->MagicCookie != DHCP_MAGIC_COOKIE)\n\t{\n\t\t++s_GrooveDHCPInvalid;\n\t\treturn FALSE;\n\t}'),
    ('\tif (CHAddr != *pOwnMACAddress)\n\t{\n\t\treturn FALSE;\n\t}', '\tif (CHAddr != *pOwnMACAddress)\n\t{\n\t\t++s_GrooveDHCPInvalid;\n\t\treturn FALSE;\n\t}'),
]
for old, new in changes:
    if s.count(old) != 1:
        raise SystemExit(f'Unexpected Circle DHCP source: {old[:70]!r}')
    s = s.replace(old, new, 1)
p.write_text(s)
print('Circle DHCP retry and receive diagnostics patched')
