#include "firing_at_position.hpp"
#include "shot_data.hpp"
#include <cstring>
namespace th20::source::player_entity {
int fire_shots_at_position(ShotController& owner,int pattern,int frame,int secondary,const sprite::Vec3& position,FiringServices& host){
 const ShotRecord* row=shot_pattern(reinterpret_cast<const void*>(owner.field_12558),pattern);
#if defined(TH20_IOS)
 if(!row)return 0;
#endif
 for(std::uint32_t index=0;;++index,++row){
  if(row->period<0)return 0;
  const bool fire=!row->period||(row->secondary_period?secondary%row->secondary_period==row->secondary_phase:frame%row->period==row->phase);
  if(fire){auto& player=*static_cast<Player*>(owner.context->objects_04[0]);auto* option=row->source?&shot_option(player,(row->source&15)-1):nullptr;create_shot(owner,(std::uint32_t(pattern)<<8)|index,frame,position,option,host);}
 }
}
}
