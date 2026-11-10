// Host authoring boundary: preserve engine, assets, FX, identity and opaque chunks.
#include "project_files.h"
#include "../../platform/desktop/file_access.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
int main(int argc,char** argv){
 using namespace groove;
 if(argc!=4){std::fprintf(stderr,"usage: preset-trim INPUT OUTPUT LINEAR_GAIN (0.125..4)\n");return 2;}
 char* end=nullptr;float gain=std::strtof(argv[3],&end);
 if(!end || end==argv[3] || *end || !valid_output_trim(gain)){std::fprintf(stderr,"Invalid gain; expected finite 0.125..4\n");return 2;}
 DesktopFiles fs;auto p=std::make_unique<ProjectState>();PreservedStore store;
 if(!read_project_file(fs,argv[1],*p,nullptr,&store) || p->content_kind==ContentKind::Song || !engine_descriptor(unsigned(p->tracks[0].instrument))){std::fprintf(stderr,"Expected a supported MGXC13 preset or kit\n");return 3;}
 // Paths remain relative hints. Caller places OUTPUT beside INPUT, or collects
 // assets separately; this operation neither moves nor rewrites references.
 p->tracks[0].output_trim=gain;
 if(!save_recoverable(fs,argv[2],*p,&store)){std::fprintf(stderr,"Save failed\n");return 4;}
 std::printf("MGXC13 PTRM gain=%.9g; identity/revision unchanged\n",gain);
}
