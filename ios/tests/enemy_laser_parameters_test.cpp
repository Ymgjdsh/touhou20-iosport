#include "../../source_reconstruction/laser_system/script_parameters.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>

using namespace th20::source;
namespace {
template<class Parameters> unsigned check_type(const char* label) {
    constexpr std::size_t prefix_size=0x24;
    std::mt19937 random(0x4970509u);
    unsigned cases=0;
    for(unsigned i=0;i<2048;++i){
        Parameters p;
        p.commands.resize(2);
        auto script=std::make_unique<char[]>(32);
        std::strcpy(script.get(),"parameter_guard");
        p.commands[0].set_script_name(script.get());
        p.commands[1].words[4]=0x1937cafeu;
        auto* commands=p.commands.data();
        std::array<std::uint8_t,prefix_size> expected{};
        for(auto& byte:expected)byte=std::uint8_t(random());
        std::memcpy(&p,expected.data(),prefix_size);
        std::uint32_t raw[3]={random(),random(),0};
        if(i<8){
            constexpr std::uint32_t special[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u,1,0x807fffffu};
            raw[0]=special[i];raw[1]=special[7-i];
        }
        sprite::Vec3 vector;std::memcpy(&vector,raw,sizeof(vector));
        // Reference is the original three-word write into a byte buffer, not
        // an x86 offset into the native runtime object.
        std::memcpy(expected.data()+0x0c,raw,sizeof(raw));
        laser::set_ecl705_parameters(p,vector);
        assert(std::memcmp(&p,expected.data(),prefix_size)==0);
        assert(p.commands.size()==2&&p.commands.data()==commands);
        assert(p.commands[0].get_script_name()==script.get());
        assert(p.commands[1].words[4]==0x1937cafeu);
        std::uint32_t scalar_bits=random();
        if(i<8)scalar_bits=raw[0];
        float scalar;std::memcpy(&scalar,&scalar_bits,sizeof(scalar));
        std::memcpy(expected.data()+0x1c,&scalar_bits,sizeof(scalar_bits));
        laser::set_ecl709_parameters(p,scalar);
        assert(std::memcmp(&p,expected.data(),prefix_size)==0);
        assert(p.commands.size()==2&&p.commands.data()==commands);
        assert(p.commands[0].get_script_name()==script.get());
        ++cases;
    }
    std::printf("PASS %s: %u pairs of ECL705/ECL709 byte-reference checks\n",label,cases);
    return cases;
}
}
int main(){
    static_assert(sizeof(void*)==8);
    unsigned cases=0;
    cases+=check_type<laser::Type0Parameters>("Type0 angle/length/field14 and width");
    cases+=check_type<laser::Type1Parameters>("Type1 velocity and angular velocity");
    cases+=check_type<laser::Type2Parameters>("Type2 angle/width/speed and color bits");
    cases+=check_type<laser::Type3Parameters>("Type3 vector0c and field1c");
    std::printf("PASS %u paired native parameter cases; includes NaN payloads, signed zero, infinities and container guards\n",cases);
}
