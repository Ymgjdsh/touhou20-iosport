#import <AVFoundation/AVFoundation.h>
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include <dsound.h>
#undef BOOL
#include "ios_host.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
struct Buffer;
std::recursive_mutex mixer_mutex;
std::vector<Buffer*> buffers;
std::atomic<unsigned> missed_callbacks{};
AVAudioEngine* engine;
AVAudioSourceNode* source;
constexpr double output_rate=44100;

struct Buffer final : IDirectSoundBuffer, IDirectSoundNotify {
    std::atomic<ULONG> references{1};
    WAVEFORMATEX format{};
    std::vector<std::uint8_t> bytes;
    std::vector<DWORD> boundaries;
    double position{};
    std::uint64_t total_frames{};
    unsigned pending{};
    LONG volume{},pan{};
    bool playing{},loop{},primary{},locked{};
    explicit Buffer(const DSBUFFERDESC& description):bytes(description.dwBufferBytes),primary((description.dwFlags&DSBCAPS_PRIMARYBUFFER)!=0) {
        if(description.lpwfxFormat)format=*description.lpwfxFormat;
        std::lock_guard guard(mixer_mutex);buffers.push_back(this);
    }
    ~Buffer() {std::lock_guard guard(mixer_mutex);buffers.erase(std::remove(buffers.begin(),buffers.end(),this),buffers.end());}
    ULONG AddRef() override {return ++references;}
    ULONG Release() override {auto value=--references;if(!value)delete this;return value;}
    HRESULT QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(std::memcmp(&iid,&IID_IDirectSoundNotify,sizeof(GUID)))return E_NOTIMPL;
        *out=static_cast<IDirectSoundNotify*>(this);AddRef();return S_OK;
    }
    HRESULT GetCaps(DSCAPS* out) override {if(!out)return E_POINTER;*out={};out->dwSize=sizeof(*out);return S_OK;}
    HRESULT GetCurrentPosition(DWORD* play,DWORD* write) override {
        std::lock_guard guard(mixer_mutex);const DWORD cursor=DWORD(position)*format.nBlockAlign;
        if(play)*play=cursor;if(write)*write=bytes.empty()?0:(cursor+DWORD(format.nSamplesPerSec/50)*format.nBlockAlign)%bytes.size();return S_OK;
    }
    HRESULT GetStatus(DWORD* out) override {if(!out)return E_POINTER;std::lock_guard guard(mixer_mutex);*out=playing?DSBSTATUS_PLAYING:0;return S_OK;}
    HRESULT Initialize(void*,const DSBUFFERDESC*) override {return E_NOTIMPL;}
    HRESULT Lock(DWORD offset,DWORD count,void** first,DWORD* first_size,void** second,DWORD* second_size,DWORD flags) override {
        if(!first||!first_size)return E_POINTER;
        mixer_mutex.lock();
        if(locked){mixer_mutex.unlock();return E_FAIL;}
        if(flags&DSBLOCK_ENTIREBUFFER){offset=0;count=DWORD(bytes.size());}
        if(!count)count=DWORD(bytes.size());
        if(bytes.empty()||offset>=bytes.size()||count>bytes.size()){mixer_mutex.unlock();return E_INVALIDARG;}
        const DWORD initial=std::min<DWORD>(count,DWORD(bytes.size())-offset);
        if(count>initial&&(!second||!second_size)){mixer_mutex.unlock();return E_INVALIDARG;}
        *first=bytes.data()+offset;*first_size=initial;
        if(second)*second=count>initial?bytes.data():nullptr;
        if(second_size)*second_size=count-initial;
        locked=true;return S_OK;
    }
    HRESULT Unlock(void*,DWORD,void*,DWORD) override {
        // The recovered DirectSound caller pairs Lock/Unlock on one thread.
        if(!locked)return E_FAIL;locked=false;mixer_mutex.unlock();return S_OK;
    }
    HRESULT Play(DWORD,DWORD,DWORD flags) override {
        std::lock_guard guard(mixer_mutex);
        if(!format.nBlockAlign||!format.nSamplesPerSec||bytes.empty())return E_INVALIDARG;
        loop=(flags&DSBPLAY_LOOPING)!=0;playing=true;return S_OK;
    }
    HRESULT SetCurrentPosition(DWORD value) override {std::lock_guard guard(mixer_mutex);if(value>=bytes.size()||!format.nBlockAlign)return E_INVALIDARG;position=value/format.nBlockAlign;pending=0;total_frames=0;return S_OK;}
    HRESULT SetFormat(const WAVEFORMATEX* value) override {
        if(!value||value->wFormatTag!=WAVE_FORMAT_PCM||(value->nChannels!=1&&value->nChannels!=2)||
           (value->wBitsPerSample!=8&&value->wBitsPerSample!=16)||!value->nSamplesPerSec||
           value->nBlockAlign!=value->nChannels*value->wBitsPerSample/8)return E_INVALIDARG;
        std::lock_guard guard(mixer_mutex);format=*value;return S_OK;
    }
    HRESULT SetPan(LONG value) override {std::lock_guard guard(mixer_mutex);pan=std::clamp<LONG>(value,-10000,10000);return S_OK;}
    HRESULT SetVolume(LONG value) override {std::lock_guard guard(mixer_mutex);volume=std::clamp<LONG>(value,-10000,0);return S_OK;}
    HRESULT Stop() override {std::lock_guard guard(mixer_mutex);playing=false;return S_OK;}
    HRESULT Restore() override {return S_OK;} // App-owned PCM is never device-lost.
    HRESULT SetNotificationPositions(DWORD count,const DSBPOSITIONNOTIFY* values) override {
        if(count&&!values)return E_POINTER;std::lock_guard guard(mixer_mutex);boundaries.clear();pending=0;
        for(DWORD i=0;i<count;++i){if(values[i].dwOffset>=bytes.size())return E_INVALIDARG;boundaries.push_back(values[i].dwOffset+1);}
        return S_OK;
    }
    float sample(unsigned frame,unsigned channel) const {
        const auto index=frame*format.nBlockAlign+std::min<unsigned>(channel,format.nChannels-1)*format.wBitsPerSample/8;
        if(format.wBitsPerSample==8)return (int(bytes[index])-128)/128.f;
        std::int16_t value;std::memcpy(&value,bytes.data()+index,2);return value/32768.f;
    }
    void mix(float* left,float* right,unsigned frames) {
        if(!playing||primary||!format.nBlockAlign)return;
        const unsigned count=unsigned(bytes.size()/format.nBlockAlign);if(!count)return;
        const double increment=double(format.nSamplesPerSec)/output_rate;
        const float gain=volume<=-10000?0:std::pow(10.f,volume/2000.f);
        const float left_gain=gain*(pan>0?std::pow(10.f,-pan/2000.f):1.f);
        const float right_gain=gain*(pan<0?std::pow(10.f,pan/2000.f):1.f);
        for(unsigned i=0;i<frames&&playing;++i){
            const unsigned first=unsigned(position),second=first+1<count?first+1:(loop?0:first);
            const float fraction=float(position-first);
            left[i]+=(sample(first,0)+(sample(second,0)-sample(first,0))*fraction)*left_gain;
            right[i]+=(sample(first,1)+(sample(second,1)-sample(first,1))*fraction)*right_gain;
            const double next=position+increment;
            const auto before=std::uint64_t(position),after=std::uint64_t(next);
            for(auto boundary:boundaries){const auto b=boundary/format.nBlockAlign;
                if(before<b&&after>=b)++pending;
                if(after>=count&&before<count&&b<=after-count&&b>0)++pending;
            }
            total_frames+=after-before;position=next;
            if(position>=count){if(loop)position=std::fmod(position,double(count));else{position=0;playing=false;}}
        }
    }
};

bool start_audio() {
    if(engine)return engine.running;
    NSError* error=nil;
    AVAudioSession* session=AVAudioSession.sharedInstance;
    if(![session setCategory:AVAudioSessionCategoryPlayback error:&error]||
       ![session setPreferredSampleRate:output_rate error:&error]||
       ![session setPreferredIOBufferDuration:0.0058 error:&error]||
       ![session setActive:YES error:&error]) {
        th20_ios_log("audio session failed: %s",error.localizedDescription.UTF8String);return false;
    }
    engine=[[AVAudioEngine alloc] init];
    AVAudioFormat* format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:output_rate channels:2];
    source=[[AVAudioSourceNode alloc] initWithFormat:format renderBlock:^OSStatus(BOOL* silent,const AudioTimeStamp*,AVAudioFrameCount count,AudioBufferList* output){
        for(unsigned channel=0;channel<output->mNumberBuffers;++channel)std::memset(output->mBuffers[channel].mData,0,output->mBuffers[channel].mDataByteSize);
        std::unique_lock guard(mixer_mutex,std::try_to_lock);
        if(!guard.owns_lock()||output->mNumberBuffers<2){++missed_callbacks;*silent=YES;return noErr;}
        auto* left=static_cast<float*>(output->mBuffers[0].mData);auto* right=static_cast<float*>(output->mBuffers[1].mData);
        for(auto* buffer:buffers)buffer->mix(left,right,count);
        for(unsigned i=0;i<count;++i){left[i]=std::clamp(left[i],-1.f,1.f);right[i]=std::clamp(right[i],-1.f,1.f);}
        *silent=NO;return noErr;
    }];
    [engine attachNode:source];[engine connect:source to:engine.mainMixerNode format:format];
    if(![engine startAndReturnError:&error]){th20_ios_log("audio engine failed: %s",error.localizedDescription.UTF8String);engine=nil;source=nil;return false;}
    th20_ios_log("audio started output_rate=%.0f session_rate=%.0f buffer_seconds=%.6f",output_rate,session.sampleRate,session.IOBufferDuration);
    return true;
}
struct Device final:IDirectSound8 {
    std::atomic<ULONG> references{1};
    ULONG AddRef() override{return ++references;}
    ULONG Release() override{auto count=--references;if(!count)delete this;return count;}
    HRESULT QueryInterface(REFIID,void** out) override{if(out)*out=nullptr;return out?E_NOTIMPL:E_POINTER;}
    HRESULT CreateSoundBuffer(const DSBUFFERDESC* description,IDirectSoundBuffer** out,IUnknown*) override{
        if(!description||!out)return E_POINTER;*out=nullptr;
        auto* buffer=new Buffer(*description);
        if(description->lpwfxFormat&&FAILED(buffer->SetFormat(description->lpwfxFormat))){delete buffer;return E_INVALIDARG;}
        *out=static_cast<IDirectSoundBuffer*>(buffer);return S_OK;
    }
    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer* original,IDirectSoundBuffer** out) override{
        if(!out)return E_POINTER;*out=nullptr;auto* original_buffer=dynamic_cast<Buffer*>(original);if(!original_buffer)return E_INVALIDARG;
        std::lock_guard guard(mixer_mutex);DSBUFFERDESC description{};description.dwBufferBytes=DWORD(original_buffer->bytes.size());description.lpwfxFormat=&original_buffer->format;
        auto* copy=new Buffer(description);copy->bytes=original_buffer->bytes;copy->volume=original_buffer->volume;copy->pan=original_buffer->pan;*out=static_cast<IDirectSoundBuffer*>(copy);return S_OK;
    }
    HRESULT SetCooperativeLevel(HWND,DWORD) override{return S_OK;} // AVAudioSession owns audio policy.
};
}
extern "C" HRESULT WINAPI DirectSoundCreate8(const GUID*,IDirectSound8** out,IUnknown*){
    if(!out)return E_POINTER;*out=nullptr;if(!start_audio())return E_FAIL;*out=new Device;return S_OK;
}
extern "C" int th20_ios_audio_notification_count(IDirectSoundBuffer* value,int consume){
    std::lock_guard guard(mixer_mutex);auto* buffer=dynamic_cast<Buffer*>(value);if(!buffer)return 0;
    const auto count=buffer->pending;if(consume&&count)--buffer->pending;return int(count);
}
extern "C" unsigned th20_ios_audio_missed_callbacks(){return missed_callbacks.load();}
extern "C" bool th20_ios_audio_suspend(bool paused){
    if(!engine)return true;if(paused){[engine pause];return true;}NSError* error=nil;
    if(![AVAudioSession.sharedInstance setActive:YES error:&error]||![engine startAndReturnError:&error]){
        th20_ios_log("audio resume failed: %s",error.localizedDescription.UTF8String);return false;
    }return true;
}
