/*
	ESP32:
		RAM:		327680 bytes.
		Flash:	1310720 bytes.
*/

//Chip select ADC.
#define ADC_CS 5

//Porta UDP locale.
#define UDP_LOCAL_PORT 5000

//Parametri del timer di campionamento.
#define PRESCALER		5000	//80.000.000Hz / 5.000 = 16.000Hz
#define AUTORELOAD	1

/*
	Il valore massimo di pacchetti UDP al secondo inviabile da un ESP32 è di circa 50.
	Questo dato è stato ricavato sperimentalmente utilizzando un payload di	poco più di 1000 byte.

	Il valore (in campioni al sec.) di UDP_PAYLOAD_SIZE si calcola quindi in questo modo:
			sample rate (campioni al sec.)					16.000Campioni
		----------------------------------	=>	------------------	=>	320 campioni per pacchetto.
					pacchetti al secondo								50 pacchetti
	
	Siccome un campione pesa 2 byte, allora verranno effettivamente inviati 640 byte.
	Essendo 640 byte < 1000 byte, l'ESP32 riuscirà a sostenere l'invio dei dati senza perdita di
	pacchetti dovuti al chip fisico del WiFi.

	Sono quindi 640 byte di RAM.
*/
#define UDP_PAYLOAD_SIZE	320

//4 volte UDP_PAYLOAD_SIZE per essere sicuri di non saturare; sono 2560 byte di RAM.
#define CIRC_BUFFER_SIZE	1280

//CIRC_BUFFER_SIZE / UDP_PAYLOAD_SIZE
#define CIRC_BUFFER_SECTIONS	4

/*
	Info sul funzionamento logico:
		-	Se si devono inviare tutti i dati contenuti nella sezione del buffer n, basta inviare tutti i
			dati nel range [n * UDP_PAYLOAD_SIZE, ((n + 1) * UDP_PAYLOAD_SIZE) - 1].
			_______________
			n	|	f(n)
			0	|	[0, 319]
			1	|	[320, 639]
			2	|	[640, 959]
			3	|	[960, 1279]

		-	Il campionamento effettivo non viene effettuato all'interno della ISR, ma viene eseguito tramite un deferred interrupt:
			https://youtu.be/qsflCf6ahXU?list=PLEBQazB0HUyQ4hAPU1cJED6t3DU0h34bz&t=519
			https://www.freertos.org/a00124.html

			Deferred interrupt avendo il task di campionamento B con priorità maggiore di tutte le altre (vedi video):
				1)	B si blocca in attesa di un semaforo binario.
				2)	Viene eseguito un altro qualsiasi task A dallo scheduler.
				3)	Scatta l'ISR che sblocca il semaforo.
				4)	All'interno della ISR viene verificato anche se qualche altro task con priorità >A
						esiste in attesa del suddetto semaforo.
				
				5)	In questo caso, B>A e quindi l'ISR ritorna ad A che torna subito il controllo allo scheduler.
				6)	A questo punto lo scheduler vede che B è ready e che B>A e quindi viene eseguito direttamente B.
*/
