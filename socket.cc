/* Univerdidad:Universidad de La Laguna.
   Grado:       Grado de ingeniería informática.
   Asignatura:  Asignatura Sistemas Operativos (SSOO).
   Autor:       Alejandro Lugo Fumero.
   Correo:      alu0101329185@ull.edu.es
   Práctica nº: 2
   Comentario:  Este programa mediante un socket envía la información de un .txt
                a otro socket guardando el mensaje en otro .txt.
   Compilar:    g++ -g -pthread -o Netcp file.cc netcp.cc socket.cc
   Ejecutar:    ./Netcp 'port' 'ip_address'
*/

#include "socket.h"

Socket::Socket(const sockaddr_in& address) {

  // Creamos el socket.
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0)
    throw std::system_error(errno, std::system_category(), "No se pudo "
                                "crear el socket");

  // Le asignamos una dirección al socket.
  int result = bind (fd_, reinterpret_cast<const sockaddr*>(&address),
                       sizeof(address));
  if (result < 0) {
    throw std::system_error(errno, std::system_category(), "No se pudo "
                            "asignar una dirección al socket 'error bind()'");
    }
}



Socket::~Socket() {

  close(fd_);
}



void 
Socket::SendTo(const Message& message, const sockaddr_in& address) {

  // Utilizamos sendto para enviar un mensaje al otro socket.
  int result = sendto(fd_, &message, sizeof(message), 0,
	reinterpret_cast<const sockaddr*>(&address), sizeof(address));

  if (result < 0) 
    throw std::system_error(errno, std::system_category(), "Falló sendto");
}



void 
Socket::ReceiveFrom(Message& message, sockaddr_in& address) {

  socklen_t src_len = sizeof(address);
  // Utilizamos recvfrom para recibir un mensaje de otro socket.
  if (recvfrom(fd_, &message, sizeof(message), 0,
	    reinterpret_cast<sockaddr*>(&address), &src_len) < 0)
    throw std::system_error(errno, std::system_category(), "Falló recvfrom");

  message.text[1023] = '\0';

  message.remote_ip = inet_netof(address.sin_addr);
  message.remote_port = ntohs(address.sin_port);
}