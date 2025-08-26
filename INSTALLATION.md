# Installation Guide - paperd.ink TRMNL Firmware

This guide will walk you through the complete installation process of the TRMNL firmware on your paperd.ink device.

## Prerequisites

### Hardware Requirements
- paperd.ink device (Classic or Merlot)
- USB-C cable for programming
- Computer with USB port
- MicroSD card (optional, for caching)

### Software Requirements
- [PlatformIO](https://platformio.org/) (recommended) or [Arduino IDE](https://www.arduino.cc/en/software)
- Git for cloning the repository
- TRMNL account at [usetrmnl.com](https://usetrmnl.com)

## Step 1: Environment Setup

### Option A: PlatformIO (Recommended)

1. **Install Visual Studio Code**
   ```bash
   # Download from https://code.visualstudio.com/
   ```

2. **Install PlatformIO Extension**
   - Open VS Code
   - Go to Extensions (Ctrl+Shift+X)
   - Search for "PlatformIO IDE"
   - Install the extension

3. **Verify Installation**
   ```bash
   pio --version
   ```

### Option B: Arduino IDE

1. **Install Arduino IDE**
   - Download from https://www.arduino.cc/en/software
   - Install for your operating system

2. **Add ESP32 Board Support**
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to Additional Board Manager URLs:
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search for "ESP32" and install

3. **Install Required Libraries**
   - ArduinoJson (by Benoit Blanchon)
   - GxEPD2 (by Jean-Marc Zingg)
   - Adafruit GFX Library
   - WiFiClientSecure (built-in)
   - HTTPClient (built-in)

## Step 2: Get the Code

### Clone Repository
```bash
git clone https://github.com/yourusername/paperdink2trmnl.git
cd paperdink2trmnl
```

### Verify Project Structure
```
paperdink2trmnl/
├── platformio.ini
├── src/
├── include/
├── lib/
└── test/
```

## Step 3: Hardware Preparation

### 1. Prepare paperd.ink Device
- Ensure device is fully charged
- Remove any existing firmware if needed
- Locate the BOOT button on the device

### 2. Connect to Computer
- Use USB-C cable to connect paperd.ink to computer
- Device should appear as a serial port

### 3. Enter Programming Mode
- Power off the device
- Hold down the BOOT button
- Power on the device while holding BOOT
- Release BOOT button after 2 seconds
- Device is now in programming mode

## Step 4: Compile and Upload

### Using PlatformIO

1. **Open Project**
   ```bash
   cd paperdink2trmnl
   code .  # Opens VS Code
   ```

2. **Build Project**
   ```bash
   pio run -e paperdink_trmnl
   ```

3. **Upload Firmware**
   ```bash
   pio run -e paperdink_trmnl -t upload
   ```

4. **Monitor Serial Output**
   ```bash
   pio device monitor -b 115200
   ```

### Using Arduino IDE

1. **Open Project**
   - Open `src/main.cpp` in Arduino IDE
   - Copy all `.h` files from `include/` to sketch folder

2. **Configure Board**
   - Tools → Board → ESP32 Arduino → ESP32 Dev Module
   - Tools → Port → Select your device port
   - Tools → Upload Speed → 921600

3. **Compile and Upload**
   - Click Verify (✓) to compile
   - Click Upload (→) to flash firmware

## Step 5: Initial Configuration

### 1. First Boot
- Power cycle the device (off/on)
- Device should show startup screen
- Look for "paperdink TRMNL" text

### 2. WiFi Setup
- Device creates access point: `paperdink-setup-XXXXXX`
- Connect to this network with password: `paperdink123`
- Open browser and go to `192.168.4.1`
- Enter your WiFi credentials

### 3. TRMNL Account Linking
- Create account at [usetrmnl.com](https://usetrmnl.com)
- Add new device in dashboard
- Enter your paperd.ink MAC address (shown on device)
- Configure plugins and playlists

## Step 6: Verification

### Test Basic Functions
1. **Display Test**
   - Device should show TRMNL content
   - Press Button 1 for manual refresh

2. **Button Test**
   - Button 1: Manual refresh
   - Button 3 (long press): Status screen
   - Button 4: Sleep mode

3. **WiFi Test**
   - Check connection status
   - Verify internet connectivity

### Check Serial Output
```bash
pio device monitor -b 115200
```
Look for:
- WiFi connection success
- TRMNL API registration
- Content download messages

## Troubleshooting Installation

### Common Upload Issues

**"Failed to connect to ESP32"**
- Ensure device is in programming mode
- Try different USB cable
- Check driver installation

**"Permission denied" (Linux/Mac)**
```bash
sudo chmod 666 /dev/ttyUSB0  # or your device port
```

**"Compilation errors"**
- Verify all libraries are installed
- Check PlatformIO version compatibility
- Clean build cache: `pio run -t clean`

### Hardware Issues

**Device not responding**
- Check battery charge level
- Verify USB connection
- Try hardware reset (hold all buttons for 10s)

**Display not updating**
- Check display cable connections
- Verify power management settings
- Test with simple display commands

### Network Issues

**WiFi connection fails**
- Use 2.4GHz network only
- Check password accuracy
- Try factory reset if needed

**TRMNL registration fails**
- Verify MAC address in dashboard
- Check internet connectivity
- Ensure TRMNL service is online

## Advanced Installation Options

### Custom Configuration
Edit `include/config.h` before compilation:
```cpp
#define DEEP_SLEEP_DURATION_SECONDS 1800  // 30 minutes
#define WIFI_CONNECT_TIMEOUT_MS 30000     // 30 seconds
#define DEBUG_ENABLED true                // Enable debug output
```

### OTA Updates (Future)
Once initial firmware is installed:
```bash
pio run -e paperdink_trmnl -t uploadfs  # Upload filesystem
```

### Development Mode
For development and testing:
```cpp
#define DEBUG_ENABLED true
#define CACHE_ENABLED false
```

## Post-Installation

### Regular Maintenance
- Monitor battery levels
- Check for firmware updates
- Clean SD card cache periodically

### Customization
- Configure refresh intervals in TRMNL dashboard
- Set up custom plugins
- Adjust power management settings

## Getting Help

If you encounter issues:
1. Check the [Troubleshooting Guide](TROUBLESHOOTING.md)
2. Review serial output for error messages
3. Open GitHub issue with detailed information
4. Join TRMNL community for support

## Next Steps
- Configure your first TRMNL plugins
- Set up playlists and schedules
- Explore advanced features like offline mode
- Consider contributing to the project

---
*Installation complete! Your paperd.ink device is now running TRMNL firmware.*