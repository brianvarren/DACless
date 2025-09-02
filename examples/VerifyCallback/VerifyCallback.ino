/*
  DACless Test Example - PWM Only Version
  Tests that the callback system is working correctly
*/

#include "DACless.h"

// Configure DACless with default settings
DAClessConfig config;
DAClessAudio audio(config);

// Test with different constant values to verify output
uint16_t testAudioSample(void* userData) {
    static uint32_t counter = 0;
    static uint16_t testValue = 0;
    
    counter++;
    
    // Change test value every 2 seconds (approximately)
    if (counter > (uint32_t)(audio.getSampleRate() * 2)) {
        counter = 0;
        
        // Cycle through different test values
        static uint8_t testPhase = 0;
        switch (testPhase) {
            case 0:
                testValue = 0;        // 0V (minimum)
                Serial.println("Output: 0V (minimum PWM)");
                break;
            case 1:
                testValue = 1365;     // ~1.1V (1/3 scale)
                Serial.println("Output: ~1.1V (1/3 scale)");
                break;
            case 2:
                testValue = 2048;     // ~1.65V (midpoint)
                Serial.println("Output: ~1.65V (midpoint)");
                break;
            case 3:
                testValue = 2731;     // ~2.2V (2/3 scale)
                Serial.println("Output: ~2.2V (2/3 scale)");
                break;
            case 4:
                testValue = 4095;     // ~3.3V (maximum)
                Serial.println("Output: ~3.3V (maximum PWM)");
                break;
        }
        testPhase = (testPhase + 1) % 5;
    }
    
    return testValue;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("DACless Callback Test - PWM Only Version");
    Serial.println("======================================");
    Serial.print("PWM Resolution: ");
    Serial.print(config.pwmBits);
    Serial.println(" bits");
    Serial.print("PWM Pin: GPIO ");
    Serial.println(config.pinPWM);
    Serial.println("Output will cycle through different DC levels");
    Serial.println("Check with oscilloscope or multimeter on GPIO 6");
    Serial.println();
    
    // Set the sample callback function
    audio.setSampleCallback(testAudioSample, nullptr);
    
    // Initialize the audio system
    audio.begin();
    
    // Unmute the output
    audio.unmute();
    
    Serial.print("Audio started at ");
    Serial.print(audio.getSampleRate());
    Serial.println(" Hz");
    Serial.println("First test: 0V (minimum PWM)");
}

void loop() {
    // Blink LED to show we're running
    static bool ledState = false;
    static unsigned long lastBlink = 0;
    
    if (millis() - lastBlink > 500) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
    
    delay(100);
}