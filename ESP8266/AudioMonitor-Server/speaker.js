const dgram = require('dgram');
const stream = require('stream');
const Speaker = require('speaker');

//Binding della socket UDP.
const HOST = process.argv[2];
const PORT = 5000;

console.clear();
console.log("Riproduzione dati da " + HOST + ":" + PORT + " (CTRL+C per uscire).");

//CTRL-C per uscire.
process.on("SIGINT", () => {
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
  sampleRate:	2000
});

//Stream di passaggio dove verranno immessi i campioni.
const readStream = new stream.Readable({ read(){} });

//socket > readStream > speaker.
readStream.pipe(speaker);

const sock = dgram.createSocket("udp4");

sock.bind(PORT, "0.0.0.0");
sock.on("message", (message, remote) => {
	/*
		Formato:
			ssss1111 11111111 22222222 22223333
			33333333 44444444 44445555 55555555
	*/

	const seq_num = (message.readInt8(7) & 0xF0) >> 4;

	const buf = Int16Array.of(
		message.readInt16LE(6) & 0xFFF,
		(message.readInt16LE(4) & 0xFFF0) >> 4,
		message.readInt16LE(3) & 0xFFF,
		(message.readInt16LE(1) & 0xFFF0) >> 4,
		message.readInt16LE(0) & 0xFFF
	);

	//Upscaling da 12 a 14 bit (aumento volume).
	for(let i=0; i<5; i++)
		buf[i] = map(buf[i], 0, 4095, -16384, 16383);
	
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
