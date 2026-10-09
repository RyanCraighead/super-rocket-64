/* Optional verified owned textures; no decoded game assets are embedded here. */
#pragma once
#include "utils/json.hpp"
#include "utils/oot_asset_path.h"
#include "utils/rocket_sha256.h"
#include <stb/stb_image.h>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace rocket_materials {
constexpr size_t BODY_VERTICES=85434,CHASSIS_VERTICES=48480;
constexpr int TEXTURE_SIZE=2048;
struct Data {
    std::vector<std::array<float,2>> uv;
    std::array<std::vector<unsigned char>,2> rgba; // chassis, body
    bool ready() const {return uv.size()==BODY_VERTICES;}
};
inline Data loadDirectory(const std::string &folder) {
    auto bytes=oot_asset_path::readFile(oot_asset_path::child(folder,"manifest.json",240),16384);
    auto j=nlohmann::json::parse(bytes.begin(),bytes.end(),[](int depth,nlohmann::json::parse_event_t,nlohmann::json &){if(depth>8)throw std::runtime_error("Material JSON depth");return true;});
    if(j.at("schema")!="octane-host-materials-v1"||j.at("vertices")!=BODY_VERTICES||j.at("chassis_vertices")!=CHASSIS_VERTICES||j.at("uv_channel")!=0||
       j.at("body_geometry_sha256")!="e1162f1643ad34857fda1284b5ecac7af7d7172069c968b2eb9473b9fdb523df")throw std::runtime_error("Unsupported car materials");
    const char *names[]={"body.uv","chassis.png","body.png"};
    const size_t sizes[]={683472,2126846,2967332};
    const char *hashes[]={"dfab6c5470e2f4889fc1b51da747caaec68ac125bd9a3662055fda6f84f5b5e5",
        "79a2e08676f22deecbbb79b450f2c9757b5ac6cbbc2099de0db0ee946f4919b5",
        "0629edaa948b2d5cbfe0f2ca6b698ba1a40bb64b2922ba5e8c5b2509c9f0298f"};
    Data result;
    for(int i=0;i<3;++i) {
        const auto &record=j.at("files").at(names[i]);
        if(record.at("size")!=sizes[i]||record.at("sha256")!=hashes[i])throw std::runtime_error("Material manifest mismatch");
        bytes=oot_asset_path::readFile(oot_asset_path::child(folder,names[i],240),sizes[i],sizes[i]);
        if(rocket_assets::sha256(bytes)!=hashes[i])throw std::runtime_error("Original material hash mismatch");
        if(!i) {
            static_assert(sizeof(std::array<float,2>)==8,"Unexpected UV layout");
            result.uv.resize(BODY_VERTICES);std::memcpy(result.uv.data(),bytes.data(),bytes.size());
            for(const auto &uv:result.uv)for(float v:uv)if(!std::isfinite(v)||std::fabs(v)>=16)throw std::runtime_error("Invalid original UV");
        } else {
            int w=0,h=0,n=0;
            if(!stbi_info_from_memory(bytes.data(),(int)bytes.size(),&w,&h,&n)||w!=TEXTURE_SIZE||h!=TEXTURE_SIZE)throw std::runtime_error("Invalid material dimensions");
            std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(stbi_load_from_memory(bytes.data(),(int)bytes.size(),&w,&h,&n,4),stbi_image_free);
            if(!pixels)throw std::runtime_error("Material decode failed");
            result.rgba[i-1].assign(pixels.get(),pixels.get()+TEXTURE_SIZE*TEXTURE_SIZE*4);
        }
    }
    return result;
}
inline Data load(const std::string &base) {return loadDirectory(oot_asset_path::child(base,"materials",240));}
}
