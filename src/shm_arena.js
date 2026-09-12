/**
 * Piolho Shared Memory Rendezvous Arena (SAB + Atomics)
 * 
 * Provides lock-free, zero-allocation rendezvous IPC synchronization between
 * true OS threads and Web Workers.
 */

const MAX_WORKERS = 64;
const SLOT_SIZE = 256;
const HEADER_SIZE = 64;
const SLOTS_OFFSET = HEADER_SIZE;
const DATA_OFFSET = HEADER_SIZE + (MAX_WORKERS * SLOT_SIZE);
const DATA_SLOT_SIZE = 65536; // 64KB per worker payload slot
const TOTAL_ARENA_SIZE = DATA_OFFSET + (MAX_WORKERS * DATA_SLOT_SIZE);

// Slot Status Constants
const STATUS_IDLE         = 0;
const STATUS_WAITING_HEAR = 1;
const STATUS_WAITING_TELL = 2;
const STATUS_DATA_READY   = 3;
const STATUS_MATCHED      = 4;
const STATUS_CLOSED       = 5;

// Status field offsets within each 256-byte slot
const OFF_STATUS       = 0;  // Int32
const OFF_LOCK         = 4;  // Int32
const OFF_TARGET_ID    = 8;  // Int32 (-1 = ANY)
const OFF_CALLER_ID    = 12; // Int32
const OFF_PAYLOAD_SIZE = 16; // Int32
const OFF_MAX_SIZE     = 20; // Int32
const OFF_NAME         = 24; // 32 bytes ASCII
const OFF_TARGET_NAME  = 56; // 32 bytes ASCII

class ShmArena {
  constructor(sharedBuffer) {
    if (!sharedBuffer) {
      this.buffer = new SharedArrayBuffer(TOTAL_ARENA_SIZE);
    } else {
      this.buffer = sharedBuffer;
    }

    this.int32 = new Int32Array(this.buffer);
    this.uint8 = new Uint8Array(this.buffer);

    // Initialize header if creator
    if (!sharedBuffer) {
      this.int32[0] = 0x50494F4C; // 'PIOL'
      this.int32[1] = 2;          // version
      this.int32[2] = MAX_WORKERS;
      Atomics.store(this.int32, 3, 0); // active workers count
      Atomics.store(this.int32, 4, 0); // total messages transferred
    }
  }

  get totalMessages() {
    return Atomics.load(this.int32, 4);
  }

  getSlotOffset(workerId) {
    return SLOTS_OFFSET + (workerId * SLOT_SIZE);
  }

  getDataOffset(workerId) {
    return DATA_OFFSET + (workerId * DATA_SLOT_SIZE);
  }

  registerWorker(workerId, name) {
    const slotOff = this.getSlotOffset(workerId);
    const intIdx = slotOff >> 2;

    Atomics.store(this.int32, intIdx + (OFF_STATUS >> 2), STATUS_IDLE);
    Atomics.store(this.int32, intIdx + (OFF_LOCK >> 2), 0);
    this.int32[intIdx + (OFF_TARGET_ID >> 2)] = -1;
    this.int32[intIdx + (OFF_CALLER_ID >> 2)] = workerId;
    this.int32[intIdx + (OFF_PAYLOAD_SIZE >> 2)] = 0;
    this.int32[intIdx + (OFF_MAX_SIZE >> 2)] = DATA_SLOT_SIZE;

    // Write name
    const nameOff = slotOff + OFF_NAME;
    this.uint8.fill(0, nameOff, nameOff + 32);
    for (let i = 0; i < 31 && i < name.length; i++) {
      this.uint8[nameOff + i] = name.charCodeAt(i);
    }
  }

  getWorkerName(workerId) {
    const nameOff = this.getSlotOffset(workerId) + OFF_NAME;
    let len = 0;
    while (len < 32 && this.uint8[nameOff + len] !== 0) len++;
    return new TextDecoder().decode(this.uint8.subarray(nameOff, nameOff + len));
  }

  findWorkerIdByName(name) {
    if (!name) return -1;
    for (let id = 0; id < MAX_WORKERS; id++) {
      const wname = this.getWorkerName(id);
      if (wname === name) return id;
    }
    return -1;
  }

  closeWorker(workerId) {
    const slotOff = this.getSlotOffset(workerId);
    const intIdx = slotOff >> 2;
    Atomics.store(this.int32, intIdx + (OFF_STATUS >> 2), STATUS_CLOSED);
    Atomics.notify(this.int32, intIdx + (OFF_STATUS >> 2));
  }
}

module.exports = {
  ShmArena,
  MAX_WORKERS,
  SLOT_SIZE,
  DATA_SLOT_SIZE,
  TOTAL_ARENA_SIZE,
  STATUS_IDLE,
  STATUS_WAITING_HEAR,
  STATUS_WAITING_TELL,
  STATUS_DATA_READY,
  STATUS_MATCHED,
  STATUS_CLOSED,
  OFF_STATUS,
  OFF_LOCK,
  OFF_TARGET_ID,
  OFF_CALLER_ID,
  OFF_PAYLOAD_SIZE,
  OFF_MAX_SIZE,
  OFF_NAME,
  OFF_TARGET_NAME
};
