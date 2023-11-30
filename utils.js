function hexDump(buffer) {
  // Display the hex dump of the buffer
  for (let i = 0; i < buffer.length; i += 16) {
    const chunk = buffer.slice(i, i + 16);
    const hexValues = Array.from(chunk).map(byte => byte.toString(16).padStart(2, '0')).join(' ');
    const asciiValues = chunk.toString('ascii').replace(/[\x00-\x1F\x7F-\xFF]/g, '.');

    console.log(`${i.toString(16).padStart(4, '0')}: ${hexValues.padEnd(49, ' ')} | ${asciiValues}`);
  }
}

module.exports = {
    hexDump
}
