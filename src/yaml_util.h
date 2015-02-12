/*******************************************************************
 *  yaml_utils.h - YAML helper utilities
 *
 *  Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
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

#if !defined(_YAML_UTILS_H_)
#define _YAML_UTILS_H_

#include <string>
#include <yaml-cpp/yaml.h>

namespace mxutils
{

    /**
     * \struct
     *
     * yaml_result is used to return results by the yaml utility
     * functions. If the function succeeds, it sets the 'result' field
     * to 'true', the 'node' field to the node, and the 'key' field to
     * that node's key. On failure, it sets the 'result' field to
     * 'false', and optionally will set the 'key' field and 'node'
     * fields as appropriate.
     *
     * Example:
     *
     *     yaml_result r = has_key("components.nettask.source.URLs", config);
     *     cout << r.result << "\tkey = " << r.key << ", node = " << r.node << endl;
     *
     */

    struct yaml_result
    {
        yaml_result(bool r, YAML::Node n)
        {
            result = r;
            node = n;
            key = "";

        }

        bool result;      //!< Result of the function, 'true' is success
        std::string key;  //!< Last good key
        YAML::Node node;  //!< Last good node
    };

    yaml_result get_yaml_node(std::string key, YAML::Node node);

}

#endif
