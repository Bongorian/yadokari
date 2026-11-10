"""Execute the production control-task admission predicate, including stale save paths."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'firmware/kernel.cpp').read_text();a=s.index('  updater->DiagnosticBusy(')+len('  updater->DiagnosticBusy(');b=s.index(');',a);expression=s[a:b]
source=r'''
#include <cassert>
#include <cstdint>
struct Busy{bool on=false;bool busy()const{return on;}bool active()const{return on;}};
struct Files{bool save=false,kit=false;bool project_save_busy()const{return save;}bool kit_load_busy()const{return kit;}} files;
Busy midi_probe,efficiency,ui_probe,deployment_check,display_calibration,dsp,fluid_probe,preset_check,kit_check,final_check_value;
Busy *efficiency_probe=&efficiency,*dsp_probe=&dsp,*final_check=&final_check_value;
void* wave4_original=nullptr;bool calibrated_ui_pending=false,optimization_dsp_pending=false,native_running=false,audio_stress_preparing=false,fx_stress_running=false,chop_probe_pending=false;
[[maybe_unused]] char save_probe_path[96]="GROOVE/CHECK-001.mgxs";
bool blocked(){return EXPRESSION;}
int main(){
 assert(!blocked()); // A path chosen before failed start is not an active owner.
 bool* owners[]={&files.save,&files.kit,&midi_probe.on,&efficiency.on,&ui_probe.on,&deployment_check.on,&display_calibration.on,&dsp.on,&fluid_probe.on,&preset_check.on,&kit_check.on,&final_check_value.on,&calibrated_ui_pending,&optimization_dsp_pending,&native_running,&audio_stress_preparing,&fx_stress_running,&chop_probe_pending};
 for(bool* owner:owners){*owner=true;assert(blocked());*owner=false;assert(!blocked());}
 wave4_original=&files;assert(blocked());wave4_original=nullptr;assert(!blocked());
}
'''.replace('EXPRESSION',expression)
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'owners.cpp';exe=Path(d)/'owners';cpp.write_text(source)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('production diagnostic owner predicate: 19 active owners block; stale failed-save filename does not latch busy')
