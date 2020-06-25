#ifndef _LOCKER_H_
#define _LOCKER_H_

#include <assert.h>
#include <windows.h>

#ifdef LOCKER_COMPATIBLE_WITH_BOOST
#include <boost/noncopyable.hpp>
#endif


namespace locker_space
{

template<class Mutex> class CScopedLock;
template<class Mutex> class CManualLock;
class CReverseLocker;

class c_aux_locker;
typedef CScopedLock< c_aux_locker > CLocker;
typedef CManualLock< c_aux_locker > CManualLocker;


#ifdef LOCKER_COMPATIBLE_WITH_BOOST
// wrapper class for CriticalSection
class mutex : boost::noncopyable
{
public:
    mutex()                     { ::InitializeCriticalSection(&cs_);                         }

    mutex(unsigned spin_count)  { ::InitializeCriticalSectionAndSpinCount(&cs_, spin_count); }

    ~mutex()                    { ::DeleteCriticalSection(&cs_);                             }


    //void lock() { pthread_mutex_lock(&mtx); }
    void lock()                 { ::EnterCriticalSection (&cs_);                             }

    //void unlock()               {   pthread_mutex_unlock(&mtx);                              }
    void unlock()               { ::LeaveCriticalSection(&cs_);                              }
private:
    CRITICAL_SECTION cs_;
};
#endif


template<class Mutex>
struct no_lock
{
    typedef Mutex mutex_type;
    no_lock(const Mutex & mutex) {}
    ~no_lock() {}
};


struct empty_locker 
{
    empty_locker() {}

    void Lock() {}
    void Unlock() {}
    bool TryLock() {}

private:
    empty_locker(const empty_locker &);
    empty_locker & operator =(const empty_locker &);
};//c_aux_locker


class c_aux_locker{
    friend class CScopedLock<c_aux_locker>;
    friend class CManualLock<c_aux_locker>;
    friend class CReverseLocker;
private:
    CRITICAL_SECTION    m_section;
public:
    c_aux_locker()
    {
        ::InitializeCriticalSection( &m_section );
    }
    c_aux_locker(DWORD dwSpinCount)
    {
        (void)::InitializeCriticalSectionAndSpinCount( &m_section, dwSpinCount );
    }
    ~c_aux_locker()
    {
        ::DeleteCriticalSection( &m_section );
    }
    DWORD SetSpinLock(DWORD dwSpinCount)
    {
        return ::SetCriticalSectionSpinCount(&m_section, dwSpinCount);
    }

private:
    c_aux_locker(const c_aux_locker &);
    c_aux_locker & operator =(const c_aux_locker &);

    void Lock()
    {
        ::EnterCriticalSection( &m_section );
    }
    void Unlock()
    {
        ::LeaveCriticalSection( &m_section );
    }
    bool TryLock()
    {
        return !!::TryEnterCriticalSection( &m_section );
    }
};//c_aux_locker


template<typename Mutex>
class CRegistredLock
{
public:
    typedef Mutex mutex_type;

protected:
    typedef CRegistredLock< Mutex > BaseLock;
    Mutex & m_raux_locker;

protected:
    CRegistredLock(mutex_type & r_auxlocker): m_raux_locker( r_auxlocker ) { }

public:
    mutex_type & mutex() const { return m_raux_locker; }

private:
    CRegistredLock(const CRegistredLock &);
    CRegistredLock & operator =(const CRegistredLock &);
};


template<typename Mutex>
class CScopedLock : public CRegistredLock< Mutex > {
public:
    CScopedLock( Mutex& r_auxlocker ) : BaseLock( r_auxlocker )
    {
        mutex().Lock();
    }
    ~CScopedLock()
    {
        mutex().Unlock();
    }
};//CScopedLock


template<typename Mutex>
class CManualLock : public CRegistredLock< Mutex >
{
    bool m_locked;
public:
    CManualLock( Mutex& r_auxlocker, bool lock = true ) : BaseLock( r_auxlocker ), m_locked(false)
    {
        if (lock)
            Lock();
    }
    ~CManualLock()
    {
        if (m_locked)
            mutex().Unlock();
    }
    //
    void Unlock()
    {
        if (m_locked) {
            mutex().Unlock();
            m_locked = false;
        }
    }
    //
    void Lock()
    {
        if (!m_locked) {
            mutex().Lock();
            m_locked = true;
        }
    }

    bool TryLock() {
        if (!m_locked) {
            m_locked = mutex().TryLock();
        }
        return m_locked;
    }

    bool IsLocked() const
    {
        return m_locked;
    }
};//CManualLock


class CReverseLocker
{
public:
    typedef CLocker::mutex_type mutex_type;
    typedef CLocker parent_lock_type;
public:
    CReverseLocker( parent_lock_type & lock ) : m_lock(lock)
    {
        m_lock.mutex().Unlock();
    }
    ~CReverseLocker()
    {
        m_lock.mutex().Lock();
    }
private:
    parent_lock_type & m_lock;
};//CManualLock


//////////////////////////////////////////////////////////////////////////////////////////////////
// Блокираторы с возможностью получения доступа к полям
//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Type, typename Mutex> class CInterThreadObjectAccessor;


template<typename Type, typename Mutex = c_aux_locker>
class CInterThreadObject 
{
    friend CInterThreadObjectAccessor< Type, Mutex >;

public:
    typedef Mutex mutex_type;
    typedef Type protected_type;

protected:
    operator protected_type & ()              { return m_value; }
    operator protected_type const & () const  { return m_value; }

public:
    CInterThreadObject() {}
    CInterThreadObject(const protected_type & obj): m_value(obj) { }

    mutex_type & getMutex() const             { return m_locker; }

private:
    mutable mutex_type m_locker;
    protected_type m_value;
};


// Because conversion from CInterThreadObject to Type is protected we should use this class
template<typename Type, typename Mutex>
class CInterThreadObjectAccessor
{
protected:
    typedef CInterThreadObject< Type, Mutex >           LockableObject;
    typedef CInterThreadObjectAccessor< Type, Mutex >   AccessorType;
    typedef typename LockableObject::mutex_type         mutex_type;
    typedef typename LockableObject::protected_type     protected_type;

    CInterThreadObjectAccessor(LockableObject & src): m_object(src) {}

    protected_type const & getObject() const    { return static_cast< protected_type const & >(m_object); }
    protected_type & getObject()                { return static_cast< protected_type & >(m_object); }

private:
    LockableObject & m_object;
};


template<typename U, typename Locker = CLocker>
class CObjectLocker :
    public CInterThreadObjectAccessor< U, typename Locker::mutex_type >,
    public Locker
{
public:
    CObjectLocker(LockableObject & src): AccessorType( src ), Locker(src.getMutex()) {}
    CObjectLocker(CObjectLocker && src): AccessorType( src.getObject() ), Locker(src.getMutex()) {}

    protected_type * operator ->()              { return &getObject(); }
    protected_type & operator *()               { return  getObject(); }
    protected_type const * operator ->() const  { return &getObject(); }
    protected_type const & operator *() const   { return  getObject(); }
    protected_type copy() const                 { return  getObject(); }

private:
    CObjectLocker(const CObjectLocker &);   // don't copy object
    CObjectLocker & operator =(const CObjectLocker &);
};


template<typename U, typename Locker>
class CObjectLocker< const U, Locker > :
    public CInterThreadObjectAccessor< U, typename Locker::mutex_type >,
    public Locker
{
public:
    CObjectLocker(const LockableObject & src)
        : AccessorType( const_cast< LockableObject & >(src) ), Locker(src.getMutex()) {}
    CObjectLocker(CObjectLocker && src): AccessorType( src.getObject() ), Locker(src.getMutex()) {}

    protected_type const * operator ->() const  { return &getObject(); }
    protected_type const & operator *() const   { return getObject();  }
    protected_type copy() const                 { return getObject();  }

private:
    CObjectLocker(const CObjectLocker &);   // don't copy object
    CObjectLocker & operator =(const CObjectLocker &);
};

template<typename Locker, typename T>
inline CObjectLocker<const T, Locker> make_object_lock(const CInterThreadObject<T, typename Locker::mutex_type> & obj) {
    return CObjectLocker<const T, Locker>(obj);
}

template<typename Locker, typename T>
inline CObjectLocker<T, Locker> make_object_lock(CInterThreadObject<T, typename Locker::mutex_type> & obj) {
    return CObjectLocker<T, Locker>(obj);
}


}// locker_space

#endif //_LOCKER_H_
