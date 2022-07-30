/* Univerdidad:Universidad de La Laguna.
   Grado:       Grado de ingeniería informática.
   Asignatura:  Asignatura Sistemas Operativos (SSOO).
   Autor:       Alejandro Lugo Fumero.
   Correo:      alu0101329185@ull.edu.es
   Práctica nº: 2
   Comentario:  Este programa mediante un socket envía la información de un .txt
                a otro socket guardando el mensaje en otro .txt.
   Compilar:    g++ -g -pthread -o Netcp file.cc netcp.cc socket.cc atomic_task.cc
   Ejecutar:    ./Netcp 'port' 'ip_address'
*/

#include "atomic_task.h"
#include "file.h"
#include "general_methods.h"
#include "socket.h"

void 
ReceiveMessage (int good_port, int dest_good_port, std::string& ip_address,
                std::atomic_bool& end, std::atomic_int& quit_app_receive,
                std::string& dir) {

  try {

    // quit_app_receive = 0 como valor base que indica que el hilo no se ha 
    // ejecutado nunca, entonces no tenemos que esperar a que acabe.
    // quit_app_receive = 1 para indicar que el hilo se esta ejecutando.
    // quit_app_receive = 2 para indicar que el hilo esta bloqueado y esperando.
    // quit_app_receive = -1 significa que el hilo ha acabado, puede ser por un
    // throw o porque ha acabado de forma correcta.
    quit_app_receive = 1;

    // Creamos nuestra estructura donde vamos a almacenar la informacion para
    // saber cuantos archivos estamos recibiendo.
    std::unordered_map<std::pair<uint32_t, in_port_t>, File, pair_hash> 
                      reception_tasks;
    std::pair<uint32_t, in_port_t> key;

    std::cout << "---RECIBIENDO MENSAJES---" << std::endl << std::endl;

    // Pasamos de std::string a char* el directorio donde vamos a almacenar
    // nuestros archivos.
    char good_dir[dir.size()];
    strcpy(good_dir, dir.c_str());

    // Creamos el directorio.
    if (mkdir(good_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
      throw std::system_error(errno, std::system_category(), "No se ha podido"
                            " crear el directorio");

    // Creamos las estructuras.
    sockaddr_in socket_local_address {};
    sockaddr_in socket_remote_address {};

    // Rellenamos las estructuras.
    socket_local_address = make_ip_address (good_port, "127.0.0.1");
    socket_remote_address = make_ip_address (0, ip_address);

    // Creamos el socket.
    Socket socket_local (socket_local_address);

    // Creamos la estructura que va a almacenar el mensaje.
    Message message {};

    if (end)
      throw true;

    do {
      // Recibimos los datos que vamos a necesitar para crear el fichero.
      quit_app_receive = 2;
      socket_local.ReceiveFrom(message, socket_remote_address);
      quit_app_receive = 1;

      if (end)
        throw true;
    
      // Pasamos la ip y el puerto del mensaje que nos acaba de llegar una
      // estructura que vamos a usar.
      key.first = message.remote_ip;
      key.second = message.remote_port;

      // Miramos si anteriormente hemos recibido algún mensaje desde ese puerto
      // y esa IP.
      if (reception_tasks.count(std::pair<uint32_t, in_port_t>(
          message.remote_ip, message.remote_port)) == 0) {

        // Si nunca hemos recibido un mensaje.
        int counter = 0;
        int parts = 0;

        // Pasamos la información recibida a varios strings, para luego
        // administrarla.
        std::string number, archive, md5_archive, number_of_send;
        while (message.text[counter] != '\0') {
          if (message.text[counter] == ' ')
            parts ++;
          if (parts == 0) 
            number.push_back(message.text[counter]);
          if (parts == 2)
            archive.push_back(message.text[counter]);
          if (parts == 4)
            md5_archive.push_back(message.text[counter]);
          if (parts == 6)
            number_of_send.push_back(message.text[counter]);
          if (parts == 1 || parts == 3 || parts == 5)
            parts ++;
          counter ++;
        }
        int size = stoi (number);

        // Escribimos el nuevo nombre del archivo = 
        // direccion + / + nombre_archivo.
        std::string original_file = archive;
        archive = dir + '/' + archive;

        File file;

        reception_tasks.insert(std::make_pair(key, file));

        // Creamos el fichero.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        reception_tasks.find(key)->second.MakeMode(archive, O_RDWR | O_CREAT | 
                                                   O_TRUNC, mode, size);

        // Guardamos la información recivida en nuestra estructura.
        reception_tasks.find(key)->second.setMd5(md5_archive);
        reception_tasks.find(key)->second.setNumberOfSend(number_of_send);
        reception_tasks.find(key)->second.setArchive(original_file);

        // Limpiamos la estructura donde se almacena el mensaje.
        message.clear();
      
        if (end)
          throw true;
      }

      // En el caso de que no sea la primera vez que recibimos un mensaje desde
      // ese puerte e IP.
      else {

        // Escribimos el mensaje en nuestro archivo mapeado en memoria.
        reception_tasks.find(key)->second.PrintArray(message.text);
        message.clear();
        
        // Si hemos llegado ya al final del archivo y hemos recibido todos los
        // mensajes.
        if (reception_tasks.find(key)->second.getEnd()) {
              
          // Creamos la tubería.
          std::array<int,2> fds;
          int return_code = pipe (fds.data());

          // Si la tubería falla.
          if (return_code < 0)
            std::cout << "---Receive NO PUDO CREAR LA TUBERÍA PARA " << 
                      reception_tasks.find(key)->second.getArchive() << 
                      " CON IDENTIFICADOR [" << 
                      reception_tasks.find(key)->second.getNumberOfSend() << 
                      "]---" << std::endl;

          else {
            // Creamos el proceso hijo.
            pid_t child = fork();

            if (child == 0) {

              // Aquí solo entra el proceso hijo.
              close (fds[0]);

              // Pasamos el std::string a Char*.
              char md5_name [reception_tasks.find(key)->
                             second.getArchive().size()];
              strcpy(md5_name, reception_tasks.find(key)->
                               second.getArchive().c_str());

              // Si no se puedo redireccionar la tubería
              if (dup2 (fds[1], STDOUT_FILENO) == -1) {
                // Le decimos al proceso padre que hubo un error.
                write (fds[1], "error",5);
                close (fds[1]);
                _exit(1);
              }
              else {
                // Como la salida del proceso hijo está redireccionada al
                // proceso padre, cuando este termine el padre recibirá lo
                // que devuelva execl().
                execl ("/bin/md5sum", "md5sum", md5_name, (char *)0);
                close (fds[1]);
               _exit(1);
              }
            }

            else if (child > 0) {
    
              // Aquí solo entra el proceso padre.
              close (fds[1]);
              char buffer [200];
              // Esperamos al mensaje del proceso hijo.
              read(fds[0], buffer, sizeof(buffer));
              close (fds[0]);

              std::string md5_archive;
              std::string aux = buffer;

              // Si el mensaje recibido está vacío es que md5sum falló.
              if (aux.empty())
                std::cout << "---Receive NO PUDO CALCULAR EL md5sum DE " <<
                          reception_tasks.find(key)->second.getArchive() << 
                          " CON IDENTIFICADOR [" << 
                          reception_tasks.find(key)->second.getNumberOfSend() 
                          << "]---" << std::endl;

              // Si el mensaje recibido es "error" significa que no se pudo
              // redireccionar la tubería.
              if (aux == "error")
                std::cout << "---Receive NO PUDO REDIRECCIONAR LA TUBERÍA PARA "
                          << reception_tasks.find(key)->second.getArchive() << 
                          " CON IDENTIFICADOR [" << reception_tasks.find(key)->
                          second.getNumberOfSend() << "]---" << std::endl;
              
              // Si el mensaje recibido es el md5sum + nombre del archivo.
              if (aux != "error" && !aux.empty()) {
                // Sacamos el md5sum que es lo que nos interesa.
                for (unsigned travel = 0; buffer[travel] != ' '; travel ++)
                  md5_archive.push_back(buffer[travel]);

                // Mostramos los nombres de los archivos, al identificador que
                // pertenecen y sus md5sum.
                std::cout << "---EL INDENTIFICADOR [" << reception_tasks.find(key)
                        ->second.getNumberOfSend() << "] ENVIÓ EL ARCHIVO " << 
                        reception_tasks.find(key)->second.getArchive() << 
                        " CON md5sum = " << reception_tasks.find(key)->
                        second.getMd5() << "---" << std::endl << 
                        "---EL ARCHIVO RECIBIDO DE IGUAL NOMBRE TIENE UN md5sum = "
                        << md5_archive << "---" << std::endl;

                // Si los md5sum coinciden.
                if (md5_archive == reception_tasks.find(key)->second.getMd5())
                  std::cout << "---EL VALOR md5sum DE LOS DOS ARCHIVOS COINCIDE "
                            "POR LO TANTO NO SE HA PERDIDO NINGUN DATO EN EL ENV"
                            "ÍO---" << std::endl << std::endl;
                else 
                  std::cout <<"---EL VALOR md5sum DE LOS DOS ARCHIVOS NO COINCI"
                            "DEN POR LO TANTO SE HA PERDIDO ALGÚN DATO EN EL ENV"
                            "ÍO, SE ACONSEJA REALIZAR EL ENVÍO DE NUEVO---" << 
                            std::endl << std::endl;
              }
            }

            else {
              // Aquí solo entra si el padre no pudo crear el proceso hijo.
              throw std::system_error(errno, std::system_category(), "No se ha" 
              " podido crear el proceso hijo");
            }
          }
        }

        if (end)
          throw true;
      }
      CleanReceiveMap(reception_tasks);
    } while (true);

    quit_app_receive = -1;
  }

  catch(const bool& error) {
    quit_app_receive = -1;
    end = false;
    std::cerr << "---PROCESO ABORTADO----" << std ::endl;
  }
  catch (std::invalid_argument& error) {
    quit_app_receive = -1;
    end = false;
    std::cerr << error.what() << '\n';
  }
  catch (std::system_error& error) {
    quit_app_receive = -1;
    end = false;
    if (error.code().value() != EINTR)
      std::cerr << error.what() << '\n';
    else 
      std::cerr << "---Receive INTERRUMPIDO CON ÉXITO---" << '\n';
  }
  catch (...) {
    quit_app_receive = -1;
    end = false;
    std::cerr << "¡¡¡ ERROR DESCONOCIDO !!!" << '\n';
  }
}



void 
SendMessage (AtomicTask& task, std::string& ip_address, std::string& archive_name
             , int good_port, int dest_good_port, int number_of_send) {

  try {

    // getQuitAppSend = 0 como valor base que indica que el hilo no se ha 
    // ejecutado nunca, entonces no tenemos que esperar a que acabe.
    // getQuitAppSend = 1 para indicar que el hilo se esta ejecutando.
    // getQuitAppSend = 2 para indicar que el hilo está esperan en un read().
    // getQuitAppSend = -1 significa que el hilo ha acabado, puede ser por un
    // throw o porque ha acabado de forma correcta.
    task.setQuitAppSend(1);

    std::cout << "---ENVIANDO MENSAJE DEL PROCESO " << "[" << number_of_send << 
              "]---" << std::endl << std::endl;

    // Creamos las estructuras.
    sockaddr_in socket_local_address {};
    sockaddr_in socket_remote_address {};

    // Rellenamos las estructuras.
    socket_local_address = make_ip_address (good_port, "127.0.0.1");
    socket_remote_address = make_ip_address (0, ip_address);

    // Creamos el socket.    
    Socket socket_remote (socket_remote_address);
    
    // Creamos la estructura que va a almacenar el mensaje.
    Message message {};
    // Abrimos el archivo
    File file (archive_name, 0000, 0);

    // Creamos la tubería.
    std::array<int,2> fds;
    int return_code = pipe (fds.data());

    // Si la tubería no se creó correctamente.
    if (return_code < 0)
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " crear la tubería");

    // Creamos el proceso hijo.
    pid_t child = fork();

    if (child == 0) {

      // Aquí solo entra el proceso hijo.
      close (fds[0]);

      char md5_name [archive_name.size()];
      strcpy(md5_name, archive_name.c_str());

      // Si la tubería no se puedo redireccionar.
      if (dup2 (fds[1], STDOUT_FILENO) == -1) {
        std::cerr << "---HA OCURRIDO UN ERROR AL REDIRECCIONAR UNA TUBERÍA---"
                  << std::endl;
        write (fds[1], "error",5);
        _exit(1);
      }
      // Redireccionamos la salida del subproceso al proceso padre, de manera
      // que el proceso padre lea que devuelve execl.
      else {
        execl ("/bin/md5sum", "md5sum", md5_name, (char *)0);
        close (fds[1]);
        _exit(1);
      }
    }

    else if (child > 0) {
    
    // Aquí solo entra el proceso padre.
    close (fds[1]);
    char buffer [200];

    if (task.getEndSend())
      throw true;

    // Esperamos a que nuestro subproceso hijo nos envíe un mensaje.
    task.setQuitAppSend(2);
    read(fds[0], buffer, sizeof(buffer));
    task.setQuitAppSend(1);
    
    close (fds[0]);

    std::string md5_archive;
    std::string aux = buffer;

    // Si no recibimos nada significa que execl falló, abortamos.
    if (aux.empty())
      throw 1;
    // Si la tubería no se pudo redireccionar.
    if (aux == "error")
      throw true;

    // Guardamos el md5sum del archivo.
    for (unsigned travel = 0; buffer[travel] != ' '; travel ++)
      md5_archive.push_back(buffer[travel]);

    std::string number_of_send_string = std::to_string(number_of_send);

    // Guardamos en un string los datos a enviar de primera mano.
    std::string data_file = file.getSize();
    data_file = data_file + " " + archive_name + " " + md5_archive + " " +
                number_of_send_string;

    // Pasamos los datos almacenados en el string a el array para luego enviarlo.
    for (unsigned travel = 0; travel < data_file.size(); travel ++) {
      message.text[travel] = data_file[travel];
    }

    if (task.getEndSend())
      throw true;
    if (task.getPause())
      while (task.getPause()) {}

    // Enviamos el mensaje y limpiamos nuestra estructura.
    socket_remote.SendTo(message, socket_local_address);
    message.clear();

    // Mientras enviemos mensajes nuestra flag = 1.
    while(!file.getEnd()) {

      std::this_thread::sleep_for(std::chrono::seconds(3));
      if (task.getEndSend())
        throw true;
      if (task.getPause())
        while (task.getPause()) {}

      // Escribimos y enviamos el mensaje.
      file.WriteArray(message.text);
      socket_remote.SendTo(message, socket_local_address);
    
      // Limpiamos la estructura donde almacenamos el mensaje.
      message.clear();

      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (task.getEndSend())
        throw true;
      if (task.getPause())
        while (task.getPause()) {}
    }
    }

    else {
      // Aquí solo entra si el padre no pudo crear el proceso hijo.
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " crear el proceso hijo");
    }
  }

  catch(const bool& error) {
    task.setQuitAppSend(-1);
    task.setEndSend(false);
    std::cerr << "---PROCESO ABORTADO " << "[" << number_of_send << "]---"
              << std::endl;
  }
  catch(const int& error) {
    task.setQuitAppSend(-1);
    task.setEndSend(false);
    std::cerr << "---ABORTANDO " << "[" << number_of_send << "] NO SE PUEDO"
              " CALCULAR EL md5sum DE " << archive_name << "---" << std::endl;
  }
  catch (std::invalid_argument& error) {
    task.setQuitAppSend(-1);
    task.setEndSend(false);
    std::cerr << error.what() << '\n';
  }
  catch (std::system_error& error) {
    task.setQuitAppSend(-1);
    task.setEndSend(false);
    if (error.code().value() != EINTR)
      std::cerr << error.what() << '\n';
    else 
      std::cerr << "---Send [" << number_of_send << "] INTERRUMPIDO CON ÉXITO---"
                << '\n';
  }
  catch (...) {
    task.setQuitAppSend(-1);
    task.setEndSend(false);
    std::cerr << "---ERROR DESCONOCIDO "<< "[" << number_of_send << "]---" 
              << std::endl;
  }
}



void 
Read (int good_port, int dest_good_port, std::exception_ptr& eptr, 
      std::string& ip_address, std::atomic_bool& close) {

  try {

    HowUse ();
    // Creamos nuestra estructura que usaremos para almacenar nuestros envíos.
    std::unordered_map<int, std::pair<std::thread, AtomicTask>> sending_tasks; 
    std::atomic_bool is_a_number {false};
    int number_of_sends = 1;

    std::string read_keyboard, archive_name, data, dir;
    std::atomic_int quit_app_receive {0};
    std::atomic_bool end_receive {false};
    std::thread process3;

    while (read_keyboard != "Quit" && !close) {
      
      // Leemos de teclado.
      getline (std::cin, read_keyboard);
      // Separamos los datos introducidos por teclado en dos std::string.
      SeparateString (read_keyboard, data);
      // Miramos si el segundo valor introducido es un número.
      is_a_number = IsANumber(data);

      // Método que mira si algún envío a terminado para eliminarlo de la lista.
      JoinSendThread (sending_tasks);

      if (read_keyboard == "Send" && data.size() != 0) {

        archive_name = data;
        
        // Metemos en nuestra estructura los datos de nuestro envío.
        std::pair<std::thread, AtomicTask> pair_task;
        std::pair<int, std::pair<std::thread, AtomicTask>> send_task 
                             (number_of_sends, std::move(pair_task));
        sending_tasks.insert(std::move(send_task));

        // Si hemos hecho algún envío cambiamos el puerto por defecto. 
        if (number_of_sends != 1)
          dest_good_port = 0;

        std::cout << "---EL IDENTIFICADOR DE ESTE PROCESO ES: " << number_of_sends
                  << "---" << std::endl;
        int number_of_process = number_of_sends;

        // Creamos nuestro hilo.
        std::thread process2 (&SendMessage, std::ref(sending_tasks.find
                    (number_of_sends)->second.second), std::ref(ip_address),
                    std::ref(archive_name), good_port, dest_good_port, 
                    number_of_sends);

        // Movemos nuestro hilo a nuestra estructura.
        sending_tasks.find(number_of_sends)->second.first = std::move(process2);
        number_of_sends ++;
      }

      // Creamos el hilo que recibe el mensaje solo si no hay otro en uso.
      if (read_keyboard == "Receive" && data.size() != 0 && (quit_app_receive 
          == 0 || quit_app_receive == -1)) {
        // Si hemos recibido un mensaje hay que esperar a que acabe el hilo
        // antes de crear otro.
        if (quit_app_receive == -1)
          process3.join();
        dir = data;
        process3 = std::thread (&ReceiveMessage, good_port, dest_good_port, 
                   std::ref(ip_address), std::ref(end_receive), 
                   std::ref(quit_app_receive), std::ref(dir));
      }

      // Para Send salimos de la pausa y le indicamos que tiene que acabar.
      if (read_keyboard == "Abort" && is_a_number) {
        
        int number = std::stoi(data);

        // Miramos si el numero introducido coincide con algún envío.
        if (sending_tasks.count(number) != 0) {
          sending_tasks.find(number)->second.second.setPause(false);
          sending_tasks.find(number)->second.second.setEndSend(true);
          sending_tasks.find(number)->second.first.join();
          sending_tasks.erase(number);
        }
        // si no coincide.
        else
          std::cout << "---HA INTRODUCIDO UN IDENTIFICADOR ERRONEO---" << std::endl;
      }

      if (read_keyboard == "Pause" && is_a_number) {

        int number = std::stoi(data);
        
        // Miramos si el numero introducido coincide con algún envío.
        if (sending_tasks.count(number) != 0) {
          // Si no está pausado ya.
          if (!sending_tasks.find(number)->second.second.getPause()) {
            sending_tasks.find(number)->second.second.setPause(true);
            std::cout << "---PAUSADO CON ÉXITO----" << std::endl;
          }
          // Si está pausado
          else 
            std::cout << "---EL Send YA SE ENCONTRABA PAUSADO---" << std::endl;
        }
        // Si no coincide.
        else
          std::cout << "---HA INTRODUCIDO UN IDENTIFICADOR ERRONEO---" << std::endl;
      }

      if (read_keyboard == "Resume" && is_a_number) {

        int number = std::stoi(data);

        // Miramos si el número introducido coincide con algún envío.
        if (sending_tasks.count(number) != 0) {
          // Si está pausado.
          if (sending_tasks.find(number)->second.second.getPause()) {
            sending_tasks.find(number)->second.second.setPause(false);
            std::cout << "---REANUDADO CON ÉXITO---" << std::endl;
          }
          // Si no está pausado.
          else
            std::cout << "---PRIMERO TIENE QUE EJECUTAR UN 'Pause' PARA PODER REANUDAR"
                      << "---" << std::endl;
        }
        // Si no coincide.
        else
          std::cout << "---HA INTRODUCIDO UN IDENTIFICADOR ERRONEO---" << std::endl;
      }

      // Para Receibe le indicamos que tiene que acabar y si está bloqueado le 
      // enviamos una señal para que salga.
      if (read_keyboard == "Abort" && data == "Receive" && quit_app_receive > 0) {
        end_receive = true;
        if (quit_app_receive == 2)
          if (pthread_kill(process3.native_handle(), SIGUSR1) != 0)
            throw std::invalid_argument("Los argumentos introducidos en pthrea"
            "d_kill son erróneos");
      }

      // Administramos los mensajes introducidos por telcado.
      MessageRead (read_keyboard, data, is_a_number, quit_app_receive);
        is_a_number = false;

      if (read_keyboard != "Quit" || (read_keyboard == "Quit" && data.size() != 0))
        read_keyboard.clear();
      data.clear();
    }

    // A todos los hilos de nuestra lista les decimos que acaben.
    QuitSendThread (sending_tasks);

    // Si el hilo Receive ha sido usado o está en ejecución le decimos que acabe
    // y esperamos
    if (quit_app_receive != 0) {
      end_receive = true;
      if (quit_app_receive == 2)
        if (pthread_kill(process3.native_handle(), SIGUSR1) != 0)
          throw std::invalid_argument("Los argumentos introducidos en pthread_"
          "kill son erróneos");
      process3.join();
    }
  }

  catch (...) {
    eptr = std::current_exception();
  }
}



int 
protected_main (int argc, char* argv[]) {

  std::string help = "--help";

  // Si el usuario escribe --help.
  if (argc == 2 && argv[1] == help) {
 
    throw std::invalid_argument("Este programa mediante un socket envía la "
          "información de un .txt a otro socket que guarda el mensaje en un"
          " .txt");
  }

  std::string port;
  std::string ip_address;
  std::string dport;

  // Si las variables de entorno no tienen ningun valor les damos nosotros uno.
  char * dest_port = getenv ("NETCP_DEST_PORT");
  if (dest_port == NULL)
    dport = "0";
  else 
    dport = dest_port;

  char * dest_ip = getenv ("NETCP_DEST_IP");
  if (dest_ip == NULL)
    ip_address= "127.0.0.1";
  else 
    ip_address = dest_ip;

  char * cport = getenv ("NETCP_DEST_PORT");
  if (cport == NULL)
    port = "8000";
  else 
    port = cport;

  // Comprobamos que se nos haya pasado un puerto "válido".
  for (unsigned travel = 0; travel < port.size(); travel++) {
    if (!std::isdigit(port[travel]))
      throw std::invalid_argument("Introduzca un puerto valido."
      "Por ejemplo: 8000, 9500 ...");
  }

  // Comprobamos que se nos haya pasado un puerto "válido".
  for (unsigned travel = 0; travel < dport.size(); travel++) {
    if (!std::isdigit(dport[travel]))
      throw std::invalid_argument("Introduzca un puerto valido."
      "Por ejemplo: 8000, 9500 ...");
  }

  // Pasamos el puerto del string a un int.
  int good_port = std::stoi(port);
  int dest_good_port = std::stoi(dport);

  // Creamos nuestro manejador de sañales.
  struct sigaction act {0};
  act.sa_handler = &ActSignal;
  sigaction (SIGUSR1, &act, NULL);

  // Creamos nuestro manejador de señales y bloqueamos las señales que no nos
  // interesen.
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGHUP);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  std::exception_ptr eptr {};
  std::atomic_bool close {false};

  // Creamos nuestro hilo que maneja los mensajes introducidos por teclado.
  std::thread process1(&Read, good_port, dest_good_port, std::ref(eptr),
                       std::ref(ip_address), std::ref(close));

  // Creamos el hilo encargado de indicar que el programa debe acabar si recibe
  // una de las señales bloqueadas arriba.
  std::thread close_signal (&Signal, std::ref(set), std::ref(close), 
                            std::ref(process1));
  close_signal.detach();

  process1.join();
  
  if (eptr)
    std::rethrow_exception(eptr);

  return 0;
}



// Tratamiento de errores.
int 
main(int argc, char* argv[]) {

  try {
    return protected_main (argc, argv);
  }

  catch (std::invalid_argument& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  catch (std::system_error& error) {
    if (error.code().value() != EINTR)
      std::cerr << error.what() << '\n';
    return 2;
  }
  catch (...) {
    std::cerr << "¡¡¡ ERROR DESCONOCIDO !!!" << '\n';
  }
  return 0;
}