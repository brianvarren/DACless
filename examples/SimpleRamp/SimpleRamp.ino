/*
  DACless Simple Example
  Generates a simple ramp wave using per-sample callback
*/

#include <DACless.h>

DAClessAudio audio;

// Audio generation function
uint16_t myAudioSample() {
    static uint16_t t = 0;
    // Generate a simple ramp from 0 to 4095
    return (t++ & 0xFFF);
}

void setup() {
    Serial.begin(115200);
    
    // Set the audio callback
    audio.setAudioSampleCallback(myAudioSample);
    
    // Initialize the audio system
    audio.begin();
    
    // Unmute the output
    audio.unmute();
    
    Serial.print("Audio running at ");
    Serial.print(audio.getSampleRate());
    Serial.println(" Hz");
}

void loop() {
    // Nothing needed here - audio runs in the background!
    delay(1000);
    
    // Optional: Read and display ADC values
    Serial.print("ADC values: ");
    for (int i = 0; i < 4; i++) {
        Serial.print(audio.getADC(i));
        Serial.print(" ");
    }
    Serial.println();
}