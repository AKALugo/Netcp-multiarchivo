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

#ifndef GENERALMETHODS_H
#define GENERALMETHODS_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

sockaddr_in 
make_ip_address(int port, const std::string& ip_address = 
                            std::string()) {

  sockaddr_in address{};
  // Nuestros socket son de la familia AF_INET ya que usamos TCP/IP.
  address.sin_family = AF_INET ;

  // Miramos que el valor del puerto sea correcto.
  if (port < 0 || port > 65525)
    throw std::invalid_argument("El valor del puerto es ERRONEO, pruebe"
          " con un puerto entre 1 y 65525, ó 0 si quieres que el sistema"
          " operativo asigne uno cualquiera que esté disponible");
  else
    address.sin_port = htons(port);

  // Miramos si "ip_address" está vacío.
  if (ip_address.size() == 0)
    address.sin_addr.s_addr = htonl(INADDR_ANY);
  // Si no está vacío convertimos el string a char* y utilizamos inet_aton.
  else {
    char ip[ip_address.size()];
    strcpy(ip, ip_address.c_str());

    if (inet_aton(ip, &address.sin_addr) == 0)
      throw std::invalid_argument("La dirrección ip introducida es "
      "ERRONEA o tiene un formato erroneo, un ejemplo de una ip válida "
      "sería: 192.168.1.68");
  }

  return address;
}


// Método que usa el hilo que espera hasta recibir alguna señal delas bloqueadas
// previamente.
void
Signal (sigset_t& set, std::atomic_bool& close, std::thread& process1) {

  int signum;
  sigwait(&set, &signum);
  // Cuando recibamos esas señales indicamos al hilo que lee de teclado que
  // tiene que acabar.
  close = true;
  // Usamos esta señal para detener la espera de "std::cin" y que el hilo pueda
  // indicar al resto que acaben.
  pthread_kill(process1.native_handle(), SIGUSR1);
}



void
HowUse () {

  std::cout << "//////////////////////////////////////////////////////////////"
            << "////////////////////" << std::endl 
            << "LOS COMANDOS DISPONIBLES SON:\n"
            << "Receive + 'NOMBRE_DIR': PARA RECIBIR DATOS DESDE OTRO SOCKET.\n"
            << "Abort Receive: PARA ABORTAR Receive.\n"
            << "Send + 'NOMBRE_ARCHIVO': PARA ENVIAR DATOS A OTRO SOCKET.\n"
            << "Abort + 'IDENTIFICADOR': PARA ABORTAR EL Send CORRESPONDIENTE A"
            << "L IDENTIFICADOR.\n"
            << "Pause 'IDENTIFICADOR': PARA PAUSAR EL Send CORRESPONDIENTE AL I"
            << "DENTIFICADOR.\n"
            << "Resume + 'IDENTIFICADOR': PARA REANUDAR EL Send CORRESPONDIENTE"
            << " AL IDENTIFICADOR.\n"
            << "Quit: PARA SALIR DEL PROGRAMA.\n"
            << "//////////////////////////////////////////////////////////////"
            << "////////////////////" << std::endl << std::endl;
}



void 
ActSignal (int signum) {

  // Método que sirve comprobar que la señal SIGUSR1 funciona correctamente.
  const char* text;
  text = "---INTERRUMPIENDO---\n";
  write(STDOUT_FILENO, text, strlen(text));
}



// Método que separa lo que escribamos en teclado en dos std::string para su
// posterior uso.
void
SeparateString (std::string& read_keyboard, std::string& data) {

  int part = 0;
  std::string aux, aux1;
  for (unsigned travel = 0; travel < read_keyboard.size(); travel++) {
    if (read_keyboard[travel] != ' ')
      aux.push_back(read_keyboard[travel]);
    if (read_keyboard[travel] == ' ')
      part ++;
    if (part == 1) {
      aux1 = aux;
      aux.clear();
      part ++;
    }
  }
  if (aux1.size() != 0 && aux.size() != 0) {
    data = aux;
    read_keyboard = aux1;
  }
  aux.clear();
  aux1.clear();
}



// Método que "administra" lo que escribamos por teclado.
void
MessageRead (std::string& read_keyboard, std::string& data, std::atomic_bool& is_a_number, std::atomic_int&
             quit_app_receive) {

  if (read_keyboard == "Receive" && data.size() != 0 && quit_app_receive > 0)
    std::cout << "---YA HAY UN Receive EN EJECUCIÓN---" << std::endl;

  if (read_keyboard == "Send" && data.size() == 0)
    std::cout << "---Send NECESITA EL NOMBRE DEL ARCHIVO---\nPOR EJEMPLO:"
              << " 'Send archivo1'\n" ;

  if (read_keyboard == "Receive" && data.size() == 0)
    std::cout << "---Receive NECESITA EL NOMBRE DEL DIRECTORIO---\nPOR EJEMPLO:"
              << " 'Receive directorio1'\n" ;

  if (read_keyboard == "Abort" && data == "Receive" && quit_app_receive < 1)
    std::cout << "---PRIMERO TIENE QUE EJECUTAR UN 'Receive' PARA PODER ABORTAR"
              << "---" << std::endl;

  if (read_keyboard != "Send" && read_keyboard != "Receive" && read_keyboard !=
      "Quit" && read_keyboard != "Abort" && read_keyboard != "Pause" &&
      read_keyboard != "Resume" && read_keyboard != "")
    std::cout << "---COMANDO ERRONEO---" << std::endl;
      
  if (read_keyboard == "Abort" && !is_a_number && data != "Receive")
    std::cout << "---COMANDO ERRONEO---" << std::endl;

  if ((read_keyboard == "Quit") && data.size() != 0)
    std::cout << "---COMBINACIÓN DE COMANDOS ERRÓNEA---" << std::endl;
}



// Metodo que devuelve true si el segundo argumento introducido por teclado es
// un número.
bool 
IsANumber (std::string& data) {

  if (data.empty())
    return false;

  for (unsigned travel = 0; travel < data.size(); travel++)
    if (!std::isdigit(data[travel]))
      return false;

  return true;
}



// Método que mira si algún hilo de nuestra lista ha terminado y lo quita.
void
JoinSendThread (std::unordered_map<int, std::pair<std::thread, AtomicTask>>& sending_tasks) {

  for (auto it = sending_tasks.begin(); it != sending_tasks.end(); ++it)
    if (it->second.second.getQuitAppSend() == -1) {
      it->second.first.join();
      sending_tasks.erase(it->first);
    }
}



// Método que cierra todos los hilos de nuestra lista
void
QuitSendThread (std::unordered_map<int, std::pair<std::thread, AtomicTask>>& sending_tasks) {

  for (auto it = sending_tasks.begin(); it != sending_tasks.end(); ++it) {
    it->second.second.setPause(false);
    it->second.second.setEndSend(true);
    if (it->second.second.getQuitAppSend() == 2)
      if (pthread_kill(it->second.first.native_handle(), SIGUSR1) != 0)
          throw std::invalid_argument("Los argumentos introducidos en pthread_"
          "kill son erróneos");
    it->second.first.join();
  }
}



// Estructura necesaria para que nuestro contenedor pueda usar pares como keys.
struct pair_hash {

  template <class T1, class T2>
  std::size_t operator () (const std::pair<T1,T2> &p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);

    return h1 ^ h2;  
  }
};



// Método que elimina de la lista de los archivos recibidos a aquellos archivos
// que ya se hayan terminado de enviar.
void
CleanReceiveMap (std::unordered_map<std::pair<uint32_t, in_port_t>, File, pair_hash>& reception_tasks) {

  for (auto it = reception_tasks.begin(); it != reception_tasks.end(); ++it)
    if (it->second.getEnd())
      reception_tasks.erase(it->first);
}
#endif