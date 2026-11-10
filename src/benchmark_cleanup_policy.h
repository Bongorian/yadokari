#pragma once
namespace groove {
inline bool benchmark_path_equal(const char* a,const char* b){while(*a && *b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return *a==*b;}
inline bool benchmark_keep_directory(const char* p){
 const char* keep[]={"GROOVE","assets","assets/samples","assets/factory","assets/presets","assets/kits"};
 for(const char* x:keep)if(benchmark_path_equal(p,x))return true;
 return false;
}
inline bool benchmark_keep_subtree(const char* p){return benchmark_path_equal(p,"firmware") || benchmark_path_equal(p,"FACTORY");}
inline bool benchmark_keep_file(const char* p){
 const char* keep[]={"GROOVE/CURRENT.TXT","GROOVE/DEVICE.CFG","GROOVE/DEVICE.TMP","BENCH.MGXS","REMOTE.ON","RECOVERY.ON","wpa_supplicant.conf",
 "assets/samples/demo-slices.wav","assets/kits/Techno-01.mgxk",
 "assets/presets/Acid-Classic.mgxp","assets/presets/Acid-Drive.mgxp","assets/presets/Chip-Noise.mgxp","assets/presets/Chip-Pulse.mgxp","assets/presets/FM-Bell.mgxp","assets/presets/FM-Percussion.mgxp","assets/presets/VA-Bass.mgxp","assets/presets/VA-Lead.mgxp",
 "assets/factory/808-Kick.wav","assets/factory/808-Snare.wav","assets/factory/909-Kick.wav","assets/factory/909-Snare.wav","assets/factory/Acid-Zap.wav","assets/factory/Chip-Perc.wav","assets/factory/Clap.wav","assets/factory/Closed-Hat.wav","assets/factory/FM-Cowbell.wav","assets/factory/FM-Metal.wav","assets/factory/FM-Tom-High.wav","assets/factory/FM-Tom-Low.wav","assets/factory/Noise-Rim.wav","assets/factory/Noise-Sweep.wav","assets/factory/Open-Hat.wav","assets/factory/VA-Hit.wav","assets/factory/NOTICE.txt"};
 for(const char* x:keep)if(benchmark_path_equal(p,x))return true;
 return false;
}
}
