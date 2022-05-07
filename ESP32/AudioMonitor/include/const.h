/*
	https://www.electronicshub.org/getting-started-with-esp32/#:~:text=Specifications%20of%20ESP32,-ESP32%20has%20a&text=Single%20or%20Dual%2DCore%2032,for%20both%20Classic%20Bluetooth%20v4.
	
	ESP32:
		Single or Dual-Core 32-bit LX6 Microprocessor with clock frequency up to 240 MHz.
		520 KB of SRAM, 448 KB of ROM and 16 KB of RTC SRAM.
		Supports 802.11 b/g/n Wi-Fi connectivity with speeds up to 150 Mbps.
		Support for both Classic Bluetooth v4.2 and BLE specifications.
		34 Programmable GPIOs.
		Up to 18 channels of 12-bit SAR ADC and 2 channels of 8-bit DAC
		Serial Connectivity include 4 x SPI, 2 x I2C, 2 x I2S, 3 x UART.
		Ethernet MAC for physical LAN Communication (requires external PHY).
		1 Host controller for SD/SDIO/MMC and 1 Slave controller for SDIO/SPI.
		Motor PWM and up to 16-channels of LED PWM.
		Secure Boot and Flash Encryption.
		Cryptographic Hardware Acceleration for AES, Hash (SHA-2), RSA, ECC and RNG.
*/

//Porta UDP locale.
#define UDP_LOCAL_PORT 5000

//Chip select ADC.
#define ADC_CS 5

/*
	Il valore massimo di pacchetti UDP al secondo inviabile da un ESP32 è di circa 50.
	Questo dato è stato ricavato sperimentalmente utilizzando un payload di	poco più
	di 1000 byte.

	Il valore (in campioni al sec.) di UDP_PAYLOAD_SIZE si calcola quindi in questo modo:
			sample rate (campioni al sec.)					16.000Campioni
		----------------------------------	=>	------------------	=>	320 campioni per pacchetto.
					pacchetti al secondo								50 pacchetti
	
	Siccome un campione pesa 2 byte, allora verranno effettivamente inviati 640 byte.
	Essendo 640 byte < 1000 byte, l'ESP32 riuscirà a sostenere l'invio dei dati
	senza perdita di pacchetti dovuti al chip fisico del WiFi.
*/
#define UDP_PAYLOAD_SIZE	320

//4 volte UDP_PAYLOAD_SIZE per essere sicuri di non avere dati sovrapposti; sono 2560 byte di RAM.
#define CIRC_BUFFER_SIZE	1280

#define PRESCALER		5000	//80.000.000Hz / 5.000 = 16.000Hz
#define AUTORELOAD	1
