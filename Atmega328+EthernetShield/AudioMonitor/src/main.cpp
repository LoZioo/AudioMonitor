#include <Arduino.h>
#include <Ethernet.h>
#include <MCP3202.h>

#define UDP_LOCAL_PORT 5000
#define ADC_CS 8

void start_sampling(), stop_sampling();
inline uint64_t encode_5_samples(uint16_t *buf, uint8_t seq_num);

MCP3202 adc(ADC_CS);

//Un MAC Address a caso, basta che non sia uguale a qualcun'altro in rete.
uint8_t mac[] = { 0x4E, 0x89, 0x33, 0x9E, 0x67, 0x27 };

//Socket UDP.
EthernetUDP sock;

/*
	Per memorizzare il client a cui inviare i dati dopo
	che questo ha inviato il primo comando di abilitazione.
*/
IPAddress remoteIP;
uint16_t remotePort;

/*
	Ogni ciclo di loop è composto da cinque fasi ed in ognuna si
	campiona e si salva in buf[phase].

	Nell'ultima fase si campiona, si fa un encoding e si invia tutto.
*/
int8_t phase = 0;
uint16_t buf[5];

//Per passare alla fase successiva; si setta da interrupt e si resetta da loop.
volatile bool ok = false;

//Numero di sequenza del pacchetto UDP [0; 15].
uint8_t seq_num = 0;

void setup(){
	Serial.begin(9600);
	pinMode(7, OUTPUT);

	//Disabilito il CS della scheda SD.
	pinMode(4, OUTPUT);
	digitalWrite(4, HIGH);

	//CS ethernet su pin 10.
	Ethernet.init(10);

	IPAddress ip(192,168,1,24);
	//MAC Address.
	Ethernet.begin(mac, ip);

	//Socket in ascolto sulla porta specificata.
	sock.begin(UDP_LOCAL_PORT);

	Serial.print("Socket UDP in ascolto su ");
	Serial.print(Ethernet.localIP());
	Serial.print(':');
	Serial.print(sock.localPort());
	Serial.println('.');
}

void loop(){
	/*
		Finché non si dà l'ok dall'interrupt per passare alla fase
		successiva, controlla se ci sono pacchetti in entrata.
	*/
	while(!ok){
		//Se ci sono pacchetti da leggere nel buffer della scheda di rete; la funzione non è bloccante.
		if(sock.parsePacket()){
			uint8_t enable;

			//Ritorna il numero di byte ricevuti.
			sock.read(&enable, sizeof(enable));

			//Salvo l'IP e la porta del client per l'invio successivo dei campioni.
			remoteIP = sock.remoteIP();
			remotePort = sock.remotePort();

			//Per eventuale incoerenza dei dati ricevuti.
			enable = constrain(enable, 0, 1);
			enable ? start_sampling() : stop_sampling();

			Serial.println(String("enable = ") + String(enable ? "true" : "false"));
		}
	}

	/*
		Disattivo tutti gli interrupt per non essere interrotto nella fase di acquisizione e di invio.
		Il prezzo da pagare è che ogni tot tempo, viene saltato un campione e viene preso il successivo.
	*/
	cli();

	//Campionamento in tutte le fasi.
	buf[phase] = adc.read();

	//Encoding ed invio nella fase 4.
	if(phase == 4){
		digitalWrite(7, !digitalRead(7));
		uint64_t val = encode_5_samples(buf, seq_num++);

		//Invio i 5 campioni.
		sock.beginPacket(remoteIP, remotePort);
		sock.write((char*) &val, sizeof(val));
		sock.endPacket();

		//-1 perché viene incrementata subito dopo e torna a 0.
		phase = -1;
	}

	ok = false;
	phase++;

	//Riattivo tutti gli interrupt.
	sei();
}

/*
	Da un buffer di 5 campioni a 16 bit, crea un unico intero a
	64 bit in questo modo:
		ssss1111 11111111 22222222 22223333
		33333333 44444444 44445555 55555555
	
	ssss sono utilizzati come numero di sequenza del pacchetto UDP.
*/
inline uint64_t encode_5_samples(uint16_t *buf, uint8_t seq_num){
	uint64_t res = seq_num;
	
	for(int i=0; i<5; i++){
		/*
			Shifto ad ogni passo di 12 bit a sinistra e faccio l'or
			con i soli 12 dei 16 bit dell'i-esimo campione.
		*/
		
		res <<= 12;
		res |= (buf[i] & 0xFFF);
	}
	
	return res;
}

void start_sampling(){
	//Normal mode.
	TCCR1A = TCCR1B = 0;

	//Prescaler ad 1.
	TCCR1B |= (1 << CS10);

	//4Hz
	// TCCR1B |= (1 << CS12);

	//Abilitazione interrupt TIMER1_OVF_vect.
	TIMSK1 |= (1 << TOIE1);
}

void stop_sampling(){
	//Prescaler disattivato.
	TCCR1B = 0;
}

ISR(TIMER1_OVF_vect){
	ok = true;

	//8kHz
	TCNT1 = 63535;

	//4kHz
	// TCNT1 = 61535;

	//2kHz
	// TCNT1 = 57535;

	//4Hz
	// TCNT1 = 49910;
}
