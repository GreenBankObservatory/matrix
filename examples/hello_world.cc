#include <string>
#include <iostream>
#include <yaml-cpp/yaml.h>

#include "Controller.h"
#include "Component.h"

using namespace std;
using namespace YAML;

// temp definition
// Example trival component
class HelloWorld : public Component
{
public:
    HelloWorld(string name) : Component(name) {}
    static Component *factory(string myname)
    { cout << "Hello World ctor" << endl; return new HelloWorld(myname); }
};

///////////////////////////////////////////////////////////////////
#if 0
class PublishBase
{
public:
    virtual void publish(string, string) {}
};

template <typename T>
class PublishAction : public PublishBase
{
    typedef void (T::*PubMethod)(string, string);

    T  *_object;
    PubMethod _faction;

    PublishAction(T *obj, PubMethod cb) :
      _object(obj),
      _faction(cb)
    {
    }

    virtual void publish(string key, string val)
    {
       _object->*_faction)(key, val); 
    }
};

class MockKeymaster {
public:
    string get(string key);
    void put(string key, bool creat=true);
    void subscribe(PublishBase *);
    void publish(string key, string value);
    
    vector<PublishBase *> subs;
};

void MockKeymaster::publish(string key, string val)
{
    for (auto k=subs.begin(); k!= subs.end(); ++k)
    {
        (*k)->publish(key, val);
    }
}   

MockKeymaster theKeymaster;

class StateComponent : public Component
{
    StateComponent(string myinstancename) : Component(myinstancename),
        my_thread(this, &StateComponent::entry)
    {
        contact_keymaster();
        initialize_fsm();
        my_thread.start();
    }
    
    void entry();
    
public:
    static Component *factory(string myname)
    {
        return new StateComponent(myname);
    }
};

void StateComponent::entry()
{
    theKeymaster.subscribe(new PublishAction<StateComponent>(this, &StateComponent::new_update));
    process_command("register");
    theKeymaster.publish(my_instance_name + ".state", get_state());
    while (!done)
    {
        // wait for a command
        command.get(cmd);
        printf("Got a command %s\n", cmd.c_str());
        // execute command
        process_command(cmd);
        // report result
        theKeymaster.publish(my_instance_name + ".state", get_state());
    }
}

void StateComponent::new_update(string key, string val)
{
}

class TestController : public Controller
{
public:
    TestController(string conf_file) : Controller(conf_file)
    {
        add_factory_method("HelloWorldComponent", &StateComponent::factory);
        create_component_instances();
    }
    bool _send_event(string);
    bool report_state(string name, string state);
    
};

bool TestController::_send_event(string event)
{
    for (auto p=component_instances.begin(); p!=component_instances.end(); ++p)
    {
        p->second->process_command(event);
    }
    return true;
}

bool TestController::report_state(string which, string newstate)
{
    if (component_instances.find(which) == component_instances.end())
    {
        cerr << __func__ << " unknown component:" << which << endl;
        return false;
    }
    // ThreadLock<Protected<map<string, string> > > l(component_status);
    component_status.lock();
    component_status[component_name] = new_status;
    component_status.unlock();
    state_condition.signal();

    return true;
}
#endif


int main(int argc, char **argv)
{

    Controller simple("hello_world.yaml");
    // The controller needs a dictionary of components names to static factory methods
    simple.add_factory_method("HelloWorld", &HelloWorld::factory);

    // first step is to create the keymaster and subscribe to its events
    if (!simple.create_the_keymaster())
    {
        cerr << "Error creating Keymaster" << endl;
    }
    // Ok, the keymaster has read the configuration. Now step through it
    // creating the components necessary.
    if (!simple.create_component_instances())
    {
        cerr << "Error creating component, or no components listed" << endl;
    }
    // Go through and subscribe to all the component status and command keys
    // The components should have created the keys in their ctor
    if (!simple.subscribe_to_components())
    {
        cerr << "Error subscribing to components" << endl;
    }
    // Now components have been instanced. Tell everyone to initialize. This
    // should leave all components in the "Standby" state.
    simple.emit_initialize();

    // wait for the keymaster events which report components in the Standby state.
    // Probably should return if any component reports and error state????
    while (!simple.check_all_in_state("Standby"))
    {
        timespec t;
        t.tv_sec = 1;
        t.tv_nsec = 0;
        nanosleep(&t, 0);
    }

    // Everybody now in standby. Get things running by issuing a start event.
    simple.emit_start();
    // Normal app would do something here.
    // tell the components to stop (They should go back to the Ready state.)
    simple.emit_stop();
    return 0;
}
