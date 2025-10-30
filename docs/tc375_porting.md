# TC375 Lite Kit  

## Phase 2:  TC375  

  Mac   TC375 Lite Kit  .

##  

### 
- TC375 Lite Kit V2
- USB  (/)
- () DAP (Debug Access Port) 

### 
- **Windows PC** ()  Linux
- AURIX Development Studio (ADS)
- TriCore GCC 
- iLLD (Infineon Low Level Drivers)

##   

### 1. ADS 
```
1. Infineon  ADS 
2.    
3. TC375  
```

### 2.  
```
File -> New -> AURIX Project
- Target: TC375TP
- Template: iLLD Base Project
```

##  

###  

**Mac :**
```cpp
#include <openssl/ssl.h>  // POSIX SSL
```

**TC375:**
```cpp
// Option 1: LwIP + mbedTLS
#include "lwip/tcp.h"
#include "mbedtls/ssl.h"

// Option 2:  TCP/IP 
```

### 

**Mac :**
```cpp
#include <thread>
std::thread worker_thread_;
```

**TC375:**
```cpp
// Option 1: FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
xTaskCreate(...);

// Option 2: AUTOSAR OS
```

###  

**Mac :**
```cpp
std::ifstream file("config.json");
```

**TC375:**
```cpp
//    or
// Flash filesystem (SPIFFS, LittleFS)
```

##   

### 1. Ethernet  (TC375)

```c
// iLLD Ethernet 
void init_ethernet(void) {
    // ETH  
    IfxEth_Eth_setPinDriver(...);
    
    // PHY 
    IfxEth_Eth_initPhy(...);
    
    // LwIP 
    lwip_init();
}
```

### 2. TLS  (mbedTLS)

```c
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;

// 
mbedtls_ssl_init(&ssl);
mbedtls_ssl_config_init(&conf);

// TLS 1.3 
mbedtls_ssl_conf_min_version(&conf, 
    MBEDTLS_SSL_MAJOR_VERSION_3, 
    MBEDTLS_SSL_MINOR_VERSION_4);
```

### 3.  

```c
// VADC (Analog-Digital Converter)
float read_temperature(void) {
    IfxVadc_Adc_Channel channel;
    // VADC   
    return result * SCALE_FACTOR;
}
```

##  

### Flash 
```
TC375: 6 MB Flash
- : 256 KB
- : 5 MB
- : 512 KB
- OTA : 
```

### RAM
```
TC375: 4.25 MB RAM
- : 1 MB
- : 128 KB
- LwIP: 500 KB
- mbedTLS: 200 KB
- : 
```

##  

### CMake -> TriCore GCC

**:**
```cmake
set(CMAKE_CXX_COMPILER /usr/bin/c++)
```

**TC375:**
```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER tricore-gcc)
set(CMAKE_CXX_COMPILER tricore-g++)
set(CMAKE_C_FLAGS "-mtc37x -fno-common")
```

## 

### UART 
```c
// UART0  
void debug_printf(const char* fmt, ...) {
    char buffer[256];
    // UART 
    IfxAsclin_Asc_write(&uart, buffer, len);
}
```

### LED 
```c
// LED  
#define LED_HEARTBEAT   P13_0
#define LED_CONNECTED   P13_1
#define LED_ERROR       P13_2
```

##  

### Phase 2-1:  
1. Ethernet  
2. TCP  
3.  

### Phase 2-2: TLS
1. mbedTLS 
2. TLS 
3.  

### Phase 2-3: 
1. JSON  (cJSON)
2.  
3.  

### Phase 2-4: 
1.  
2.  
3.  

##  

### 1. 
- JSON    
-   
-   

### 2. CPU
-   
- DMA 
-   

### 3. 
- Nagle  
-   
- Keep-alive 

##  

- Infineon TC375 User Manual
- iLLD Documentation
- LwIP Documentation
- mbedTLS Porting Guide
- FreeRTOS TC375 Port

##   

GitHub Issues: https://github.com/zlseong/MCUs/issues

