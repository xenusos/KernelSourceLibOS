/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#pragma once

template <typename T>
class XArray
{
private:
    size_t _size;
    dyn_list_head_p _head;
    error_t _internal_error = kStatusOkay;

public:
    XArray<T>::XArray();

    XArray<T>::~XArray();

    error_t Append(T & entry, T *& out);
    error_t Preappend(T & entry, T *& out);
    error_t Append(T & entry);
    error_t Preappend(T & entry);

    error_t GetAtIndex(size_t index, T *&entry);
    error_t GetByBuffer(T & buffer, size_t & index);
    
    error_t Iterate(void(* callback)(T *, void *), void *);

    error_t Slice(size_t index, size_t cnt);
    error_t Splice(size_t index, T & entry, T *& out);
    error_t Splice(size_t index, T & entry);
    error_t FreeIndex(size_t index);

    error_t Length(size_t &);
    size_t DataSize();

    error_t InternalError();
};

template<typename T>
XArray<T>::XArray()
{
    _size = sizeof(T);
    _head = DYN_LIST_CREATE(T);
    if (!_head)
        _internal_error = XENUS_ERROR_OUT_OF_MEMORY;
}

template<typename T>
XArray<T>::~XArray()
{
    if (_head)
    {
        if (ERROR(dyn_list_destory(_head)))
        {
            printf("XArray<T>::~XArray failed to deallocate head!\n");
            // MAYBE (PANIC), should we panic? 
            // at this point in time, some internal error is causing leak, which could become massive.s
        }
    }
}

template<typename T>
error_t XArray<T>::Append(T & entry, T *& out)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_append(_head, &data)))
        return ret;

    if (data != memcpy(data, &entry, _size))
        return kErrorGenericFailure;

    out = static_cast<T *>(data);
    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Append(T & entry)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_append(_head, &data)))
        return ret;

    if (data != memcpy(data, &entry, _size))
        return kErrorGenericFailure;

    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Preappend(T & entry, T *& out)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_preappend(_head, &data)))
        return ret;

    if (data != memcpy(data, &entry, _size))
        return kErrorGenericFailure;

    out = static_cast<T *>(data);
    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Preappend(T & entry)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_preappend(_head, &data)))
        return ret;

    if (data != memcpy(data, &entry, _size))
        return kErrorGenericFailure;

    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::GetAtIndex(size_t index, T *& entry)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_get_by_index(_head, index, &data)))
        return ret;

    if (ret == kStatusListsNotFound)
        return kErrorListsNotFound;

    entry = static_cast<T *>(data);
    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::GetByBuffer(T & buffer, size_t & index)
{
    error_t ret;
    size_t i;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_get_by_buffer(_head, &buffer, &i)))
        return ret;

    if (ret == kStatusListsNotFound)
        return kErrorListsNotFound;

    index = i;
    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Iterate(void(* callback)(T * obj, void *), void * ctx)
{
    error_t ret;
    size_t length;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = Length(length)))
        return ret;

    for (size_t i = 0; i < length; i++)
    {
        T * entry;
        if (NO_ERROR(GetAtIndex(i, entry)))
            callback(entry);
    }

    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Slice(size_t index, size_t cnt)
{
    error_t ret;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_slice(_head, index, cnt)))
        return ret;

    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Splice(size_t index, T & entry, T *& out)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_splice(_head, index, &data)))
        return ret;

    if (data != memcpy(data, &entry, _size))
        return kErrorGenericFailure;

    out = static_cast<T *>(data);
    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::Splice(size_t index, T & entry)
{
    error_t ret;
    void * data;

    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_splice(_head, index, &data)))
        return ret;

    if (data != memcpy(data, &entry, _size))
        return kErrorGenericFailure;

    return kStatusOkay;
}

template<typename T>
error_t XArray<T>::FreeIndex(size_t index)
{
    return Slice(index, 1);
}

template<typename T>
error_t XArray<T>::Length(size_t & length)
{
    error_t ret;
    size_t len;
    
    if (ERROR(_internal_error))
        return kErrorInternalError;

    if (ERROR(ret = dyn_list_entries(_head, &len))) 
        return ret;

    length = len;

    return kStatusOkay;
}

template<typename T>
size_t XArray<T>::DataSize()
{
    return _size;
}

template<typename T>
error_t XArray<T>::InternalError()
{
    return _internal_error;
}
