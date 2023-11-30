const net = require("net");
const fs = require("fs");

const { hexDump } = require("./utils");
const PORT = 3001;

const SOH = 0x01; // start of header
const EOT = 0x04; // end of trans
const ACK = 0x06; // ack
const NAK = 0x15; // ! ack
const CAN = 0x18; // cancel

const BLOCK_SIZE = 128;
const BLOCK_DATA_OFFSET = 3;

const fileBuff = fs.readFileSync("test.bin");
const FILE_SIZE = fileBuff.length;
console.log("FILE SIZE: " + FILE_SIZE);

const createNextBlock = (blockNum, currentOffset) => {
    var bufferCopy = Buffer.from(fileBuff);
    var blockCopy = bufferCopy.slice(currentOffset, currentOffset + 128);

    console.log('CO: ', currentOffset)
    //hexDump(blockCopy);

    var blockBuffer = Buffer.alloc(132);

    var isLastBlock = currentOffset + BLOCK_SIZE >= FILE_SIZE;

    if (isLastBlock) {
        console.log("last block!");
    }

    blockBuffer[0] = isLastBlock ? EOT : SOH;
    blockBuffer[1] = blockNum;
    blockBuffer[2] = 0xff - blockNum;

    var sum = 0;
    for (var o = BLOCK_DATA_OFFSET; o < BLOCK_SIZE + BLOCK_DATA_OFFSET; o++) {
        blockBuffer[o] = blockCopy[o - BLOCK_DATA_OFFSET];
        sum += blockCopy[o - BLOCK_DATA_OFFSET];
        sum = sum & 0xff;
    }

    blockBuffer[131] = sum;

   // hexDump(blockBuffer);
    return blockBuffer;
};

const server = net.createServer((socket) => {
    console.log("Client connected");

    var transferStarted = false;
    var transferComplete = false;
    var awaitingResponse = false;
    var currentBlock = 1;
    var currentOffset = 0;
    var eot = 0;

    socket.on("data", (data) => {

        if(transferComplete) {
            return;
        }

        var firstByte = data.readInt8(0);
        if (firstByte === CAN) {
            console.log("Transfer Error, recieved CAN from client.");
            socket.end();
            return;
        }

        if (awaitingResponse) {
            if (firstByte === ACK) {
                currentBlock++;
                currentOffset += BLOCK_SIZE;
                console.log(
                    "BLOCK ACK recieved, cb:" + currentBlock + ' off: ' + currentOffset
                );

            }

            if (firstByte === NAK && eot == 0) {
                console.log("block bad. resend increase retry counter");
                currentOffset += BLOCK_SIZE;
            }

            var currBuff = createNextBlock(currentBlock, currentOffset);
            if(currBuff.readUInt8(0) == EOT) {
                eot++
                console.log('EOT:',eot);
            }

            if(eot == 2) {
                console.log('END!')
                transferComplete = true;
                awaitingResponse = false;
                socket.end();
                return;
            }
            socket.write(currBuff);
        }

        if (!transferStarted) {
        console.log('first nak');
            if (firstByte === NAK) {
                transferStarted = true;

                var currBuff = createNextBlock(currentBlock, currentOffset);
                socket.write(currBuff);
                awaitingResponse = true;
            }
        }
    });

    socket.on("end", () => {
        console.log("Client disconnected");
    });
});

server.listen(PORT, () => {
    console.log(`Server listening on port ${PORT}`);
});
