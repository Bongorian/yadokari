#include "file_dhcp.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <unistd.h>
using namespace groove;
int main(){
 FileDhcp dhcp;uint8_t input[300]{},out[320]{};input[0]=input[1]=1;input[2]=6;std::memcpy(input+236,"\x63\x82\x53\x63",4);input[240]=53;input[241]=1;input[242]=1;input[243]=255;
 auto request=[&](unsigned mac,unsigned type,uint64_t time){input[28]=uint8_t(mac);input[242]=uint8_t(type);return dhcp.reply(input,sizeof input,out,sizeof out,time);};
 for(unsigned i=1;i<=8;++i){assert(request(i,1,0)==300 && out[19]==i+1);}assert(!request(9,1,29999999));assert(request(9,1,30000000)==300 && out[19]==2);assert(request(9,3,31000000)==300);
 // Repeated discovers may refresh an offer but cannot shorten a committed lease.
 assert(request(9,1,32000000)==300);for(unsigned i=10;i<17;++i)assert(request(i,3,32000000)==300);assert(!request(17,1,63000000));assert(request(9,7,64000000)==0);assert(request(17,1,64000000)==300 && out[19]==2);
 assert(request(18,1,633000000)==300); // committed leases have now expired too
 // A NAK must not consume an otherwise available slot.
 FileDhcp nak;input[243]=50;input[244]=4;std::memcpy(input+245,"\xc0\xa8\x4c\xfe",4);input[249]=255;input[242]=3;input[28]=30;assert(nak.reply(input,sizeof input,out,sizeof out,0)==300 && out[242]==6);input[243]=255;for(unsigned i=1;i<=8;++i){input[28]=uint8_t(i);input[242]=3;assert(nak.reply(input,sizeof input,out,sizeof out,0)==300 && out[19]==i+1);}
 auto old=std::filesystem::current_path();char root[]="/tmp/wave2-files-XXXXXX";assert(mkdtemp(root) && chdir(root)==0);DesktopFiles development,volume(true);assert(!development.browser_file_management_allowed() && volume.browser_file_management_allowed());
 assert(volume.directory("PROJECTS"));assert(volume.write("PROJECTS/first.mgxs",0,"first",5,true));FileEntry entries[2];unsigned count=0;assert(volume.list("PROJECTS",entries,1)==1 && !std::strcmp(entries[0].name,"first.mgxs"));assert(volume.list_checked("PROJECTS",entries,1,count) && count==1);
 // Exact-full browsing must not inspect an extra entry. Checked enumeration
 // must distinguish truncation and include hidden transfer files.
 assert(volume.write("PROJECTS/second.mgxs",0,"second",6,true));assert(volume.list("PROJECTS",entries,1)==1);assert(!volume.list_checked("PROJECTS",entries,1,count) && count==1);assert(volume.list_checked("PROJECTS",entries,2,count) && count==2);
 assert(volume.write("PROJECTS/.MGX-TRANSFER.tmp",0,"owned",5,true));assert(volume.list("PROJECTS",entries,2)==2);assert(!volume.list_checked("PROJECTS",entries,2,count) && count==2);
 assert(volume.list("../",entries,2)==0 && volume.list(root,entries,2)==0 && !volume.list_checked("../",entries,2,count));assert(symlink("/tmp","escape")==0);assert(!volume.list("escape",entries,2) && !volume.list_checked("escape",entries,2,count));
 assert(volume.list("PROJECTS",nullptr,0)==0);assert(!volume.list_checked("PROJECTS",nullptr,0,count) && count==0);std::filesystem::current_path(old);std::filesystem::remove_all(root);
 std::puts("Wave2 edges: DHCP offer/lease expiry, renew/release/NAK reuse; explicit DATA management, bounded listing/truncation/hidden files and path/symlink boundary passed");
}
