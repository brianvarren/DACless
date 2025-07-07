#ifndef DACLESS_H
#define DACLESS_H

#include <Arduino.h>

// Maximum supported values - compile-time constants
#define DACLESS_MAX_BLOCK_SIZE 512
#define DACLESS_MAX_ADC_INPUTS 4

// Configuration struct for each DACless instance
struct DAClessConfig {
    uint pinPWM = 6;           // Which GPIO pin is used for PWM audio (default: GP6)
    uint16_t pwmBits = 12;     // Audio resolution in bits (default: 12 bits for 4096 steps)
    uint16_t blockSize = 128;  // Number of samples processed in each audio block (default: 128)
    uint8_t nAdcInputs = 4;    // Number of ADC inputs to scan (default: 4)
};

class DAClessAudio {
public:
    // Function pointer types for audio callbacks
    using SampleCallback = uint16_t(*)(void*);
    using BlockCallback = void(*)(void*, uint16_t*);
    
    // Create a new DACless audio engine with a specific configuration
    explicit DAClessAudio(const DAClessConfig& cfg = {});
    ~DAClessAudio();
    
    // Main control methods
    void begin();                   // Initialize the audio system
    void mute();                   // Mute audio output
    void unmute();                 // Unmute audio output
    
    // Set audio callbacks with optional user data
    void setSampleCallback(SampleCallback cb, void* userdata = nullptr);
    void setBlockCallback(BlockCallback cb, void* userdata = nullptr);
    
    // ADC access
    uint16_t getADC(uint8_t channel) const;  // Get ADC value for channel
    
    // Get current audio sample rate
    float getSampleRate() const { return sampleRate_; }
    
    // Get configuration
    const DAClessConfig& getConfig() const { return cfg_; }
    
    // Public access to buffers for compatibility (read-only)
    const volatile uint16_t* getOutBufPtr() const { return outBufPtr_; }
    const volatile uint16_t* getAdcBuffer() const { return adcBuf_; }

private:
    DAClessConfig cfg_;  // Stores settings for this audio instance
    
    // DMA channels used by this instance (-1 means not allocated)
    uint dmaA_ = -1u;
    uint dmaB_ = -1u;
    uint dmaAdcSamp_ = -1u;
    uint dmaAdcCtrl_ = -1u;
    float sampleRate_;
    
    // Static buffers - no dynamic allocation!
    // Aligned for DMA access
    // volatile uint16_t adcBuf_[DACLESS_MAX_ADC_INPUTS] __attribute__((aligned(8)));
    // uint16_t pwmBufA_[DACLESS_MAX_BLOCK_SIZE] __attribute__((aligned(8)));
    // uint16_t pwmBufB_[DACLESS_MAX_BLOCK_SIZE] __attribute__((aligned(8)));
    
    alignas(DACLESS_MAX_BLOCK_SIZE * sizeof(uint16_t))
    uint16_t pwmBufA_[DACLESS_MAX_BLOCK_SIZE];
    alignas(DACLESS_MAX_BLOCK_SIZE * sizeof(uint16_t))
    uint16_t pwmBufB_[DACLESS_MAX_BLOCK_SIZE];
    alignas(DACLESS_MAX_ADC_INPUTS * sizeof(uint16_t))
    volatile uint16_t adcBuf_[DACLESS_MAX_ADC_INPUTS];

    volatile uint16_t* outBufPtr_ = nullptr; // Pointer to current audio output buffer
    volatile bool      bufReady_  = false;   // Set true when buffer is ready to fill
    
    // For callbacks
    SampleCallback  sampleCb_ = nullptr;
    BlockCallback   blockCb_  = nullptr;
    void*           userPtr_  = nullptr;
    
    // Internal setup methods
    void setupInterpolators();
    void configurePWM_DMA();
    void configureADC_DMA();
    void handleDmaIrq(uint channel); // Called when DMA transfer finishes for this instance
    
    // Helper to calculate DMA ring buffer size bits
    static uint calculateRingBits(uint bufferSizeBytes);
    
    // Static registry for IRQ routing (fixed size array to avoid dynamic allocation)
    static constexpr uint MAX_INSTANCES = 4;
    static DAClessAudio* instances_[MAX_INSTANCES];
    static uint instanceCount_;
    static DAClessAudio* findInstanceByDmaChannel(uint channel);
    
    // Friend function for IRQ handler
    friend void dma_irq1_handler();
};

// Interpolation functions (available globally for user convenience)
uint16_t interpolate(uint16_t x, uint16_t y, uint16_t mu_scaled);
uint16_t interpolate1(uint16_t x, uint16_t y, uint16_t mu_scaled);

// Global variables that need to be accessible for compatibility
extern float audio_rate;
extern volatile uint16_t* out_buf_ptr;
extern const volatile uint16_t* adc_results_buf;

#endif // DACLESS_H