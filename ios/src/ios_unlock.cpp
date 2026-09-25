#include "ios_unlock.h"
#include "ios_host.h"
#include "title_system/player_data.hpp"
#include "title_system/title.hpp"
#include "progress_state/file_codec.hpp"
#include "archive/resource_manager.hpp"
#include "program_entry/program_entry.hpp"
#include "runtime_core/runtime_core.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace th20::ios {
namespace {
namespace progress=source::progress;
namespace title=source::title;
namespace runtime=source::runtime;
bool original_content_unlocked(const progress::Snapshot& snapshot) {
    for(unsigned card=0;card<113;++card){
        const auto* record=snapshot.profiles[18].bytes+0xb08+card*0xe0;
        if(!progress::read<unsigned>(record,0xc8)&&!progress::read<unsigned>(record,0xcc))return false;
    }
    for(unsigned index=0;index<18;++index){
        const auto* profile=snapshot.profiles[index].bytes;
        if(!progress::read<unsigned>(profile,0x76f0))return false;
        for(unsigned difficulty=0;difficulty<6;++difficulty)
            for(unsigned stage=0;stage<8;++stage)
                if(!profile[0x7701+difficulty*0x90+stage*16])return false;
    }
    return true;
}
bool saved_records_match(const progress::Snapshot& expected) {
    const auto path=std::filesystem::path(source::program_entry::window_state.user_data_directory)/"scoreth20.dat";
    std::ifstream stream(path,std::ios::binary);
    if(!stream)return false;
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    if(bytes.size()<44||bytes.size()>0xffffffffu)return false;
    auto cleanup=[](progress::Snapshot* value){progress::release_snapshot_buffers(*value);delete value;};
    std::unique_ptr<progress::Snapshot,decltype(cleanup)> actual(new progress::Snapshot{},cleanup);
    actual->file_size=static_cast<unsigned>(bytes.size());
    actual->file_buffer=static_cast<unsigned char*>(runtime::allocate_bytes(bytes.size()));
    if(!actual->file_buffer)throw std::bad_alloc();
    std::memcpy(actual->file_buffer,bytes.data(),bytes.size());
    progress::parse_snapshot(*actual,source::resources::decode_shared);
    return std::memcmp(actual->profiles,expected.profiles,sizeof(expected.profiles))==0&&
        std::memcmp(&actual->metadata,&expected.metadata,sizeof(expected.metadata))==0;
}
}
int apply_unlock_code(const char* code) noexcept {
    if(!code||std::strcmp(code,"ymgjdsh")!=0)return 0;
    auto* manager=progress::manager;if(!manager)return -1;
    try {
        runtime::join_worker(manager->worker);
        {
            std::lock_guard<std::recursive_mutex> guard(runtime::shared_locks().slot(20));
            if(manager->current.metadata.bytes[0x1d1]!=progress::metadata_checksum(manager->current.metadata))
                throw std::runtime_error("Cannot unlock content in an invalid progress record");
            // The original hidden sequence unlocks all Extra profiles, stage
            // practice and 113 spell-practice records. Keep its exact routine.
            if(!original_content_unlocked(manager->current))
                title::unlock_all_data(*manager,source::state::random_streams[1]);
            auto* metadata=manager->current.metadata.bytes;
            // Music Room uses 32 availability flags. Stone selection uses
            // nine inventory counters (maximum 9), independent of scores.
            std::fill(metadata+0x36,metadata+0x36+32,1);
            // The stone-record gallery hides its 41 titles/descriptions behind
            // achievement flags. The user's all-content code exposes these
            // records too, as well as the 18 endings and four ending extras.
            std::fill(metadata+0x68,metadata+0x68+41,1);
            for(unsigned ending=0;ending<18;++ending)metadata[0x16+ending]|=0x0f;
            for(unsigned extra=18;extra<22;++extra)metadata[0x16+extra]|=1;
            for(unsigned stone=0;stone<9;++stone)progress::write(metadata,0x168+stone*4,9u);
            progress::update_metadata_checksum(manager->current.metadata,source::state::random_streams[1]);
        }
        manager->commit();runtime::join_worker(manager->worker);
        if(!saved_records_match(manager->current))throw std::runtime_error("Unlock progress could not be verified after saving");
        // Main-menu exclusions were cached when entering the page. Enable
        // Extra immediately when Settings is dismissed, without a restart.
        if(auto* page=title::controller();page&&page->state==1){
            auto& excluded=page->cursor.excluded;
            excluded.erase(std::remove(excluded.begin(),excluded.end(),1),excluded.end());
        }
        th20_ios_log("unlock: saved/read back Extra/stage/spell unlock, all music, stone inventory/gallery and ending flags");
        return 1;
    }catch(const std::exception& error){th20_ios_log("ERROR unlock: %s",error.what());}
    catch(...){th20_ios_log("ERROR unlock: unknown save error");}
    return -2;
}
}
