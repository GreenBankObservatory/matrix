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

*/
