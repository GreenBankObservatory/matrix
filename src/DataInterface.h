//# Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
//# 
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//# 
//# This program is distributed in the hope that it will be useful, but
//# WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//# General Public License for more details.
//# 
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//# 
//# Correspondence concerning GBT software should be addressed as follows:
//#     GBT Operations
//#     National Radio Astronomy Observatory
//#     P. O. Box 2
//#     Green Bank, WV 24944-0002 USA

#ifndef DataInterface_h
#define DataInterface_h

#include<string>
#include<memory>
#include<yaml.h>


/// A strategy pattern class for the source-->sink data interface data between Components.
/// The intent is to provide a transport independent interface for publishing
/// data using various 0MQ and/or real-time extensions.

/**
DataSources:
------------
A Component data source specification which specifies a DataSource
binding (using the derived ZMQDataSource) to the tcp and inproc transport 
transport will contain keymaster keys from the configuration file:

      components:
         myProducer:
            type: DataProducer
            source:
                A: transport [tcp, inproc, rtinproc]

Resulting in the following keys in the keymaster database:

   * components.myProducer.source.A.transport[0] = "tcp"
   * components.myProducer.source.A.transport[1] = "inproc"
   * components.myProducer.source.A.transport[2] = "rtinproc"

As the Component creates and binds the DataSource to the various transports
it adds urn nodes which detail the run-time bound urns. Continuing with
the example above, the Component would add to the keymaster database:

   * components.myProducer.source.A.urn[0] = "tcp://*.9925"
   * components.myProducer.source.A.urn[1] = "inproc://myLogger_A"
   * components.myProducer.source.A.urn[2] = "rtinproc://0x26A4000"

Thus indicating to DataSources exactly how to bind to the desired
transport. DataSinks, given the path to the DataSource and which
transport to use can then easily locate the (optionally) dynamically
assigned tranport port number or address.
 
***/

class DataSource
{
public:
    DataSource();
    virtual ~DataSource();
    
    bool register_urn(std::string urn_to_keymaster)
    {
        return _register_urn(urn_to_keymaster);
    }
    bool unregister_urn(std::string urn_to_keymaster)
    {
        return _unregister_urn(urn_to_keymaster);
    }
    bool publish(const void *data, size_t size_of_data)
    {
        return _publish(data, size_of_data);
    }
    bool publish(const std::string & data)
    {
        return _publish(data);
    }
    bool bind(YAML::Node &binding_node)
    {
        return _bind(binding_node);
    }

protected:
    virtual bool _register_urn(std::string urn_to_keymaster);
    virtual bool _unregister_urn(std::string urn_to_keymaster);
    virtual bool _bind(YAML::Node &bindnode);
    virtual bool _publish(const void *data, size_t size_of_data);
    virtual bool _publish(const std::string &data);
};


/// A strategy based class for the Component data sinks.

/**
DataSinks:
----------
After the Component DataSources are constructed, the Controller may issue
a "prepare" event to instruct the Components to connect their DataSinks
to the now bound DataSources. Each DataSink then searches the keymaster
for the bound urn for the DataSource it needs. Recall that for DataSinks, each
component is provided a path of the DataSource, and a transport to
use. An example of the keys for the DataSource might be (repeating a
portion of the the example from the DataSource page) the key/values for the
bound DataSource 'A' would be:

   * components.myProducer.source.A.urn[0] = "tcp://*.9925"
   * components.myProducer.source.A.urn[1] = "inproc://myLogger_A"
   * components.myProducer.source.A.urn[2] = "rtinproc://0x26A4000"

The configuration file for various Components includes a *connections.mode*
section, which has the syntax:

   * [<Component name>, <source name>, <Component name>, <sink name>, transport] 
   
So provided a hypothetical entry for the connections section:

        connections:
            VEGAS_1:
                - [myProducer, A, myLogger, input_data, tcp]
   
The myLogger Component input_data DataSink wanting to get data from the DataSource noted above
would generate and query for the key:

   * components.myProducer.sink.A.urn

Then it would use the connections transport field to search for a binding of the same type.
In this case it would select:

   * components.myProducer.source.A.urn[0] = "tcp://*:9925"

The DataSink would then use the ZMQDataSink to connect to the tcp transport at the localhost at port 9925.



***/

class DataSink
{
public:
    DataSink();
    virtual ~DataSink();
    
    bool connect(std::string urn_from_keymaster)
    {
        return _connect(urn_from_keymaster);
    }
    
    bool subscribe(std::string urn_from_keymaster)
    {
        return _subscribe(urn_from_keymaster);
    }
    bool unsubscribe(std::string urn_from_keymaster)
    {
        return _unsubscribe(urn_from_keymaster);
    }    
    bool get(void *v, size_t &size_of_data)
    {
        return _get(v, size_of_data);
    }
    bool get(std::string &data)
    {
        return _get(data);
    }
    
protected:
    virtual bool _connect(std::string urn_from_keymaster);
    virtual bool _subscribe(std::string urn_from_keymaster);
    virtual bool _unsubscribe(std::string urn_from_keymaster);
    virtual bool _get(void *v, size_t &size_of_data);
    virtual bool _get(std::string &data);
         
};


#endif
