/*******************************************************************
 *  GenericDataConsumer.h - A component that is capable of consuming
 *  any piece of data from any DataSource, provided that this data is
 *  described (and thus describable) by the Keymaster.
 *
 *  Copyright (C) 2016 Associated Universities, Inc. Washington DC, USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Correspondence concerning GBT software should be addressed as follows:
 *  GBT Operations
 *  National Radio Astronomy Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *******************************************************************/

#if !defined(_GENERIC_DATA_CONSUMER_H_)
#define _GENERIC_DATA_CONSUMER_H_

#include "matrix/Component.h"
#include "matrix/Thread.h"
#include "matrix/DataInterface.h"

#include <string>
#include <list>
#include <map>

namespace matrix
{
    class GenericDataConsumer : public Component
    {
    public:
        static Component *factory(std::string, std::string);
        virtual ~GenericDataConsumer();

        void add_data_handler(std::unique_ptr<GenericBufferHandler> hp)
        {
            _handler = std::move(hp);
        }

    protected:
        GenericDataConsumer(std::string name, std::string km_url);

        void _task();

        virtual bool _do_start();
        virtual bool _do_stop();

        bool connect();
        bool disconnect();
        void data_configuration_changed(std::string, YAML::Node);

        matrix::DataSink<matrix::GenericBuffer, matrix::select_only> _sink;

        Thread<GenericDataConsumer> _thread;
        TCondition<bool> _thread_started;
        TCondition<bool> _run;
        std::unique_ptr<GenericBufferHandler> _handler;
    };

}

#endif
