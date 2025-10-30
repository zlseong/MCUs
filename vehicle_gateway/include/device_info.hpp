/**
 * VMG Device Information
 */

#ifndef DEVICE_INFO_HPP
#define DEVICE_INFO_HPP

#include <string>
#include <cstdint>

struct VMG_Info {
    std::string device_id;
    std::string hardware_version;
    std::string software_version;
    std::string doip_address;
    uint16_t doip_port;
    
    VMG_Info() 
        : device_id("VMG-001"),
          hardware_version("1.0.0"),
          software_version("1.0.0"),
          doip_address("0.0.0.0"),
          doip_port(13400) {}
};

#endif

