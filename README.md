
````markdown
# DACless Audio Library for RP2040

A high-performance, zero-heap, multi-instance audio library for the Raspberry Pi Pico (RP2040), generating audio via PWM output and DMA.  
Designed for synthesis, effects, and creative hacking—**no DAC required.** Also supports simultaneous ADC input with DMA, hardware interpolation, and efficient background audio for modern embedded workflows.

## Features

- **True background audio:** All audio and ADC handled by DMA, minimal CPU load.
- **Multi-instance support:** Up to 4 independent audio engines (see limitations).
- **12-bit PWM audio output:** Sample rate auto-determined by system clock and PWM resolution.
- **Double-buffered playback:** Smooth, glitch-free audio blocks.
- **Simultaneous ADC input:** Read up to 4 analog channels (GPIO 26–29) with DMA.
- **Block and per-sample callbacks:** Choose the best method for your use case.
- **Hardware interpolation:** Fast, linear interpolation via RP2040's hardware.
- **Direct buffer access:** For advanced use or integration with existing codebases.
- **No dynamic allocation:** All buffers are static and pre-allocated.

## Installation

1. Download the library as a ZIP file or clone from GitHub.
2. In Arduino IDE: `Sketch → Include Library → Add .ZIP Library`, select the downloaded ZIP.
3. Or copy the `DACless` folder to your Arduino `libraries` directory.

## Quick Start Example

```cpp
#include <DACless.h>

// Configure the audio engine (defaults shown)
DAClessConfig cfg;
cfg.pinPWM = 6;        // PWM audio out on GPIO 6
cfg.pwmBits = 12;      // 12-bit resolution
cfg.blockSize = 128;   // Block size per audio buffer
cfg.nAdcInputs = 4;    // Up to 4 ADC channels

DAClessAudio audio(cfg);

uint16_t synthSample(void*) {
    static uint16_t t = 0;
    return (t++ & 0xFFF); // Simple ramp
}

void setup() {
    audio.setSampleCallback(synthSample, nullptr);
    audio.begin();
    audio.unmute();
}

void loop() {
    // Audio runs in background
}


### Block Callback Example

```cpp
void fillBlock(void* userdata, uint16_t* buffer) {
    for (int i = 0; i < 128; ++i)
        buffer[i] = random(4096); // White noise
}

void setup() {
    audio.setBlockCallback(fillBlock, nullptr);
    audio.begin();
    audio.unmute();
}
```

## API Reference

### DAClessAudio

* `DAClessAudio(const DAClessConfig&)` — create a new audio engine
* `void begin()` — initialize all hardware, DMA, buffers
* `void mute()` — set PWM output to DC midpoint, disables output
* `void unmute()` — enables audio output
* `void setSampleCallback(SampleCallback, void* userPtr = nullptr)`
* `void setBlockCallback(BlockCallback, void* userPtr = nullptr)`
* `uint16_t getADC(uint8_t channel) const` — latest ADC value for channel 0–3
* `float getSampleRate() const` — actual audio sample rate in Hz
* `const DAClessConfig& getConfig() const` — inspect instance config
* `const volatile uint16_t* getOutBufPtr() const` — raw output buffer
* `const volatile uint16_t* getAdcBuffer() const` — raw ADC buffer

### Global Helper Functions

* `uint16_t interpolate(uint16_t x, uint16_t y, uint16_t mu_scaled)`
* `uint16_t interpolate1(uint16_t x, uint16_t y, uint16_t mu_scaled)`

### DAClessConfig

* `.pinPWM` — GPIO pin for audio output (default 6)
* `.pwmBits` — PWM bit depth (default 12)
* `.blockSize` — audio block size (max 512)
* `.nAdcInputs` — number of ADC channels to scan (default 4, max 4)

---

## Hardware Connections

* **Audio Out**: PWM pin (default: GPIO 6; configurable)
* **ADC Inputs**: GPIO 26, 27, 28, 29 (up to 4 channels)

> *For best results, low-pass filter the output:*
>
> ```
> GPIOx ----[220Ω]----+---- Audio Out
>                     |
>                    === 100nF
>                     |
>                    GND
> ```

---

## Known Limitations and Caveats

* **Only 4 DAClessAudio instances max.** Hard limit, no dynamic allocation.
* **Each instance must use a unique PWM slice.** (Pins sharing a slice will conflict—see [RP2040 PWM docs](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)).
* **Only DMA IRQ1 is supported.** The library claims exclusive use of IRQ 1 for safe operation. No other DMA IRQ1 users allowed in the same sketch.
* **ADC: Only GPIO 26–29 supported.** Up to 4 channels; no support for nonstandard or external ADCs.
* **Audio sample rate is fixed** by system clock and PWM bits; not user-configurable at runtime.
* **Globals (`audio_rate`, `out_buf_ptr`, `adc_results_buf`)** only reflect the first created instance, for compatibility.
* **No resource collision prevention:** User is responsible for not configuring two instances on the same hardware resource (e.g., PWM slice or ADC input).
* **Not thread-safe:** Interrupt safety is enforced for instance registration only.
* **No dynamic reconfiguration:** Once constructed and `.begin()` is called, instance settings are fixed for that object.
* **Arduino-only:** Assumes [Earle Philhower’s arduino-pico core](https://github.com/earlephilhower/arduino-pico). Not tested elsewhere.

---

## Technical Details

* **All hardware control via Pico SDK API calls.**
* **No heap allocation.** All buffers static, sized by config (max block size 512, max 4 ADC channels).
* **DMA double buffering for PWM.** Audio runs with zero CPU intervention between callbacks.
* **DMA double buffering for ADC.** All enabled ADC channels sampled via chained DMA.
* **Registry of up to 4 instances.** Each routes DMA IRQs to correct callback.
* **Hardware interpolators** are configured for linear blending per instance, but are global resources (shared).
* **No use of C++ RTTI or exceptions.**

---

## Examples

See the `examples/` folder for more usage.

* `SimpleSample` — ramp generator
* `BlockProcessing` — block-based noise with ADC monitoring

---

## Future Roadmap

The current release (v1.0.0) is focused on robustness, reliability, and simplicity for audio-rate outputs and manageable numbers of high-quality PWM audio channels. This approach ensures a clean, predictable API and easy integration for most modular synth and embedded audio projects.

Planned and under consideration for future releases:

- **Batch LFO/Group Modulation Output:**  
  Support for batch/timer-based LFOs or grouped CV outputs, enabling massive multi-channel modulation (e.g., 8, 12, or 16 outputs) for step sequencers, mega-LFO modules, or lighting control.  
  - This will be implemented with careful resource management, optional timer-based update modes, and APIs for efficient batch processing.
- **Dynamic Output Assignment:**  
  Smarter mapping of available DMA channels, PWM slices, and hardware resources, allowing more flexible use of the RP2040’s capabilities.
- **Mixing Audio and Modulation Modes:**  
  Ability to select per-output update strategy (DMA audio, timer batch, etc.) for hybrid modules.
- **Advanced Error Handling:**  
  Better detection of hardware resource exhaustion, pin conflicts, and API misuse.
- **Community Feedback:**  
  Your bug reports, feature requests, and wild ideas will help shape the next major release. Open an issue or discussion on GitHub!


Want to help design the next phase or have a unique use case?  
Open an issue or discussion: [https://github.com/brianvarren/DACless](https://github.com/brianvarren/DACless)

---

## License

MIT License.
Copyright (c) Brian Varren.

---

**For questions, feedback, or to report a bug,** open an Issue or PR on [GitHub](https://github.com/brianvarren/DACless).

````
