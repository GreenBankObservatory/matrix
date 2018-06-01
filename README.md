Matrix
======

Rationale
---------

Matrix is a data-flow application framework, designed for maximum
flexibility. At its core a Matrix application consists of various
components that produce and consume data. Due to the single data
publication and subscription API, components may reside in the same
application space, as another application on the same host, or on
another host on the same network, with few or no changes to the
application code.

An important feature of Matrix is that the connection topology can be
altered at will during runtime, using either preset modes encoded in
the YAML configuration file, or using modes that may be built on the
fly on the Keymaster, provided the components to support it exist.

Features of Matrix:

* externally visible application state, via a key/value store handled
  by the Keymaster component. The keymaster keeps this state using
  YAML, and local and/or remote applications may subscribe to it and
  write to it, provided a transport is selected that supports
  this. The YAML store is initialized via a YAML configuration file
  that is used to specify the components, transports, and connection
  modes. In addition, new connection modes may be specified during
  runtime.
  
* Unidirectional Source/Sink data channels. These sources and sinks
  support fan-out, fan-in, or any combination thereof. Data
  compatibility between data sent by a source and received by a sink
  is determined at runtime and by the application, not by
  type-checking. Rather than impose type restrictins, the Matrix
  framework allows different strategies to be used at the application
  level, such as using the same type on both end, or using a
  serialization library such as the excellent MessagePack library,
  etc.
  
* Matrix supports several types of transports to conduct the data from
  source to sink, via an extensible class structure. The transport
  system is transparent to the producers and consumers of the
  data. Transports are specified in the YAML configuration in the
  Keymaster, and may be changed during runtime. If a new transport
  system is added to the Matrix framework old applications can make
  use of it simply by recompiling against the newer library, and
  specifying the new transport in the YAML configuration file. Most
  Matrix transports are provided by the ZMQ library, with the
  exception of the fast intra-process rtinproc transport.
  
* An Architect component creates the various component in accordance
  with the YAML key/value store maintained by the Keymaster. The
  architect is agnostic to the actual type of the components created,
  using a factory method system to do this, allowing it to create
  user-defined components. Currently the Architect only works within
  the application space.
  
* A state engine is used to start, stop, and configure the Matrix
  application. An Off state exists that allows the application to
  disconnect from any hardware. While in Standby configurations can be
  specified in the Keymaster. When the application is transitioned to
  On, the specified mode is configured by creating the components (if
  necessary), and connecting them together as specified in the
  Keymaster. Finally, data flows when the application is transitioned
  to the Running state.

sample configuration database
-----------------------------

(from the Helloworld example)

    Keymaster:
      URLS:
        Initial:
          - inproc://matrix.keymaster
          - ipc:///tmp/helloworld.keymaster
          - tcp://*:42000

      # temporary fix to limit yaml memory use	
      clone_interval: 50
    # Components in the system
    #
    # Each component has a name by which it is known. Some components have 0
    # or more data sources, and 0 or more data sinks. The sources are known
    # by their URLs. The sinks are just listed by name (for now).

    components:
      clock:
        time: 0
        type: ClockComponent
      ZZ:
        type: IndicatorComponent
      AA:
        type: IndicatorComponent
    # Connection mapping for the various configurations. The mapping is a
    # list of lists, which each element of the outer list being a 4-element
    # inner list: [source_component, source_name, sink_component, sink_name]

    connections:
      CLOCK:
        - [ZZ]
        - [clock]
    - [AA]

Building Matrix
---------------

## Dependencies ##

  * [yaml-cpp](https://github.com/jbeder/yaml-cpp) __from source__,
    and it needs to be built with shared libraries (it is too old in
    distro packages).
    
The remainder may be installed from distro packages. Ensure you also
install the `-dev` or `-devel` (depending on your distro) development
files.

  * check
  
  * [libzmq](https://github.com/zeromq/libzmq)
  
  * [cppzmq](https://github.com/zeromq/cppzmq)
  
  * [msgpack](https://github.com/msgpack/msgpack-c)
  
  * readline (for keychain)
  
  * cfitsio (for slogger)
  
  * cppunit (for unit tests)
  
  * fftw3 (for toy scope)

## Autogen: ##

    $ cd matrix
    $ ./autogen.sh
    $ ./configure --prefix=/usr/local
    
Autogen is not well supported; we recommend CMake.


## CMake: ##

    $ mkdir build && cd build
    $ cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
  
use `-DCMAKE_BUILD_TYPE=Debug`
use `-DTHIRDPARTYDIR=</your/nonstandard/library/prefix>` for if any
needed library is not in a standard location.
use `-DBUILD_SHARED_LIBS=1` to build a `.so` shared library.
