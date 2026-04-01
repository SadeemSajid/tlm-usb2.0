# USB 2.0 Device Controller (SystemC TLM)

A Transaction Level Modeling (TLM) implementation of a USB 2.0 device controller, written in SystemC. Designed for simulation and verification of USB device firmware before hardware deployment.

## Overview

This USB device implements a subset of the USB 2.0 specification, focused on device enumeration and control transfers via Endpoint 0 (EP0). It is intended for connection to a CPU module running baremetal firmware.

### Key Features

- USB 2.0 Full-Speed compliant device descriptor
- Control transfer support via EP0
- TLM 2.0 generic payload interface
- Configurable vendor/product strings
- Standard USB descriptor hierarchy
- DMA-capable host controller

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CPU Module                                      │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                          Internal RAM (64 bytes)                      │  │
│  │   - Stores setup request data                                         │  │
│  │   - Receives descriptor data from device via DMA                      │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  socket (initiator)                    dma (target)                          │
│       │                                     ▲                                │
└───────┼─────────────────────────────────────┼────────────────────────────────┘
        │                                     │
        │ TLM Write/Read                      │ TLM Write (DMA)
        ▼                                     │
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Host Controller                                     │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │  Registers:                                                           │  │
│  │  - REG_USB_CMD (0x00): Run/Stop, Reset                               │  │
│  │  - REG_USB_STS (0x04): Idle, Error, Transaction Complete, Busy        │  │
│  │  - REG_PORT_SC (0x08): Connect, Port Reset                            │  │
│  │  - REG_ADDR_ENDP (0x0C): Address + Endpoint                           │  │
│  │  - REG_DATA_PTR (0x10): DMA pointer to System RAM                    │  │
│  │  - REG_TOKEN (0x14): Write to trigger transaction                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  cpu_in_sock (target)      dev_out_sock (initiator)      dma_sock (initiator)│
│       ▲                           │                              │           │
└───────┼───────────────────────────┼──────────────────────────────┼───────────┘
        │                           │                              │
        │                           │ TLM Write                    │ TLM Read/Write
        │                           ▼                              ▼
        │              ┌───────────────────────────────────────────────────────┐
        │              │                    USB Device                         │
        │              │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐ │
        │              │  │ Descriptors │  │ Device      │  │ Transmission    │ │
        │              │  │ - Device    │  │ State       │  │ State Machine   │ │
        │              │  │ - Config    │  │ Machine     │  │                 │ │
        │              │  │ - Interface │  │             │  │                 │ │
        │              │  │ - Strings   │  │             │  │                 │ │
        │              │  └─────────────┘  └─────────────┘  └─────────────────┘ │
        │              │                                                       │
        │              │  target (target socket)                               │
        │              └───────────────────────────────────────────────────────┘
        │                                                                      ▲
        └──────────────────────────────────────────────────────────────────────┘
```

### Module Hierarchy

| Module | File | Purpose |
|--------|------|---------|
| `USB_Device` | `device/device.h`, `device/device.cpp` | Main USB device implementation with descriptors and state machines |
| `Controller` | `controller/controller.h`, `controller/controller.cpp` | Host controller with register interface and DMA capability |
| `CPU` | `cpu/cpu.h`, `cpu/cpu.cpp` | CPU module with internal RAM and firmware |
| `USB_Device_TB` | `device/device_tb.h`, `device/device_tb.cpp` | Device testbench for standalone testing |

---

## Device State Machine

```
                              ┌──────────────┐
                              │ USB_ATTACHED │
                              └──────┬───────┘
                                     │ VBUS applied
                                     ▼
                              ┌──────────────┐
                              │ USB_POWERED  │
                              └──────┬───────┘
                                     │ Reset
                                     ▼
                              ┌──────────────┐
                              │ USB_DEFAULT  │◄─────┐
                              └──────┬───────┘      │ Reset
                                     │              │
                                     │ SET_ADDRESS  │
                                     ▼              │
                              ┌──────────────┐      │
                              │ USB_ADDRESS  │──────┘
                              └──────┬───────┘
                                     │
                                     │ SET_CONFIGURATION
                                     ▼
                              ┌──────────────┐
                              │USB_CONFIGURED│
                              └──────┬───────┘
                                     │
                                     │ Suspend
                                     ▼
                              ┌──────────────┐
                              │USB_SUSPENDED │
                              └──────────────┘
```

### State Definitions

| State | Value | Description |
|-------|-------|-------------|
| `USB_ATTACHED` | 0 | Device attached to bus, VBUS not present |
| `USB_POWERED` | 1 | Device powered, VBUS present |
| `USB_DEFAULT` | 2 | Device powered, waiting for reset (Address = 0) |
| `USB_ADDRESS` | 3 | Device has received address, not configured |
| `USB_CONFIGURED` | 4 | Device configured, ready for data transfers |
| `USB_SUSPENDED` | 5 | Device in low-power suspend mode |

---

## Controller State Machine

```
                    ┌───────────────┐
                    │  HC_STOPPED   │
                    └───────┬───────┘
                            │ HC_CMD_RUN
                            ▼
                    ┌───────────────┐
                    │  HC_RUNNING   │
                    └───────┬───────┘
                            │ Token written to REG_TOKEN
                            ▼
                    ┌───────────────┐
                    │ HC_OPERATION  │◄──────────────┐
                    └───────┬───────┘               │
                            │ Transaction Complete   │
                            ▼                       │
                    ┌───────────────┐               │
                    │  HC_RUNNING   │───────────────┘
                    └───────┬───────┘
                            │ HC_CMD_RESET
                            ▼
                    ┌───────────────┐
                    │   HC_RESET    │
                    └───────┬───────┘
                            │ Reset Complete
                            ▼
                    ┌───────────────┐
                    │  HC_STOPPED   │
                    └───────────────┘

                    On Error:
                    ┌───────────────┐
                    │   HC_ERROR    │
                    └───────────────┘
```

### Controller States

| State | Description |
|-------|-------------|
| `HC_STOPPED` | Controller idle, not processing transactions |
| `HC_RUNNING` | Controller enabled, waiting for token |
| `HC_OPERATION` | Controller executing USB transaction |
| `HC_RESET` | Controller resetting internal state |
| `HC_ERROR` | Error occurred during transaction |

---

## Transmission State Machine (Device)

Controls the ordering of USB transactions within a control transfer.

```
                    ┌─────────────┐
                    │ USB_TOKEN   │◄────────────────────┐
                    └──────┬──────┘                     │
                           │ Token packet received      │
                           ▼                            │
                    ┌─────────────┐                     │
                    │  USB_DATA   │────────────────────┘
                    └─────────────┘   Data/status packet
```

### States

| State | Description |
|-------|-------------|
| `USB_TOKEN` | Waiting for token packet (SETUP/IN/OUT/SOF) |
| `USB_DATA` | Waiting for data or status stage packet |
| `USB_NO_DATA` | No data stage (control transfers with no data phase) |

---

## Control State Machine (Device)

Manages the stages of a USB control transfer.

```
┌──────────────┐    SETUP received    ┌───────────────┐
│ USB_CTRL_NONE │────────────────────►│ USB_CTRL_SETUP │
└──────────────┘                      └───────┬───────┘
       ▲                                      │
       │         SETUP received               │ Data transfer (optional)
       │                                      ▼
       │                               ┌───────────────┐
       │                               │  USB_CTRL_DATA │
       │                               └───────┬───────┘
       │                                       │
       │         No DATA stage (SET_ADDRESS)   │ Status transfer
       │         (ctrl_data_skip = true)       ▼
       │                               ┌───────────────┐
       └───────────────────────────────│ USB_CTRL_STATUS│
                                       └───────┬───────┘
                                               │ Complete
                                               ▼
                                        ┌──────────────┐
                                        │ USB_CTRL_NONE │
                                        └──────────────┘
```

### Control States

| State | Description |
|-------|-------------|
| `USB_CTRL_NONE` | Idle, no active control transfer |
| `USB_CTRL_SETUP` | SETUP packet received, processing request |
| `USB_CTRL_DATA` | DATA stage in progress |
| `USB_CTRL_STATUS` | STATUS stage in progress |

---

## Supported Transfers

### Control Transfer (EP0)

The device supports standard control transfers via Endpoint 0.

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│ SETUP   │────►│  DATA   │────►│ STATUS  │────►│  IDLE   │
│  Stage  │     │  Stage  │     │  Stage  │     │         │
└─────────┘     └─────────┘     └─────────┘     └─────────┘
     │                               ▲
     │   (No DATA stage)             │
     │   e.g., SET_ADDRESS           │
     └───────────────────────────────┘
```

#### SETUP Stage
- Host sends `SETUP` token
- Device sends `DATA0` packet containing 8-byte setup request
- Device responds with `ACK`

#### DATA Stage (optional)
- Direction determined by `bmRequestType.D7`:
  - `0`: Host-to-Device (OUT)
  - `1`: Device-to-Host (IN)
- Uses `DATA0`/`DATA1` toggle for synchronization
- **Can be skipped** for requests like `SET_ADDRESS` that have no data phase

#### STATUS Stage
- Opposite direction of DATA stage
- For transfers with DATA stage:
  - Host sends `IN` (for OUT transfers) or `OUT` (for IN transfers)
  - Device responds with `ACK` or status data
- For transfers without DATA stage (e.g., `SET_ADDRESS`):
  - Host sends `IN` token
  - Device responds with `DATA1` ZLP (Zero Length Packet)

### Supported Standard Requests

| Request | bRequest | Description | Status |
|---------|----------|-------------|--------|
| `GET_DESCRIPTOR` | 0x06 | Retrieve descriptor | Implemented |
| `SET_ADDRESS` | 0x05 | Set device address | Implemented |
| `SET_CONFIGURATION` | 0x09 | Set configuration | TODO |

### Supported Descriptor Types

| Type | Value | Description |
|------|-------|-------------|
| `DEVICE` | 0x01 | Device descriptor |
| `CONFIGURATION` | 0x02 | Configuration + Interface descriptors |
| `STRING` | 0x03 | String descriptors |
| `INTERFACE` | 0x04 | Interface descriptor |

---

## Descriptors

### Device Descriptor

Returns device identification and device-level information.

| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 1 | 18 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x01 | DEVICE descriptor |
| 2 | `bcdUSB` | 2 | 0x0200 | USB 2.0 |
| 4 | `bDeviceClass` | 1 | 0x00 | Per-interface class |
| 5 | `bDeviceSubClass` | 1 | 0x00 | - |
| 6 | `bDeviceProtocol` | 1 | 0x00 | - |
| 7 | `bMaxPacketSize0` | 1 | 32 | Max EP0 packet size |
| 8 | `idVendor` | 2 | 0x1234 | Vendor ID |
| 10 | `idProduct` | 2 | 0x5678 | Product ID |
| 12 | `bcdDevice` | 2 | 0x0000 | Device release |
| 14 | `iManufacturer` | 1 | 1 | String index: Manufacturer |
| 15 | `iProduct` | 1 | 2 | String index: Product |
| 16 | `iSerialNumber` | 1 | 0 | No serial number |
| 17 | `bNumConfigurations` | 1 | 1 | Number of configurations |

### Configuration Descriptor

| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 1 | 9 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x02 | CONFIGURATION descriptor |
| 2 | `wTotalLength` | 2 | 18 | Total bytes returned |
| 4 | `bNumInterfaces` | 1 | 1 | Number of interfaces |
| 5 | `bConfigurationValue` | 1 | 1 | Configuration value |
| 6 | `iConfiguration` | 1 | 0 | No string |
| 7 | `bmAttributes` | 1 | 0xC0 | Self-powered, remote wakeup |
| 8 | `bMaxPower` | 1 | 0 | 0mA |

### Interface Descriptor

| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 1 | 9 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x04 | INTERFACE descriptor |
| 2 | `bInterfaceNumber` | 1 | 0 | Interface 0 |
| 3 | `bAlternateSetting` | 1 | 0 | Alternate 0 |
| 4 | `bNumEndpoints` | 1 | 0 | No additional endpoints |
| 5 | `bInterfaceClass` | 1 | 0x00 | Vendor-specific |
| 6 | `bInterfaceSubclass` | 1 | 0x00 | - |
| 7 | `bInterfaceProtocol` | 1 | 0x00 | - |
| 8 | `iInterface` | 1 | 0 | No string |

### String Descriptors

#### Language ID Descriptor (Index 0)
| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 1 | 4 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x03 | STRING descriptor |
| 2 | `wLANGID` | 2 | 0x0409 | English (US) |

#### Manufacturer String (Index 1)
| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 2+2n | 10 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x03 | STRING descriptor |
| 2+ | `bString` | 2n | UTF-16LE | String data |

#### Product String (Index 2)
| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 2+2n | 10 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x03 | STRING descriptor |
| 2+ | `bString` | 2n | UTF-16LE | String data |

---

## Packet Formats

### Token Packet

```
┌──────────┬────────────────────────────────────┐
│  PID(8)  │  ADDR(7)  │  ENDP(4)  │  CRC5(5)  │
└──────────┴───────────┴───────────┴───────────┘
```

### Data Packet

```
┌──────────┬───────────────────────────────────┬──────────┐
│  PID(8)  │            DATA(n)                │ CRC16(16)│
└──────────┴───────────────────────────────────┴──────────┘
```

### Handshake Packet

```
┌──────────┐
│   PID(8) │
└──────────┘
```

### PID Types

#### Token PIDs
| Name | Value | Description |
|------|-------|-------------|
| `PID_TOKEN_OUT` | 0x1 | Addressed to device |
| `PID_TOKEN_IN` | 0x9 | Device-to-host transfer |
| `PID_TOKEN_SOF` | 0x5 | Start of Frame marker |
| `PID_TOKEN_SETUP` | 0xD | Control transfer setup |

#### Data PIDs
| Name | Value | Description |
|------|-------|-------------|
| `PID_DATA_DATA0` | 0x3 | Data packet, even toggle |
| `PID_DATA_DATA1` | 0xB | Data packet, odd toggle |
| `PID_DATA_DATA2` | 0x7 | High-bandwidth ISO (not used) |
| `PID_DATA_MDATA` | 0xF | High-bandwidth ISO (not used) |

#### Handshake PIDs
| Name | Value | Description |
|------|-------|-------------|
| `PID_HANDSHAKE_ACK` | 0x2 | Packet accepted |
| `PID_HANDSHAKE_NAK` | 0xA | Cannot accept (busy) |
| `PID_HANDSHAKE_STALL` | 0xE | Request not supported |
| `PID_HANDSHAKE_NYET` | 0x6 | Not yet (HS-split) |

---

## TLM Interface

### CPU Module

| Socket | Type | Direction | Purpose |
|--------|------|-----------|---------|
| `socket` | `simple_initiator_socket` | Outbound | Write to Controller registers, poll status |
| `dma` | `simple_target_socket` | Inbound | DMA writes from Controller (descriptor data) |

### Controller Module

| Socket | Type | Direction | Purpose |
|--------|------|-----------|---------|
| `cpu_in_sock` | `simple_target_socket` | Inbound | Receive register writes/reads from CPU |
| `dev_out_sock` | `simple_initiator_socket` | Outbound | Send USB packets to Device |
| `dma_sock` | `simple_initiator_socket` | Outbound | DMA read/write to CPU RAM |

### Device Module

| Socket | Type | Direction | Purpose |
|--------|------|-----------|---------|
| `target` | `simple_target_socket` | Inbound | Receive USB packets from Controller |

### Transaction Format

Transactions use `tlm::tlm_generic_payload` with the following data pointer types:

| Phase | Data Pointer Type | Description |
|-------|-------------------|-------------|
| Token | `token_t*` | PID + Address + Endpoint + CRC |
| Data | `data_t*` | PID + Data pointer + Length + CRC |
| Handshake | `handshake_t*` | PID only |

### Controller Register Map

| Offset | Register | Access | Description |
|--------|----------|--------|-------------|
| 0x00 | `REG_USB_CMD` | R/W | Bit 0: Run/Stop, Bit 1: Reset |
| 0x04 | `REG_USB_STS` | R | Bit 0: Idle, Bit 1: Error, Bit 2: Transaction Complete, Bit 3: Busy, Bit 4: Stopped |
| 0x08 | `REG_PORT_SC` | R/W | Bit 0: Connect, Bit 1: Port Reset |
| 0x0C | `REG_ADDR_ENDP` | R/W | [6:0] Address, [10:7] Endpoint |
| 0x10 | `REG_DATA_PTR` | R/W | Pointer to System RAM (DMA Address) |
| 0x14 | `REG_TOKEN` | W | Write to trigger: 0=SETUP, 1=IN, 2=OUT |

### Response Status

| Status | Meaning |
|--------|---------|
| `TLM_OK_RESPONSE` | Transaction successful |
| `TLM_GENERIC_ERROR_RESPONSE` | Error occurred |
| `TLM_ADDRESS_ERROR_RESPONSE` | DMA address out of bounds |

---

## Configuration

### Build Configuration

| Parameter | Value | Location |
|-----------|-------|----------|
| `MAX_PACKET_SIZE` | 32 | `common/packet.h:6` |
| Vendor ID | 0x1234 | `device/device.h:149` |
| Product ID | 0x5678 | `device/device.h:150` |
| USB Version | 2.0 (0x0200) | `device/device.h:16` |
| CPU RAM Size | 64 bytes | `cpu/cpu.h:10` |

### String Customization

Edit the string descriptor macros in `device/device.h`:

```cpp
// Manufacturer string (UTF-16LE, 4 characters)
#define STRING_VEN_B_LANG_ID "DEAD"

// Product string (UTF-16LE, 4 characters)
#define STRING_PROD_B_LANG_ID "BEEF"
```

---

## Limitations

- **No endpoint descriptors**: Only EP0 is supported
- **No CRC validation**: Token and data CRC are not verified
- **No suspend/resume**: `USB_SUSPENDED` state not implemented
- **No interrupt/bulk/isochronous transfers**: Control only
- **No stall handling**: STALL response not fully implemented
- **No address filtering**: Accepts packets for any address (partially implemented)

---

## Future Work

### High Priority
- [ ] Implement `SET_CONFIGURATION` request handler
- [ ] Add `GET_DESCRIPTOR` for CONFIGURATION type with endpoint descriptors
- [ ] Implement explicit ACK handshake responses (replace implicit TLM_OK)
- [ ] Error status handling in CPU firmware (`cpu.cpp:66`, `cpu.cpp:121`)

### Medium Priority
- [ ] Return STALL for unsupported requests in STATUS stage (`device.cpp:10`)
- [ ] Add endpoint descriptors (`device.cpp:211`)
- [ ] Set transaction data length properly (`device.cpp:298`)
- [ ] Implement port control functionality (`controller.cpp:101`)
- [ ] Add NAK/STALL responses
- [ ] Implement data toggle synchronization with controller
- [ ] Add CRC validation
- [ ] Complete address filtering

### Low Priority
- [ ] HUB functionality (`controller.h:51`)
- [ ] HUB IN endpoint for reporting device attachment events (`controller.h:52`)
- [ ] HOST periodic polling to learn new or removed devices (`controller.h:54`)
- [ ] Endpoint descriptors (EP0 IN/OUT)
- [ ] Bulk endpoint support
- [ ] Interrupt endpoint handling
- [ ] Suspend/resume support
- [ ] Testbench with enumeration sequence

---

## File Structure

```
usb2.0/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── main.cpp                 # Entry point, module instantiation
├── common/
│   ├── common.h             # Device states, control states, transmission states
│   ├── packet.h             # USB packet structures, PIDs, setup request
│   └── log.h                # Logging macros (DEBUG, INFO, ERROR)
├── controller/
│   ├── controller.h         # Host controller class, registers, states
│   └── controller.cpp       # Controller implementation, DMA, USB transactions
├── cpu/
│   ├── cpu.h                # CPU class with RAM, sockets
│   └── cpu.cpp              # CPU firmware, DMA handler
└── device/
    ├── device.h             # Device class, descriptors
    ├── device.cpp           # Device implementation, state machines
    ├── device_tb.h          # Device testbench header
    └── device_tb.cpp        # Device testbench implementation
```

---

## Building

```bash
# Configure
mkdir build && cd build
cmake -DSYSTEMC_HOME=$SYSTEMC_HOME ..

# Build
make -j$(nproc)

# Run
./usb
```

---

## License

[To be determined]
