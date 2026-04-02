/*
Canal
Espera a que este el emisor disponible
Escribe
Manda senal que esta habilitado la lectura
Espera la confirmacion de lectura
una vez recibida suelta el emisor

EL lector
Espera la senal de que esta el msg disponible
Lee
Envia la confirmacion de lectura
*/

#include "channels.hh"


Channel::Channel(const char *debugName) {
	name = debugName;
	writer = new Semaphore("writer", 1); // Emisor
	reader  = new Semaphore("reader", 0);	// Receptor
	consumed = new Semaphore("consumed",0);
}

Channel::~Channel() {
	delete writer;
	delete reader;
	delete consumed;
}

void
Channel::Write(int value) {
	writer->P(); // Espera a que se pueda escribir
	msg = value;
	reader->V(); // Avisa que escribio
	consumed->P(); // Espera a que le avisen que se consumio el msj
	writer->V(); // Libera el escritor
}

int
Channel::Read() {
	int readMessage;
	reader->P(); // Espera a que se pueda recibir un mensaje
	readMessage = msg;
	consumed->V(); // Avisa que se consumio el mensaje

	return readMessage;
}