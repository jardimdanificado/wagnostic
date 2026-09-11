/**
 * Zero-dependency pure JavaScript TAR archive extractor.
 */

function extractFromTar(buf, targetFileName) {
  let offset = 0;
  let lastFound = null;
  const u8 = new Uint8Array(buf.buffer || buf);

  while (offset + 512 <= u8.byteLength) {
    if (u8[offset] === 0) break;
    let nameLen = 0;
    while (nameLen < 100 && u8[offset + nameLen] !== 0) nameLen++;
    const name = new TextDecoder().decode(u8.subarray(offset, offset + nameLen));

    let sizeStr = '';
    for (let i = 0; i < 12; i++) {
      const ch = u8[offset + 124 + i];
      if (ch >= 48 && ch <= 55) sizeStr += String.fromCharCode(ch);
    }
    const size = parseInt(sizeStr, 8) || 0;

    if (name === targetFileName || name.endsWith('/' + targetFileName)) {
      lastFound = u8.subarray(offset + 512, offset + 512 + size);
    }
    const skip = size + ((512 - (size % 512)) % 512);
    offset += 512 + skip;
  }
  return lastFound;
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { extractFromTar };
}
