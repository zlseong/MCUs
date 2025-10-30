# TC375    

## [TARGET]    

```
TC375 PFLASH (6 MB) -   
---------------------------------------------------

 0x80000000  BMI Header (256 B)          [WARNING] Hardware
---------------------------------------------------
 0x80000100  SSW (64 KB)                 [X] 
              (Stage 1)                      
---------------------------------------------------
 0x80010000  Reserved (TP: 64 KB)          
---------------------------------------------------
 0x80020000  Reserved (HSM: 512 KB)        
---------------------------------------------------
 0x800A0000  Bootloader A (160 KB)       [OK] OTA 
              - Meta: 0x800A0000 (4 KB)
              - Code: 0x800A1000 (156 KB)
---------------------------------------------------
 0x800C8000  Bootloader B (160 KB)       [OK] OTA 
              - Meta: 0x800C8000 (4 KB)
              - Code: 0x800C9000 (156 KB)
---------------------------------------------------
 0x800F0000  Application A (3 MB)        [OK] OTA 
              - Meta: 0x800F0000 (4 KB)
              - Code: 0x800F1000 (~3 MB)
---------------------------------------------------
 0x803F0000  Application B (3 MB)        [OK] OTA 
              - Meta: 0x803F0000 (4 KB)
              - Code: 0x803F1000 (~3 MB)
---------------------------------------------------
```

---

## [TABLE]   

|   |  |   |  | OTA |
|-----------|-----------|----------|------|-----|
| SSW (Stage 1) | - | 0x80000100 | 64 KB | [X] |
| Bootloader A | 0x800A0000 | 0x800A1000 | 160 KB | [OK] |
| Bootloader B | 0x800C8000 | 0x800C9000 | 160 KB | [OK] |
| Application A | 0x800F0000 | 0x800F1000 | 3 MB | [OK] |
| Application B | 0x803F0000 | 0x803F1000 | 3 MB | [OK] |

---

## [CONFIG] Linker Script  ( )

### SSW (Stage 1)
```ld
MEMORY {
    SSW_FLASH (rx)  : ORIGIN = 0x80000100, LENGTH = 64K
    SSW_RAM (rwx)   : ORIGIN = 0x70000000, LENGTH = 240K
}
```

### Bootloader A
```ld
MEMORY {
    BOOT_META (r)   : ORIGIN = 0x800A0000, LENGTH = 4K
    BOOT_FLASH (rx) : ORIGIN = 0x800A1000, LENGTH = 156K
    BOOT_RAM (rwx)  : ORIGIN = 0x70010000, LENGTH = 128K
}
```

### Bootloader B
```ld
MEMORY {
    BOOT_META (r)   : ORIGIN = 0x800C8000, LENGTH = 4K
    BOOT_FLASH (rx) : ORIGIN = 0x800C9000, LENGTH = 156K
    BOOT_RAM (rwx)  : ORIGIN = 0x70010000, LENGTH = 128K
}
```

### Application A
```ld
MEMORY {
    APP_META (r)    : ORIGIN = 0x800F0000, LENGTH = 4K
    APP_FLASH (rx)  : ORIGIN = 0x800F1000, LENGTH = 3M
    APP_RAM (rwx)   : ORIGIN = 0x70040000, LENGTH = 256K
}
```

### Application B
```ld
MEMORY {
    APP_META (r)    : ORIGIN = 0x803F0000, LENGTH = 4K
    APP_FLASH (rx)  : ORIGIN = 0x803F1000, LENGTH = 3M
    APP_RAM (rwx)   : ORIGIN = 0x70040000, LENGTH = 256K
}
```

---

## [STORAGE] DFLASH 

|  |  |  |  |
|------|------|------|------|
| Boot Config | 0xAF000000 | 4 KB |   |
| App Data | 0xAF001000 | 60 KB |   |
| OTA Buffer | 0xAF010000 | 64 KB | OTA  |
| UCB (Reserved) | 0xAF400000 | 64 KB | Infineon  |

---

## [START] OTA  

### Application 
```bash
# : App A  
# Target: App B
OTA_TARGET_ADDRESS=0x803F1000
```

### Bootloader 
```bash
# : Bootloader A  
# Target: Bootloader B
OTA_TARGET_ADDRESS=0x800C9000
```

---

## [WARNING] 

###     :
```
0x80000000 - 0x8000FFFF  SSW (!)
0x80010000 - 0x8009FFFF  Reserved (TP/HSM)
```

### S40  :
```
0x800A0000 ~  <-   !
```

---

## [REFERENCE]  

-   : `docs/tc375_memory_map.md`
- Infineon : `docs/tc375_infineon_bootloader_mapping.md`
-  : `tc375_bootloader/common/boot_common.h`

