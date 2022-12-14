
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class AtomicTask {

  public:
    AtomicTask ();
    AtomicTask (const AtomicTask& A);
    ~AtomicTask ();

    void setQuitAppSend (int value);
    const std::atomic_int& getQuitAppSend () const;
    std::atomic_int& getQuitAppSend ();

    void setEndSend (bool value);
    const std::atomic_bool& getEndSend () const;
    std::atomic_bool& getEndSend ();

    void setPause (bool value);
    const std::atomic_bool& getPause () const;
    std::atomic_bool& getPause ();

    AtomicTask operator= (AtomicTask& A);
    bool operator== (AtomicTask & A);

  private:
    std::mutex mutex;
    std::atomic_bool end_send_;
    std::atomic_bool pause_;
    std::atomic_int quit_app_send_;
};