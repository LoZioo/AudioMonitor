//Librerie
//--------------------------------------------------------------------------------//

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

#include <MCP3202.h>
#include <const.h>

// Credenziali WiFi
#include <password.h>

//Acquisizione dati
//--------------------------------------------------------------------------------//

//ADC MCP3202
MCP3202 adc(ADC_CS);

//Timer hardware.
hw_timer_t *timer0;

//ISR Interrupt: IRAM_ATTR indica che deve risiedere in RAM e non in FLASH.
void IRAM_ATTR onTimerISR();
inline void start_sampling(), stop_sampling();

//Buffering dati
//--------------------------------------------------------------------------------//

//Buffer circolare ed indice corrente di scrittura.
uint16_t buf[CIRC_BUFFER_SIZE], cur;

//Sezioni in uso nel buffer circolare.
uint8_t wBufSection, rBufSection;

//Comunicazione ed interazione esterna
//--------------------------------------------------------------------------------//

//Socket UDP per trasmissione e ricezione di dati e comandi.
WiFiUDP sendSock, recvSock;

//Per memorizzare il client a cui inviare i dati dopo che questo
//ha inviato il primo comando di abilitazione.
IPAddress remoteIP;
uint16_t remotePort;

//Thread ed IPC
//--------------------------------------------------------------------------------//

void sample_thread(void*), send_thread(void*), control_thread(void*);
TaskHandle_t sample_thread_handle, send_thread_handle, enable_thread_handle;

SemaphoreHandle_t sem_ready_for_sampling = NULL;

//Codice
//--------------------------------------------------------------------------------//

void setup(){
	Serial.begin(115200);
	
	//Setup WiFi.
	WiFi.mode(WIFI_STA);
	WiFi.begin(STASSID, STAPSK);

	while(WiFi.status() != WL_CONNECTED)
		delay(500);

	//Bind socket di ricezione.
	recvSock.begin(UDP_LOCAL_PORT);

	//Setup timer0.
	timer0 = timerBegin(0, PRESCALER, true);
	timerAlarmWrite(timer0, AUTORELOAD, true);
	timerAttachInterrupt(timer0, &onTimerISR, true);

	//Semaforo binario di campionamento.
	//xSemaphoreCreateBinary lo inizializza a zero.
	sem_ready_for_sampling = xSemaphoreCreateBinary();

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

inline void start_sampling(){
	timerWrite(timer0, 0);
	timerAlarmEnable(timer0);
}

inline void stop_sampling(){
	timerAlarmDisable(timer0);
}

void control_thread(void *parameter){}

void send_thread(void *parameter){
	for(;;){
		

		sendSock.beginPacket(remoteIP, remotePort);
		sendSock.write((uint8_t*) buf, sizeof(buf));
		sendSock.endPacket();
	}
}

void sample_thread(void *parameter){
	for(;;){
		//Attendi che dalla ISR ci sia il via per campionare.
		xSemaphoreTake(sem_ready_for_sampling, portMAX_DELAY);
		
		buf[cur++] = adc.read();
		wBufSection = map(cur, 0, CIRC_BUFFER_SIZE, 0, CIRC_BUFFER_SECTIONS);

		if(cur == CIRC_BUFFER_SIZE)
			cur = 0;
	}
}

//ISR overflow timer0.
void IRAM_ATTR onTimerISR(){
	BaseType_t task_woken = pdFALSE;

	//Deferred interrupt per campionare.
	//Se sample_thread non riesce a gestire la velocità impostata,
	//viene visualizzato questo errore.
	if(xSemaphoreGiveFromISR(sem_ready_for_sampling, &task_woken) == errQUEUE_FULL)
		Serial.println("errQUEUE_FULL in xSemaphoreGiveFromISR.");
  
	//API per implementare il deferred interrupt.
  //Exit from ISR (Vanilla FreeRTOS)
  //portYIELD_FROM_ISR(task_woken);

  //Exit from ISR (ESP-IDF)
  if(task_woken)
    portYIELD_FROM_ISR();
}
