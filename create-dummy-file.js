const fs = require('fs');

const buff = Buffer.alloc(512);

for(var i =0; i < 512;i++) {
    buff[i]= 0xFF;
}


fs.writeFileSync('test.bin', buff);
console.log('file written');
