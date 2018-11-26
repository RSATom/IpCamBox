#pragma once

#include <utility>


class RefCounterBase
{
    RefCounterBase& operator = (const RefCounterBase&) = delete;
    RefCounterBase& operator = (RefCounterBase&&) = delete;

public:
    unsigned refsCount() const
        { return *_counter; }
    bool hasRefs() const
        { return *_counter > 0; }

protected:
    inline explicit RefCounterBase(void*);
    inline explicit RefCounterBase(const RefCounterBase&);
    inline explicit RefCounterBase(RefCounterBase&&);
    inline ~RefCounterBase();

    void* ptr()
        { return _ptr; }
    const void* ptr() const
        { return _ptr; }

    bool lastRef() const
        { return _counter && 1 == *_counter; }

private:
    void* _ptr;
    unsigned* _counter;
};

RefCounterBase::RefCounterBase(void* ptr) :
    _ptr(ptr), _counter(new unsigned())
{
}

RefCounterBase::RefCounterBase(const RefCounterBase& other) :
    _ptr(other._ptr), _counter(other._counter)
{
    ++*_counter;
}

RefCounterBase::RefCounterBase(RefCounterBase&& other) :
    _ptr(other._ptr), _counter(other._counter)
{
    other._ptr = nullptr;
    other._counter = nullptr;
}

RefCounterBase::~RefCounterBase()
{
    if(_counter) {
        if(*_counter > 0)
            --(*_counter);
        else {
            delete _counter;
            _counter = nullptr;
        }
    }
}


template<typename T, void (T::*NoMoreRefs)() = nullptr>
class RefCounter : public RefCounterBase
{
public:
    inline explicit RefCounter(T*);
    inline explicit RefCounter(const RefCounter&);
    inline explicit RefCounter(RefCounter&&);
    inline ~RefCounter();

    inline T* ptr();
    inline const T* ptr() const;

    inline T* operator -> ();
    inline const T* operator -> () const;

    inline T& operator * ();
    inline const T& operator * () const;
};

template<typename T, void (T::*NoMoreRefs)()>
RefCounter<T, NoMoreRefs>::RefCounter(T* ptr) :
    RefCounterBase(ptr)
{
}

template<typename T, void (T::*NoMoreRefs)()>
RefCounter<T, NoMoreRefs>::RefCounter(const RefCounter& other) :
    RefCounterBase(other)
{
}

template<typename T, void (T::*NoMoreRefs)()>
RefCounter<T, NoMoreRefs>::RefCounter(RefCounter&& other) :
    RefCounterBase(std::move(other))
{
}

template<typename T, void (T::*NoMoreRefs)()>
RefCounter<T, NoMoreRefs>::~RefCounter()
{
    if(nullptr != NoMoreRefs) {
        if(lastRef())
            (ptr()->*NoMoreRefs)();
    }
}

template<typename T, void (T::*NoMoreRefs)()>
T* RefCounter<T, NoMoreRefs>::ptr()
{
    return static_cast<T*>(RefCounterBase::ptr());
}

template<typename T, void (T::*NoMoreRefs)()>
const T* RefCounter<T, NoMoreRefs>::ptr() const
{
    return static_cast<const T*>(RefCounterBase::ptr());
}

template<typename T, void (T::*NoMoreRefs)()>
T* RefCounter<T, NoMoreRefs>::operator -> ()
{
    return ptr();
}

template<typename T, void (T::*NoMoreRefs)()>
const T* RefCounter<T, NoMoreRefs>::operator -> () const
{
    return ptr();
}

template<typename T, void (T::*NoMoreRefs)()>
T& RefCounter<T, NoMoreRefs>::operator * ()
{
    return *ptr();
}

template<typename T, void (T::*NoMoreRefs)()>
const T& RefCounter<T, NoMoreRefs>::operator * () const
{
    return *ptr();
}
