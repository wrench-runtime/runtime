// Adapted from: https://www.codeproject.com/Articles/25569/Cross-Platform-Mutex
#include "mutex.h"

int MUTEX_INIT(MUTEX *mutex)
{
    #if defined(__unix__)
        return pthread_mutex_init (mutex, NULL);;
    #elif defined(_WIN32)
        *mutex = CreateMutex(0, FALSE, 0);;
        return (*mutex==0);
    #endif
    return -1;
}

int MUTEX_LOCK(MUTEX *mutex)
{
    #if defined(__unix__)
        return pthread_mutex_lock( mutex );
    #elif defined(_WIN32)
        return (WaitForSingleObject(*mutex, INFINITE)==WAIT_FAILED?1:0);
    #endif
    return -1;
}

int MUTEX_UNLOCK(MUTEX *mutex)
{
    #if defined(__unix__)
        return pthread_mutex_unlock( mutex );
    #elif defined(_WIN32)
        return (ReleaseMutex(*mutex)==0);
    #endif
    return -1;
}