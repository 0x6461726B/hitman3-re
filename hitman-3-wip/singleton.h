#pragma once
template <typename T> class Singleton {
  protected:
    Singleton() = default; 
    ~Singleton() = default;

  public:
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
    Singleton(Singleton &&) = delete;
    Singleton &operator=(Singleton &&) = delete;

    static T &getInstance() {
        static T instance; 
        return instance;
    }
};