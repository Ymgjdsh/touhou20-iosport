#include "../runtime_core/runtime_core.hpp"
#include <cstdio>
#include <stdexcept>
class TrackedResource final : public std::pmr::memory_resource {
public:
    unsigned allocations=0,deallocations=0;
private:
    void* do_allocate(std::size_t count,std::size_t alignment) override {
        ++allocations;return std::pmr::new_delete_resource()->allocate(count,alignment);
    }
    void do_deallocate(void* pointer,std::size_t count,std::size_t alignment) override {
        ++deallocations;std::pmr::new_delete_resource()->deallocate(pointer,count,alignment);
    }
    bool do_is_equal(const memory_resource& other) const noexcept override {return this==&other;}
};
int main() {
    TrackedResource resource;
    auto* previous=std::pmr::set_default_resource(&resource);
    {
        std::pmr::string text(2048,'x');std::pmr::vector<int> numbers(4096,7);
        if(text.get_allocator().resource()!=&resource||numbers.get_allocator().resource()!=&resource)return 1;
        for(std::size_t alignment:{4u,8u,16u,64u,4096u}) {
            auto* pointer=resource.allocate(13,alignment);
            if(reinterpret_cast<std::uintptr_t>(pointer)%alignment)return 2;
            resource.deallocate(pointer,13,alignment);
        }
    }
    std::pmr::set_default_resource(previous);
    if(resource.allocations!=resource.deallocations||resource.allocations<7)return 3;
    std::printf("TH20 iOS14 PMR PASS allocations=%u log_size=%zu\n",resource.allocations,sizeof(th20::source::runtime::Log));
}
