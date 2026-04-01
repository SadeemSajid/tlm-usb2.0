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

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         CPU Module                          │
│                   (Baremetal Firmware)                      │
└────────────────────────┬────────────────────────────────────┘
                         │ TLM Initiator Socket
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                     Host Controller                         │
│                                                         │
└────────────────────────┬────────────────────────────────────┘
                         │ TLM Initiator Socket
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                    USB Device Module                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Descriptors │  │ State       │  │ Transmission        │  │
│  │ - Device    │  │ Machine     │  │ State Machine       │  │
│  │ - Config    │  │             │  │                     │  │
│  │ - Interface │  │             │  │                     │  │
│  │ - Strings   │  │             │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└────────────────────────┬────────────────────────────────────┘
                         │ TLM Target Socket
```

### Module Hierarchy

| Module | File | Purpose |
|--------|------|---------|
| `USB_Device` | `device/device.cpp`, `device.h` | Main USB device implementation |
| `USB_Device_TB` | `device/device_tb.cpp`, `device_tb.h` | Device testbench |
| `Controller` | `controller/controller.h` | Host controller (placeholder) |
| `CPU` | `cpu/cpu.h` | CPU module (placeholder) |

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
                              │ USB_CONFIGURED│
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

## Transmission State Machine

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

## Control State Machine

Manages the stages of a USB control transfer.

```
┌──────────────┐    SETUP received    ┌───────────────┐
│ USB_CTRL_NONE │────────────────────►│ USB_CTRL_SETUP │
└──────────────┘                      └───────┬───────┘
       ▲                                      │
       │         SETUP received               │ Data transfer
       │                                      ▼
       │                               ┌───────────────┐
       └──────────────────────────────│  USB_CTRL_DATA │
                                      └───────┬───────┘
                                              │
                                              │ Status transfer
                                              ▼
                                      ┌───────────────┐
                                      │ USB_CTRL_STATUS│
                                      └───────┬───────┘
                                              │
                                              │ Complete
                                              ▼
                                       ┌──────────────┐
                                       │ USB_CTRL_NONE│
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

#### STATUS Stage
- Opposite direction of DATA stage
- Host sends `IN` (for OUT transfers) or `OUT`/`SETUP` (for IN transfers)
- Device responds with `ACK` or status data

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
| 0 | `bLength` | 2+2n | 22 | Descriptor length |
| 1 | `bDescriptorType` | 1 | 0x03 | STRING descriptor |
| 2+ | `bString` | 2n | UTF-16LE | String data |

#### Product String (Index 2)
| Offset | Field | Size | Value | Description |
|--------|-------|------|-------|-------------|
| 0 | `bLength` | 2+2n | 24 | Descriptor length |
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

### Target Socket

The device exposes a `tlm_utils::simple_target_socket` for receiving transactions from the host controller.

```cpp
tlm_utils::simple_target_socket<USB_Device> target;
```

### Transaction Format

Transactions use `tlm::tlm_generic_payload` with the following data pointer types:

| Phase | Data Pointer Type | Description |
|-------|-------------------|-------------|
| Token | `token_t*` | PID + Address + Endpoint + CRC |
| Data | `data_t*` | PID + Data pointer + Length + CRC |
| Handshake | `handshake_t*` | PID only |

### Response Status

| Status | Meaning |
|--------|---------|
| `TLM_OK_RESPONSE` | Transaction successful |
| `TLM_GENERIC_ERROR_RESPONSE` | Error occurred |

---

## Configuration

### Build Configuration

| Parameter | Value | Location |
|-----------|-------|----------|
| `MAX_PACKET_SIZE` | 32 | `common/packet.h:6` |
| Vendor ID | 0x1234 | `device/device.h:147` |
| Product ID | 0x5678 | `device/device.h:148` |
| USB Version | 2.0 (0x0200) | `device/device.h:16` |

### String Customization

Edit the string descriptor macros in `device/device.h`:

```cpp
// Manufacturer string (UTF-16LE, 11 characters)
#define STRING_VEN_B_LANG_ID "Test Vendor"

// Product string (UTF-16LE, 12 characters)
#define STRING_PROD_B_LANG_ID "Test Product"
```

---

## Limitations

- **No endpoint descriptors**: Only EP0 is supported
- **No CRC validation**: Token and data CRC are not verified
- **No suspend/resume**: `USB_SUSPENDED` state not implemented
- **No interrupt/bulk/isochronous transfers**: Control only
- **No stall handling**: STALL response not implemented
- **No address filtering**: Accepts packets for any address

---

## Future Work

### High Priority
- [x] Implement `SET_ADDRESS` request handler
- [ ] Implement `SET_CONFIGURATION` request handler
- [x] Add `GET_DESCRIPTOR` for STRING type
- [ ] Add `GET_DESCRIPTOR` for CONFIGURATION type
- [x] Implement state transitions

### Medium Priority
- [ ] Add NAK/STALL responses
- [ ] Implement data toggle synchronization with controller
- [ ] Add CRC validation
- [ ] Implement address filtering
- [ ] Complete host controller module

### Low Priority
- [ ] Endpoint descriptors (EP0 IN/OUT)
- [ ] Bulk endpoint support
- [ ] DMA support
- [ ] Interrupt endpoint handling
- [ ] Suspend/resume support
- [ ] Testbench with enumeration sequence

---

## File Structure

```
usb2.0/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── main.cpp                 # Entry point
├── common/
│   ├── common.h             # Device states, control states
│   └── packet.h             # USB packet structures, PIDs
├── controller/
│   └── controller.h         # Host controller (placeholder)
├── cpu/
│   └── cpu.h                # CPU module (placeholder)
└── device/
    ├── device.h             # Device class, descriptors
    ├── device.cpp           # Device implementation
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
