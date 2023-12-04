const fs = require('fs');
const BLOCK_SIZE=128;
const FILE_SIZE=BLOCK_SIZE * 5;
const buff = Buffer.alloc(FILE_SIZE);

for(var i=0; i < FILE_SIZE; i++) {
    buff[i] = i ^ 0xff;
}
fs.writeFileSync(`test-${FILE_SIZE}.bin`, buff);
console.log('file written');
