/**
 * Zero-dependency pure JavaScript LZW GIF Encoder (GIF89a).
 */

class MinimalGifEncoder {
  constructor(width, height, delayCs = 2) {
    this.width = width;
    this.height = height;
    this.delayCs = delayCs;
    this.frames = [];
  }

  addFrame(pixelsRgba) {
    const indexed = new Uint8Array(this.width * this.height);
    for (let i = 0; i < indexed.length; i++) {
      const px = pixelsRgba[i];
      const r = (px & 0xFF) >> 5;
      const g = ((px >> 8) & 0xFF) >> 5;
      const b = ((px >> 16) & 0xFF) >> 6;
      indexed[i] = (r << 5) | (g << 2) | b;
    }
    this.frames.push(indexed);
  }

  save() {
    const out = [];
    const pushStr = (str) => {
      for (let i = 0; i < str.length; i++) out.push(str.charCodeAt(i));
    };
    const push16 = (v) => { out.push(v & 0xFF, (v >> 8) & 0xFF); };

    pushStr('GIF89a');
    push16(this.width);
    push16(this.height);
    out.push(0xF7, 0, 0);

    for (let i = 0; i < 256; i++) {
      const r = (i >> 5) & 7;
      const g = (i >> 2) & 7;
      const b = i & 3;
      out.push(
        Math.round((r / 7) * 255),
        Math.round((g / 7) * 255),
        Math.round((b / 3) * 255)
      );
    }

    out.push(0x21, 0xFF, 0x0B);
    pushStr('NETSCAPE2.0');
    out.push(0x03, 0x01);
    push16(0);
    out.push(0x00);

    for (const frame of this.frames) {
      out.push(0x21, 0xF9, 0x04, 0x00);
      push16(this.delayCs);
      out.push(0x00, 0x00);

      out.push(0x2C);
      push16(0); push16(0);
      push16(this.width); push16(this.height);
      out.push(0x00);

      const minCodeSize = 8;
      out.push(minCodeSize);

      const clearCode = 1 << minCodeSize;
      const eoiCode = clearCode + 1;
      let nextCode = eoiCode + 1;
      let curCodeSize = minCodeSize + 1;

      let codeBuf = 0;
      let codeBits = 0;
      const packet = [];

      const writeBits = (val, bits) => {
        codeBuf |= (val << codeBits);
        codeBits += bits;
        while (codeBits >= 8) {
          packet.push(codeBuf & 0xFF);
          codeBuf >>= 8;
          codeBits -= 8;
          if (packet.length === 254) {
            out.push(packet.length, ...packet);
            packet.length = 0;
          }
        }
      };

      const codeTable = new Map();
      const resetTable = () => {
        codeTable.clear();
        for (let i = 0; i < clearCode; i++) codeTable.set(String.fromCharCode(i), i);
        nextCode = eoiCode + 1;
        curCodeSize = minCodeSize + 1;
      };

      resetTable();
      writeBits(clearCode, curCodeSize);

      let curStr = '';
      for (let i = 0; i < frame.length; i++) {
        const k = String.fromCharCode(frame[i]);
        const testStr = curStr + k;
        if (codeTable.has(testStr)) {
          curStr = testStr;
        } else {
          writeBits(codeTable.get(curStr), curCodeSize);
          if (nextCode < 4096) {
            codeTable.set(testStr, nextCode++);
            if (nextCode > (1 << curCodeSize) && curCodeSize < 12) {
              curCodeSize++;
            }
          } else {
            writeBits(clearCode, curCodeSize);
            resetTable();
          }
          curStr = k;
        }
      }

      if (curStr.length > 0) {
        writeBits(codeTable.get(curStr), curCodeSize);
      }
      writeBits(eoiCode, curCodeSize);

      if (codeBits > 0) {
        packet.push(codeBuf & 0xFF);
      }
      if (packet.length > 0) {
        out.push(packet.length, ...packet);
      }
      out.push(0x00);
    }

    out.push(0x3B);
    return new Uint8Array(out);
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { MinimalGifEncoder };
}
