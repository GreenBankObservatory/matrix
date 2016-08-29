/*******************************************************************
 *  DataSink.cc - DataSink.
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

#include "matrix/DataSink.h"

using namespace std;

namespace matrix
{
/**
  * Reconnects a sink to its source. Given a KeymasterHeartbeatCB, it
  * can verify that the Keymaster is still alive. If so, it checks to
  * see if the given DataSink's source URL is the same as the
  * currently configured one. If not, it will disconnect the DataSink
  * and reconnect it, setting it up with the newly restored source.
  *
  * @param ds: The DataSinkBase pointer to the DataSink of interest.
  * @param km: A Keymaster object
  * @param kmhb: A KeymasterHeartbeatCB callback, reflects the last
  * state of the Keymaster
  * @param comp: The component name to reconnect to
  * @param src: The component's source name to reconnect to
  * @param transport: The transport to use, defaults to ""
  *
  * @return true if it reconnected, false otherwise.
  *
  */

    bool reconnectDataSink(DataSinkBase *ds, Keymaster &km, KeymasterHeartbeatCB &kmhb,
                           string comp, string src, string transport)
    {
        Time::Time_t hb = kmhb.last_update();
        Time::Time_t now = Time::getUTC();
        bool rval = false;

        // assume Keymaster is gone if hearbeat is older than 5 seconds.
        if (now - hb < 5000000000L)
        {
            try
            {
                auto urns = km.get_as<vector<string> >(ds->current_source_key());
                string urn = ds->current_source_urn();

                if (!any_of(urns.begin(), urns.end(), [urn](string i){return i == urn;}))
                {
                    // changed, reconnect
                    ds->disconnect();
                    ds->connect(comp, src, transport);
                    rval = true;
                }
            }
            catch (KeymasterException &e)
            {
                cerr << Time::isoDateTime(Time::getUTC())
                     << " -- " << e.what() << endl;
            }
        }

        return rval;
    }
}
