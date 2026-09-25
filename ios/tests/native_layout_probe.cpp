#include "../../source_reconstruction/program_entry/program_entry.hpp"
#include "../../source_reconstruction/input/input.hpp"
#include "../../source_reconstruction/platform_window/frame_statistics.hpp"
#include <cstdio>

int main() {
    using namespace th20::source::program_entry;
#define FIELD(Type,member) std::printf(#Type "." #member "=%zu\n",offsetof(Type,member))
    std::printf("WindowStatePrefix=%zu GraphicsStatePrefix=%zu\n",sizeof(WindowStatePrefix),sizeof(GraphicsStatePrefix));
    FIELD(WindowStatePrefix,active);FIELD(WindowStatePrefix,draw_counter);
    FIELD(WindowStatePrefix,performance_frequency);FIELD(WindowStatePrefix,user_data_directory);
    FIELD(WindowStatePrefix,display_mode);FIELD(WindowStatePrefix,flags);
    FIELD(WindowStatePrefix,current_time);FIELD(WindowStatePrefix,repeat);
    FIELD(GraphicsStatePrefix,presentation);FIELD(GraphicsStatePrefix,configuration);
    FIELD(GraphicsStatePrefix,disable_vsync);FIELD(GraphicsStatePrefix,device_caps);
    FIELD(GraphicsStatePrefix,worker_storage);FIELD(GraphicsStatePrefix,update_duration);
    std::printf("native input and statistics assertions passed\n");
}
