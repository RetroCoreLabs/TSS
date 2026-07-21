# nd100x — Implementer's Guide: Adding a New I/O Device

**Purpose:** how to add a new device that answers `IOX` instructions at a chosen
device number (target use case: a swapping drum at `IOX 540`) to the **nd100x**
ND-100 emulator.

**Method / honesty rule:** every architectural claim below is marked
`[VERIFIED]` with the nd100x source file + line and a quote, or `[INFERRED]`
(a conclusion I drew but did not see stated), or listed under **UNKNOWN**. I did
not modify any emulator source. Nothing here is invented API — where I could not
confirm something I say so.

Emulator canonical path: `E:\Dev\Emulators\ND\nd100x\`
All paths below are absolute.

---

## 1. Emulator location, build, and run

### 1.1 Build system and toolchain (this Windows machine) `[VERIFIED]`

- CMake + **Ninja**, compiler **w64devkit MinGW** (`cc.exe`).
  From `E:\Dev\Emulators\ND\nd100x\build\CMakeCache.txt`:
  ```
  CMAKE_C_COMPILER:STRING=C:/Utils/w64devkit/bin/cc.exe
  CMAKE_MAKE_PROGRAM:FILEPATH=.../ninja.exe
  CMAKE_GENERATOR:INTERNAL=Ninja
  CMAKE_BUILD_TYPE:STRING=Debug
  ```
- Windows build procedure (from `E:\Dev\Emulators\ND\nd100x\README.md:161-174`):
  from a `cmd.exe` at the repo root run `build.bat debug` (release: `build.bat release`).
  `build.bat` "stages `w64devkit\bin` on PATH … runs `make debug`, and leaves
  `build\bin\nd100x.exe` ready to run."

### 1.2 Build confirmed working `[VERIFIED]`

- A current binary exists: `E:\Dev\Emulators\ND\nd100x\build\bin\nd100x.exe`
  (built the same day as this investigation).
- Read-only incremental build ran clean. With `w64devkit\bin` on PATH, in
  `E:\Dev\Emulators\ND\nd100x\build`:
  ```
  ninja nd100x   ->  "ninja: no work to do."  (exit 0)
  ```
  So the tree compiles as-is. After adding a new `.c` file you rebuild the same way
  (`ninja nd100x`, or `build.bat debug` from the repo root in cmd.exe).

### 1.3 Running with a BPUN, and DAP `[VERIFIED]`

CLI options (`README.md:205-231`):
```
-b, --boot=TYPE     Boot type (bp, bpun, aout, floppy, smd)
-i, --image=FILE    Image file to load
-d, --debugger      Enable DAP debugger
-p, --port=PORT     Debugger port (default 4711)
-S, --smd-debug     SMD disk controller debug log (stderr)
-t, --trace         CPU execution trace to stderr
-B, --breakpoint=ADDR
```
Run a BPUN:  `nd100x -b bpun -i FILE.BPUN`
Boot SMD disk: `nd100x -b smd` (auto-mounts `SMD0.IMG`).

**DAP caveat — important `[VERIFIED]`:** the **native Windows (w64devkit) build has
NO DAP debugger**. `README.md:178`:
> `--debugger` (DAP server) is unavailable — `external/libdap` uses POSIX-only
> socket headers and hasn't been ported yet.

`docs/HOWTO_BUILD.md:95` confirms DAP is gated behind the `WITH_DEBUGGER` symbol,
and `CMakeCache.txt` on this Windows build does **not** define it. To use DAP
(`-d -p 4711`) you must build/run under **WSL/Linux**. `-t` (CPU trace) and
`-S` (SMD debug log) *do* work in the native Windows build.

---

## 2. The IOX dispatch model (quoted)

### 2.1 CPU executes IOX `[VERIFIED]`

`E:\Dev\Emulators\ND\nd100x\src\cpu\cpu_instr.c:5081` registers the opcode:
```c
Instruction_Add_Mask(0164000, 0xF800, &ndfunc_iox); /* IOX */
```
The handler (`cpu_instr.c:1510-1520`):
```c
/* IOX (Privileged) */
void ndfunc_iox(ushort operand)
{
    if (!CheckPriv())
        return;
    if (!UpdateMemoryIO())
        gA = io_op(operand & 0x07ff, gA);
}
```
So the **device/IO address is the low 11 bits of the instruction word**
(`operand & 0x07ff`), and the current **A register** is passed in and the result
is written back to A. `IOXT` (`cpu_instr.c:1524-1533`) is the same but takes the
IO address from the **T register** (`io_op(gT, gA)`), giving the full 16-bit
addresses the 11-bit `IOX` field cannot reach.

### 2.2 Read vs write is decided by address parity `[VERIFIED]`

`E:\Dev\Emulators\ND\nd100x\src\machine\io.c:72-89`:
```c
ushort io_op(ushort ioadd, ushort regA)
{
    // Even addresses are read operations, odd addresses are write operations
    if (ioadd & 1) {          // Odd address - write
        IO_Write(ioadd, regA);
        return regA;
    } else {                  // Even address - read
        ushort val = IO_Read(ioadd);
        return val;
    }
}
```
`IO_Read`/`IO_Write` (`io.c:47-55`) forward to `DeviceManager_Read` /
`DeviceManager_Write`.

### 2.3 Routing to a device — linear scan over address ranges, NOT a device-number table `[VERIFIED]`

There is **no** table indexed by device number and **no** per-number callback
registry. The device manager holds a dynamic array of `Device*` and linearly
scans it, matching the IO address against each device's `[startAddress,endAddress]`
range. `E:\Dev\Emulators\ND\nd100x\src\devices\devicemanager.c:290-309`:
```c
uint16_t DeviceManager_Read(uint32_t address)
{
    for (int i = 0; i < deviceManager.deviceCount; i++) {
        Device *dev = deviceManager.devices[i].device;
        if (dev && Device_IsInAddress(dev, address))
            return Device_Read(dev, address);
    }
    interrupt(14, 1 << 7); /* IOX error lvl14 */
    return 0;
}
```
`Device_IsInAddress` (`E:\Dev\Emulators\ND\nd100x\src\devices\device.c:154-159`):
```c
bool Device_IsInAddress(Device *dev, uint32_t address) {
    return (address >= dev->startAddress && address <= dev->endAddress);
}
```
**Consequences for you:**
- Register a device by giving it a `startAddress`/`endAddress` range; the
  manager finds it automatically. There is no number to register in a switch.
- An IOX to an address no device claims raises an **IOX error on level 14**
  (`interrupt(14, 1<<7)`), same as real hardware (`devicemanager.c:304`,`:330`).
- The **sub-register index inside a device** is `address - startAddress`, via
  `Device_RegisterAddress` (`device.c:161-166`). For an 8-address device
  (`540..547`) that yields reg `0..7`; even regs arrive through `Read`, odd regs
  through `Write` (because of the parity split in §2.2).

---

## 3. Anatomy of a device (the `Device` struct) `[VERIFIED]`

`E:\Dev\Emulators\ND\nd100x\src\devices\devices_types.h:128-171`. A device is a
`Device` struct carrying its address range, interrupt state, and **function
pointers** the manager calls:
```c
typedef struct Device {
    uint32_t startAddress;        // first IOX address it answers
    uint32_t endAddress;          // last IOX address it answers
    uint16_t interruptBits;       // pending IRQ levels, bit (1<<level)
    uint16_t interruptLevel;      // this device's IRQ level
    uint16_t identCode;           // value returned to IDENT
    uint16_t logicalDevice;
    DeviceType type;              // concrete type tag (set at creation)
    char memoryName[MAX_DEVICE_NAME];
    ...
    void     (*Reset)(struct Device *self);
    uint16_t (*Tick)(struct Device *self);
    int      (*Boot)(struct Device *self, uint16_t device_id);
    uint16_t (*Read)(struct Device *self, uint32_t address);
    void     (*Write)(struct Device *self, uint32_t address, uint16_t value);
    uint16_t (*Ident)(struct Device *self, uint16_t level);
    void     (*Destroy)(struct Device *self);
    DeviceClass deviceClass;      // STANDARD / CHARACTER / BLOCK / RTC
    size_t   blockSizeBytes;      // sector size for BLOCK devices
    ...
    BlockDeviceCallbacks blockCallbacks;  // host file I/O hooks (BLOCK class)
    void    *deviceData;          // your private per-device state
} Device;
```
Handler signatures you implement (all take `self` and use `deviceData` for state):
- `uint16_t Read(Device *self, uint32_t address)` — return the A-reg value.
- `void     Write(Device *self, uint32_t address, uint16_t value)` — `value` is A.
- `uint16_t Ident(Device *self, uint16_t level)` — return `identCode`, clear IRQ.
- `uint16_t Tick(Device *self)` — return `self->interruptBits` (drives IRQs).
- `void Reset/Destroy(Device *self)`, `int Boot(Device *self, uint16_t device_id)`.

There is **no BSKP/skip return path** in this model: `IOX` only reads/writes A.
Skip-condition ("device ready?") tests are done by the driver **reading a status
register word** and testing a bit (see §7). `[VERIFIED]` — `io_op` returns only a
16-bit word into A (`io.c:72-89`); there is no separate skip flag anywhere in the
`Read`/`Write` signatures (`devices_types.h:152-153`).

---

## 4. Annotated template: the SMD disk (the DMA / block device to copy)

Files: `E:\Dev\Emulators\ND\nd100x\src\devices\smd\deviceSMD.c` (logic + factory),
`deviceSMD.h` (register map + bitfields), `diskSMD.c/.h` (geometry). This is the
closest analogue to a swapping drum: programmed-I/O registers to set up a transfer,
then a **DMA block move to/from main memory**, then a delayed completion interrupt.

### 4.1 Factory: address, ident, level, class `[VERIFIED]`

`deviceSMD.c:1265-1362`. Creation allocates the `Device` + private `SMDData`,
initialises the base as a **BLOCK** device, wires the function pointers, and sets
the address range / ident / level from the thumbwheel:
```c
Device *CreateSMDDevice(uint8_t thumbwheel) {
    Device  *dev  = malloc(sizeof(Device));
    SMDData *data = malloc(sizeof(SMDData));
    ...
    Device_Init(dev, thumbwheel, DEVICE_CLASS_BLOCK, 2048); // class + default block size
    dev->deviceData = data;
    dev->Read  = SMD_Read;   dev->Write   = SMD_Write;
    dev->Tick  = SMD_Tick;   dev->Reset   = SMD_Reset;
    dev->Ident = SMD_Ident;  dev->Boot    = SMD_Boot;
    dev->Destroy = SMD_Destroy;
    ...
    switch (thumbwheel) {
    case 0:
        strcpy(dev->memoryName, "SMD 1540");
        dev->identCode    = 017;      // returned to IDENT
        dev->startAddress = 01540;
        break;
    ...
    }
    dev->interruptLevel = 11;                 // disk IRQ level
    dev->endAddress     = dev->startAddress + 7;  // 8 IOX addresses
    return dev;
}
```
Note `dev->type` is stamped later by the manager (`devicemanager.c:240`,
`dev->type = type;`), and for BLOCK devices the manager auto-hooks host file I/O
(see §5.3).

### 4.2 Register decode inside Read/Write `[VERIFIED]`

`deviceSMD.c:118-124` (`SMD_Read`) and `:340-342` (`SMD_Write`):
```c
uint32_t reg = Device_RegisterAddress(self, address);  // = address - startAddress
switch (reg) {
case SMD_READ_MEMORY_ADDRESS:  ... // reg 0 (even -> Read)
case SMD_READ_SEEK_CONDITION:  ... // reg 2
case SMD_READ_STATUS_REGISTER: ... // reg 4  <-- status poll path
case SMD_READ_BLOCK_ADDRESS:   ... // reg 6
}
```
The register enum (`deviceSMD.h:66-77`) shows the even/odd pairing that the §2.2
parity split produces: even offsets are reads, odd offsets are writes
(`SMD_LOAD_MEMORY_ADDRESS = 1`, `SMD_LOAD_CONTROL_WORD = 5`, …).

### 4.3 The transfer setup and the "GO" `[VERIFIED]`

Writing the **control word** (reg 5, `deviceSMD.c:417-531`) latches unit, device
operation (M0 read / M1 write / M4 seek …), and interrupt-enable bits, then — if
the *active* bit is set — calls `ExecuteGO(self)` to perform the transfer. The
core address and word count were loaded by earlier register writes
(`SMD_LOAD_MEMORY_ADDRESS`, `SMD_LOAD_WORD_COUNTER`).

### 4.4 The DMA block move (the part a drum needs) `[VERIFIED]`

`ExecuteGO` reads the disk image into a host buffer via the block callback, then
**DMAs word-by-word into ND main memory**. `deviceSMD.c:875-926` (read path):
```c
uint32_t wordCounter = (data->regs.wordCounterHI << 16 | data->regs.wordCounter);
uint32_t coreAddress = (data->regs.coreAddressHiBits << 16 | data->regs.coreAddress);
uint32_t blockCounter = (wordCounter * 2) / self->blockSizeBytes;
...
// host-side: pull blocks off the image file into 'buffer'
blocksRead = self->blockCallbacks.readFunc(self, buffer, blockCounter, lba,
                                            data->regs.selectedDisk->unit);
...
// DMA transfer to memory
while (wordCounter > 0) {
    uint32_t readData = Device_IO_BufferReadWord(self, buffer, buffer_ptr++);
    Device_DMAWrite(coreAddress, (uint16_t)readData);   // <-- writes ND core memory
    coreAddress = IncrementCoreAddress(regs);
    wordCounter = DecrementWordCounter(regs);
}
free(buffer);
Device_QueueIODelay(self, IODELAY_HDD_SMD, (IODelayedCallback)SMDReadEnd,
                    data->regs.selectedDisk->unit, self->interruptLevel);
```
The write path (`deviceSMD.c:928-977`) is the mirror image: `Device_DMARead(coreAddress)`
to pull each word out of ND memory, pack it into the host buffer, then
`writeFunc(...)` the blocks to the image. **This read/DMA-out and DMA-in/write
pattern is exactly what a swapping drum does** — copy it.

### 4.5 Completion + interrupt via a delayed callback `[VERIFIED]`

`ExecuteGO` does not interrupt immediately; it queues `SMDReadEnd` to fire after
`IODELAY_HDD_SMD` ticks. `SMDReadEnd` (`deviceSMD.c:1127-1150`) clears *active*,
sets *ready-for-transfer*, and **returns `true` to request the interrupt**:
```c
static bool SMDReadEnd(Device *self, int drive) {
    data->statusRegister.bits.active = 0;
    data->statusRegister.bits.readyForTransfer = 1;
    ...
    if (data->statusRegister.bits.interruptEnabled)
        return true;   // returning true triggers GenerateInterrupt() at the queued level
    return false;
}
```
The delayed-callback plumbing that turns that `true` into an interrupt is in
`device.c:215-238` (`Device_TickIODelay`), which calls
`Device_GenerateInterrupt(dev, delay->level)`.

### 4.6 IDENT clears the interrupt `[VERIFIED]`

`SMD_Ident` (`deviceSMD.c:664-680`):
```c
static uint16_t SMD_Ident(Device *self, uint16_t level) {
    if ((self->interruptBits & (1 << level)) != 0) {
        data->statusRegister.bits.interruptEnabled = 0;
        Device_SetInterruptStatus(self, false, level); // clear my pending IRQ bit
        return self->identCode;                         // hand IDENT code to CPU
    }
    return 0;
}
```

---

## 5. Recipe: add a new device (e.g. a swapping drum at `IOX 540-547`)

### 5.1 Create the source files

1. Make a folder `E:\Dev\Emulators\ND\nd100x\src\devices\drum\` with
   `deviceDrum.c` and `deviceDrum.h`. Start by copying `smd/deviceSMD.*` and
   renaming, or the simpler `papertape/devicePapertape.*` if you do **not** need
   DMA. For a drum you need DMA, so SMD is the right template.

2. In `deviceDrum.h`, define your private state struct (like `SMDData`), the
   register offset enum (`0..7`), any bitfield unions for control/status, and
   declare the factory:
   ```c
   Device *CreateDrumDevice(uint8_t thumbwheel);
   ```

3. In `deviceDrum.c`, implement `Drum_Read/Write/Ident/Tick/Reset/Destroy`
   (+ `Drum_Boot` if it should be bootable), and `CreateDrumDevice`. The
   **handler signatures are fixed** by the struct (`devices_types.h:149-156`):
   ```c
   static uint16_t Drum_Read (Device *self, uint32_t address);
   static void     Drum_Write(Device *self, uint32_t address, uint16_t value);
   static uint16_t Drum_Ident(Device *self, uint16_t level);
   static uint16_t Drum_Tick (Device *self);           // return self->interruptBits
   static void     Drum_Reset(Device *self);
   ```
   In the factory set the range/ident/level:
   ```c
   Device_Init(dev, thumbwheel, DEVICE_CLASS_BLOCK, 1024); // block class + sector bytes
   dev->deviceData   = data;
   dev->Read = Drum_Read; dev->Write = Drum_Write; dev->Ident = Drum_Ident;
   dev->Tick = Drum_Tick; dev->Reset = Drum_Reset; dev->Destroy = Drum_Destroy;
   dev->startAddress   = 0540;
   dev->endAddress     = 0540 + 7;      // answers 540..547
   dev->identCode      = /* see UNKNOWN */;
   dev->interruptLevel = /* see UNKNOWN, SMD uses 11 */;
   ```
   Inside `Read`/`Write`, decode with `Device_RegisterAddress(self, address)`
   exactly as SMD does.

   **Address-collision note `[VERIFIED]`:** `540` is free in the *default* config
   (only SMD `01540` and floppy/RTC/etc. are added — `devicemanager.c:95-127`),
   but the SMD factory's `thumbwheel==2` case *also* uses `startAddress=0540`
   (`deviceSMD.c:1339-1343`). That case is not instantiated by default, so there
   is no runtime conflict — just don't enable both.

### 5.2 Register the device type and construction `[VERIFIED]`

Four edits, all in existing files:

1. `E:\Dev\Emulators\ND\nd100x\src\devices\devices_types.h:113-125` — add an enum
   value to `DeviceType`:
   ```c
   DEVICE_TYPE_DRUM,
   ```
   and add `#include "./drum/deviceDrum.h"` near the other device includes
   (`devices_types.h:197-216`).

2. `E:\Dev\Emulators\ND\nd100x\src\devices\devicemanager.c` — in
   `CreateDevice()` (`:151-245`) add a `case`:
   ```c
   case DEVICE_TYPE_DRUM:
       dev = CreateDrumDevice(thumbwheel);
       if (!dev) { Log(LOG_ERROR, "Failed to create drum device\n"); return NULL; }
       break;
   ```

3. `devicemanager.c` — in `DeviceManager_AddAllDevices()` (`:95-127`) add:
   ```c
   // Add the Drum at octal 540-547
   DeviceManager_AddDevice(DEVICE_TYPE_DRUM, 0);
   ```
   (`DeviceManager_AddDevice` is the registration call — `devicemanager.c:258`.)

That is the whole registration path: **no device-number table to edit** — the
`startAddress`/`endAddress` you set in the factory is the registration.

### 5.3 Build-system edits `[VERIFIED]`

`E:\Dev\Emulators\ND\nd100x\src\devices\CMakeLists.txt`. Device `.c` files are
found by per-folder `file(GLOB ...)`, so a new folder must be added in **three
spots**:

1. Add the glob (`CMakeLists.txt:9-17`):
   ```cmake
   file(GLOB DRUM_SOURCES "drum/*.c")
   ```
   and append `${DRUM_SOURCES}` to the `list(APPEND SOURCES ...)` block (`:20-30`).

2. Add a prototype-generation line so `devices_protos.h` gets `CreateDrumDevice`
   (`:33-73`; `devices_protos.h` is AUTO-GENERATED by `mkptypes` — do not hand-edit
   it, per its header `devices_protos.h:1`):
   ```cmake
   COMMAND ${CMAKE_SOURCE_DIR}/tools/mkptypes/mkptypes ${CMAKE_CURRENT_SOURCE_DIR}/drum/deviceDrum.c >> ${CMAKE_CURRENT_SOURCE_DIR}/devices_protos.h
   ```

3. Add the include dir (`:86-96`):
   ```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/drum
   ```

Then rebuild: `build.bat debug` (cmd.exe at repo root) or `ninja nd100x` in
`build\`. (A fresh CMake configure is needed the first time because the GLOB set
changed — `build.bat`/`make` re-run CMake; a bare `ninja` may not pick up a
brand-new folder until CMake reconfigures.)

### 5.4 How your handler reads/writes CPU registers `[VERIFIED]`

- The **A register** is the `value` argument to `Write` and the return value of
  `Read` (§2.1 — `gA = io_op(operand&0x7ff, gA)`). You do not touch `gA` directly.
- The **T register** is only relevant if the driver uses `IOXT` (16-bit IO
  address); your device still just sees the resolved address.
- For **main memory** (not CPU registers) use the DMA API in §6.

---

## 6. DMA / block-transfer API (a device moving a block to/from memory)

nd100x supports true **DMA to physical memory**, used by SMD, floppy-DMA, and HDLC.

### 6.1 Physical-memory DMA words `[VERIFIED]`

`E:\Dev\Emulators\ND\nd100x\src\devices\device.c:365-383`:
```c
extern bool gDMAAccess;   // tells the MMU to bypass shadow/page-table checks
void Device_DMAWrite(uint32_t coreAddress, uint16_t data) {
    gDMAAccess = true;
    WritePhysicalMemory(coreAddress & 0xFFFFFF, data, false);
    gDMAAccess = false;
}
int32_t Device_DMARead(uint32_t coreAddress) {
    gDMAAccess = true;
    int32_t result = ReadPhysicalMemory(coreAddress & 0xFFFFFF, false);
    gDMAAccess = false;
    return result;
}
```
- `coreAddress` is a **physical** word address, masked to **24 bits** (`0xFFFFFF`).
  Data is a **16-bit word**.
- `Device_DMAWrite` = device → memory (disk/drum read). `Device_DMARead` =
  memory → device (disk/drum write). Use these in a loop over your word count,
  incrementing the core address (see SMD `IncrementCoreAddress` /
  `DecrementWordCounter`, `deviceSMD.c:1243-1263`).
- The `gDMAAccess` flag makes the transfer bypass the page tables (a physical bus
  transfer), commented at `device.c:365-367`.

### 6.2 Host image file I/O (block layer) `[VERIFIED]`

A BLOCK-class device also gets host-side block callbacks so it can stage disk
sectors into a buffer before/after DMA. The manager auto-hooks them at add time —
`devicemanager.c:274-278`:
```c
if (dev->deviceClass == DEVICE_CLASS_BLOCK) {
    Device_SetBlockRead (dev, (BlockDeviceReadFunc)machine_block_read,  NULL);
    Device_SetBlockWrite(dev, (BlockDeviceWriteFunc)machine_block_write, NULL);
    Device_SetBlockDiskInfo(dev, (BlockDeviceDiskInfoFunc)machine_block_disk_info, NULL);
}
```
Callback signatures (`devices_types.h:88-90`):
```c
int readFunc (Device*, uint8_t *buffer, size_t size, uint32_t blockAddress, int unit);
int writeFunc(Device*, const uint8_t *buffer, size_t size, uint32_t blockAddress, int unit);
int diskInfoFunc(Device*, size_t *image_size, bool *is_write_protected, int unit);
```
Semantics (`E:\Dev\Emulators\ND\nd100x\src\machine\machine.c:780-852`):
`size` = **number of blocks**, `blockAddress` = **block/LBA index**, and byte
offset = `blockAddress * device->blockSizeBytes`, byte count = `size *
device->blockSizeBytes`. Return value = number of blocks transferred (`-1` error).
You set `blockSizeBytes` on the device (SMD uses 1024).

**Gotcha for a drum `[VERIFIED]`:** `machine_block_read/write` pick the host image
array purely from `device->type` (`machine.c:783`):
```c
DRIVE_TYPE drive_type = (device->type == DEVICE_TYPE_DISC_SMD) ? DRIVE_SMD : DRIVE_FLOPPY;
```
A new `DEVICE_TYPE_DRUM` therefore falls into the **`DRIVE_FLOPPY`** branch and
would read the floppy image, not a drum image. `DRIVE_TYPE` has only
`DRIVE_SMD` / `DRIVE_FLOPPY` (`machine_types.h:50-52`). So to back a drum with a
real image via this plumbing you must **extend** `machine.c` (add a `DRIVE_DRUM`,
a `drum_drives` array, a `mount_drum`, and the type→drive mapping) —
**or** have the drum manage its own `FILE*` directly and not rely on the machine
block callbacks. Decide this early. `[INFERRED]` the "own FILE*" route is simplest
for a single-image swap drum, but I did not find an existing device that does its
own file I/O for DMA (SMD, floppy-DMA and HDLC all use the machine callbacks).

---

## 7. Interrupts and status polling

### 7.1 Raising an interrupt `[VERIFIED]`

Per-device pending IRQs live in `dev->interruptBits` as a bitmask of levels.
`E:\Dev\Emulators\ND\nd100x\src\devices\device.c:255-267`:
```c
void Device_GenerateInterrupt(Device *dev, uint16_t level) {
    if ((level >= 10) && (level <= 13)) {          // only levels 10-13 are device IRQs
        if ((dev->interruptBits & (1 << level)) == 0)
            dev->interruptBits |= (1 << level);
    }
}
void Device_ClearInterrupt(Device *dev, uint16_t level);          // device.c:240
void Device_SetInterruptStatus(Device *dev, bool active, level);  // device.c:269 (set/clear helper)
```
The usual path is **not** to call `Device_GenerateInterrupt` directly from a
register write, but to schedule a delayed completion (models device latency):
`Device_QueueIODelay(dev, ticks, callback, param, level)` (`device.c:189-213`).
When the timer expires, `Device_TickIODelay` (`device.c:215-238`) runs the
callback and, **if the callback returns `true` and `level>0`**, calls
`Device_GenerateInterrupt(dev, level)`. This is precisely how SMD signals
"operation complete" (§4.5).

### 7.2 How the CPU picks it up `[VERIFIED]`

`IO_Tick` (`E:\Dev\Emulators\ND\nd100x\src\machine\io.c:62-69`) is called each
tick; it ORs every device's `Tick()` return and hands the bitmask to the CPU:
```c
void IO_Tick(void) {
    uint16_t interruptBits = DeviceManager_Tick();  // OR of each dev->interruptBits
    if (interruptBits)
        device_interrupt(interruptBits);
}
```
`DeviceManager_Tick` (`devicemanager.c:359-372`) ORs `Device_Tick(dev)` — so your
`Tick` **must return `self->interruptBits`** (SMD: `deviceSMD.c:635-662`; paper
tape: `devicePapertape.c:52-57`). `device_interrupt` (`cpu.c:458-477`) posts those
levels into the CPU's pending-interrupt register `gPID` (masking to levels
10-13,15) and flags `gCHKIT` so the CPU re-evaluates its priority level.

### 7.3 IDENT handshake `[VERIFIED]`

When the CPU takes the interrupt it issues `IDENT PLxx`. `DoIDENT`
(`cpu_instr.c:3930-3947`) calls `IO_Ident(level)` → `DeviceManager_Ident`
(`devicemanager.c:336-357`), which scans devices whose `interruptBits` has that
level set and calls their `Ident` handler. Your `Ident` returns `identCode` **and
clears its own pending bit** (SMD pattern, §4.6). The returned code lands in `gA`.

### 7.4 Status-poll (skip-bit) path `[VERIFIED]`

If the driver polls instead of using interrupts, it reads a **status register**
word and tests bits. Implement this as one of your even register offsets that
returns a status word — SMD's `SMD_READ_STATUS_REGISTER` (reg 4,
`deviceSMD.c:255-320`) returns `data->statusRegister.raw` with bits like
`active` (bit 2), `readyForTransfer` (bit 3), error bits, etc.
(bit layout: `deviceSMD.h:79-103`). The driver's `IOX`+skip logic lives in the
guest OS; the emulator just has to return the correct status word. There is no
host-side skip flag (§3).

---

## 8. Memory-access API summary `[VERIFIED]`

| Need | Function | Source |
|---|---|---|
| DMA word → ND memory | `Device_DMAWrite(uint32_t coreAddr, uint16_t data)` | `device.c:371` |
| DMA word ← ND memory | `int32_t Device_DMARead(uint32_t coreAddr)` | `device.c:377` |
| (underlying phys write) | `void WritePhysicalMemory(int physAddr, uint16_t val, bool priv)` | `cpu_mms.c:855` |
| (underlying phys read) | `int ReadPhysicalMemory(int physAddr, bool priv)` | `cpu_mms.c:822` |

- Address space: **24-bit physical word address** (masked `& 0xFFFFFF` in the DMA
  wrappers). Word size **16 bits**. `ReadPhysicalMemory` returns `int` (`0x00` for
  a protected/`<0` address, `cpu_mms.c:824-828`).
- From a device, prefer the `Device_DMARead/DMAWrite` wrappers — they set the
  `gDMAAccess` bypass flag; calling `Read/WritePhysicalMemory` directly would go
  through the MMU/page tables.
- These are declared for devices in `devices_types.h:38-40`.

---

## 9. Minimal checklist for the drum at IOX 540

1. `src\devices\drum\deviceDrum.h` + `deviceDrum.c` (copy SMD, keep the
   control-word → `ExecuteGO` → `Device_DMAWrite`/`DMARead` loop → `QueueIODelay`
   completion pattern).
2. Factory: `startAddress=0540`, `endAddress=0547`, `interruptLevel` and
   `identCode` **from the driver** (UNKNOWN below), `DEVICE_CLASS_BLOCK`,
   `blockSizeBytes` = drum sector size.
3. `devices_types.h`: add `DEVICE_TYPE_DRUM` + `#include "./drum/deviceDrum.h"`.
4. `devicemanager.c`: `CreateDevice` case + `DeviceManager_AddAllDevices` line.
5. `src\devices\CMakeLists.txt`: GLOB + `list(APPEND ...)` + mkptypes line +
   include dir.
6. Backing store: either extend `machine.c` (`DRIVE_DRUM` + `drum_drives` +
   `mount_drum` + the `device->type` mapping at `machine.c:783,858,904`) **or**
   open your own `FILE*` in the device and skip the machine block callbacks.
7. `build.bat debug`; run `nd100x -b bpun -i FILE.BPUN` (add `-t` to trace, `-S`
   only affects SMD). For DAP stepping, build+run under WSL with `-d -p 4711`.

---

## 10. UNKNOWN / could not determine

- **Drum IDENT code and interrupt level at IOX 540.** SMD uses level 11 /
  ident 017 (`deviceSMD.c:1331,1356`). The correct values for the TSS/NORD swap
  drum must come from the **driver** (the guest expects specific IDENT/level);
  do not invent them. `[VERIFIED]` these are device-specific fields; `[UNKNOWN]`
  the drum's values.
- **Drum register layout / control-word bit meaning at 540-547.** The SMD layout
  (`deviceSMD.h`) is SMD-specific. The drum's register semantics (which offset is
  memory-address, word-count, block-address, control, status; the "GO"/operation
  bits) are **not present in nd100x** (no drum device exists) and must come from
  the drum controller spec / the TSS `XDRUM` driver. `[VERIFIED]` no drum device
  is registered anywhere (`devicemanager.c:95-127`; `DeviceType` enum
  `devices_types.h:113-125`).
- **Whether `IOX 540` needs `IOXT` (16-bit) addressing.** `540` fits in the 11-bit
  `IOX` field, so plain `IOX` reaches it `[INFERRED]` from `operand & 0x07ff`
  (`cpu_instr.c:1518`). Confirm against the actual driver's instruction stream.
- **Backing-store wiring for a drum.** I confirmed the SMD/floppy block callbacks
  are keyed on `device->type` and only handle `DRIVE_SMD`/`DRIVE_FLOPPY`
  (`machine.c:783`). I did **not** find any existing device that does self-managed
  file I/O for DMA, so the "own FILE*" route is untested in this codebase
  `[INFERRED]`.
- **DAP on native Windows.** Confirmed **unavailable** in the w64devkit build
  (`README.md:178`; `WITH_DEBUGGER` not set in `build\CMakeCache.txt`). Use WSL
  for DAP. `[VERIFIED]`
