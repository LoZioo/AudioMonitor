# Specifiche dei test effettuati

## Testo di esempio utilizzato
Un'insieme di vettori di uno spazio vettoriale è formato da vettori linearmente indipendenti se nessuno di essi può essere espresso come combinazione lineare degli altri vettori dell'insieme.

---

## Descrizione dei test effettuati
-	Il file audio `Final-test.mp3` è stato registrato con:
	-	Frequenza di campionamento di `16kHz`.
	-	Una profondità di `12bit`.

<br>

-	Il file `Final-test.aup3` è un file progetto di Audacity che contiene gli audio test effettuati alla distanza di:
	-	0cm
	-	50cm
	-	150cm
	-	250cm

<br>

-	Ogni test avrà il suo corrispettivo audio a cui sono stati applicati i seguenti enhanchments:
	-	0/50/150/250cm normalized\
		La forma d'onda della registrazione è stata normalizzata tra `[-1db, 1db]`.

	<br>

	-	0/50/150/250cm filtered\
		La registrazione è stata sottoposta ad un filtro notch a `5530Hz` per poter filtrare in maniera mirata il rumore periodico.
