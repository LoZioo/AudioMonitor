#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// #include <Soft_MCP3202.h>
#include <MCP3202.h>
#include <const.h>

// Credenziali WiFi
#include <password.h>

//NB: L'OTA non è disponibile su ESP-01: la flash è troppo piccola.

//ISR Interrupt: IRAM_ATTR indica che deve risiedere in RAM e non in FLASH.
void IRAM_ATTR onTimerISR();

void start_sampling();
#define stop_sampling timer1_disable

inline void encode_5_samples();

// Soft_MCP3202 adc(SOFTSPI_MOSI, SOFTSPI_MISO, SOFTSPI_SCK, ADC_CS);
MCP3202 adc(ADC_CS);
WiFiUDP sock;

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
uint64_t payload;

//Numero di sequenza del pacchetto UDP [0; 15].
uint8_t seq_num = 0;

//Per passare alla fase successiva; si setta da interrupt e si resetta da loop.
volatile bool ok = false;

void setup(){
	// system_update_cpu_freq(SYS_CPU_160MHZ);

	pinMode(LED_BUILTIN_AUX, OUTPUT);
	digitalWrite(LED_BUILTIN_AUX, HIGH);

	timer1_attachInterrupt(onTimerISR);

	WiFi.mode(WIFI_STA);
	WiFi.begin(STASSID, STAPSK);

	while(WiFi.status() != WL_CONNECTED){
		digitalWrite(LED_BUILTIN_AUX, !digitalRead(LED_BUILTIN_AUX));
		delay(500);
	}

	digitalWrite(LED_BUILTIN_AUX, HIGH);

	sock.begin(UDP_LOCAL_PORT);
}

void loop(){
	/*
		Finché non si dà l'ok dall'interrupt per passare alla fase
		successiva, controlla se ci sono pacchetti in entrata.
	*/
	while(!ok){
		//Se ci sono pacchetti da leggere nel buffer della scheda di rete; la funzione non è bloccante.
		if(sock.parsePacket()){
			uint8_t action;

			//Ritorna il numero di byte ricevuti.
			sock.read(&action, sizeof(action));

			switch(action){
				//Abilitazione.
				case 0:
				case 1:
					//Salvo l'IP e la porta del client per l'invio successivo dei campioni.
					remoteIP = sock.remoteIP();
					remotePort = sock.remotePort();
					
					action ? start_sampling() : stop_sampling();
					digitalWrite(LED_BUILTIN_AUX, !action);
					break;
				
				//Reset da remoto.
				case 2:
					stop_sampling();
					ESP.restart();
					break;
			}
		}
	}

	/*
		Disattivo tutti gli interrupt per non essere interrotto nella fase di acquisizione e di invio.
		Il prezzo da pagare è che ogni tot tempo, viene saltato un campione e viene preso il successivo
		(su ESP8266, non funziona; su AVR funziona).
	*/
	cli();

	//Campionamento in tutte le fasi.
	buf[phase] = adc.read();

	//Encoding ed invio nella fase 4.
	if(phase == 4){
		encode_5_samples();

		//Invio i 5 campioni.
		sock.beginPacket(remoteIP, remotePort);
		sock.write((char*) &payload, sizeof(payload));
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
	Da un buffer di 5 campioni a 16 bit (buf), crea un
	unico intero a 64 bit (payload) in questo modo:
		ssss1111 11111111 22222222 22223333
		33333333 44444444 44445555 55555555
	
	ssss sono utilizzati come numero di sequenza del pacchetto UDP.
*/
inline void encode_5_samples(){
	payload = seq_num++;
	
	for(int i=0; i<5; i++){
		/*
			Shifto ad ogni passo di 12 bit a sinistra e faccio l'or
			con i soli 12 dei 16 bit dell'i-esimo campione.
		*/
		
		payload <<= 12;
		payload |= (buf[i] & 0xFFF);
	}
}

void start_sampling(){
	/*
		Autoreload (ARR) del timer1:
			ARR = (80.000.000 / PRE) / Freq. voluta
			In questo caso: Freq. voluta = 8kHz.
			
			max(ARR) = 8.388.607
	*/

	//8kHz
	// timer1_write(625);

	//4kHz
	// timer1_write(1250);
	
	//2kHz
	timer1_write(2500);

	//20Hz, debug.
	// timer1_write(250000);

	/*
		Abilita il timer.
		Parametri (ESP8266 freq. CPU: 80MHz):
			Prescaler (PRE): TIM_DIV1, TIM_DIV16, TIM_DIV256.
			Modalità di funzionamento: TIM_EDGE; questa dovrebbe essere la sola modalità supportata dal timer1 (overflow).
			Clear del timer: TIM_SINGLE, TIM_LOOP:
				TIM_SINGLE non resetta il counter del timer alla fine dell'esecuzione dell'ISR.
				TIM_LOOP resetta il counter del timer automaticamente alla fine dell'esecuzione dell'ISR.
	*/
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
}

//ISR overflow timer1.
void IRAM_ATTR onTimerISR(){
	ok = true;
}
