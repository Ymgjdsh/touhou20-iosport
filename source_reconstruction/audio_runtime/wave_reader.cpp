#include "wave_reader.hpp"
#include <cstring>
#include <stdexcept>
#if defined(TH20_IOS)
#include <cstdio>
#include <limits>
#endif

namespace th20::source::audio {
WaveReader::WaveReader() noexcept {std::memset(this,0,sizeof *this);}
WaveReader::~WaveReader() {close();}
HRESULT WaveReader::open_file(const char* path,TrackFormat* format,std::uint32_t mode,std::uint32_t base_offset) {
    file_mode=mode;memory_mode=0;
    if(file_mode!=1 || !path) return E_INVALIDARG;
#if defined(TH20_IOS)
    file=std::fopen(path,"rb");
    if(!file){file=INVALID_HANDLE_VALUE;return E_FAIL;}
#else
    wchar_t wide[262]{};
    if(!MultiByteToWideChar(932,0,path,-1,wide,260)) return E_INVALIDARG;
    file=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0x08000080,nullptr);
    if(file==INVALID_HANDLE_VALUE) return E_FAIL;
#endif
    track=format;filename=path;
#if defined(TH20_IOS)
    const auto result=reset(false,0,base_offset);if(FAILED(result)){close();return result;}
#else
    reset(false,0,base_offset);
#endif
    initial_remaining=remaining;looped=0;return S_OK;
}
HRESULT WaveReader::open_memory(const std::uint8_t* bytes,std::uint32_t size,TrackFormat* format,int flag) {
#if defined(TH20_IOS)
    if(!bytes||!format||format->loop_start>format->total_bytes||format->total_bytes>size)return E_INVALIDARG;
#endif
    track=format;memory_size=size;memory_start=bytes;memory_current=memory_start;memory_mode=1;
    return flag==1?S_OK:E_NOTIMPL; // original returns E_NOTIMPL after initializing when flag=0
}
HRESULT WaveReader::reopen(TrackFormat* format,std::uint32_t position,std::uint32_t base_offset) {
    if(memory_mode) return E_FAIL;
#if defined(TH20_IOS)
    if(file==INVALID_HANDLE_VALUE||!file){const auto result=open_file(filename,format,1,base_offset);if(FAILED(result))return result;}
#else
    if(file==INVALID_HANDLE_VALUE) open_file(filename,format,1,base_offset);
#endif
    if(file==INVALID_HANDLE_VALUE) return E_FAIL;
    looped=0;track=format;
#if defined(TH20_IOS)
    const auto result=reset(false,position,base_offset);if(FAILED(result))return result;
#else
    reset(false,position,base_offset);
#endif
    initial_remaining=remaining;return S_OK;
}
HRESULT WaveReader::read(void* output,std::uint32_t requested,std::uint32_t* received) {
    if(memory_mode) {
#if defined(TH20_IOS)
        if(!output&&requested)return E_INVALIDARG;
#endif
        if(!memory_current) return CO_E_NOTINITIALIZED;
        if(received) *received=0;
        const auto consumed=static_cast<std::uint32_t>(memory_current-memory_start);
        if(consumed>memory_size) throw std::out_of_range("Audio memory cursor exceeds allocation");
        if(requested>memory_size-consumed) requested=memory_size-consumed;
        std::memcpy(output,memory_current,requested);memory_current+=requested;
        if(received) *received=requested;return S_OK;
    }
    if(!file) return CO_E_NOTINITIALIZED;
    if(!output || !received) return E_INVALIDARG;
    const auto amount=requested>remaining?remaining:requested;
#if defined(TH20_IOS)
    if(file==INVALID_HANDLE_VALUE){*received=0;return CO_E_NOTINITIALIZED;}
    const auto actual=std::fread(output,1,amount,static_cast<std::FILE*>(file));
    *received=static_cast<std::uint32_t>(actual);remaining-=*received;
    // A truncated bundled BGM must be reported, rather than looping stale PCM.
    return actual==amount?S_OK:E_FAIL;
#else
    remaining-=amount;
    DWORD actual=0;ReadFile(file,output,amount,&actual,nullptr);*received=actual;
    return S_OK; // original ignores the BOOL return; failed I/O's byte count is outside the proven domain
#endif
}
HRESULT WaveReader::reset(bool loop,std::uint32_t position,std::uint32_t base_offset) {
    if(memory_mode) {
        memory_current=memory_start;
        if(static_cast<std::int32_t>(track->total_bytes)>0) memory_size=track->total_bytes;
        if(loop && static_cast<std::int32_t>(track->loop_start)>0) memory_current+=track->loop_start;
        return S_OK; // original memory path ignores position
    }
    if(file==INVALID_HANDLE_VALUE || !file) return CO_E_NOTINITIALIZED;
#if defined(TH20_IOS)
    if(!track||track->loop_start>track->total_bytes)return E_INVALIDARG;
    if(!loop||static_cast<std::int32_t>(track->loop_start)<1){
        if(track->total_bytes<=position){
            const auto length=track->total_bytes-track->loop_start;
            if(!length)return E_INVALIDARG;
            position=track->loop_start+(position-track->total_bytes)%length;
        }
    }else position=track->loop_start;
    const std::uint64_t offset=std::uint64_t(base_offset)+track->file_offset+position;
    if(offset>std::uint64_t(std::numeric_limits<off_t>::max())||fseeko(static_cast<std::FILE*>(file),static_cast<off_t>(offset),SEEK_SET)!=0)return E_FAIL;
    remaining=track->total_bytes-position;return S_OK;
#else
    if(!loop || static_cast<std::int32_t>(track->loop_start)<1) {
        if(track->total_bytes<=position) position-=track->total_bytes-track->loop_start;
        SetFilePointer(file,static_cast<LONG>(base_offset+track->file_offset+position),nullptr,FILE_BEGIN);
        remaining=track->total_bytes-position;
    } else {
        SetFilePointer(file,static_cast<LONG>(base_offset+track->file_offset+track->loop_start),nullptr,FILE_BEGIN);
        remaining=track->total_bytes-track->loop_start;
    }
    return S_OK;
#endif
}
HRESULT WaveReader::close() {
    if(file_mode==1) {
#if defined(TH20_IOS)
        if(file&&file!=INVALID_HANDLE_VALUE)std::fclose(static_cast<std::FILE*>(file));
#else
        CloseHandle(file);
#endif
        file=INVALID_HANDLE_VALUE;
    }
    return S_OK;
}
std::uint32_t WaveReader::tell() const {
#if defined(TH20_IOS)
    if(memory_mode)return track?track->file_offset+static_cast<std::uint32_t>(memory_current-memory_start):0;
    if(!file||file==INVALID_HANDLE_VALUE)return 0;
    const auto value=ftello(static_cast<std::FILE*>(file));
    return value<0?0:static_cast<std::uint32_t>(value);
#else
    return SetFilePointer(file,0,nullptr,FILE_CURRENT);
#endif
}
}
