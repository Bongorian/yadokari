#include "software_canvas.h"
#include <cstdio>
#include <cstring>
#include <memory>
using groove::SoftwareCanvas;
void reference(SoftwareCanvas& c,unsigned side,int x,int y,int w,int h,int radius,uint32_t color,unsigned thickness,unsigned alpha){if(w<=0 || h<=0)return;for(int j=0;j<h;++j)for(int i=0;i<w;++i){float outer=c.rounded_coverage(i+0.5f,j+0.5f,float(w),float(h),float(radius));float inner=0;if(i>=int(thickness) && j>=int(thickness) && i<w-int(thickness) && j<h-int(thickness))inner=c.rounded_coverage(i+0.5f-thickness,j+0.5f-thickness,float(w-2*int(thickness)),float(h-2*int(thickness)),float(radius>int(thickness)?radius-int(thickness):0));c.blend(side,x+i,y+j,color,unsigned(groove::ui::clamp01(outer-inner)*alpha));}}
int main(){auto a=std::make_unique<SoftwareCanvas>();auto b=std::make_unique<SoftwareCanvas>();unsigned z=1;for(int n=0;n<2000;n++){z=z*1664525+1013904223;int w=1+z%100;z=z*1664525+1013904223;int h=1+z%80;z=z*1664525+1013904223;int r=int(z%80)-5;z=z*1664525+1013904223;unsigned t=z%12;z=z*1664525+1013904223;unsigned alpha=z%256;int x=n%3?20:-20,y=n%4?20:-10;reference(*a,0,x,y,w,h,r,0xff1475a6,t,alpha);b->outline(0,x,y,w,h,r,0xff1475a6,t,alpha);if(memcmp(a->pixels,b->pixels,sizeof(a->pixels))){printf("mismatch %d w%d h%d r%d t%u a%u\n",n,w,h,r,t,alpha);return 1;}}puts("2000 outline comparisons passed");}
