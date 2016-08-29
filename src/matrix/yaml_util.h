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
        yaml_result(bool r = true, YAML::Node n = YAML::Node(), std::string k = "", std::string em = "")
        {
            result = r;
            node = n;
            key = k;
            err = em;
        }

        yaml_result(YAML::Node yr)
        {
            from_yaml_node(yr);
        }

        ~yaml_result()
        {
            node.reset();
        }

        yaml_result & operator=(yaml_result const &rhs)
        {
            result = rhs.result;
            node.reset();
            node = YAML::Clone(rhs.node);
            key = rhs.key;
            err = rhs.err;
            return *this;
        };

        YAML::Node to_yaml_node() const;
        void from_yaml_node(YAML::Node yr);

        bool result;      //!< Result of the function, 'true' is success
        std::string key;  //!< Last good key
        std::string err;  //!< any error message (from exception, etc.)
        YAML::Node node;  //!< Last good node

        friend std::ostream &operator<<(std::ostream &os, const yaml_result &yr);
    };

    yaml_result get_yaml_node(YAML::Node node, std::string keychain);
    yaml_result put_yaml_node(YAML::Node node, std::string keychain,
                              YAML::Node val,  bool create = false);
    yaml_result delete_yaml_node(YAML::Node node, std::string keychain);

/**
 * This template will insert a YAML::Node containing any type that is
 * supported by the YAML::Node() templated constructor. These are all
 * the types supported by YAML (POD types, vectors, maps).
 *
 * @param node: the root node to update
 *
 * @param keychain: the string of keys leading to the desired node
 *
 * @param val: the value to set
 *
 * @param create: if true, will create the needed node(s).
 *
 * @return `yaml_result`, set to 'true' on success, 'false' otherwise.
 *
 */

    template <typename T>
    yaml_result put_yaml_val(YAML::Node node, std::string keychain, T val, bool create = false)
    {
        return put_yaml_node(node, keychain, YAML::Node(val), create);
    }
}

#endif
