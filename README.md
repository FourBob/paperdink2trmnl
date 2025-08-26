# paperd.ink TRMNL Firmware

A TRMNL-compatible firmware for the paperd.ink E-Paper device with extended features.

## Features

### 🎯 TRMNL Compatibility
- Complete TRMNL API integration
- Automatic device registration
- Screen updates via TRMNL server
- OTA firmware updates

### 🔘 4-Button Navigation
- **Button 1**: Manual refresh / Wake up
- **Button 2**: Playlist/Screen switching (future)
- **Button 3**: Status/Configuration (long press)
- **Button 4**: Sleep mode / Factory reset (very long press)

### 💾 SD Card Features
- Local caching of screen content
- Offline mode when internet connection is unavailable
- Extended storage capabilities
- Debug logs and statistics

### 🔋 Advanced Power Management
- Intelligent sleep modes
- Battery monitoring with percentage display
- Charging status detection
- Critical battery warning

### 🎨 Tri-Color Display Support
- Optimized rendering for 3-color E-Paper
- Advanced dithering algorithms
- Partial updates for faster refresh

## Hardware Specifications

### paperd.ink Device
- **MCU**: ESP32-WROOM-32
- **Display**: 400x300px E-Paper (Monochrome/Tri-Color)
- **Buttons**: 4x Tactile buttons
- **Storage**: MicroSD card slot
- **Battery**: 1900mAh LiPo with charging circuit
- **Connectivity**: WiFi & Bluetooth

### Pin Assignment
```
E-Paper Display:
- CS: GPIO22, DC: GPIO15, BUSY: GPIO34, RESET: GPIO13, ENABLE: GPIO12

SD Card:
- CS: GPIO21, ENABLE: GPIO5

Buttons:
- Button 1: GPIO14, Button 2: GPIO27, Button 3: GPIO4, Button 4: GPIO2

I2C:
- SDA: GPIO16, SCL: GPIO17

SPI:
- SCK: GPIO18, MOSI: GPIO23, MISO: GPIO19

Power Management:
- Battery Voltage: GPIO39, Battery Enable: GPIO25, Charging: GPIO36

Buzzer:
- GPIO26
```

## Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) or [Arduino IDE](https://www.arduino.cc/en/software)
- paperd.ink device
- USB-C cable
- TRMNL account (https://usetrmnl.com)

### 1. Clone Repository
```bash
git clone https://github.com/yourusername/paperdink2trmnl.git
cd paperdink2trmnl
```

### 2. Compile with PlatformIO
```bash
# Open project
pio project init

# Compile
pio run -e paperdink_trmnl

# Upload
pio run -e paperdink_trmnl -t upload
```

### 3. Using Arduino IDE
1. Open Arduino IDE
2. Install ESP32 board package
3. Install libraries (see platformio.ini)
4. Select board: "ESP32 Dev Module"
5. Compile and upload sketch

### 4. Initial Setup
1. Power on device
2. Look for WiFi network "paperdink-setup-XXXXXX"
3. Connect and open browser (192.168.4.1)
4. Enter WiFi credentials
5. Link TRMNL account

## Configuration

### WiFi Setup
The device creates an access point on first boot:
- **SSID**: `paperdink-setup-[MAC]`
- **Password**: `paperdink123`
- **IP**: `192.168.4.1`

### TRMNL Integration
1. Create TRMNL account: https://usetrmnl.com
2. Add device in TRMNL dashboard
3. Enter paperd.ink MAC address
4. Configure plugins and playlists

### Button Functions
- **Short press Button 1**: Immediate content refresh
- **Short press Button 2**: Next screen (future)
- **Long press Button 3**: Show status screen
- **Short press Button 4**: Activate sleep mode
- **Very long press Button 4**: Factory reset (during boot)

## Advanced Features

### Offline Mode
When internet connection is unavailable, the device automatically displays cached content.

### SD Card Usage
- Automatic caching of last 10 screen contents
- Local storage of debug logs
- Possibility for custom content

### Power Management
- Automatic sleep after inactivity
- Configurable refresh intervals
- Battery protection at critical charge level

## Development

### Project Structure
```
paperdink2trmnl/
├── src/
│   ├── main.cpp              # Main program
│   ├── paperdink_hardware.cpp # Hardware abstraction
│   └── trmnl_client.cpp      # TRMNL API client
├── include/
│   ├── config.h              # Configuration
│   ├── paperdink_hardware.h  # Hardware header
│   └── trmnl_client.h        # TRMNL client header
├── test/                     # Unit tests
├── platformio.ini            # PlatformIO configuration
└── min_spiffs.csv           # Partition table
```

### Debugging
Debug output via Serial (115200 baud):
```cpp
#define DEBUG_ENABLED true
```

### Running Tests
```bash
pio test -e native
```

## API Reference

### TRMNL API Endpoints
- **Setup**: `GET /api/setup` - Device registration
- **Display**: `GET /api/display` - Fetch content
- **Logs**: `POST /api/logs` - Send debug logs

### Hardware API
See `include/paperdink_hardware.h` for complete API documentation.

## Troubleshooting

### Common Issues

**WiFi connection fails**
- Check SSID and password
- Use 2.4GHz network
- Perform factory reset

**Display shows nothing**
- Check battery charge
- Verify display cable
- Perform hardware reset

**TRMNL registration fails**
- Enter MAC address correctly in TRMNL dashboard
- Check internet connection
- Verify API status on usetrmnl.com

### Factory Reset
1. Power off device
2. Hold Button 4
3. Power on device
4. Keep holding Button 4 (>5 seconds)
5. Wait for confirmation beeps

## License

GPL-3.0 License - see LICENSE file

## Contributing

Pull requests are welcome! For major changes, please open an issue first.

## Support

- GitHub Issues: https://github.com/yourusername/paperdink2trmnl/issues
- TRMNL Community: https://usetrmnl.com
- paperd.ink Documentation: https://docs.paperd.ink