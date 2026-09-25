#include <cstdio>
#include "../../source_reconstruction/bullet_system/bullet.hpp"
#include "../../source_reconstruction/bullet_system/command.hpp"
#include "../../source_reconstruction/bullet_system/style.hpp"
#include "../../source_reconstruction/laser_system/laser.hpp"
#include "../../source_reconstruction/laser_system/type0.hpp"
#include "../../source_reconstruction/laser_system/type1.hpp"
#include "../../source_reconstruction/laser_system/type2.hpp"
#include "../../source_reconstruction/laser_system/type3.hpp"
#include "../../source_reconstruction/damage_regions/regions.hpp"
#include "../../source_reconstruction/bomb_system/bomb.hpp"
#include "../../source_reconstruction/bomb_system/marisa.hpp"
#include "../../source_reconstruction/bomb_system/reimu.hpp"
#include "../../source_reconstruction/item_system/item.hpp"
#include "../../source_reconstruction/effect_system/burst_rings.hpp"
#include "../../source_reconstruction/effect_system/converging_particles.hpp"
#include "../../source_reconstruction/effect_system/effect.hpp"
#include "../../source_reconstruction/effect_system/radial_trails.hpp"
#include "../../source_reconstruction/effect_system/rings.hpp"
#include "../../source_reconstruction/effect_system/rounded_panel.hpp"
#include "../../source_reconstruction/effect_system/short_line.hpp"
#include "../../source_reconstruction/effect_system/spiral_trail.hpp"
#include "../../source_reconstruction/effect_system/stone_selection.hpp"
#include "../../source_reconstruction/effect_system/transition_panels.hpp"
#include "../../source_reconstruction/effect_system/wavering_trail.hpp"
int main(){
{using namespace th20::source::bullet; std::printf("0,0=%zu\n",sizeof(ExtendedCommand));}
{using namespace th20::source::bullet; std::printf("0,1=%zu\n",sizeof(Bullet));}
{using namespace th20::source::bullet; std::printf("1,0=%zu\n",offsetof(Bullet,metadata));}
{using namespace th20::source::bullet; std::printf("1,1=%zu\n",offsetof(Bullet,commands));}
{using namespace th20::source::bullet; std::printf("1,2=%zu\n",offsetof(Bullet,interpolation_420));}
{using namespace th20::source::bullet; std::printf("2,0=%zu\n",offsetof(Bullet,timer_4d8));}
{using namespace th20::source::bullet; std::printf("2,1=%zu\n",offsetof(Bullet,context));}
{using namespace th20::source::bullet; std::printf("3,0=%zu\n",offsetof(Controller,pool));}
{using namespace th20::source::bullet; std::printf("3,1=%zu\n",offsetof(Controller,handles));}
{using namespace th20::source::bullet; std::printf("4,0=%zu\n",offsetof(Controller,free));}
{using namespace th20::source::bullet; std::printf("4,1=%zu\n",offsetof(Controller,context));}
{using namespace th20::source::bullet; std::printf("4,2=%zu\n",sizeof(Controller));}
{using namespace th20::source::bullet; std::printf("5,0=%zu\n",sizeof(Command));}
{using namespace th20::source::bullet; std::printf("5,1=%zu\n",sizeof(ShotMetadata));}
{using namespace th20::source::bullet; std::printf("5,2=%zu\n",sizeof(ShotParameters));}
{using namespace th20::source::bullet; std::printf("6,0=%zu\n",offsetof(ShotMetadata,commands));}
{using namespace th20::source::bullet; std::printf("6,1=%zu\n",offsetof(ShotMetadata,type));}
{using namespace th20::source::bullet; std::printf("7,0=%zu\n",sizeof(Style));}
{using namespace th20::source::bullet; std::printf("7,1=%zu\n",offsetof(Style,radius));}
{using namespace th20::source::laser; std::printf("8,0=%zu\n",offsetof(Laser,link));}
{using namespace th20::source::laser; std::printf("8,1=%zu\n",offsetof(Laser,flags));}
{using namespace th20::source::laser; std::printf("8,2=%zu\n",offsetof(Laser,age));}
{using namespace th20::source::laser; std::printf("9,0=%zu\n",offsetof(Laser,commands));}
{using namespace th20::source::laser; std::printf("9,1=%zu\n",offsetof(Laser,allocated_6d8));}
{using namespace th20::source::laser; std::printf("9,2=%zu\n",offsetof(Laser,context));}
{using namespace th20::source::laser; std::printf("9,3=%zu\n",sizeof(Laser));}
{using namespace th20::source::laser; std::printf("10,0=%zu\n",sizeof(Controller));}
{using namespace th20::source::laser; std::printf("10,1=%zu\n",offsetof(Controller,cancel_position));}
{using namespace th20::source::laser; std::printf("10,2=%zu\n",offsetof(Controller,context));}
{using namespace th20::source::laser; std::printf("11,0=%zu\n",sizeof(Type0Parameters));}
{using namespace th20::source::laser; std::printf("11,1=%zu\n",offsetof(Type0Parameters,commands));}
{using namespace th20::source::laser; std::printf("12,0=%zu\n",offsetof(Type0Laser,animation));}
{using namespace th20::source::laser; std::printf("12,1=%zu\n",offsetof(Type0Laser,tip_animation));}
{using namespace th20::source::laser; std::printf("12,2=%zu\n",sizeof(Type0Laser));}
{using namespace th20::source::laser; std::printf("13,0=%zu\n",sizeof(Segment));}
{using namespace th20::source::laser; std::printf("14,0=%zu\n",sizeof(Type1Parameters));}
{using namespace th20::source::laser; std::printf("14,1=%zu\n",offsetof(Type1Parameters,commands));}
{using namespace th20::source::laser; std::printf("15,0=%zu\n",offsetof(Type1Laser,parameters));}
{using namespace th20::source::laser; std::printf("15,1=%zu\n",offsetof(Type1Laser,animation));}
{using namespace th20::source::laser; std::printf("15,2=%zu\n",sizeof(Type1Laser));}
{using namespace th20::source::laser; std::printf("16,0=%zu\n",sizeof(CurveNode));}
{using namespace th20::source::laser; std::printf("16,1=%zu\n",offsetof(CurveNode,angle));}
{using namespace th20::source::laser; std::printf("17,0=%zu\n",sizeof(CurveSample));}
{using namespace th20::source::laser; std::printf("18,0=%zu\n",sizeof(Type2Parameters));}
{using namespace th20::source::laser; std::printf("18,1=%zu\n",offsetof(Type2Parameters,commands));}
{using namespace th20::source::laser; std::printf("19,0=%zu\n",offsetof(Type2Laser,animation));}
{using namespace th20::source::laser; std::printf("19,1=%zu\n",offsetof(Type2Laser,path));}
{using namespace th20::source::laser; std::printf("19,2=%zu\n",sizeof(Type2Laser));}
{using namespace th20::source::laser; std::printf("20,0=%zu\n",sizeof(Type3Parameters));}
{using namespace th20::source::laser; std::printf("20,1=%zu\n",offsetof(Type3Parameters,commands));}
{using namespace th20::source::laser; std::printf("21,0=%zu\n",offsetof(Type3Laser,parameters));}
{using namespace th20::source::laser; std::printf("21,1=%zu\n",offsetof(Type3Laser,animation));}
{using namespace th20::source::laser; std::printf("22,0=%zu\n",offsetof(Type3Laser,cursor));}
{using namespace th20::source::laser; std::printf("22,1=%zu\n",sizeof(Type3Laser));}
{using namespace th20::source::damage; std::printf("23,0=%zu\n",offsetof(Region,motion));}
{using namespace th20::source::damage; std::printf("23,1=%zu\n",offsetof(Region,lifetime));}
{using namespace th20::source::damage; std::printf("23,2=%zu\n",offsetof(Region,handle));}
{using namespace th20::source::damage; std::printf("23,3=%zu\n",sizeof(Region));}
{using namespace th20::source::damage; std::printf("24,0=%zu\n",offsetof(HitCtrlInf,pool));}
{using namespace th20::source::damage; std::printf("24,1=%zu\n",offsetof(HitCtrlInf,next_handle));}
{using namespace th20::source::damage; std::printf("25,0=%zu\n",offsetof(HitCtrlInf,age));}
{using namespace th20::source::damage; std::printf("25,1=%zu\n",sizeof(HitCtrlInf));}
{using namespace th20::source::bomb; std::printf("26,0=%zu\n",sizeof(Bomb));}
{using namespace th20::source::bomb; std::printf("26,1=%zu\n",offsetof(Bomb,motion));}
{using namespace th20::source::bomb; std::printf("26,2=%zu\n",offsetof(Bomb,interpolation));}
{using namespace th20::source::bomb; std::printf("27,0=%zu\n",sizeof(Controller));}
{using namespace th20::source::bomb; std::printf("27,1=%zu\n",offsetof(Controller,timer));}
{using namespace th20::source::bomb; std::printf("28,0=%zu\n",sizeof(MarisaBomb));}
{using namespace th20::source::bomb; std::printf("28,1=%zu\n",offsetof(MarisaBomb,position));}
{using namespace th20::source::bomb; std::printf("29,0=%zu\n",sizeof(ReimuOrb));}
{using namespace th20::source::bomb; std::printf("29,1=%zu\n",offsetof(ReimuOrb,active));}
{using namespace th20::source::bomb; std::printf("29,2=%zu\n",offsetof(ReimuOrb,damage_handle));}
{using namespace th20::source::bomb; std::printf("30,0=%zu\n",sizeof(ReimuBomb));}
{using namespace th20::source::bomb; std::printf("30,1=%zu\n",offsetof(ReimuBomb,orbs));}
{using namespace th20::source::item; std::printf("31,0=%zu\n",offsetof(Item,position));}
{using namespace th20::source::item; std::printf("31,1=%zu\n",offsetof(Item,timer));}
{using namespace th20::source::item; std::printf("31,2=%zu\n",offsetof(Item,state));}
{using namespace th20::source::item; std::printf("31,3=%zu\n",sizeof(Item));}
{using namespace th20::source::item; std::printf("32,0=%zu\n",offsetof(ItemInf,pool));}
{using namespace th20::source::item; std::printf("32,1=%zu\n",offsetof(ItemInf,active));}
{using namespace th20::source::item; std::printf("32,2=%zu\n",offsetof(ItemInf,speed_scale));}
{using namespace th20::source::item; std::printf("32,3=%zu\n",sizeof(ItemInf));}
{using namespace th20::source::effects; std::printf("33,0=%zu\n",sizeof(BurstRingGeometry));}
{using namespace th20::source::effects; std::printf("33,1=%zu\n",offsetof(BurstRingGeometry,radii));}
{using namespace th20::source::effects; std::printf("34,0=%zu\n",sizeof(BurstRings));}
{using namespace th20::source::effects; std::printf("34,1=%zu\n",offsetof(BurstRings,age));}
{using namespace th20::source::effects; std::printf("35,0=%zu\n",sizeof(ConvergingParticles));}
{using namespace th20::source::effects; std::printf("35,1=%zu\n",offsetof(ConvergingParticles,age));}
{using namespace th20::source::effects; std::printf("35,2=%zu\n",offsetof(ConvergingParticles,stages));}
{using namespace th20::source::effects; std::printf("36,0=%zu\n",sizeof(Parameters));}
{using namespace th20::source::effects; std::printf("36,1=%zu\n",sizeof(Request));}
{using namespace th20::source::effects; std::printf("37,0=%zu\n",sizeof(Descriptor));}
{using namespace th20::source::effects; std::printf("38,0=%zu\n",offsetof(Controller,files));}
{using namespace th20::source::effects; std::printf("38,1=%zu\n",offsetof(Controller,worker));}
{using namespace th20::source::effects; std::printf("39,0=%zu\n",offsetof(Controller,handles));}
{using namespace th20::source::effects; std::printf("39,1=%zu\n",offsetof(Controller,requests));}
{using namespace th20::source::effects; std::printf("40,0=%zu\n",offsetof(Controller,view_index));}
{using namespace th20::source::effects; std::printf("40,1=%zu\n",sizeof(Controller));}
{using namespace th20::source::effects; std::printf("41,0=%zu\n",sizeof(RadialTrail));}
{using namespace th20::source::effects; std::printf("42,0=%zu\n",sizeof(RadialTrails));}
{using namespace th20::source::effects; std::printf("42,1=%zu\n",offsetof(RadialTrails,age));}
{using namespace th20::source::effects; std::printf("43,0=%zu\n",sizeof(RingGeometry));}
{using namespace th20::source::effects; std::printf("44,0=%zu\n",sizeof(FilledRing));}
{using namespace th20::source::effects; std::printf("44,1=%zu\n",sizeof(TripleRing));}
{using namespace th20::source::effects; std::printf("45,0=%zu\n",sizeof(AttachedCallback));}
{using namespace th20::source::effects; std::printf("46,0=%zu\n",offsetof(RoundedPanel,target_position));}
{using namespace th20::source::effects; std::printf("46,1=%zu\n",offsetof(RoundedPanel,age));}
{using namespace th20::source::effects; std::printf("46,2=%zu\n",sizeof(RoundedPanel));}
{using namespace th20::source::effects; std::printf("47,0=%zu\n",sizeof(ShortLine20));}
{using namespace th20::source::effects; std::printf("47,1=%zu\n",offsetof(ShortLine20,age));}
{using namespace th20::source::effects; std::printf("48,0=%zu\n",sizeof(ShortLine30));}
{using namespace th20::source::effects; std::printf("48,1=%zu\n",offsetof(ShortLine30,age));}
{using namespace th20::source::effects; std::printf("49,0=%zu\n",sizeof(LongLine));}
{using namespace th20::source::effects; std::printf("49,1=%zu\n",offsetof(LongLine,view_index));}
{using namespace th20::source::effects; std::printf("50,0=%zu\n",sizeof(SpiralTrail));}
{using namespace th20::source::effects; std::printf("50,1=%zu\n",offsetof(SpiralTrail,colors));}
{using namespace th20::source::effects; std::printf("50,2=%zu\n",offsetof(SpiralTrail,age));}
{using namespace th20::source::effects; std::printf("51,0=%zu\n",sizeof(ReverseSpiralTrail));}
{using namespace th20::source::effects; std::printf("51,1=%zu\n",offsetof(ReverseSpiralTrail,follow_player));}
{using namespace th20::source::effects; std::printf("52,0=%zu\n",sizeof(StoneSelection));}
{using namespace th20::source::effects; std::printf("52,1=%zu\n",offsetof(StoneSelection,handles));}
{using namespace th20::source::effects; std::printf("52,2=%zu\n",offsetof(StoneSelection,pulses));}
{using namespace th20::source::effects; std::printf("52,3=%zu\n",offsetof(StoneSelection,buttons));}
{using namespace th20::source::effects; std::printf("53,0=%zu\n",sizeof(TransitionPanels));}
{using namespace th20::source::effects; std::printf("53,1=%zu\n",offsetof(TransitionPanels,mask));}
{using namespace th20::source::effects; std::printf("53,2=%zu\n",offsetof(TransitionPanels,mode));}
{using namespace th20::source::effects; std::printf("54,0=%zu\n",sizeof(WaveringTrail));}
{using namespace th20::source::effects; std::printf("54,1=%zu\n",offsetof(WaveringTrail,age));}
{using namespace th20::source::effects; std::printf("54,2=%zu\n",offsetof(WaveringTrail,extent));}
}
