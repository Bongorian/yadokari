"""Approved PNG quantization and actual CircleDisplay runtime-version handoff."""
import pathlib, subprocess, tempfile
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(root/'tools/ui/boot_splash.py'),'--check'],check=True)
im=Image.open(root/'assets/boot/splash-yadokari-480x272.png')
assert im.size==(480,272) and im.mode=='RGB'
values=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
expected=b''.join(v.to_bytes(2,'little') for v in values)
# Reuse declarations only, then execute the production transfer/backend bodies.
script=(root/'tests/usb_hot_paths_test.py').read_text()
ns={'__file__':str(root/'tests/usb_hot_paths_test.py')}
exec(compile(script[:script.index('with tempfile.TemporaryDirectory() as directory:')],str(root/'tests/usb_hot_paths_test.py'),'exec'),ns)
common=ns['common'].replace('std::vector<CUSBRequest*> queued;', 'std::vector<CUSBRequest*> queued;std::vector<std::vector<u8>> wire;').replace('queued.push_back(r);return true;', 'queued.push_back(r);wire.emplace_back(static_cast<u8*>(r->buffer),static_cast<u8*>(r->buffer)+r->size);return true;')
code=r'''
#include <cstdio>
#include <array>
uint32_t GrooveFirmwareVersion(){return TEST_VERSION;}
using Image=std::array<std::vector<u16>,2>;
static void decode(Image& shown,const std::vector<u8>& bytes){
 unsigned side=bytes[2],x=(bytes[8]<<8)|bytes[9],y=(bytes[10]<<8)|bytes[11],w=(bytes[12]<<8)|bytes[13],h=(bytes[14]<<8)|bytes[15],at=16,pixel=0;
 assert(side<2 && x+w<=480 && y+h<=272 && bytes.size()<=groove::kDisplayPacketCapacity);
 while(pixel<w*h){assert(at+4<=bytes.size()-8);unsigned op=bytes[at],count=(unsigned(bytes[at+1])<<16)|(unsigned(bytes[at+2])<<8)|bytes[at+3];at+=4;assert(op<=1 && count && pixel+2*count<=w*h && at+(op?4:count*4)<=bytes.size()-8);
  for(unsigned n=0;n<count;++n){unsigned data=op?at:at+n*4;for(unsigned j=0;j<2;++j){shown[side][(y+pixel/w)*480+x+pixel%w]=u16((bytes[data+j*2]<<8)|bytes[data+j*2+1]);++pixel;}}
  at+=op?4:count*4;
 }
 assert(at+8==bytes.size() && bytes[at]==3 && bytes[at+4]==0x40);
}
static Image artwork(){Image image;for(unsigned side=0;side<2;++side){image[side].resize(480*272);assert(groove::boot_splash_shadow(side,image[side].data()));}return image;}
static void frame(CircleDisplay& screen,CMk3Display& device,Image& shown,const Image& wanted,unsigned& next,bool expect_error=false){
 unsigned errors=0;std::vector<u32> pixels[2];for(unsigned side=0;side<2;++side)for(auto p:wanted[side])pixels[side].push_back(groove::ui::expand565(p));screen.NewFrame();
 for(unsigned n=0;n<100 && !screen.FrameComplete();++n){
  while(next<device.host.queued.size()){decode(shown,device.host.wire[next]);device.host.queued[next++]->finish();}screen.Poll();
  for(unsigned side=0;side<2;++side)errors+=!screen.present(side,pixels[side].data(),480,272);
 }
 assert(screen.FrameComplete() && shown==wanted && (errors!=0)==expect_error);
}
static void versions(){
 struct Case{uint32_t raw;const char* text;};
 for(auto c:{Case{0,"v0"},Case{568,"v568"},Case{569,"v569"},Case{899,"v899"},Case{900,"v0.9.0"},Case{901,"v0.9.1"},Case{10000,"v1.0.0"},Case{123456,"v12.34.56"},Case{0xffffffffu,"v429496.72.95"}}){char text[24];groove::boot_splash_version(c.raw,text);assert(!std::strcmp(text,c.text));assert(groove::ui::text_width(text,groove::ui::FontRole::Small)<groove::kBootVersionRect.w);}
 auto image=artwork();std::vector<u8> packet(groove::kBootVersionPacketBytes,0x5a),before=packet;unsigned bytes=77;
 assert(!groove::boot_splash_version_packet(568,nullptr,packet.data(),packet.size(),bytes) && !bytes && packet==before);
 assert(!groove::boot_splash_version_packet(568,image[1].data(),nullptr,packet.size(),bytes));
 assert(!groove::boot_splash_version_packet(568,image[1].data(),packet.data(),packet.size()-1,bytes) && packet==before);
 assert(!groove::boot_splash_shadow(2,image[0].data()) && !groove::boot_splash_shadow(0,nullptr));
}
static Image normal(){
 auto original=artwork(),shown=original;CMk3Display device;CircleDisplay screen(static_cast<u8*>(palloc()));
 assert(!screen.BootSplashVersion(GrooveFirmwareVersion()));assert(screen.BootSplash(&device) && device.host.wire.size()==2);
 for(unsigned side=0;side<2;++side){unsigned bytes=0;const auto* p=groove::boot_splash_packet(side,bytes);assert(device.host.wire[side]==std::vector<u8>(p,p+bytes));}
 assert(!screen.BootSplashVersion(GrooveFirmwareVersion()));device.host.queued[0]->finish();assert(!screen.BootSplashVersion(GrooveFirmwareVersion()));
 auto* inflight=device.host.queued[1];assert(!std::memcmp(inflight->buffer,device.host.wire[1].data(),inflight->size));inflight->finish();
 assert(screen.BootSplashVersion(GrooveFirmwareVersion()) && device.host.wire.size()==3);
 assert(device.host.wire[2].size()==groove::kBootVersionPacketBytes && device.host.wire[2][2]==1);
 assert(screen.BootSplashVersion(GrooveFirmwareVersion()+1) && device.host.wire.size()==3); // One overlay per boot.
 decode(shown,device.host.wire[2]);unsigned changed=0,minx=480,maxx=0;
 for(unsigned i=0;i<480*272;++i){assert(!shown[0][i]);if(shown[1][i]!=original[1][i]){++changed;unsigned x=i%480,y=i/480;assert(x>=176 && x<304 && y>=244 && y<264);if(x<minx)minx=x;if(x>maxx)maxx=x;assert(shown[1][i]<=groove::ui::pack565(0xa8a8a8));}}
 assert(changed>30 && minx+maxx>=476 && minx+maxx<=482);
 device.host.queued[2]->finish();
 // Run's handoff call must reap the completed patch before the first PnP
 // invalidation. No explicit Poll is made during SD/project initialization.
 assert(screen.GetStats().completed==2);
 assert(screen.BootSplashVersion(GrooveFirmwareVersion()) && screen.GetStats().completed==3);
 assert(screen.BootSplashComplete());
 unsigned next=3;frame(screen,device,shown,shown,next);assert(device.host.wire.size()==3); // The committed shadow contains the caption.
 frame(screen,device,shown,original,next); // First real screen removes caption without stale pixels.
 return original;
}
static void faults(unsigned fault){
 auto original=artwork(),shown=original;CMk3Display device;CircleDisplay screen(static_cast<u8*>(palloc()));assert(screen.BootSplash(&device));device.host.queued[0]->finish();device.host.queued[1]->finish();assert(screen.BootSplashComplete());
 if(fault==0){device.host.reject_at=device.host.calls;assert(!screen.BootSplashVersion(568));unsigned next=2;frame(screen,device,shown,original,next);assert(device.host.wire.size()==2);return;}
 assert(screen.BootSplashVersion(568));if(fault==3)screen.Invalidate();device.host.queued[2]->finish(fault!=1,fault==2);screen.Poll();
 unsigned next=3;frame(screen,device,shown,original,next,fault==1 || fault==2); // Failed/short/stale ACK cannot commit caption pixels.
}
int main(){versions();auto original=normal();for(unsigned fault=0;fault<4;++fault)faults(fault);
 assert(fwrite(original[1].data(),2,original[1].size(),stdout)==original[1].size());
 // A second image, emitted by the real packet renderer, is useful for a native-size review.
 std::vector<u8> bytes(groove::kBootVersionPacketBytes);unsigned size=0;assert(groove::boot_splash_version_packet(GrooveFirmwareVersion(),original[1].data(),bytes.data(),bytes.size(),size));decode(original,bytes);
 assert(fwrite(original[1].data(),2,original[1].size(),stdout)==original[1].size());
}
'''
with tempfile.TemporaryDirectory() as d:
 d=pathlib.Path(d);p=d/'splash.cpp';p.write_text(common+ns['pipeline_headers']+ns['structs']+ns['stub']+ns['helpers']+ns['methods']+ns['display']+code)
 images=[]
 for version in [568,900]:
  exe=d/('splash-'+str(version))
  subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DTEST_VERSION='+str(version),'-I'+str(root/'src'),str(p),'-o',str(exe)],check=True)
  output=subprocess.check_output([str(exe)]);assert len(output)==2*len(expected) and output[:len(expected)]==expected
  images.append(output[len(expected):])
  preview=Image.new('RGB',(960,272));pixels=[]
  for at in range(0,len(expected),2):
   v=int.from_bytes(output[len(expected)+at:len(expected)+at+2],'little');r=v>>11;g=(v>>5)&63;b=v&31;pixels.append((r<<3|r>>2,g<<2|g>>4,b<<3|b>>2))
  right=Image.new('RGB',(480,272));right.putdata(pixels);preview.paste(right,(480,0))
  destination=root/'build/splash-yadokari';destination.mkdir(exist_ok=True);preview.save(destination/('runtime-'+str(version)+'.png'))
 assert images[0]!=images[1]
source=(root/'firmware/kernel.cpp').read_text();assert source.index('m_Display->BootSplash(screen)')<source.index('m_Display->BootSplashVersion(GrooveFirmwareVersion())')<source.index('m_EMMC.Initialize()')
assert 'CircleDisplay display(' not in source and 'auto& display=*m_Display;' in source
assert source.count('BootSplashVersion(GrooveFirmwareVersion())')==2
print('PASS splash: approved native PNG exact RGB565, black left, actual Barlow runtime caption at two build versions, bounded real USB ACK/shadow/failure/handoff paths; first-frame timing and shared backend preserved')
