#include "native_font.hpp"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

extern "C" void th20_ios_log(const char* format,...) {
    va_list arguments;va_start(arguments,format);std::vfprintf(stderr,format,arguments);va_end(arguments);std::fputc('\n',stderr);
}
namespace text=th20::source::text;
int failures=0,checks=0;
void check(bool pass,const char* name){++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL %s\n",name);}}
void image(const std::string& path,const std::vector<unsigned char>& pixels,int width,int height){
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<" "<<height<<"\n255\n";
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto* p=&pixels[(y*width+x)*4];
        const unsigned a=p[3];const char rgb[]{char((p[2]*a)/255),char((p[1]*a)/255),char((p[0]*a)/255)};out.write(rgb,3);}
}
int main(int argc,char** argv){
    const std::string prefix=argc>1?argv[1]:"font";
    text::initialize_native_fonts();
    for(int font=0;font<22;++font){
        const int width=160,height=100,pitch=width*4+16;
        std::vector<unsigned char> raster(pitch*height,0x7b);
        text::raster_native_text(raster.data(),pitch,width,height,"F",font,0,0xff65c8fa,0,false,2,0,3);
        int first=height,last=-1,left=width,right=-1;unsigned top=0,bottom=0;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(raster[y*pitch+x*4+3]){first=std::min(first,y);last=std::max(last,y);left=std::min(left,x);right=std::max(right,x);}
        check(first<last,"glyph has nonempty vertical extent");
        for(int y=first;y<=last;++y)for(int x=left+(right-left)*3/4;x<=right;++x){
            const unsigned a=raster[y*pitch+x*4+3];if(y<(first+last+1)/2)top+=a;else bottom+=a;
        }
        // Compare the right-hand bar tips; small serif fonts can have more total ink in the lower stem. This detects a vertical
        // reflection independently of the implementation's row-copy choice.
        check(top>bottom,"F top bars are above its lower stem");
        check(first<30,"glyph starts near requested top, not bottom of allocation");
        bool guard=true;for(int y=0;y<height;++y)for(int x=width*4;x<pitch;++x)guard&=raster[y*pitch+x]==0x7b;
        check(guard,"destination row padding untouched");
        std::vector<unsigned char> sample(720*100*4);
        text::raster_native_text(sample.data(),720*4,720,100,"No. 1  ABC Fgy  \x93\x8c\x95\xfb\x81\x40\x82\xa0\x82\xa2\x82\xa4",font,0,0xffffffff,0xff006080,true,2,0,3);
        image(prefix+"-"+std::to_string(font)+".ppm",sample,720,100);
    }
    std::vector<unsigned char> bitmap(640*80*4);
    auto run=[&](bool unique){const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<200;++i){const std::string value="No. "+std::to_string(unique?i:i%10)+"  \x93\x8c\x95\xfb\x81\x40\x82\xa0\x82\xa2\x82\xa4";
            text::measure_native_text(value.c_str(),4);
            text::raster_native_text(bitmap.data(),640*4,640,80,value.c_str(),4,0,0xffffffff,0xff000000,true,2,0,3);}
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
    const double cold=run(true),warm=run(false);
    bool bad=false;try{text::measure_native_text("\x81",4);}catch(...){bad=true;}check(bad,"invalid CP932 remains an error");
    const auto natural=text::measure_native_text("AB",4);
    auto spaced=text::raster_native_text(bitmap.data(),640*4,640,80,"AB",4,42,0xff60b2e0,0xff000000,true,2,0,3);
    check(spaced.width==84&&spaced.height==natural.height,"fixed spacing extent");
    std::printf("{\"checks\":%d,\"failures\":%d,\"cold_200_ms\":%.3f,\"warm_200_ms\":%.3f}\n",checks,failures,cold,warm);
    text::release_native_fonts();return failures?1:0;
}
