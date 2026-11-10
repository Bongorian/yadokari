#include "benchmark_project.h"
#include "project_codec.h"
#include <cstdio>
#include <memory>
struct FileWriter { FILE* file; bool write(const uint8_t* p,size_t n){return std::fwrite(p,1,n,file)==n;} };
int main(int argc,char** argv){
 if(argc!=2){std::fprintf(stderr,"usage: benchmark-project OUTPUT.mgxs\n");return 2;}
 auto project=std::make_unique<groove::ProjectState>();groove::make_benchmark_project(*project);
 FILE* file=std::fopen(argv[1],"wb");if(!file)return 1;
 FileWriter writer{file};bool ok=groove::encode_project(writer,*project);if(std::fclose(file)!=0)ok=false;
 return ok?0:1;
}
