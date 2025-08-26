#!/bin/bash

# paperd.ink TRMNL Firmware Build Script
# This script helps build the firmware using different methods

echo "=== paperd.ink TRMNL Firmware Build Script ==="
echo "Version: 1.0.0"
echo ""

# Check if PlatformIO is available
export PATH=$PATH:/Users/p340344/.platformio/penv/bin
if command -v pio &> /dev/null; then
    echo "✅ PlatformIO found"
    echo "Building with PlatformIO..."

    # Clean previous build
    pio run -t clean

    # Build firmware
    pio run -e paperdink_trmnl

    if [ $? -eq 0 ]; then
        echo "✅ Build successful!"
        echo "Firmware binary: .pio/build/paperdink_trmnl/firmware.bin"
        echo ""
        echo "To upload:"
        echo "  pio run -e paperdink_trmnl -t upload"
        echo ""
        echo "To monitor serial:"
        echo "  pio device monitor -b 115200"
    else
        echo "❌ Build failed!"
        exit 1
    fi

elif command -v arduino-cli &> /dev/null; then
    echo "✅ Arduino CLI found"
    echo "Building with Arduino CLI..."

    # Install ESP32 core if not present
    arduino-cli core update-index
    arduino-cli core install esp32:esp32

    # Install required libraries
    arduino-cli lib install "ArduinoJson"
    arduino-cli lib install "GxEPD2"
    arduino-cli lib install "Adafruit GFX Library"

    # Create Arduino sketch structure
    mkdir -p arduino_build/paperdink_trmnl

    # Copy and rename main.cpp to .ino
    cp src/main.cpp arduino_build/paperdink_trmnl/paperdink_trmnl.ino

    # Copy other source files
    cp src/*.cpp arduino_build/paperdink_trmnl/
    cp -r include arduino_build/paperdink_trmnl/

    # Build
    arduino-cli compile --fqbn esp32:esp32:esp32 arduino_build/paperdink_trmnl

    if [ $? -eq 0 ]; then
        echo "✅ Build successful!"
        echo "Firmware binary: arduino_build/paperdink_trmnl/build/esp32.esp32.esp32/paperdink_trmnl.ino.bin"
    else
        echo "❌ Build failed!"
        exit 1
    fi

else
    echo "❌ Neither PlatformIO nor Arduino CLI found!"
    echo ""
    echo "Please install one of the following:"
    echo ""
    echo "1. PlatformIO (recommended):"
    echo "   https://platformio.org/install"
    echo ""
    echo "2. Arduino CLI:"
    echo "   https://arduino.github.io/arduino-cli/installation/"
    echo ""
    echo "3. Arduino IDE:"
    echo "   https://www.arduino.cc/en/software"
    echo "   Then manually open src/main.cpp and compile"
    exit 1
fi

echo ""
echo "=== Build Complete ==="