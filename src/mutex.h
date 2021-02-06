//Adapted from: https://www.codeproject.com/Articles/25569/Cross-Platform-Mutex
#ifndef mutex_h
#define mutex_h

//Headers
#if defined(__unix__)
    #include <pthread.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <process.h>
#endif

//Data types
#if defined(__unix__)
    #define MUTEX pthread_mutex_t
#elif defined(_WIN32)
    #define MUTEX HANDLE
#endif

//Functions
int MUTEX_INIT(MUTEX *mutex);
int MUTEX_LOCK(MUTEX *mutex);
int MUTEX_UNLOCK(MUTEX *mutex);

#endif