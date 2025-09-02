/*
  DACless Polyphonic Synthesizer Example - PWM Only Version
  Generates multiple sine waves simultaneously to create chords
  Demonstrates advanced PWM audio synthesis capabilities
*/

#include "DACless.h"

// Configure DACless for higher quality audio
DAClessConfig config = {
    .pinPWM = 6,        // GPIO 6 for audio output
    .pwmBits = 12,      // 12-bit resolution (4096 steps)
    .blockSize = 64     // Larger blocks for better efficiency
};
DAClessAudio audio(config);

// Sine wave lookup table (full wave, 256 entries)
const uint16_t sineTable[256] = {
    2048, 2098, 2148, 2198, 2248, 2298, 2348, 2398, 2447, 2497, 2546, 2595, 2644, 2693, 2741, 2790,
    2838, 2886, 2933, 2981, 3028, 3075, 3121, 3168, 3214, 3259, 3305, 3350, 3394, 3439, 3483, 3526,
    2569, 3612, 3655, 3697, 3738, 3779, 3820, 3860, 3900, 3940, 3979, 4017, 4055, 4093, 4130, 4166,
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

// Oscillator structure
struct Oscillator {
    uint32_t phase;
    uint32_t phaseIncrement;
    uint16_t amplitude;    // 0-4095
    bool active;
};

// Support up to 4 simultaneous voices
#define MAX_VOICES 4
Oscillator voices[MAX_VOICES];

// Musical note frequencies (phase increments for 32-bit phase accumulator)
const uint32_t noteFreqs[] = {
    // C4, D4, E4, F4, G4, A4, B4, C5 (middle C octave)
    4506338,  5060424,  5679764,  6017788,  6755296,  7582456,  8509350,  9012677,
    // C5, D5, E5, F5, G5, A5, B5, C6
    9012677, 10120849, 11359529, 12035577, 13510593, 15164913, 17018701, 18025355
};

// Chord progressions (indices into noteFreqs array)
const uint8_t chords[][4] = {
    {0, 2, 4, 7},   // C major (C, E, G, C)
    {5, 1, 3, 6},   // A minor (A, D, F, B) - relative minor
    {3, 5, 0, 2},   // F major (F, A, C, E)
    {4, 6, 1, 4}    // G major (G, B, D, G)
};

uint8_t currentChord = 0;
uint32_t chordTimer = 0;

// Fast sine lookup with linear interpolation
uint16_t fastSine(uint32_t phase) {
    uint8_t index = (phase >> 24) & 0xFF;
    return sineTable[index];
}

// Block callback - generates polyphonic audio
void polyphonicBlock(void* userData, uint16_t* buffer) {
    // Change chord every 4 seconds (approximate)
    chordTimer++;
    if (chordTimer > 300) {  // Adjust based on your actual block rate
        chordTimer = 0;
        currentChord = (currentChord + 1) % 4;
        
        // Update voice frequencies to new chord
        for (int i = 0; i < 4; i++) {
            voices[i].phaseIncrement = noteFreqs[chords[currentChord][i]];
            voices[i].amplitude = 800;  // Moderate volume per voice
            voices[i].active = true;
        }
        
        const char* chordNames[] = {"C Major", "A Minor", "F Major", "G Major"};
        Serial.print("Playing: ");
        Serial.println(chordNames[currentChord]);
    }
    
    // Generate audio block
    for (uint16_t sample = 0; sample < config.blockSize; sample++) {
        int32_t mixedSample = 0;
        
        // Mix all active voices
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].active) {
                // Get sine wave value for this voice
                uint16_t sineValue = fastSine(voices[voice].phase);
                
                // Apply amplitude and add to mix
                int32_t voiceSample = ((int32_t)(sineValue - 2048) * voices[voice].amplitude) >> 12;
                mixedSample += voiceSample;
                
                // Update phase
                voices[voice].phase += voices[voice].phaseIncrement;
            }
        }
        
        // Apply soft clipping to prevent harsh distortion
        if (mixedSample > 2047) mixedSample = 2047;
        if (mixedSample < -2048) mixedSample = -2048;
        
        // Convert back to unsigned 12-bit
        buffer[sample] = (uint16_t)(mixedSample + 2048);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("DACless Polyphonic Synthesizer - PWM Only");
    Serial.println("==========================================");
    
    // Initialize all voices as inactive
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].phase = 0;
        voices[i].phaseIncrement = 0;
        voices[i].amplitude = 0;
        voices[i].active = false;
    }
    
    // Set the block callback
    audio.setBlockCallback(polyphonicBlock, nullptr);
    
    // Initialize audio system
    audio.begin();
    delay(100);
    audio.unmute();
    
    Serial.print("Audio initialized at ");
    Serial.print(audio.getSampleRate());
    Serial.println(" Hz");
    Serial.print("Block size: ");
    Serial.println(config.blockSize);
    Serial.println("Listen to GPIO 6 for polyphonic chord progression");
    Serial.println("C Major -> A Minor -> F Major -> G Major -> repeat");
    Serial.println();
}

void loop() {
    // Status LED blink
    static bool ledState = false;
    static unsigned long lastBlink = 0;
    
    if (millis() - lastBlink > 2000) {  // Blink every 2 seconds
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
        
        // Print voice status
        Serial.print("Active voices: ");
        int activeCount = 0;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (voices[i].active) activeCount++;
        }
        Serial.println(activeCount);
    }
    
    delay(100);
}