#include "matrix/SharedObjectRegistry.h"
#include <cstdio>
#include <memory>
#include "matrix/Keymaster.h"

using namespace std;
using namespace matrix;

namespace matrix
{
/// Create a registry
    SharedObjectRegistry::SharedObjectRegistry(shared_ptr <Keymaster> &km) :
            keymaster(km)
    {
        // create a string of the this pointer address.
        sprintf(addr_string, "%p", this);
    }

/// Add a shared object. Note that there is no automatic
/// memory management here. It is the caller's responsibility
/// to first call remove_shared_object() before deleting.
/// Also note that deletion **should only** be called while in
/// the standby state. Subscribers should only request shared
/// objects in the ready state.
    void SharedObjectRegistry::add_shared_object(std::string key, void *ptr)
    {
        shared_objs[key] = ptr;
        keymaster->put(key, addr_string, true);
    }

/// Called by other Registries to get a reference to a shared object
    void *SharedObjectRegistry::_get_shared_obj(string key)
    {
        auto p = shared_objs.find(key);
        if (p == shared_objs.end())
        {
            return nullptr;
        }
        return p->second;
    }

/// Called by owning Component to unregister a shared object.
    void *SharedObjectRegistry::remove_shared_object(string key)
    {
        void *r_ptr = nullptr;

        try
        {
            keymaster->del(key);
        }
        catch (KeymasterException &e)
        {
            cerr << "SharedObjectRegistry: No such object to remove"
            << key << endl;
        }
        auto p = shared_objs.find(key);
        if (p != shared_objs.end())
        {
            r_ptr = p->second;
        }

        shared_objs.erase(key);
        return r_ptr;
    }

/// Get a remote shared object reference
/// Returns either a pointer to the shared object or nullptr,
/// indicating a keymaster failure, or non-existence of the remote object.
    void *SharedObjectRegistry::get_shared_obj(string key)
    {
        mxutils::yaml_result yr;

        if (!keymaster->get(key, yr))
        {
            cerr << "SharedObjectRegistry::get_shared_object: no such key "
            << key << endl;
            return nullptr;
        }
        auto pv = yr.node.as<unsigned long>();
        auto rr = (SharedObjectRegistry *) pv;
        if (rr == nullptr)
        {
            return nullptr;
        }
        return rr->_get_shared_obj(key);
    }

};
