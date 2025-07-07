#include <DACless.h>
#include <hardware/adc.h>
#include <hardware/pwm.h>
#include <hardware/dma.h>
#include "hardware/interp.h"
#include <hardware/regs/dreq.h>
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include <string.h>  // for memset

// Static registry of instances (no dynamic allocation)
DAClessAudio* DAClessAudio::instances_[DAClessAudio::MAX_INSTANCES] = {nullptr};
uint DAClessAudio::instanceCount_ = 0;

// Compatibility globals (point to first/active instance)
float audio_rate = 0;
volatile uint16_t* out_buf_ptr = nullptr;
const volatile uint16_t* adc_results_buf = nullptr;

// Helper to find instance by DMA channel
DAClessAudio* DAClessAudio::findInstanceByDmaChannel(uint channel) {
    for (uint i = 0; i < instanceCount_; i++) {
        DAClessAudio* inst = instances_[i];
        if (inst && (inst->dmaA_ == channel || inst->dmaB_ == channel)) {
            return inst;
        }
    }
    return nullptr;
}

// Global IRQ handler that routes to the correct instance
void dma_irq1_handler() {
    uint32_t pending = dma_hw->ints1;
    
    // Check all possible DMA channels (RP2040 has 12)
    for (uint ch = 0; ch < 12; ch++) {
        if (pending & (1u << ch)) {
            dma_hw->ints1 = 1u << ch;  // Clear interrupt
            
            // Find which instance owns this channel and pass the channel number
            DAClessAudio* inst = DAClessAudio::findInstanceByDmaChannel(ch);
            if (inst) {
                inst->handleDmaIrq(ch);
            }
        }
    }
}

// Constructor
DAClessAudio::DAClessAudio(const DAClessConfig& cfg) : cfg_(cfg), 
    dmaA_(-1u), dmaB_(-1u), dmaAdcSamp_(-1u), dmaAdcCtrl_(-1u) {
    
    // Validate configuration
    if (cfg_.blockSize > DACLESS_MAX_BLOCK_SIZE) {
        cfg_.blockSize = DACLESS_MAX_BLOCK_SIZE;
    }
    if (cfg_.nAdcInputs > DACLESS_MAX_ADC_INPUTS) {
        cfg_.nAdcInputs = DACLESS_MAX_ADC_INPUTS;
    }
    
    // Register this instance (protect against concurrent access)
    uint32_t save = save_and_disable_interrupts();
    if (instanceCount_ < MAX_INSTANCES) {
        instances_[instanceCount_++] = this;
    }
    restore_interrupts(save);
    
    // Calculate sample rate based on configuration (matching pwm_set_wrap)
    sampleRate_ = clock_get_hz(clk_sys) / (1u << cfg_.pwmBits);
    
    // Update compatibility globals if this is the first instance
    if (instances_[0] == this) {
        audio_rate = sampleRate_;
    }
    
    // Clear buffers
    memset((void*)adcBuf_, 0, sizeof(adcBuf_));
    memset(pwmBufA_, 0, sizeof(pwmBufA_));
    memset(pwmBufB_, 0, sizeof(pwmBufB_));
}

// Destructor
DAClessAudio::~DAClessAudio() {
    // Stop and release DMA channels
    if (dmaA_ != -1u) {
        dma_channel_abort(dmaA_);
        dma_channel_unclaim(dmaA_);
    }
    if (dmaB_ != -1u) {
        dma_channel_abort(dmaB_);
        dma_channel_unclaim(dmaB_);
    }
    if (dmaAdcSamp_ != -1u) {
        dma_channel_abort(dmaAdcSamp_);
        dma_channel_unclaim(dmaAdcSamp_);
    }
    if (dmaAdcCtrl_ != -1u) {
        dma_channel_abort(dmaAdcCtrl_);
        dma_channel_unclaim(dmaAdcCtrl_);
    }
    
    // Remove from registry (protect against concurrent access)
    uint32_t save = save_and_disable_interrupts();
    for (uint i = 0; i < instanceCount_; i++) {
        if (instances_[i] == this) {
            // Shift remaining instances down
            for (uint j = i; j < instanceCount_ - 1; j++) {
                instances_[j] = instances_[j + 1];
            }
            instanceCount_--;
            break;
        }
    }
    restore_interrupts(save);
}

void DAClessAudio::begin() {
    // Initialize buffers to midpoint
    uint16_t midpoint = (1u << cfg_.pwmBits) / 2;
    for (uint i = 0; i < cfg_.blockSize; i++) {
        pwmBufA_[i] = midpoint;
        pwmBufB_[i] = midpoint;
    }
    
    // Update compatibility globals
    if (instances_[0] == this) {
        adc_results_buf = adcBuf_;
    }
    
    // Set up hardware
    setupInterpolators();
    configurePWM_DMA();
    configureADC_DMA();
}

void DAClessAudio::setSampleCallback(SampleCallback cb, void* userdata) {
    sampleCb_ = cb;
    userPtr_ = userdata;
}

void DAClessAudio::setBlockCallback(BlockCallback cb, void* userdata) {
    blockCb_ = cb;
    userPtr_ = userdata;
}

void DAClessAudio::mute() {
    uint slice_num = pwm_gpio_to_slice_num(cfg_.pinPWM);
    pwm_set_gpio_level(cfg_.pinPWM, (1u << cfg_.pwmBits) / 2);
    pwm_set_enabled(slice_num, false);
}

void DAClessAudio::unmute() {
    uint slice_num = pwm_gpio_to_slice_num(cfg_.pinPWM);
    pwm_set_enabled(slice_num, true);
}

uint16_t DAClessAudio::getADC(uint8_t channel) const {
    if (channel >= cfg_.nAdcInputs) {
        return 0;
    }
    return adcBuf_[channel];
}

void DAClessAudio::handleDmaIrq(uint channel) {
    // Determine which buffer just started playing and prepare the other
    if (channel == dmaA_) {
        outBufPtr_ = pwmBufA_;
        bufReady_ = true;
    } else if (channel == dmaB_) {
        outBufPtr_ = pwmBufB_;
        bufReady_ = true;
    }
    
    // Update compatibility global
    out_buf_ptr = outBufPtr_;
    
    // Call appropriate callback to fill the buffer
    if (bufReady_) {
        if (blockCb_) {
            // Use block callback
            blockCb_(userPtr_, const_cast<uint16_t*>(outBufPtr_));
        } else if (sampleCb_) {
            // Fall back to sample callback
            for (uint16_t i = 0; i < cfg_.blockSize; i++) {
                const_cast<uint16_t*>(outBufPtr_)[i] = sampleCb_(userPtr_);
            }
        } else {
            // Default: fill with silence
            uint16_t midpoint = (1u << cfg_.pwmBits) / 2;
            for (uint16_t i = 0; i < cfg_.blockSize; i++) {
                const_cast<uint16_t*>(outBufPtr_)[i] = midpoint;
            }
        }
        bufReady_ = false;
    }
}

void DAClessAudio::setupInterpolators() {
    // Configure hardware interpolators for efficient linear interpolation
    // NOTE: These are global hardware resources shared between all instances.
    // If instances need different interpolator settings, this needs to be
    // managed at the application level.
    interp_config cfg_0 = interp_default_config();
    interp_config_set_blend(&cfg_0, true);
    interp_set_config(interp0, 0, &cfg_0);
    cfg_0 = interp_default_config();
    interp_set_config(interp0, 1, &cfg_0);

    interp_config cfg_1 = interp_default_config();
    interp_config_set_blend(&cfg_1, true);
    interp_set_config(interp1, 0, &cfg_1);
    cfg_1 = interp_default_config();
    interp_set_config(interp1, 1, &cfg_1);
}

uint DAClessAudio::calculateRingBits(uint bufferSizeBytes) {
    // Find the log2 of buffer size in bytes
    // Hardware wants log2(bytes) for the ring buffer wrap
    return 31 - __builtin_clz(bufferSizeBytes);
}

void DAClessAudio::configurePWM_DMA() {
    gpio_set_function(cfg_.pinPWM, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(cfg_.pinPWM);
    
    // NOTE: If multiple instances share the same PWM slice (e.g., pins 6&7),
    // they'll overwrite each other's settings. Consider tracking slice usage.
    pwm_set_clkdiv(slice_num, 1);
    pwm_set_wrap(slice_num, (1u << cfg_.pwmBits) - 1);  // Wrap value is inclusive
    pwm_set_enabled(slice_num, true);
    pwm_set_irq_enabled(slice_num, true);

    dmaA_ = dma_claim_unused_channel(true);
    dmaB_ = dma_claim_unused_channel(true);
    
    // Calculate size bits for ring buffer
    uint size_bytes = cfg_.blockSize * sizeof(uint16_t);
    uint size_bits = calculateRingBits(size_bytes);

    // Configure DMA channel A
    dma_channel_config cfg_0_a = dma_channel_get_default_config(dmaA_);
    channel_config_set_transfer_data_size(&cfg_0_a, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg_0_a, true);
    channel_config_set_dreq(&cfg_0_a, DREQ_PWM_WRAP0 + slice_num);
    channel_config_set_ring(&cfg_0_a, false, size_bits);
    channel_config_set_chain_to(&cfg_0_a, dmaB_);

    dma_channel_configure(
        dmaA_, &cfg_0_a,
        &pwm_hw->slice[slice_num].cc,
        pwmBufA_,
        cfg_.blockSize,
        false
    );

    dma_channel_set_irq1_enabled(dmaA_, true);

    // Configure DMA channel B
    dma_channel_config cfg_0_b = dma_channel_get_default_config(dmaB_);
    channel_config_set_transfer_data_size(&cfg_0_b, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg_0_b, true);
    channel_config_set_dreq(&cfg_0_b, DREQ_PWM_WRAP0 + slice_num);
    channel_config_set_ring(&cfg_0_b, false, size_bits);
    channel_config_set_chain_to(&cfg_0_b, dmaA_);
    
    dma_channel_configure(
        dmaB_, &cfg_0_b,
        &pwm_hw->slice[slice_num].cc,
        pwmBufB_,
        cfg_.blockSize,
        false
    );

    dma_channel_set_irq1_enabled(dmaB_, true);
    
    // Set up IRQ handler only once
    static bool irq_initialized = false;
    if (!irq_initialized) {
        irq_set_exclusive_handler(DMA_IRQ_1, dma_irq1_handler);
        irq_set_enabled(DMA_IRQ_1, true);
        irq_initialized = true;
    }

    dma_channel_start(dmaA_);
}

void DAClessAudio::configureADC_DMA() {
    // Only configure ADC if we have inputs
    if (cfg_.nAdcInputs == 0) return;
    
    // Setup ADC on GPIO 26-29 (up to 4 channels)
    for (uint8_t i = 0; i < cfg_.nAdcInputs && i < 4; i++) {
        adc_gpio_init(26 + i);
    }
    
    adc_init();
    adc_set_clkdiv(1);
    
    // Set up round-robin mask based on number of inputs
    uint8_t rr_mask = (1u << cfg_.nAdcInputs) - 1;
    adc_set_round_robin(rr_mask);
    adc_select_input(0);
    adc_fifo_setup(true, true, cfg_.nAdcInputs, false, false);
    adc_fifo_drain();

    // Get DMA channels
    dmaAdcSamp_ = dma_claim_unused_channel(true);
    dmaAdcCtrl_ = dma_claim_unused_channel(true);
    
    // Setup Sample Channel
    dma_channel_config samp_conf = dma_channel_get_default_config(dmaAdcSamp_);
    channel_config_set_transfer_data_size(&samp_conf, DMA_SIZE_16);
    channel_config_set_read_increment(&samp_conf, false);
    channel_config_set_write_increment(&samp_conf, true);
    channel_config_set_irq_quiet(&samp_conf, true);
    channel_config_set_dreq(&samp_conf, DREQ_ADC);
    channel_config_set_chain_to(&samp_conf, dmaAdcCtrl_);
    channel_config_set_enable(&samp_conf, true);
    
    // Setup Control Channel (for continuous operation)
    dma_channel_config ctrl_conf = dma_channel_get_default_config(dmaAdcCtrl_);
    channel_config_set_transfer_data_size(&ctrl_conf, DMA_SIZE_32);
    channel_config_set_read_increment(&ctrl_conf, false);
    channel_config_set_write_increment(&ctrl_conf, false);
    channel_config_set_irq_quiet(&ctrl_conf, true);
    channel_config_set_dreq(&ctrl_conf, DREQ_FORCE);
    channel_config_set_enable(&ctrl_conf, true);
    
    // Create pointer to ADC buffer for control channel
    volatile uint16_t* adc_ptr = adcBuf_;
    
    dma_channel_configure(
        dmaAdcSamp_, &samp_conf,
        const_cast<uint16_t*>(adc_ptr),  // Cast away volatile for DMA setup
        &adc_hw->fifo,
        cfg_.nAdcInputs,
        false
    );
    
    dma_channel_configure(
        dmaAdcCtrl_, &ctrl_conf,
        &dma_hw->ch[dmaAdcSamp_].al2_write_addr_trig,
        &adc_ptr,
        1,
        false
    );
    
    // Start the sample channel first
    dma_channel_start(dmaAdcSamp_);
    // Then start the control channel
    dma_channel_start(dmaAdcCtrl_);
    adc_run(true);
}

// Global interpolation functions
uint16_t interpolate(uint16_t x, uint16_t y, uint16_t mu_scaled) {
    interp0->base[0] = x;
    interp0->base[1] = y;
    interp0->accum[1] = mu_scaled;
    return interp0->peek[1];
}

uint16_t interpolate1(uint16_t x, uint16_t y, uint16_t mu_scaled) {
    interp1->base[0] = x;
    interp1->base[1] = y;
    interp1->accum[1] = mu_scaled;
    return interp1->peek[1];
}