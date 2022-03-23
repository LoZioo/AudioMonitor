//Porta UDP locale.
#define UDP_LOCAL_PORT 5000

//Chip select ADC.
#define ADC_CS 5

/*
	Il valore massimo di pacchetti UDP al secondo inviabile da un ESP32 è di circa 50.

	Con 50 pacchetti al secondo si riesce ad ottenere un invio periodico fino a circa
	poco più di 1000 byte di payload.

	Il valore (in campioni al sec.) di BUF_SIZE si calcola quindi in questo modo:
			sample rate (campioni al sec.)					16.000Campioni
		----------------------------------	=>	------------------	=>	320 campioni per pacchetto.
					pacchetti al secondo								50 pacchetti
	
	Siccome un campione pesa 2 byte, allora verranno effettivamente inviati 640byte.
*/

#define BUF_SIZE 320

#define PRESCALER		5000	//80.000.000Hz / 5.000 = 16.000Hz
#define AUTORELOAD	1
