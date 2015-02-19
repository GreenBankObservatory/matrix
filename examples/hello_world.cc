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
    simple.initialize();

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
    simple.start();
    // Normal app would do something here.
    // tell the components to stop (They should go back to the Ready state.)
    simple.stop();
    return 0;
}
