/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

class OObject 
{
public:
    inline OObject();
    inline void Destory();

    inline bool IsDead();
protected:
    inline void Invalidate();
    virtual void InvalidateImp() {};
private:
    bool _is_dead;
};
#include "ObjectsObject.imp"

#define CHK_DEAD {if (IsDead()) return kErrorObjectDead; }
#define CHK_DEAD_RET_NULL {if (IsDead()) return nullptr; }

template<class T>
class OPtr
{
public:
    inline OPtr(void * ahh);
    inline OPtr(T * ahh);
    inline OPtr();

    inline T *			GetTypedObject();
    inline OObject *	GetObject();

    inline void *		SetObject(void * obj, bool allow_destory);
    inline T *			SetObject(T * obj, bool allow_destory);

    inline T *			operator->();

    void Destory()
    {
        if (_allow_destory)
            if (_os_obj)
                _os_obj->Destory();
            else
                panic("OPtr destruction on null object");
        else
            panic("OPtr destruction isn't allowed. Check UncontrollableRefernce usage");
    }
private:
    bool _allow_destory;
    union 
    {
        T * _object;
        OObject * _os_obj;
        void * _unk_obj;
    };
};
#include "ObjectsOPtr.imp"

// Uncontrollable reference parameters assign an arbitrary pointer pointer with an object that has its life controlled by a parent object or other system
template<class T>
class OUncontrollableRef
{
public:
    inline OUncontrollableRef()					: _has_optr(false), _has_generic(false), _optr(_optr_hack), _generic_ptr(_generic_hack) {}
    inline OUncontrollableRef(OPtr<T> & ptr)	: _has_optr(true),  _has_generic(false), _optr(ptr),        _generic_ptr(_generic_hack) {}
    inline OUncontrollableRef(T *& ptr)			: _has_optr(false), _has_generic(true),  _optr(_optr_hack), _generic_ptr(ptr)           {}

    inline void *	SetObject(void * obj)	const;
    inline T *		SetObject(T * obj)		const;
protected:
    OPtr<T> _optr_hack;
    OPtr<T> & _optr;
    bool _has_optr;

    T * _generic_hack;
    T *& _generic_ptr;
    bool _has_generic;
};
#include "ObjectsRef.imp"

class OReferenceCounter;
template<class T>
class ORetardPtr
{
public:
    inline ORetardPtr(T * object);
    inline ORetardPtr();
    inline ~ORetardPtr();

    inline T* operator->();
    inline T& operator* ();
    inline ORetardPtr<T>& operator=(const ORetardPtr<T>& predecessor);
    inline ORetardPtr<T>& SwapValue(const ORetardPtr<T>& predecessor);

    void Destory() { printf("Caught illegal destory call to a retard pointer. Did you think this was an O(s)Object? Legacy code?\n"); }

    bool IsValid();
protected:
    OReferenceCounter * _ref_counter;
    union
    {
        T * _object;
        OObject * _os_obj;
    };

    void Collect();
};
#include "ObjectsRetardPointer.imp"

// Outlivable reference parameters, like uncontrollable references, assign values to an arbitrary pointer pointer. however, with these parameters, you are responsible for the objects life-span. 
template<class T>
class OOutlivableRef
{
public:
    inline OOutlivableRef()                    : _has_generic(false), _has_optr(false), _has_retard(false), _optr(_optr_hack), _generic_ptr(_generic_hack), _retard_ptr(_retarded_hack) {}
    inline OOutlivableRef(OPtr<T> & ptr)       : _has_generic(false), _has_optr(true),  _has_retard(false), _optr(ptr),        _generic_ptr(_generic_hack), _retard_ptr(_retarded_hack) {}
    inline OOutlivableRef(ORetardPtr<T> & ptr) : _has_generic(false), _has_optr(false), _has_retard(true),  _optr(_optr_hack), _generic_ptr(_generic_hack), _retard_ptr(ptr)            {}
    inline OOutlivableRef(T *& ptr)            : _has_generic(true),  _has_optr(false), _has_retard(false), _optr(_optr_hack), _generic_ptr(ptr),           _retard_ptr(_retarded_hack) {}

    inline void *		PassOwnership(void * obj)	const;
    inline T *			PassOwnership(T * obj)		const;
private:
    OPtr<T> _optr_hack;
    OPtr<T> & _optr;
    bool _has_optr;

    T * _generic_hack;
    T *& _generic_ptr;
    bool _has_generic;

    ORetardPtr<T> _retarded_hack;
    ORetardPtr<T> & _retard_ptr;
    bool _has_retard;
};
#include "ObjectsOutlivableRef.imp"

class OReferenceCounter
{
public:
    inline OReferenceCounter();
    inline long Reference();
    inline long Deference();
private:
    long _count;
};
#include "ObjectsReferenceCounter.imp"