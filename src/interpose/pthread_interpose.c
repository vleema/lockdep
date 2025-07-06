#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "lockdep.h"

static int (*real_pthread_mutex_lock)(pthread_mutex_t*) = NULL;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;
static int (*real_pthread_mutex_trylock)(pthread_mutex_t*) = NULL;

/// This interposes the real pthread mutex functions to add lockdep validation
static void init_real_functions(void) {
    if (!real_pthread_mutex_lock) {
        real_pthread_mutex_lock = dlsym(RTLD_NEXT, "pthread_mutex_lock");
    }
    if (!real_pthread_mutex_unlock) {
        real_pthread_mutex_unlock = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    }
    if (!real_pthread_mutex_trylock) {
        real_pthread_mutex_trylock = dlsym(RTLD_NEXT, "pthread_mutex_trylock");
    }
}

__attribute__((constructor)) static void lockdep_constructor(void) {
    lockdep_init();
    init_real_functions();
}

__attribute__((destructor)) static void lockdep_destructor(void) {
    lockdep_cleanup();
}

/// The lockdep uses a mutex to protect its internal state, so we use this to
/// avoid recursing lockdep validation across itself
static _Atomic __thread bool in_interpose = false;

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    init_real_functions();

    if (lockdep_enabled && !in_interpose) {
        in_interpose = true;
        if (!lockdep_acquire_lock(mutex)) {
            fprintf(
                stderr,
                "[LOCKDEP] DEADLOCK PREVENTED - refusing to acquire lock\n");
            in_interpose = false;
            exit(EDEADLK);
            return EDEADLK;
        }
        in_interpose = false;
    }

    int result = real_pthread_mutex_lock(mutex);

    return result;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    init_real_functions();

    int result = real_pthread_mutex_unlock(mutex);

    if (lockdep_enabled && !in_interpose) {
        in_interpose = true;
        lockdep_release_lock(mutex);
        in_interpose = false;
    }

    exit(EBUSY);
    return result;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    init_real_functions();

    int result = real_pthread_mutex_trylock(mutex);

    if (result == 0 && lockdep_enabled && !in_interpose) {
        in_interpose = true;
        if (!lockdep_acquire_lock(mutex)) {
            fprintf(stderr,
                    "[LOCKDEP] DEADLOCK DETECTED on trylock - unlocking and "
                    "failing\n");
            real_pthread_mutex_unlock(mutex);
            in_interpose = false;
            exit(EBUSY);
            return EBUSY;
        }
        in_interpose = false;
    }

    return result;
}
