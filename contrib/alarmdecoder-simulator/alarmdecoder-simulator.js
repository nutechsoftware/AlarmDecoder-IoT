/**
 *  @file alarmdecoder-simulator.js
 *
 *  @author Sean Mathews <coder@f34r.com>
 *    I made this for testing logic that parses an AlarmDecoder stream.
 *  This does require the client being tested to support ser2sock.
 *
 *  @version 1.0 - 09/5/21
 *
 *  @brief This nodejs program creates a listening socket on port 10000 and
 *  when a client connects it begins sending at a fixed rate a repeating stream
 *  of messages stored in a text file. It also allows pausing the stream by
 *  pressing space. To exit the program press control-c or escape.
 *
 *  @param Filename with AD2* protocol messages to replay.
 *
 *  @example nodejs alarmdecoder-simulator.js AlarmDecoder_Log_1.txt
 *
 */
const net = require('net');
const fs = require('fs');
const readline = require('readline');
const process = require('process');
const keypress = require('keypress');

// line transmit rate
var replaySpeed = 800;

// get the file with AlarmDecoder messages to spool
process.argv.splice(0, 2);
var AD2ProtocolFile = process.argv.join(' ');

// check if the argument is a file that exists
if (!fs.existsSync(AD2ProtocolFile)) {
    console.log('Error file `' + AD2ProtocolFile + '` not found for spooling. Done.');
    process.exit(1);
}

// Load the contents of the file into an array of lines into memory.
var AD2ProtocolArray = fs.readFileSync(AD2ProtocolFile).toString().split("\n");
console.log('AlarmDecoder protocol stream loaded with `' + AD2ProtocolArray.length + '` lines of AlarmDecoder messages.');
console.log('Press Q escape or control-c to exit. Press space to pause and resume stream.')
// decorate stdin to send keypress events
keypress(process.stdin);
process.stdin.setRawMode(true);
process.stdin.resume();

// pause state var
var pause = false;

// keypress event for PAUSE and exit control
process.stdin.on('keypress', function (str, key) {
    if ((key.ctrl & key.name === 'c') || key.name === 'escape' || key.name === 'q') {
        console.log("Exit key pressed. Done.")
        process.exit(0);
    } else
    if (key.name === 'space') {
        pause ^= true;
        if (pause) {
            console.log('Paused..');
        } else {
            console.log('Resumed..');
        }
    }
});


// fake ser2sock server with file replay Interval event handler
var server = net.createServer(function(connection) {
    console.log('client connected');

    // index into array of AlarmDecoder messages being sent
    var i = 0;

    // start play timer
    var socketTimer = setInterval(function () {
        // if paused do nothing
        if (pause) return;

        // get line to send and increment to next line
        var line = AD2ProtocolArray[i++];

        // don't send empty lines.
        if (line.length) {
            console.log(line);
            line += '\r\n';
            connection.write(line);
        }

        // all done? rewind.
        if (i>=AD2ProtocolArray.length) {
            i = 0;
        }
    }, replaySpeed);

    // cleanup when client disconnects
    connection.on('end', function() {
        clearInterval(socketTimer);
        console.log('client disconnected');
    });

    connection.pipe(connection);
});

// start the fake ser2sock
server.listen(10000, function() {
    console.log('ser2sock AlarmDecoder simulator is listening for connections.');
});
