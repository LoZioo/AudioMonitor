#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

#include <MCP3202.h>
#include <const.h>

// Credenziali WiFi
#include <password.h>

//ISR Interrupt: IRAM_ATTR indica che deve risiedere in RAM e non in FLASH.
void IRAM_ATTR onTimerISR();
void start_sampling(), stop_sampling();

//Codice eseguito su Core0.
void Core0code(void*);
TaskHandle_t Core0;

//Timer hardware.
hw_timer_t *timer;

MCP3202 adc(ADC_CS);
WiFiUDP sendSock, recvSock;

/*
	Per memorizzare il client a cui inviare i dati dopo
	che questo ha inviato il primo comando di abilitazione.
*/
IPAddress remoteIP;
uint16_t remotePort;

//Per passare all'istante di campionamento; si setta da interrupt e si resetta da loop.
volatile bool ok = false;

//Rispettivamente, il buffer dei campioni acquisiti e l'indice del campione corrente.
uint16_t buf[BUF_SIZE];
int curr = 0;

void setup(){
	Serial.begin(115200);
	pinMode(22, OUTPUT);
	
	timer = timerBegin(0, PRESCALER, true);
	timerAlarmWrite(timer, AUTORELOAD, true);
	timerAttachInterrupt(timer, &onTimerISR, true);

	WiFi.mode(WIFI_STA);
	WiFi.begin(STASSID, STAPSK);

	while(WiFi.status() != WL_CONNECTED)
		delay(500);

	recvSock.begin(UDP_LOCAL_PORT);

	xTaskCreatePinnedToCore(
		Core0code,	/* Function to implement the task */
		"Core0",		/* Name of the task */
		10000,			/* Stack size in words */
		NULL,				/* Task input parameter */
		0,					/* Priority of the task */
		&Core0,			/* Task handle. */
		0						/* Core where the task should run */
	);
}

void loop(){
	while(!ok)
		asm("nop");
	
	digitalWrite(22, !digitalRead(22));
	buf[curr] = adc.read();

	//Se il buffer è pieno, invia i dati.
	if(curr == BUF_SIZE - 1){
		curr = -1;

		sendSock.beginPacket(remoteIP, remotePort);
		sendSock.write((uint8_t*) buf, sizeof(buf));
		sendSock.endPacket();
	}
	curr++;

	//Attendo il prossimo istante di campionamento.
	ok = false;
}

void Core0code(void *parameter){
	//Gestisci le richieste dall'esterno.
  for(;;){
		//Se ci sono pacchetti da leggere nel buffer della scheda di rete; la funzione non è bloccante.
		if(recvSock.parsePacket()){
			uint8_t action;

			//Ritorna il numero di byte ricevuti.
			recvSock.read(&action, sizeof(action));

			switch(action){
				//Abilitazione.
				case 0:
				case 1:
					//Salvo l'IP e la porta del client per l'invio successivo dei campioni.
					remoteIP = recvSock.remoteIP();
					remotePort = recvSock.remotePort();
					
					action ? start_sampling() : stop_sampling();
					break;
				
				//Reset da remoto.
				case 2:
					stop_sampling();
					ESP.restart();
					break;
			}
		}

		delay(100);
	}
}

void start_sampling(){
	timerWrite(timer, 0);
	timerAlarmEnable(timer);
}

void stop_sampling(){
	timerAlarmDisable(timer);
}

//ISR overflow timer1.
void IRAM_ATTR onTimerISR(){
	ok = true;
}
