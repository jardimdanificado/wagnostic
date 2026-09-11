/**
 * Piolho Wire Protocol & Frame Parser
 * 
 * Zero-dependency binary framing for Remote IPC Rendezvous over any stream/datagram.
 */

const MAGIC = 0x50494F4C; // "PIOL"

const MSG_HELLO     = 1;
const MSG_HELLO_ACK = 2;
const MSG_TELL      = 3;
const MSG_PING      = 4;
const MSG_PONG      = 5;

function encodePacket(msgType, sender, target, payload) {
  const enc = new TextEncoder();
  const senderBytes = enc.encode(sender || '');
  const targetBytes = enc.encode(target || '');
  const payloadBytes = payload ? (payload instanceof Uint8Array ? payload : new Uint8Array(payload)) : new Uint8Array(0);

  const headerLen = 16;
  const totalLen = headerLen + senderBytes.length + targetBytes.length + payloadBytes.length;
  const buf = new Uint8Array(totalLen);
  const view = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);

  view.setUint32(0, MAGIC, false); // Big endian
  view.setUint8(4, msgType);
  view.setUint8(5, 0); // flags
  view.setUint8(6, senderBytes.length);
  view.setUint8(7, targetBytes.length);
  view.setUint32(8, payloadBytes.length, false);
  view.setUint32(12, 0, false); // reserved

  let offset = headerLen;
  buf.set(senderBytes, offset);
  offset += senderBytes.length;
  buf.set(targetBytes, offset);
  offset += targetBytes.length;
  if (payloadBytes.length > 0) {
    buf.set(payloadBytes, offset);
  }

  return buf;
}

class PacketParser {
  constructor(onPacket) {
    this.onPacket = onPacket;
    this.buffer = new Uint8Array(0);
  }

  push(chunk) {
    if (!chunk || chunk.length === 0) return;
    const newBuf = new Uint8Array(this.buffer.length + chunk.length);
    newBuf.set(this.buffer);
    newBuf.set(chunk, this.buffer.length);
    this.buffer = newBuf;

    while (this.buffer.length >= 16) {
      const view = new DataView(this.buffer.buffer, this.buffer.byteOffset, this.buffer.byteLength);
      const magic = view.getUint32(0, false);
      if (magic !== MAGIC) {
        // Corrupted stream, advance 1 byte to re-sync
        this.buffer = this.buffer.subarray(1);
        continue;
      }

      const msgType = view.getUint8(4);
      const senderLen = view.getUint8(6);
      const targetLen = view.getUint8(7);
      const payloadLen = view.getUint32(8, false);

      const packetLen = 16 + senderLen + targetLen + payloadLen;
      if (this.buffer.length < packetLen) {
        // Need more data
        break;
      }

      let offset = 16;
      const dec = new TextDecoder();
      const sender = dec.decode(this.buffer.subarray(offset, offset + senderLen));
      offset += senderLen;
      const target = dec.decode(this.buffer.subarray(offset, offset + targetLen));
      offset += targetLen;
      const payload = this.buffer.slice(offset, offset + payloadLen);

      this.buffer = this.buffer.subarray(packetLen);

      if (this.onPacket) {
        this.onPacket({ msgType, sender, target, payload });
      }
    }
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    MAGIC,
    MSG_HELLO,
    MSG_HELLO_ACK,
    MSG_TELL,
    MSG_PING,
    MSG_PONG,
    encodePacket,
    PacketParser
  };
}
