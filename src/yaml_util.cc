/*******************************************************************
 *  yaml_utils.cc - implementation of YAML helper utilities
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

#include "yaml_util.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

using namespace std;

namespace mxutils
{

/**
 * Given an integer representing the YAML types (NodeType), returns
 * a string with the name of the type.
 *
 * @param t: The type number: { Undefined, Null, Scalar, Sequence,
 * Map }
 */

    string yaml_type_name(int t)
    {
        switch (t)
        {
        case YAML::NodeType::Undefined:
            return "Undefined";
        case YAML::NodeType::Null:
            return "Null";
        case YAML::NodeType::Scalar:
            return "Scalar";
        case YAML::NodeType::Sequence:
            return "Sequence";
        case YAML::NodeType::Map:
            return "Map";
        default:
            return "Unknown Type";
        }
    }

/**
 * Given a string representing a hierarchy of keys separated by periods
 * (i.e. "foo.bar.baz" if 'bar' is a key under 'foo', and 'baz' is a key
 * under 'bar'), and a YAML::Node to use the keys on, returns a value
 * indicating if the keychain is valid, and a node represented by that
 * keychain.
 *
 * Example:
 *
 *     yaml_result r = get_yaml_node("components.nettask.source.URLs", config);
 *     cout << r.result << "\t" << r.node << endl;
 *
 * If this succeeds, `r.result` will be 'true', `r.key` will be "URLs",
 * and `r.node` will be the URLs node. Suppose however it fails at
 * "URLs". In this case `r.result` will be 'false', `r.key` will be
 * "source", and `r.node` will be the "source" node.
 *
 * @param keychain: A std::string containing a period-separated list of
 *                  the keys to be searched.
 *
 * @param node: The initial node to be searched for the keys.
 *
 * @return Returns a `yaml_result` struct. The contents of the struct
 * will depend on whether the key was valid in its entirety. If so, the
 * struct's `result` field will be 'true'. If not, it will be
 * 'false'. If 'false', the `key` field of the `yaml_result` will
 * contain the last good key, and the `node` field of the `yaml_result` will
 * contain the last valid node, or the original `node` parameter if none
 * of the keys were valid.
 *
 */

    yaml_result get_yaml_node(std::string keychain, YAML::Node node)
    {
        yaml_result rval(true, node);
        vector<string> keys;
        boost::split(keys, keychain, boost::is_any_of("."));

        for (vector<string>::const_iterator i = keys.begin(); i != keys.end(); ++i)
        {
            if (rval.node[*i])
            {
                rval.node = rval.node[*i];
                rval.key = *i;
            }
            else
            {
                rval.result = false;
                break;
            }
        }

        return rval;
    }

}
