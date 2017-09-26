#ifndef SharedObjectRegistry_h
#define SharedObjectRegistry_h

/// SharedObjectRegistry - A class for managing inter-component shared objects.
///
/// Sometimes it is necessary to use direct references between Components,
/// instead of using the normal mechanisms in the Keymaster. This
/// project has a number of connections which must operate in real-time,
/// without going through the Keymaster.
///
/// The concept here is to use the Keymaster during initialization to
/// install references to a Component's SharedObjectRegistry. For
/// example, we might have the following Keymaster entries:
///
/// * Shared.widget = 0xff78000
/// * Shared.foo    = 0xff78000
/// * Shared.bar    = 0xaa80000
/// * Shared.fuzz   = 0xaa80000
///
/// A Component wanting to share an object examines the predefined key
/// for the this pointer associated with the object. Note several keys
/// will map to the same address, because the entries are the address
/// of the SharedObjectRegistry which owns the original object.
///

#include<map>
#include<string>
#include<memory>



namespace matrix
{
    class Keymaster;

    class SharedObjectRegistry
    {
    public:
        SharedObjectRegistry(std::shared_ptr<matrix::Keymaster> &km);

        void *get_shared_obj(std::string);

        void add_shared_object(std::string key, void *p);

        void *remove_shared_object(std::string key);

    protected:
        void *_get_shared_obj(std::string);

        std::map<std::string, void *> shared_objs;
        char addr_string[32];
        std::shared_ptr<matrix::Keymaster> keymaster;
    };

/// A template for accessing the shared object with a cleaner feel.
/// Nothing magic here, and no resource management.
    template<typename T>
    class SharedObject
    {
    public:
        SharedObject(void *p = nullptr) : ptr((T *) p)
        {
        }

        void set_ptr(void *p)
        {
            ptr = (T *) p;
        }

        T get()
        {
            return *ptr;
        }

        T *get_ptr()
        {
            return ptr;
        }

        void set(T v)
        {
            *ptr = v;
        }

        bool valid()
        {
            return ptr != nullptr;
        }

        T *operator->()
        {
            return ptr;
        }

        // historical Node interface
        bool getValue(T &x)
        {
            if (valid())
            {
                x = get();
                return true;
            }
            else
            {
                return false;
            }
        }

        bool setValue(T x)
        {
            if (valid())
            {
                set(x);
                return true;
            }
            else
            {
                return false;
            }
        }

    protected:
        T *ptr;
    };

};


#endif
