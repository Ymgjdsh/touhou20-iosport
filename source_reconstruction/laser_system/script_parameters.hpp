#pragma once
#include "type0.hpp"
#include "type1.hpp"
#include "type2.hpp"
#include "type3.hpp"
#include <cstring>
#include <stdexcept>

namespace th20::source::laser {
// ECL705 originally copies three words at Laser+0x704, i.e. parameters+0x0c.
// ECL709 copies one float's bits at Laser+0x714, i.e. parameters+0x1c.
// These slots have different meanings for the four concrete beam classes.
// The parameter prefixes are pointer-free; assert the original evidence here.
static_assert(offsetof(Type0Parameters,angle)==0x0c && offsetof(Type0Parameters,width)==0x1c);
static_assert(offsetof(Type1Parameters,velocity)==0x0c && offsetof(Type1Parameters,angular_velocity)==0x1c);
static_assert(offsetof(Type2Parameters,angle)==0x0c && offsetof(Type2Parameters,color)==0x1c);
static_assert(offsetof(Type3Parameters,vector_0c)==0x0c && offsetof(Type3Parameters,field_1c)==0x1c);

inline void set_ecl705_parameters(Type0Parameters& p,const sprite::Vec3& value) noexcept {
    p.angle=value.x;p.length=value.y;p.field_14=value.z;
}
inline void set_ecl705_parameters(Type1Parameters& p,const sprite::Vec3& value) noexcept {p.velocity=value;}
inline void set_ecl705_parameters(Type2Parameters& p,const sprite::Vec3& value) noexcept {
    p.angle=value.x;p.width=value.y;p.speed=value.z;
}
inline void set_ecl705_parameters(Type3Parameters& p,const sprite::Vec3& value) noexcept {p.vector_0c=value;}

inline void set_ecl709_parameters(Type0Parameters& p,float value) noexcept {p.width=value;}
inline void set_ecl709_parameters(Type1Parameters& p,float value) noexcept {p.angular_velocity=value;}
inline void set_ecl709_parameters(Type2Parameters& p,float value) noexcept {
    // Original MOVSS writes raw bits to this integer slot, without conversion.
    std::memcpy(&p.color,&value,sizeof(value));
}
inline void set_ecl709_parameters(Type3Parameters& p,float value) noexcept {p.field_1c=value;}

template<class Apply> void apply_script_parameters(Laser& beam,Apply&& apply) {
    // Every concrete initialize() sets field_24 to its class's immutable kind.
    switch(beam.field_24){
    case 0:apply(static_cast<Type0Laser&>(beam).parameters);break;
    case 1:apply(static_cast<Type1Laser&>(beam).parameters);break;
    case 2:apply(static_cast<Type2Laser&>(beam).parameters);break;
    case 3:apply(static_cast<Type3Laser&>(beam).parameters);break;
    default:throw std::out_of_range("ECL laser parameter update has an unknown concrete beam kind");
    }
}
}
