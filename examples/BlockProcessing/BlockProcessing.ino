/*
  DACless Block Processing Example - PWM Only Version
  Generates a sine wave with frequency that automatically sweeps
*/

#include "DACless.h"

// Configure DACless with default settings
DAClessConfig config;
DAClessAudio audio(config);

// Sine wave lookup table (quarter wave, 12-bit resolution)
const uint16_t sineTable[256] = {
    2048, 2098, 2148, 2198, 2248, 2298, 2348, 2398, 2447, 2497, 2546, 2595, 2644, 2693, 2741, 2790,
    2838, 2886, 2933, 2981, 3028, 3075, 3121, 3168, 3214, 3259, 3305, 3350, 3394, 3439, 3483, 3526,
    3569, 3612, 3655, 3697, 3738, 3779, 3820, 3860, 3900, 3940, 3979, 4017, 4055, 4093, 4130, 4166,
    4202, 4238, 4273, 4308, 4342, 4375, 4408, 4441, 4473, 4504, 4535, 4565, 4595, 4624, 4653, 4681,
    4708, 4735, 4762, 4787, 4813, 4837, 4861, 4885, 4908, 4930, 4952, 4973, 4993, 5013, 5033, 5052,
    5070, 5087, 5104, 5121, 5137, 5152, 5167, 5181, 5194, 5207, 5220, 5231, 5243, 5253, 5263, 5273,
    5282, 5290, 5298, 5305, 5312, 5318, 5324, 5329, 5333, 5337, 5341, 5344, 5346, 5348, 5349, 5350,
    5350, 5350, 5349, 5348, 5346, 5344, 5341, 5337, 5333, 5329, 5324, 5318, 5312, 5305, 5298, 5290,
    5282, 5273, 5263, 5253, 5243, 5231, 5220, 5207, 5194, 5181, 5167, 5152, 5137, 5121, 5104, 5087,
    5070, 5052, 5033, 5013, 4993, 4973, 4952, 4930, 4908, 4885, 4861, 4837, 4813, 4787, 4762, 4735,
    4708, 4681, 4653, 4624, 4595, 4565, 4535, 4504, 4473, 4441, 4408, 4375, 4342, 4308, 4273, 4238,
    4202, 4166, 4130, 4093, 4055, 4017, 3979, 3940, 3900, 3860, 3820, 3779, 3738, 3697, 3655, 3612,
    3569, 3526, 3483, 3439, 3394, 3350, 3305, 3259, 3214, 3168, 3121, 3075, 3028, 2981, 2933, 2886,
    2838, 2790, 2741, 2693, 2644, 2595, 2546, 2497, 2447, 2398, 2348, 2298, 2248, 2198, 2148, 2098,
    2048, 1997, 1947, 1897, 1847, 1797, 1747, 1697, 1648, 1598, 1549, 1500, 1451, 1402, 1354, 1305,
    1257, 1209, 1162, 1114, 1067, 1020, 974, 927, 881, 836, 790, 745, 701, 656, 612, 569
};

uint32_t phase = 0;
uint32_t phaseIncrement = 2000; // Base frequency
uint32_t sweepCounter = 0;

// Get sine value with full wave reconstruction
uint16_t getSineValue(uint32_t phase) {
    uint16_t index = (phase >> 24) & 0xFF;
    uint16_t quadrant = (phase >> 30) & 0x03;
    uint16_t value;
    
    if (quadrant == 0) {
        value = sineTable[index];
    } else if (quadrant == 1) {
        value = sineTable[255 - index];
    } else if (quadrant == 2) {
        value = 4096 - sineTable[index];
    } else {
        value = 4096 - sineTable[255 - index];
    }
    
    // Scale to 12-bit range (0-4095)
    return (value * 4095) / 5350;
}

// Block callback function - fills entire buffer at once
void updateAudioBlock(void* userData, uint16_t* buffer) {
    // Create a frequency sweep effect
    sweepCounter++;
    if (sweepCounter > 1000) {  // Change frequency every ~1000 blocks
        sweepCounter = 0;
        static bool ascending = true;
        static uint32_t freqMultiplier = 1;
        
        if (ascending) {
            freqMultiplier++;
            if (freqMultiplier > 8) ascending = false;
        } else {
            freqMultiplier--;
            if (freqMultiplier < 1) ascending = true;
        }
        phaseIncrement = 1000 * freqMultiplier;
    }
    
    // Fill the buffer with sine wave samples
    for (int i = 0; i < config.blockSize; i++) {
        buffer[i] = getSineValue(phase);
        phase += phaseIncrement;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("DACless Block Processing Example - PWM Only");
    Serial.print("Block size: ");
    Serial.println(config.blockSize);
    
    // Set the block callback function
    audio.setBlockCallback(updateAudioBlock, nullptr);
    
    // Initialize the audio system
    audio.begin();
    
    // Audio starts muted by default, unmute when ready
    delay(100);
    audio.unmute();
    
    Serial.print("DACless Audio initialized at ");
    Serial.print(audio.getSampleRate());
    Serial.println(" Hz");
    Serial.println("Listen to GPIO 6 for sweeping sine wave");
}

void loop() {
    // Display current frequency every 2 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
        lastPrint = millis();
        
        float frequency = (audio.getSampleRate() * phaseIncrement) / 4294967296.0;
        Serial.print("Current frequency: ");
        Serial.print(frequency, 1);
        Serial.println(" Hz");
    }
}