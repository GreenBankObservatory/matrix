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

#ifndef ZMQDataInterface_h
#define ZMQDataInterface_h

#include "DataInterface.h"
#include<string>


/// A Zero-MQ based implementation of a DataSource. Handles 
/// all of the defined ZMQ transport types.
class ZMQDataSource : public DataSource
{
public:
    ZMQDataSource();
    virtual ~ZMQDataSource();
    
protected:
    bool _register_urn(std::string urn_to_keymaster);
    bool _unregister_urn(std::string urn_to_keymaster);
    bool _bind(YAML::Node &bindnode);
    bool _publish(const void *data, size_t size_of_data);
    bool _publish(const std::string &data);
};

/// A concrete DataSink using the Zero-MQ based tranport.
/// Handles all of the defined ZMQ transport types.
class ZMQDataSink : public DataSink
{
public:
    ZMQDataSink();
    virtual ~ZMQDataSink();
    
protected:
    virtual bool _connect(std::string urn_from_keymaster);
    virtual bool _subscribe(std::string urn_from_keymaster);
    virtual bool _unsubscribe(std::string urn_from_keymaster);
    virtual bool _get(void *v, size_t &size_of_data);
    virtual bool _get(std::string &data);
};
    


#endif
