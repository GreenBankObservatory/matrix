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


#include "DataInterface.h"
#include <iostream>

using namespace std;


DataSource::DataSource()
{
}

DataSource::~DataSource()
{
}

// These methods are meant to be abstract. However, we may
// find some common functionality. For now we just emit
// an error message.
bool DataSource::_register_urn(std::string urn_to_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSource::_unregister_urn(std::string urn_to_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

// This will probably replace register_urn above
bool DataSource::_bind(YAML::Node &bind_node)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSource::_publish(const void *data, size_t size_of_data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSource::_publish(const std::string &data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}


DataSink::DataSink()
{
}

DataSink::~DataSink()
{
}

bool DataSink::_connect(std::string urn_from_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_subscribe(std::string urn_from_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_unsubscribe(std::string urn_from_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_get(void *v, size_t &size_of_data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_get(std::string &data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}



