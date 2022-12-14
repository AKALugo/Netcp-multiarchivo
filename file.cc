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

#include "file.h"

File::File () {

  file_ = -1;
  actual_position_ = -1;
  size_ = -1;
  end_ = false;
  read_ = false;
}



File::File (const File& A) {

  file_ = A.getFile();
  actual_position_ = A.getActualPosition();
  size_ = A.getSize();
  memory_region_ = A.getMemoryRegion();
  end_ = A.getEnd();
  read_ = A.getRead();
  md5_archive_ = A.getMd5();
  archive_ = A.getArchive();
}



File::File (const std::string& archive, int flags, int size) {

  Make (archive, flags, size);
}



void
File::Make (const std::string& archive, int flags, int size) {

  // Convertimos el string a char*.
  char pathname[archive.size()] ;
  strcpy(pathname, archive.c_str());

  // Abrimos el fichero.
  file_ = open (pathname, flags);
    
  if (file_ < 0)
    throw std::system_error(errno, std::system_category(), "No se pudo abrir "
                        "el archivo, compruebe el nombre y vuelva a intentar");
  
  // Si size==0 significa que el archivo se va a mapear para lectura y ya tiene.
  // un tamaño de base.
  if (size == 0) {
    size_ = lseek (file_, 0, SEEK_END);
    memory_region_ = (char*)mmap(NULL, size_, PROT_READ, MAP_SHARED, file_,0);
    
    if (memory_region_ == MAP_FAILED)
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " mapear el archivo en memoria");
  }

  // En el caso contrario tendremos que cambiar el tamaño del archivo que hemos
  // creado (ya que de base es 0 el tamaño) y lo mapeamos para escritura.
  else {
    size_ = size;
    if (ftruncate (file_, size_) == -1) 
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " cambiar el tamaño del archivo antes de mapearlo en memoria.");
    
    memory_region_ = (char*)mmap(NULL, size_, PROT_WRITE, MAP_SHARED, file_,0);
    if (memory_region_ == MAP_FAILED)
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " mapear el archivo en memoria");

    // Bloqueamos el archivo.
    if (lockf(file_, F_LOCK, 0) == -1)
    throw std::system_error(errno, std::system_category(), "No se ha podido"
    " bloquear el acceso al archivo");
    read_ = true;
  }

  actual_position_ = 0;
  end_ = false;
}



File::File (const std::string& archive, int flags, mode_t mode, int size) {

  MakeMode (archive, flags, mode, size);
}



void
File::MakeMode (const std::string& archive, int flags, mode_t mode, int size) {

  // Convertimos el string a char*.
  char pathname[archive.size()] ;
  strcpy(pathname, archive.c_str());

  // Abrimos el fichero.
  file_ = open (pathname, flags, mode);

  if (file_ < 0)
    throw std::system_error(errno, std::system_category(), "No se pudo abrir "
                            "el archivo");

  // Si size==0 significa que el archivo se va a mapear para lectura y ya tiene.
  // un tamaño de base.
  if (size == 0) {
    size_ = lseek (file_, 0, SEEK_END);
    memory_region_ = (char*)mmap(NULL, size_, PROT_READ, MAP_SHARED, file_,0);
    
    if (memory_region_ == MAP_FAILED)
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " mapear el archivo en memoria");
  }

  // En el caso contrario tendremos que cambiar el tamaño del archivo que hemos
  // creado (ya que de base es 0 el tamaño) y lo mapeamos para escritura.
  else {
    size_ = size;
    if (ftruncate (file_, size_) == -1) 
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " cambiar el tamaño del archivo antes de mapearlo en memoria.");
    
    memory_region_ = (char*)mmap(NULL, size_, PROT_WRITE, MAP_SHARED, file_,0);
    if (memory_region_ == MAP_FAILED)
      throw std::system_error(errno, std::system_category(), "No se ha podido"
      " mapear el archivo en memoria");

    // Bloqueamos el archivo.
    if (lockf(file_, F_LOCK, 0) == -1)
    throw std::system_error(errno, std::system_category(), "No se ha podido"
    " bloquear el acceso al archivo");

    read_ = true;
  }

  actual_position_ = 0;
  end_ = false;
}



File::~File(){
    
  if (read_)
    lockf(file_, F_ULOCK, 0);
  
  munmap(memory_region_,size_);
  close(file_);
}



std::string
File::getSize() {

  std::string value = std::to_string(size_);

  return value;
}



std::string 
File::getMd5() {

  return md5_archive_;
}



std::string 
File::getNumberOfSend() {

  return number_of_send_;
}



std::string 
File::getArchive() {

  return archive_;
}



void 
File::setMd5 (std::string& md5_archive) {

  md5_archive_ = md5_archive;
}



void
File::setNumberOfSend (std::string& number_of_send) {

  number_of_send_ = number_of_send;
}



void
File::setArchive (std::string& archive) {

  archive_ = archive;
}



// Pasamos los datos del archivo mapeado en memoria a nuestro array
void 
File::WriteArray (std::array<char, 1024>& text) {

  for (int travel = 0; travel < 1023; travel++) {
    if (actual_position_ < size_) {
      text[travel] = memory_region_[actual_position_];
      actual_position_++;
    }
  }
  if (actual_position_ == size_)
    end_ = true;
}



// Pasamos los datos del array a nuestro archivo mapeado en memoria.
void 
File::PrintArray (std::array<char, 1024>& text) {

  for (int travel = 0; travel < 1024; travel++) {
    if (text[travel] != '\0') {
      memory_region_[actual_position_] = text[travel];
      actual_position_++;
    }
  }
  if (actual_position_ == size_)
    end_ = true;
}



// Bool que devuelve true si hemos llegado al final del archivo.
bool
File::getEnd () {

  return end_;
}



const int& 
File::getFile () const {

  return file_;
}



const int&
File::getActualPosition () const {

  return actual_position_;
}



const int&
File::getSize () const {

  return size_;
}



char*
File::getMemoryRegion () const {

  return memory_region_;
}



const bool&
File::getEnd () const {

  return end_;
}



const bool& 
File::getRead () const {

  return read_;
}



const std::string 
File::getMd5 () const {

  return md5_archive_;
}



const std::string 
File::getArchive() const {

  return archive_;
}



File 
File::operator= (File& A) {

  File copy(A);
  return A;
}