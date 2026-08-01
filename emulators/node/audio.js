const sdl = require('@kmamal/sdl');

class WagnosticAudio {
  constructor() {
    this.device = null;
    this.channels = 0;
    this.sampleRate = 0;
    this.bpp = 0;
  }

  setup(state) {
    const size = state.audioSize;
    if (size === 0) return;

    const channels = state.audioChannels || 2;
    const sampleRate = state.audioSampleRate || 44100;
    const bpp = state.audioBpp || 16;

    if (this.device && this.channels === channels && this.sampleRate === sampleRate && this.bpp === bpp) {
      return;
    }

    if (this.device) {
      try { this.device.close(); } catch (e) {}
      this.device = null;
    }

    let format = 's16lsb';
    if (bpp === 8) format = 'u8';
    else if (bpp === 32) format = 'f32lsb';

    try {
      this.device = sdl.audio.openDevice({
        type: 'playback',
        channels,
        frequency: sampleRate,
        format,
      });
      this.channels = channels;
      this.sampleRate = sampleRate;
      this.bpp = bpp;
      this.device.play();
    } catch (err) {
      console.warn('[Audio] Failed to open SDL audio device:', err.message);
      this.device = null;
    }
  }

  process(state, memoryBuffer) {
    const size = state.audioSize;
    if (!this.device || size === 0) return;

    let write = state.audioWrite;
    let read = state.audioRead;
    if (write === read) return;

    const bufOffset = state.statePtr + state.audioBufferOffset;
    const uint8 = new Uint8Array(memoryBuffer);

    let bytesAvailable = 0;
    if (write >= read) {
      bytesAvailable = write - read;
    } else {
      bytesAvailable = (size - read) + write;
    }

    if (bytesAvailable <= 0) return;

    // Pull chunks into a JS Buffer to enqueue into SDL audio device
    let chunk;
    if (write >= read) {
      chunk = Buffer.from(uint8.buffer, uint8.byteOffset + bufOffset + read, bytesAvailable);
      read = (read + bytesAvailable) % size;
    } else {
      const part1Size = size - read;
      const part1 = uint8.subarray(bufOffset + read, bufOffset + size);
      const part2 = uint8.subarray(bufOffset, bufOffset + write);
      chunk = Buffer.concat([Buffer.from(part1.buffer, part1.byteOffset, part1.byteLength), Buffer.from(part2.buffer, part2.byteOffset, part2.byteLength)]);
      read = write;
    }

    try {
      this.device.enqueue(chunk);
      state.setAudioRead(read);
    } catch (e) {
      // Ignore queue full / error
    }
  }

  close() {
    if (this.device) {
      try { this.device.close(); } catch (e) {}
      this.device = null;
    }
  }
}

module.exports = WagnosticAudio;
