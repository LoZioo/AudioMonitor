//Uso comando:	node recorder IP nomeFile

const dgram = require('dgram');
const stream = require('stream');
const readline = require('readline');
const lame = require('@suldashi/lame');
const fs = require('fs');

//Binding della socket UDP.
const HOST = process.argv[2];
const PORT = 5000;

//Nome file mp3 in cui salvare l'audio.
const FILENAME = process.argv[3];

console.clear();
console.log("Salvataggio dati da " + HOST + ":" + PORT + " in " + FILENAME + ".mp3 (premi un tasto per salvare ed uscire).");

//Premere un tasto per uscire.
readline.emitKeypressEvents(process.stdin);

if(process.stdin.isTTY)
	process.stdin.setRawMode(true);

process.stdin.on("keypress", (chunk, key) => {
	console.log("Salvo e disconnetto...");

	//Comando di fine campionamento.
	const message = Buffer.alloc(1);
	message.writeUInt8(0, 0);

	sock.send(message, 0, message.byteLength, PORT, HOST, () => {
		sock.unref();
		sock.close();

		file.end(() => process.exit());
	});
});

//File MP3.
const file = fs.createWriteStream(FILENAME + ".mp3");

//Encoder MP3.
const encoder = new lame.Encoder({
	//Input
	channels:		1,
	bitDepth:		16,
	sampleRate:	16000,

	//Output
	bitRate:				128,
	outSampleRate:	16000,
	mode:						lame.MONO		// STEREO (default), JOINTSTEREO, DUALCHANNEL or MONO.
});

//Stream di passaggio dove verranno immessi i campioni.
const readStream = new stream.Readable({ read(){} });

//socket > readStream > encoder > file.
readStream
	.pipe(encoder)
	.pipe(file);

const sock = dgram.createSocket("udp4");

sock.bind(PORT, "0.0.0.0");
sock.on("message", (message, remote) => {
	const buf = new Int16Array(message.length/2);

	for(let i=0; i<message.length/2; i++){
		//Upscaling da 12 a 14 bit (aumento volume).
		buf[i] = map(message.readInt16LE(i * 2), 0, 4095, -16384, 16383);
	}
	
	readStream.push(Buffer.from(buf.buffer));
});

//Comando di inizio campionamento.
const message = Buffer.alloc(1);
message.writeUInt8(1, 0);

sock.send(message, 0, message.byteLength, PORT, HOST);

//------------------------------------------------------------//

function map(x, in_min, in_max, out_min, out_max){
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
