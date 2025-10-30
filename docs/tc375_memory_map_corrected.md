# TC375    (Hardware Bank Switching)

## [TARGET]  

TC375 **Hardware Bank Switching** :
- Region A (3 MB) @ 0x80000000
- Region B (3 MB) @ 0x82000000
- Hardware  A ↔ B 

---

## [TABLE]  

### Region A (3 MB) - Primary
```
+------------------------------------------+
| 0x80000000 - 0x800000FF: BMI Header      | 256 B
|                          (, )   |
+------------------------------------------+
| 0x80000100 - 0x8000FFFF: SSW             | ~64 KB
|                          (, )   |
+------------------------------------------+
| 0x80010000 - 0x8001FFFF: TP Reserved     | 64 KB
|                          (, )   |
+------------------------------------------+
| 0x80020000 - 0x8009FFFF: HSM PCODE       | 512 KB
|                          (, )   | <- !
+------------------------------------------+
| 0x800A0000 - 0x800A0FFF: Boot Meta       | 4 KB
| 0x800A1000 - 0x800D1FFF: Bootloader      | 196 KB
|                          (OTA )       |
+------------------------------------------+
| 0x800D2000 - 0x800D2FFF: App Meta        | 4 KB
| 0x800D3000 - 0x81FFFFFF: Application     | ~2.1 MB
|                          (OTA )       |
+------------------------------------------+
```

### Region B (3 MB) - Backup/Secondary
```
+------------------------------------------+
| 0x82000000 - 0x820000FF: BMI Header      | 256 B
|                          (A )       |
+------------------------------------------+
| 0x82000100 - 0x8200FFFF: SSW             | ~64 KB
|                          (A )       |
+------------------------------------------+
| 0x82010000 - 0x8201FFFF: TP Reserved     | 64 KB
|                          (A )       |
+------------------------------------------+
| 0x82020000 - 0x8209FFFF: HSM PCODE       | 512 KB
|                          (A )       | <- !
+------------------------------------------+
| 0x820A0000 - 0x820A0FFF: Boot Meta       | 4 KB
| 0x820A1000 - 0x820D1FFF: Bootloader      | 196 KB
|                          ( )      |
+------------------------------------------+
| 0x820D2000 - 0x820D2FFF: App Meta        | 4 KB
| 0x820D3000 - 0x83FFFFFF: Application     | ~2.1 MB
|                          ( )      |
+------------------------------------------+
```

---

## [KEY]  

### 1.   ( )

```c
//   Region A B  
// Factory       

 :
- BMI Header     (256 B)
- SSW            (~64 KB)
- TP Reserved    (64 KB)
- HSM PCODE      (512 KB)  <-  !

Total: ~640 KB
```

**:**
- SSW:  , Region A/B   
- HSM PCODE: Region A active B active HSM   

### 2. OTA  

```c
//   Region A B   

OTA  :
- Bootloader     (200 KB)
- Application    (2.1 MB)

Total: ~2.3 MB
```

**:**
- Region A: v1.0 bootloader + v2.0 app
- Region B: v1.1 bootloader + v2.1 app
- OTA  !

---

## [UPDATE] OTA 

### Scenario: Region A Region B OTA

```
Before OTA:
------------------------------------
Region A (Active):
  SSW ()
  HSM PCODE ()       <- 
  Bootloader v1.0
  Application v2.0

Region B (Inactive):
  SSW ()
  HSM PCODE ()       <- 
  Bootloader v1.0
  Application v2.0

During OTA:
------------------------------------
Region A (Active,  ):
  SSW ()
  HSM PCODE ()
  Bootloader v1.0        <-  
  Application v2.0       <-  

Region B (Update):
  SSW ()             <-  
  HSM PCODE ()       <-  
  Bootloader v1.1        <-  
  Application v2.1       <-  

After OTA (Reboot):
------------------------------------
Region A (Backup):
  Bootloader v1.0        <-  
  Application v2.0       <-  

Region B (Active):
  SSW ()
  HSM PCODE ()       <- Region B !
  Bootloader v1.1        <-  
  Application v2.1       <-  
```

**Region B HSM  :**
```c
// Region B Application
hsm_call(0x82020000);  // Region B HSM PCODE

//    (hardware remapping)
hsm_call(0x80020000);  // Hardware  0x82020000
```

---

## [INFO]  HSM PCODE ?

### : "Region A   ?"

**:  !**

```
Region B active :

Application B (0x820D3000)  
    |
HSM   
    |
HSM PCODE  
    |
0x80020000 ? <- Region A  (inactive!)
    ->    !
    
:
0x82020000 HSM PCODE ! [OK]
```

---

## [DESIGN]   

|  | Region A | Region B |  | ? |
|------|----------|----------|------|-------|
| **BMI** | 0x80000000 | 0x82000000 | 256 B | [OK]  |
| **SSW** | 0x80000100 | 0x82000100 | 64 KB | [OK]  |
| **TP** | 0x80010000 | 0x82010000 | 64 KB | [OK]  |
| **HSM** | 0x80020000 | 0x82020000 | 512 KB | [OK]  |
| **Bootloader** | 0x800A1000 | 0x820A1000 | 200 KB | [X]  |
| **Application** | 0x800D3000 | 0x820D3000 | 2.1 MB | [X]  |

---

## [CONFIG] boot_common.h 

```c
// Immutable regions (identical in both A and B)
#define BMI_SIZE            0x00000100  // 256 B
#define SSW_SIZE            0x0000FF00  // ~64 KB
#define TP_SIZE             0x00010000  // 64 KB
#define HSM_PCODE_SIZE      0x00080000  // 512 KB

#define IMMUTABLE_SIZE      (BMI_SIZE + SSW_SIZE + TP_SIZE + HSM_PCODE_SIZE)
// Total: ~640 KB

// OTA-capable regions (can differ between A and B)
#define BOOTLOADER_SIZE     0x00032000  // 200 KB
#define APPLICATION_SIZE    0x0112D000  // ~2.1 MB

#define OTA_REGION_SIZE     (BOOTLOADER_SIZE + APPLICATION_SIZE)
// Total: ~2.3 MB

// Region boundaries
#define REGION_SIZE         0x02000000  // 3 MB each
```

---

## [OK]  

**:** "HSM Region A/B ?"

**:** **! !**

**:**
1. Region B active  HSM  
2. SSW    ()
3. Factory   , OTA   
4. ~640 KB " " 

**  !** [TARGET]

