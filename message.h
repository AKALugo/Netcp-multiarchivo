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

#ifndef MESSAGE_H
#define MESSAGE_H

#include <netinet/in.h>
#include <sys/socket.h>

#include <array>

struct Message {

  // Nuestra estructura va a poder almacenar 1024 caracteres.
  std::array<char, 1024> text;

  // Metodo que limpia nuestra estructura.
  void clear () {

    for (int travel = 0; travel < 1024; travel ++) 
      text[travel] = '\0';
  }

  uint32_t remote_ip;
  in_port_t remote_port;
};
#endif