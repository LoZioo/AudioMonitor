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

//Comunicazione ed interazione esterna
//--------------------------------------------------------------------------------//

//Per memorizzare il client a cui inviare i dati dopo che questo ha inviato il
//primo comando di abilitazione; variabili protette da mutex_remote_ip_port.
IPAddress remoteIP;
uint16_t remotePort;

//Thread ed IPC
//--------------------------------------------------------------------------------//

//Thread.
void sample_thread(void*), send_thread(void*), control_thread(void*);
TaskHandle_t sample_thread_handle, send_thread_handle, control_thread_handle;

//Semafori.
SemaphoreHandle_t sem_ready_for_sampling = NULL;
SemaphoreHandle_t sem_ready_for_sending = NULL;

//Mutex.
SemaphoreHandle_t mutex_remote_ip_port = NULL;

//NB: in architetture MCU, esistono differenze tra mutex e spinlock:
//https://www.esp32.com/viewtopic.php?t=14727

//Codice
//--------------------------------------------------------------------------------//

void setup(){
	Serial.begin(115200);
	
	//Setup WiFi.
	WiFi.mode(WIFI_STA);
	WiFi.begin(STASSID, STAPSK);

	while(WiFi.status() != WL_CONNECTED)
		delay(500);

	//Setup timer0.
	timer0 = timerBegin(0, PRESCALER, true);
	timerAlarmWrite(timer0, AUTORELOAD, true);
	timerAttachInterrupt(timer0, &onTimerISR, true);

	//Semaforo binario per il campionamento.
	//xSemaphoreCreateBinary inizializza a ZERO.
	//Questo va bene in quanto si tratta di un semaforo binario.
	sem_ready_for_sampling = xSemaphoreCreateBinary();

	//Semaforo contatore per l'invio.
	//Il valore iniziale è zero ed il massimo disponibile è pari alle
	//sezioni del buffer meno uno in quanto in una si starà scrivendo.
	sem_ready_for_sending = xSemaphoreCreateCounting(CIRC_BUFFER_SECTIONS - 1, 0);

	//Mutex per la sezione critica di remoteIP e remotePort.
	//xSemaphoreCreateMutex inizializza a UNO.
	//Questo va bene in quanto si tratta di un mutex.
	mutex_remote_ip_port = xSemaphoreCreateMutex();

	/*
		Parameters:
			Function to implement the task.
			Name of the task.
			Stack size in bytes (words in vanilla FreeRTOS).
			Task input parameter.
			Priority of the task.
			Task handle.
			Core where the task should run.
	*/
	xTaskCreatePinnedToCore(sample_thread,	"sample",		10240,	NULL,	3,	&sample_thread_handle,	tskNO_AFFINITY);
	xTaskCreatePinnedToCore(send_thread,		"send",			10240,	NULL,	2,	&send_thread_handle,		tskNO_AFFINITY);
	xTaskCreatePinnedToCore(control_thread,	"control",	10240,	NULL,	1,	&control_thread_handle,	tskNO_AFFINITY);

	//Rimozione del task del setup.
	vTaskDelete(NULL);
}

void loop(){}

void control_thread(void *parameter){
	//Socket di ricezione.
	WiFiUDP sock;

	//Bind socket.
	sock.begin(UDP_LOCAL_PORT);

	//Gestisci le richieste dall'esterno.
  for(;;){
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
					xSemaphoreTake(mutex_remote_ip_port, portMAX_DELAY);
					remoteIP = sock.remoteIP();
					remotePort = sock.remotePort();
					xSemaphoreGive(mutex_remote_ip_port);
					
					action ? start_sampling() : stop_sampling();
					break;
				
				//Reset da remoto.
				case 2:
					stop_sampling();
					ESP.restart();
					break;
			}
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void send_thread(void *parameter){
	//Socket di trasmissione.
	WiFiUDP sock;

	//Sezione di lettura attuale del buffer.
	uint8_t section = 0;

	for(;;){
		//Attendi che da sample_thread ci sia il via per inviare.
		xSemaphoreTake(sem_ready_for_sending, portMAX_DELAY);
		
		//Invia i dati nella sezione attuale.
		xSemaphoreTake(mutex_remote_ip_port, portMAX_DELAY);
		sock.beginPacket(remoteIP, remotePort);
		sock.write((uint8_t*) &buf[section * UDP_PAYLOAD_SIZE], (((section + 1) * UDP_PAYLOAD_SIZE) - 1) * sizeof(uint16_t));
		sock.endPacket();
		xSemaphoreGive(mutex_remote_ip_port);

		section++;
		if(section == CIRC_BUFFER_SECTIONS)
			section = 0;
	}
}

void sample_thread(void *parameter){
	//Sezione di scrittura attuale del buffer.
	uint8_t prev_section, section = 0;

	for(;;){
		//Attendi che dalla ISR ci sia il via per campionare.
		xSemaphoreTake(sem_ready_for_sampling, portMAX_DELAY);
		buf[cur] = adc.read();

		//Controllo se la sezione del buffer è stata riempita completamente.
		prev_section = section;
		section = map(cur, 0, CIRC_BUFFER_SIZE, 0, CIRC_BUFFER_SECTIONS);

		//Se send_thread non riesce a gestire la velocità impostata,
		//viene visualizzato questo errore.
		if(prev_section != section && xSemaphoreGive(sem_ready_for_sending) == pdFALSE)
			Serial.println("pdFALSE in xSemaphoreGive.");

		cur++;
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

inline void start_sampling(){
	timerWrite(timer0, 0);
	timerAlarmEnable(timer0);
}

inline void stop_sampling(){
	timerAlarmDisable(timer0);
}
