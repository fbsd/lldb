//===-- ReadWriteLock.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ReadWriteLock_h_
#define liblldb_ReadWriteLock_h_
#if defined(__cplusplus)

#include "lldb/Host/Mutex.h"
#include "lldb/Host/Condition.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

//----------------------------------------------------------------------
/// Enumerations for broadcasting.
//----------------------------------------------------------------------
namespace lldb_private {

//----------------------------------------------------------------------
/// @class ReadWriteLock ReadWriteLock.h "lldb/Host/ReadWriteLock.h"
/// @brief A C++ wrapper class for providing threaded access to a value
/// of type T.
///
/// A templatized class that provides multi-threaded access to a value
/// of type T. Threads can efficiently wait for bits within T to be set
/// or reset, or wait for T to be set to be equal/not equal to a
/// specified values.
//----------------------------------------------------------------------
    
class ReadWriteLock
{
public:
    ReadWriteLock () :
        m_rwlock(),
        m_running(false)
    {
        int err = ::pthread_rwlock_init(&m_rwlock, NULL); (void)err;
//#if LLDB_CONFIGURATION_DEBUG
//        assert(err == 0);
//#endif
    }

    ~ReadWriteLock ()
    {
        int err = ::pthread_rwlock_destroy (&m_rwlock); (void)err;
//#if LLDB_CONFIGURATION_DEBUG
//        assert(err == 0);
//#endif
    }

    bool
    ReadTryLock ()
    {
        ::pthread_rwlock_rdlock (&m_rwlock);
        if (m_running == false)
        {
            return true;
        }
        ::pthread_rwlock_unlock (&m_rwlock);
        return false;
    }

    bool
    ReadUnlock ()
    {
        return ::pthread_rwlock_unlock (&m_rwlock) == 0;
    }
    
    bool
    WriteLock()
    {
        ::pthread_rwlock_wrlock (&m_rwlock);
        m_running = true;
        ::pthread_rwlock_unlock (&m_rwlock);
        return true;
    }
    
    bool
    WriteTryLock()
    {
        if (::pthread_rwlock_trywrlock (&m_rwlock) == 0)
        {
            m_running = true;
            ::pthread_rwlock_unlock (&m_rwlock);
            return true;
        }
        return false;
    }
    
    bool
    WriteUnlock ()
    {
        ::pthread_rwlock_wrlock (&m_rwlock);
        m_running = false;
        ::pthread_rwlock_unlock (&m_rwlock);
        return true;
    }

    class ReadLocker
    {
    public:
        ReadLocker () :
            m_lock (NULL)
        {
        }

        ~ReadLocker()
        {
            Unlock();
        }

        // Try to lock the read lock, but only do so if there are no writers.
        bool
        TryLock (ReadWriteLock *lock)
        {
            if (m_lock)
            {
                if (m_lock == lock)
                    return true; // We already have this lock locked
                else
                    Unlock();
            }
            if (lock)
            {
                if (lock->ReadTryLock())
                {
                    m_lock = lock;
                    return true;
                }
            }
            return false;
        }

        void
        Unlock ()
        {
            if (m_lock)
            {
                m_lock->ReadUnlock();
                m_lock = NULL;
            }
        }
        
    protected:
        ReadWriteLock *m_lock;
    private:
        DISALLOW_COPY_AND_ASSIGN(ReadLocker);
    };

protected:
    pthread_rwlock_t m_rwlock;
    bool m_running;
private:
    DISALLOW_COPY_AND_ASSIGN(ReadWriteLock);
};

} // namespace lldb_private

#endif  // #if defined(__cplusplus)
#endif // #ifndef liblldb_ReadWriteLock_h_
