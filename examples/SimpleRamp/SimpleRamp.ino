/*
  DACless Simple Example - PWM Only Version
  Generates a simple ramp wave using per-sample callback
*/

#include "DACless.h"

// Configure DACless with default settings
DAClessConfig config;
DAClessAudio audio(config);

// Audio generation function - called for each sample
uint16_t myAudioSample(void* userData) {
    static uint16_t t = 0;
    
    // Generate different waveforms based on time
    static uint32_t counter = 0;
    static uint8_t waveform = 0;
    
    counter++;
    if (counter > 44100) { // Change waveform every second (approximately)
        counter = 0;
        waveform = (waveform + 1) % 4;
        
        const char* waveNames[] = {"Ramp Up", "Ramp Down", "Triangle", "Square"};
        Serial.print("Switching to: ");
        Serial.println(waveNames[waveform]);
    }
    
    uint16_t sample;
    switch (waveform) {
        case 0: // Ramp up (sawtooth)
            sample = t & 0xFFF;
            break;
            
        case 1: // Ramp down
            sample = (~t) & 0xFFF;
            break;
            
        case 2: // Triangle
            if (t & 0x800) {
                sample = ((~t) & 0x7FF) << 1;
            } else {
                sample = (t & 0x7FF) << 1;
            }
            break;
            
        case 3: // Square wave
            sample = (t & 0x800) ? 4095 : 0;
            break;
            
        default:
            sample = 2048; // DC midpoint
            break;
    }
    
    t++;
    return sample;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("DACless Simple Ramp Example - PWM Only");
    
    // Set the sample callback function
    audio.setSampleCallback(myAudioSample, nullptr);
    
    // Initialize the audio system
    audio.begin();
    
    // Unmute the output
    audio.unmute();
    
    Serial.print("Audio running at ");
    Serial.print(audio.getSampleRate());
    Serial.println(" Hz");
    Serial.println("Listen to GPIO 6 for cycling waveforms");
}

void loop() {
    // Just blink the LED to show we're alive
    static bool ledState = false;
    static unsigned long lastBlink = 0;
    
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
    
    delay(100);
}