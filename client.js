const net = require('net');
const fs = require('fs');

const SOH = 0x01; // start of header
const EOT = 0x04; // end of trans
const ACK = 0x06; // ack
const NAK = 0x15; // ! ack
const CAN = 0x18; // cancel

console.log('client trying to connect.');

const { hexDump } = require('./utils');
const client = new net.Socket();

const PORT = 3001;
const HOST = 'localhost';

const buff = Buffer.alloc(512);

let currentBlock=1;
let currentOffset =0;
let eot = 0;

const createBuffForVal = (val) => {
    var b = Buffer.alloc(1);
    b[0] = val;
    return b;
}

client.connect(PORT,HOST, () => {
    console.log('Sending Starting NAK');
    client.write(createBuffForVal(NAK));
});

client.on('data', (data) => {
    console.log('Incoming packet: ', currentBlock)
    hexDump(data);

    if(data.readUInt8(0) === EOT) {
        eot++;

        if(eot === 1) {
            client.write(createBuffForVal(NAK));
        }

        if(eot === 2) {
            console.log('Transfer Complete. Sending final ACK');
            client.write(createBuffForVal(ACK));
            client.end();
            return;
        }
        //Store last bit of data??
        
    }
    
    var firstByte = data.readUInt8(0);
    console.log('FIRST BYTE: ', firstByte);

    if(firstByte !== SOH && firstByte !== EOT) { console.log('soh?');client.write(createBuffForVal(CAN));  }
    if(data.readUInt8(1) !== currentBlock) { console.log('block?');client.write(createBuffForVal(CAN));  }
    if(data.readUInt8(2) !== 255-currentBlock) { console.log('1sb'); client.write(createBuffForVal(CAN));  }

    // console.log('Block processed successfully');
    var sum = 0;
    for(var o = 3; o < 131; o++) {
        sum += data.readUInt8(o);
        sum = sum & 0xFF;
    }

    var sentChecksum = data.readUInt8(131);
    if(sentChecksum == sum) {
        for(var n=0; n<128;n++) {
           buff[n+currentOffset] = data.readUInt8(n+3); 
        }
        //WRITE DATA to file buffer
        //increase block and offset
        client.write(createBuffForVal(ACK));
        currentBlock++;
        currentOffset += 128;
        console.log('Good!');
    } else {
        //IF BAD send NACK
        console.log('Bad!');
        client.write(createBuffForVal(NAK));
    }
});


client.on('close', () => {
    console.log('its over');
    fs.writeFileSync('out.bin', buff);
})
