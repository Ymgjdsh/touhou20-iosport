// Integration probe for the native iOS audio backend, using generated PCM only.
// This is a diagnostic test; it is not linked into the finished game by default.
#include "music_stream.hpp"
#include "ios_host.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <thread>

#if !defined(TH20_IOS)
#error The native audio probe is intended for TH20_IOS only.
#endif
extern "C" int th20_ios_audio_notification_count(IDirectSoundBuffer*,int consume);
namespace a=th20::source::audio;
namespace rt=th20::source::runtime;
namespace p=th20::source::platform;
namespace {
int tests;
void require(bool condition,const char* name){
    ++tests;th20_ios_log("audio-probe: %s | %s",condition?"PASS":"FAIL",name);
    if(!condition)throw std::runtime_error(name);
}
}
extern "C" int th20_ios_audio_runtime_probe(const char* directory){
    tests=0;std::unique_ptr<a::SoundInf> sound;p::Configuration configuration{};rt::Log log;std::unique_ptr<a::Context> services;std::vector<std::uint8_t> wav,metadata;std::chrono::steady_clock::time_point origin;
    try {
        std::filesystem::path folder=std::filesystem::path(directory)/"audio-probe";
        std::filesystem::create_directories(folder);
        const std::string path=(folder/"silent_music.pcm").string();
        std::vector<std::uint8_t> pcm(44100*4,0);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(pcm.data()),pcm.size());require(bool(file),"write generated PCM fixture");}
        wav.assign(44+4096,0);
        const auto put=[&](unsigned offset,std::uint32_t value){std::memcpy(wav.data()+offset,&value,4);};
        std::memcpy(wav.data(),"RIFF",4);put(4,std::uint32_t(wav.size()-8));std::memcpy(wav.data()+8,"WAVEfmt ",8);put(16,16);
        WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;
        format.nAvgBytesPerSec=176400;format.nBlockAlign=4;format.wBitsPerSample=16;
        std::memcpy(wav.data()+20,&format,16);std::memcpy(wav.data()+36,"data",4);put(40,4096);
        a::TrackFormat records[2]{};strcpy_s(records[0].name,"silent.wav");records[0].preload_bytes=records[0].total_bytes=std::uint32_t(pcm.size());
        records[0].loop_start=44100*2;records[0].format=format;
        metadata.resize(sizeof records);std::memcpy(metadata.data(),records,sizeof records);
        configuration.value_75=configuration.value_76=1;configuration.value_7e=100;configuration.value_7f=80;
        origin=std::chrono::steady_clock::now();
        services=std::make_unique<a::Context>(a::Context{configuration,log,[&](const char* name)->std::optional<std::vector<std::uint8_t>>{
            if(std::strcmp(name,"thbgm.fmt")==0)return metadata;
            return wav;
        },[&]{return std::chrono::duration<double>(std::chrono::steady_clock::now()-origin).count();},nullptr});
        auto& context=*services;
        sound=std::make_unique<a::SoundInf>();sound->context=&context;
        require(sound->load_formats("thbgm.fmt")==0,"load packed music metadata");
        sound->initialize(nullptr,context);
        require(sound->device_owner&&sound->direct_sound&&sound->silent_buffer,"initialize AVAudioEngine DirectSound backend");
        unsigned effect_count=0;for(const auto& effect:sound->effects)if(effect.buffer)++effect_count;
        require(effect_count==90,"create all 90 effect channels");
        require(!sound->notify_thread&&!sound->notification&&!sound->notify_thread_id,"no Windows audio worker or event");
        require(sound->apply_configuration()==0&&sound->effect_level==80,"apply volume configuration");
        sound->request_effect(0,0);sound->request_effect(0,400);sound->poll();
        DWORD status{};sound->effects[0].buffer->GetStatus(&status);
        require((status&DSBSTATUS_PLAYING)!=0&&sound->effects[0].pan==200,"merge effect requests and play native PCM");
        require(sound->start_stream(path.c_str())==0&&sound->stream,"create native file stream");
        require(SUCCEEDED(sound->stream->play(0,DSBPLAY_LOOPING,0)),"start looping native stream");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!th20_ios_audio_notification_count(sound->stream->buffer(0),0)&&std::chrono::steady_clock::now()<deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        require(th20_ios_audio_notification_count(sound->stream->buffer(0),0)>0,"mixer produces crossed notification boundaries");
        const auto before=sound->stream->next_write;sound->poll();
        require(sound->stream->next_write!=before&&!sound->stream->busy,"poll refills only crossed ring blocks");
        require(SUCCEEDED(sound->stream->pause())&&!sound->stream->playing,"pause stream and close file");
        require(SUCCEEDED(sound->stream->resume())&&sound->stream->playing,"resume stream and reopen file");
        require(SUCCEEDED(sound->stream->seek_seconds(.125)),"seek stream through native buffer recreation");
        const auto missing=(folder/"missing.pcm").string();
        a::WaveReader absent;require(FAILED(absent.open_file(missing.c_str(),records,1,0)),"missing music file returns failure");
        // Force a read error after Lock to exercise its exception/error cleanup.
        auto* buffer=sound->stream->buffer(0);sound->stream->wave->close();
        require(FAILED(sound->stream->fill(buffer,false,0)),"closed file refill reports failure");
        void* first{};void* second{};DWORD first_size{},second_size{};
        require(SUCCEEDED(buffer->Lock(0,4,&first,&first_size,&second,&second_size,0)),"failed refill releases native mixer lock");
        buffer->Unlock(first,first_size,second,second_size);
        sound->stop_stream();require(!sound->stream&&!sound->notify_thread,"stop native stream without worker join");
        configuration.flags|=16;sound->preload(0,"silent.wav");
        require(!sound->preloaded.empty()&&sound->preloaded[0].allocation,"preload PCM with native file access");
        require(sound->load_track(0)==0&&sound->stream,"create memory-backed music stream");
        require(SUCCEEDED(sound->stream->play(0,DSBPLAY_LOOPING,0)),"play preloaded stream");
        sound->stop_stream();require(sound->shutdown()==0,"shutdown buffers and audio device");
        require(!sound->device_owner&&!sound->silent_buffer&&!sound->track_formats&&!sound->preloaded[0].allocation,"release owned audio resources");
        sound.reset();th20_ios_log("audio-probe: RESULT %d checks PASS",tests);return 0;
    }catch(const std::exception& error){
        th20_ios_log("audio-probe: FAIL after %d checks: %s",tests,error.what());
        if(sound){try{sound->shutdown();}catch(...){}}
        return 1;
    }
}
