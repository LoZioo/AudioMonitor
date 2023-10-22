//Uso comando:	node speaker IP

const dgram = require('dgram');					//UDP Socket.
const stream = require('stream');				//File as stream.
const readline = require('readline');		//Read from stdin.
const Speaker = require('speaker');			//Write into the speaker buffer.

//Binding della socket UDP.
const HOST = process.argv[2];
const PORT = 5000;

console.clear();
console.log("Riproduzione dati da " + HOST + ":" + PORT + " (premi un tasto per uscire).");

//Premere un tasto per uscire.
readline.emitKeypressEvents(process.stdin);

if(process.stdin.isTTY)
	process.stdin.setRawMode(true);

process.stdin.on("keypress", (chunk, key) => {
	console.log("Disconnetto...")

	//Comando di fine campionamento.
	const message = Buffer.alloc(1);
	message.writeUInt8(0, 0);

	sock.send(message, 0, message.byteLength, PORT, HOST, () => {
		sock.unref();
		sock.close();

		process.exit();
	});
});

//Parametri dei dati PCM in input.
const speaker = new Speaker({
  channels:		1,
  bitDepth:		16,
  sampleRate:	16000
});

//Stream di passaggio dove verranno immessi i campioni.
const readStream = new stream.Readable({ read(){} });

//socket > readStream > speaker.
readStream.pipe(speaker);

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
