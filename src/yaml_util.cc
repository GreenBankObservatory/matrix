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

#include "matrix/yaml_util.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <iostream>

using namespace std;

namespace mxutils
{

/**
 * Returns a YAML::Node representation of the `yaml_result` object.
 *
 * @return A YAML::Node. The node portion of the `yaml_result` is a
 * clone (deep copy) of the `yaml_result`'s Node object.
 *
 */

    YAML::Node yaml_result::to_yaml_node() const
    {
        YAML::Node n;

        n["result"] = result;
        n["key"] = key;
        n["err"] = err;
        n["node"] = YAML::Clone(node);
        return n;
    }

/**
 * Updates the `yaml_result` object from a YAML::Node representation.
 *
 * @param n: A YAML::Node representation of a yaml_result object.
 *
 */

    void yaml_result::from_yaml_node(YAML::Node n)
    {
        result = n["result"].as<bool>();
        key = n["key"].as<string>();
        err = n["err"].as<string>();
        node = YAML::Clone(n["node"]);
    }

/**
 * Outputs a `yaml_result` to stream.
 *
 * @param os: the stream.
 *
 * @param yr: the `yaml_result` object.
 *
 * @return returns the `ostream` stream.
 *
 */

    ostream & operator <<(ostream &os, const yaml_result &yr)
    {
        YAML::Node n = yr.to_yaml_node();
        os << endl << n << endl;
        n.reset();
        return os;
    }

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
 * This static helper sets up and returns a `yaml_result` based on the
 * given parameters. It will set the keychain to the last good one, and
 * provide the last known good node based on that keychain.
 *
 * @param keys: The complete vector of keys based on the keychain given
 * to one of the get/set_yaml_node functions. This will provide the
 * basis for the known good keys.
 *
 * @param ns: The vector of YAML::Nodes aliased to the starting
 * node. The first element is the starting node, and each subsequent
 * entry is the node returned by using the key on the previous node. The
 * last entry therefore is the last known good node.
 *
 * @param res: A boolean, the result. If true, the requested/updated node will
 * be found in the returned `yaml_result` object. If false, the last
 * good node will be found there.
 *
 * @param msg: An optional message (for example, an exception's 'what()'
 * message). Will be added to the `yaml_result` err member.
 *
 * @return A `yaml_result` object filled in appropriately.
 *
 */

    static yaml_result set_yaml_result(vector<string> keys, vector<YAML::Node> ns, bool res, string msg = "")
    {
        size_t i = ns.size() - 1;
        ostringstream m;
        string ms;

        if (!res)
        {
            m << "No such key: " << keys[i];

            if (!msg.empty())
            {
                m << "; " << msg;
            }

            ms = m.str();
        }

        vector<string> new_keychain(keys.begin(), keys.begin() + i);
        string last_good_key = boost::algorithm::join(new_keychain, ".");
        return yaml_result(res, YAML::Clone(ns.back()), last_good_key, ms);
    }

/**
 * This helper walks down the node tree following the branches named in the provided
 * keys. It will optionally create new branches from the point that the
 * first key is not found (useful for adding new nodes).
 *
 * @param keys: The node keys to follow
 *
 * @param nodes: As the keys are checked, the corresponding nodes are
 * added to this vector. _It must first be seeded with the starting node
 * before being given to this function_. This method of using a vector
 * to keep the node copies is used because of how operator =() works for
 * YAML::Nodes, by aliasing them. Thus if you have `YAML::Node root`,
 * and you say `YAML::Node x = root["foo"]`, `x` points as expected to
 * the Node whose key is 'foo'. But if you reuse `x`, such as `x =
 * x["bar"]`, thinking you're walking `x` down the tree, you actually
 * wipe out the previous contents of `x`, instead of just moving it to
 * the next node. By pushing back a new copy of the returned nodes onto
 * a vector we avoid this possibility. And the node we want is easy to
 * get at, it is always `nodes.back()`.
 *
 * @param create: If this flag is set to 'true', rather than return
 * 'false' on not finding a node that corresponds to a key it will
 * create a new one and assign it to that key.
 *
 * @return A boolean 'true' if it was able to walk down all the keys
 * successfully, 'false' otherwise.
 *
 */

    static bool walk_the_nodes(vector<string> &keys, vector<YAML::Node> &nodes, bool create)
    {
        size_t i = 0;

        if (nodes.empty() || keys.empty())
        {
            return false;
        }

        for (i = 0; i < keys.size(); ++i)
        {
            if (!nodes.back()[keys[i]])
            {
                if (create)
                {
                    nodes.back()[keys[i]] = YAML::Node();
                }
                else
                {
                    return false;
                }
            }

            nodes.push_back(nodes.back()[keys[i]]);
        }

        return true;
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
 * "source" (the last-known-good key), and `r.node` will be the "source"
 * node (the last-known-good node).
 *
 * @param node: The initial node to be searched for the keys.
 *
 * @param keychain: A std::string containing a period-separated list of
 *                  the keys to be searched.
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

    yaml_result get_yaml_node(YAML::Node node, std::string keychain)
    {
        vector<string> keys;
        vector<YAML::Node> nodes;

        try
        {
            if (keychain.empty())
            {
                yaml_result r(true, node, "");
                return r;
            }

            boost::split(keys, keychain, boost::is_any_of("."));
            nodes.push_back(node);
            bool rval = walk_the_nodes(keys, nodes, false);
            return set_yaml_result(keys, nodes, rval);
        }
        catch (YAML::BadSubscript &e)
        {
            return set_yaml_result(keys, nodes, false, e.what());
        }
    }

/**
 * Puts a new value at the node corresponding to the end of the
 * keychain. Optionally will create any number of intervening nodes if
 * the key does not yet exist.
 *
 * example:
 *
 *      YAML::Node v = YAML::Load("1111");
 *      yaml_result r = put_yaml_node(node, "components.foocomponent.ID", v, true);
 *
 * This sets the value at 'components.foocomponent.ID' to 1111. If
 * 'ID'--or any other node up the chain--does not exist, it will be
 * created because the `create` flag is `true`. If the `create` flag is
 * `false` (or omitted), then the function will fail if 'ID' or any key
 * upstream does not exist.
 *
 * @param node: root YAML::Node
 *
 * @param keychain: The string of keys that point to the desired node.
 *
 * @param val: The new value for the node indicated by `keychain`
 *
 * @param create: If this flag is set, then the function will create one
 * or more new nodes, if needed, to satisfy the `keychain`.
 *
 * @return A `yaml_result`, set to true with the newly set node if this
 * succeeded, or false with the last good node and key, and an error
 * message, if it fails.
 *
 */

    yaml_result put_yaml_node(YAML::Node node, std::string keychain, YAML::Node val, bool create)
    {
        vector<YAML::Node> nodes;
        vector<string> keys;

        try
        {
            // if key == "" we want the top-level node. Just replace
            // 'node' with 'val'.
            if (keychain.empty())
            {
                node = val;
                nodes.push_back(node);
                keys.push_back("");
                return set_yaml_result(keys, nodes, true);
            }

            boost::split(keys, keychain, boost::is_any_of("."));
            nodes.push_back(node); // node[keys[0]]);
            bool rval = walk_the_nodes(keys, nodes, create);

            if (rval)
            {
                nodes.back() = val;
            }

            return set_yaml_result(keys, nodes, rval);
        }
        catch (YAML::BadSubscript &e)
        {
            return set_yaml_result(keys, nodes, false, e.what());
        }
    }

/**
 * Deletes the node specified by `keychain` from the given node `node`.
 *
 * example:
 *
 *     yaml_result r = delete_yaml_node(node, "components.bar");
 *
 * Here, the value denoted by "bar" will be deleted, if it exists.
 *
 * @param node: the YAML::Node to delete the key and its value from.
 *
 * @param keychain: The string of keys, delimited by a period (".") that
 * leads to the desired key/value pair to delete.
 *
 * @return a `yaml_result` with the field `result` set to true if the
 * delete succeeded, `false` otherwise. The `node` field will be set to
 * the previous node up if the delete succeeded, or to the last known
 * good node if the delete failed (because the key doesn't exist). The
 * `key` field will contain the last valid key, and the `err` field will
 * contain an error message as appropriate.
 *
 */

    yaml_result delete_yaml_node(YAML::Node node, std::string keychain)
    {
        vector<string> keys;
        vector<YAML::Node> nodes;

        try
        {
            boost::split(keys, keychain, boost::is_any_of("."));
            nodes.push_back(node);
            bool rval = walk_the_nodes(keys, nodes, false);

            if (rval)
            {
                int k = nodes.size() - 2;
                nodes[k].remove(keys.back());
                nodes.pop_back();
            }

            return set_yaml_result(keys, nodes, rval);
        }
        catch (YAML::BadSubscript &e)
        {
            return set_yaml_result(keys, nodes, false, e.what());
        }
    }

}
