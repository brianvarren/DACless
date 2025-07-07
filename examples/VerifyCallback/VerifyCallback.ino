/*
  DACless Test Example
  Tests that the callback system is working correctly
*/

#include <DACless.h>

DAClessAudio audio;

// Test with constant value to verify output
uint16_t testAudioSample() {
    return 4095;  // Full scale output - should see ~3.3V DC on scope
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port
    }
    
    Serial.println("DACless Callback Test");
    Serial.println("Output should be constant 3.3V DC");
    
    // Set the audio callback
    audio.setAudioSampleCallback(testAudioSample);
    
    // Initialize the audio system
    audio.begin();
    
    // Unmute the output
    audio.unmute();
    
    Serial.println("Audio started - check GPIO 6 with oscilloscope");
}

void loop() {
    // Blink LED to show we're running
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}