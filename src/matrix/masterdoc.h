/** @mainpage

'Matrix' is a component-based software framework designed for general
use in monitor and control systems. Component-based software emphasizes
clear separation of functionality and loose coupling between independent
components which promotes effective software re-use and maintenance.
Matrix accomplishes this through the use of well defined interfaces and
the use of state-of-the art libraries and techniques.

The framework will provide control, status and data interfaces. The
control and status interfaces utilize the ZMQ communications library. The
data interface is implemented such that a variety of data transports can
be used. Current transport options include ZMQ for general use and shared
memory buffers for real-time applications.

Independent components, implementing some combination of interfaces,
will provide processing functionality (viz., VEGAS/GUPPI-style
net thread, or SRP data acquisition module). These components will be
managed by a controller component. The controller component will only
implement the control and status interfaces and will coordinate component
operation including inter-component data connection setup. The components
coordinate with the controller using the common set of events and states
provided by the interfaces.

Matrix also includes a KeyMaster class which implements the status
interface and maintains system state using key/value pairs. It can be
accessed using Publisher/Subscriber or Request/Reply messaging.

Installation & Required Packages:
=================================

The Matrix depends upon the *zero-mq* and *yaml-cpp* packages. These can
either be installed system wide in default locations or in custom areas
(e.g /usr/local/mydir) -- your choice.

- Begin by running the autogen.sh script, which creates some dirs and runs
     autoconf.
- Next run the configure command. There are too many options to describe here,
  but the most commonly used are noted below. Each option begins with a double dash.
   - --prefix=somedir  This specifies the directory you want to install the matrix library in after it is built.
   - --with-yamlcpp=/some/custom/yamldir  This is how to specify a private yaml-cpp installation.
   - --with-zeromq=/some/custom/zmqdir   This is how to specify a private zero-mq installation.
- At this point you should be able to run make. If it complains about not finding a makefile, then run ./config.status


YAML Configuration File Format and Conventions
==============================================

The configration file uses the YAML format. (See http://www.yaml.org) The file is read by the 
Keymaster, and made available through a query interface. A publish-subscribe service is also
available for clients to monitor changes to the information.

An example file is shown below. One concept to note is that much of the file is 'free format' 
in the sense that it may contain any legal YAML text, however there are a few keywords which 
have semantics defined.  Comments are denoted with the '#' character.

Components Section:
-------------------

At the top level, the keyword 'components' begins the section which describes each
component. An example would be:

            ...
            components:
                my_component_instance_name:
                    type=MyComponentType   # Note required keyword
                    Sources:
                        outputData1: A     # output stream name : transport identifier
                        outVarname2: A
                    Transports:
                        A:
                            Specified: [inproc]
                    sinks:
                        inputVarname1: ipc
                        inputVarname2: tcp
                    # Additional component-defined keywords can be entered here also e.g:
                    board_id: 1
                    
                some_other_component:
                    type: AnotherType
                    
            ...

Where components indicates the enclosed information relates to the instantiation of
individual components. The next level headings are names of components (which can
be any legal YAML scalar symbol). For each component name, a required keyword is type.
The type keyword indicates what type of component is to be created.

So a typical controller would iterate though the entries under components and look
for the required 'component.component_name.type' entry and use the associated
factory method to create the component.  Component outputs are denoted in the
component.component_name.sources section. Each entry lists the source (output) name,
followed by a list of available protocols which the component will bind the
source's publisher. Likewise, component inputs or sinks are listed under the
'component.component_name.sinks' section.

Connection Section:
-------------------

The connection section lists source to sink connections between Components. The
connections are listed under sections which describe a particular system 'mode'.
Application which are not multi-modal should list their more static connections
under the 'default' heading.  If a component is not listed in the mode section,
it will be considered to be inactive for that mode. For example:

            # List the various modes and component connections
            connections:
                # All four components are active
                default:
                    - [Comp1, outputA, Comp2, inputB]
                    - [Comp3, outputC, Comp2, inputA]
                    - [Comp4, xxx,     Comp2, yyy]

                # In this mode, Components Comp1,Comp2 and Comp3 are active.
                # but Comp4 is not listed and therefore is inactive in this mode.
                Configuration_One: 
                    - [Comp1, specialoutput, Comp2, specialinput]
                    - [Comp1, outputA, Comp2, inputB]
                    - [Comp3] # Component Comp3 is active but has no connections in this mode
            ...


*/
