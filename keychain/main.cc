/*******************************************************************
 ** main.cc - Main program entry point for 'keychain'.
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

#include "cmdparam.h"
#include "Mutex.h"
#include "ThreadLock.h"
#include "Keymaster.h"
#include "zmq_util.h"
#include "matrix_util.h"

#include <sys/select.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <map>
#include <set>
#include <list>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <iomanip>

using namespace std;
using namespace mxutils;
using namespace std::placeholders;

typedef void (*V_FP)(CmdParam &);

void add_cmd(string key, V_FP func);

void ls(CmdParam &);
void tree(CmdParam &);
void cd(CmdParam &);
void pwd(CmdParam &);
void read(CmdParam &);
void write(CmdParam &);
void new_node(CmdParam &);
void del_node(CmdParam &);
void help(CmdParam &);

bool init(int argc, char *argv[]);
void run();
void sig_handler(int sig);
bool print_help(CmdParam &p, string help);

map<string, V_FP> cmds;
volatile bool quit = false;
CmdParam cmdline;
bool output = true;

YAML::Node current_node;
Mutex node_mtx;
vector<string> current_path = {""};
vector<string> yaml_type_names =
{
    "Undefined",
    "Null",
    "Scalar",
    "Sequence",
    "Map"
};

shared_ptr<Keymaster> keymaster;

// Any updates on the keymaster will be reflected here
struct KeychainCallback : public KeymasterCallbackBase
{
private:
    void _call(string key, YAML::Node val)
    {
        ThreadLock<Mutex> l(node_mtx);
        l.lock();
        current_node = YAML::Clone(val);
    }
};

KeychainCallback km_cb;

/**
 * Returns a keychain from a vector of keys, according to the following
 * rules:
 *
 *   * if vector cp is empty, return ""
 *   * if cp.size() == 1 return cp.front()
 *   * if cp.size() > 1
 *      * if cp.front().empty(), skip the front, join the rest on '.'
 *      * otherwise join all of cp on '.'
 *
 * The idea is to ensure that the keychain will always be "", or "key",
 * or "key1.key2.key3" etc. and never have a leading period.
 *
 * @param cp: the vector of keys with which to create the keychain
 *
 * @return a keychain string as described above.
 *
 */

string key_from(vector<string> &cp)
{
    if (cp.size() == 0)
    {
        return "";
    }
    else if (cp.size() == 1)
    {
        return cp.front();
    }
    else
    {
        fn_string_join fn(".");

        if (cp.front().empty())
        {
            return fn(vector<string>(cp.begin() + 1, cp.end()));
        }
        else
        {
            return fn(cp);
        }
    }
}

/**
 * For key completion. Any string given to readline will be deleted by readline.
 *
 * @param char *s: The string to copy
 *
 * @return a new string allocated on the heap.
 *
 */

static char *dupstr(char *s)
{
    char *r;

    r = (char *)malloc(strlen(s) + 1);
    strcpy (r, s);
    return (r);
}

/**
 * Generator function for key completion. The goal is to autocomplete on
 * a key at the current level, and if a period '.' is given in that key
 * to decend to the next level (if that key is good) and autocomplete
 * there on a new set of keys, etc.
 *
 * @param state: indicates whether to start from scratch. When 0, we
 * start fresh by generating a list of possible completions at this
 * level.
 *
 * @param text: the text to consider.
 *
 * @return returns a char * to an allocated string if there is a match,
 * or NULL otherwise.
 *
 */

static char * key_generator(const char *text, int state)
{
    static vector<string> possibles, keys;
    static int list_index, parts = 0;
    string name, entered(text);
    int new_count;

    // starting from scratch:
    if (!state)
    {
        list_index = 0;
        parts = 0;
        possibles.clear();
        keys.clear();
        YAML::iterator i;

        for (i = current_node.begin(); i != current_node.end(); ++i)
        {
            ostringstream str;
            str << i->first;
            possibles.push_back(str.str());
        }

        possibles.push_back(string("")); // term.
    }

    auto fn_equal_to = bind(equal_to<char>(), _1, '.');

    // the given key has a period in it. Split the key, saving all but
    // the last part in 'keys'. The last part is what we need to
    // autocomplete on.
    if (entered.find(".") != string::npos)
    {
        keys.clear();
        boost::split(keys, entered, boost::is_any_of("."));
        entered = keys.back();
        keys.pop_back();
        string s(text); // use just to count dots '.'

        // Check if the count has changed. If so, another key has been
        // added to the keychain, so decend into the next level and
        // compile a new list of possible completions
        if ((new_count = count_if(s.begin(), s.end(), fn_equal_to)) > parts)
        {
            parts = new_count;
            list_index = 0;
            yaml_result yr = get_yaml_node(current_node, key_from(keys));

            if (yr.result)
            {
                YAML::iterator i;
                possibles.clear();
                list_index = 0;

                for (i = yr.node.begin(); i != yr.node.end(); ++i)
                {
                    ostringstream str;
                    str << i->first;
                    possibles.push_back(str.str());
                }

                possibles.push_back(string(""));
            }
            else
            {
                return (char *)NULL;
            }
        }
    }

    // look for the last component of the keychain.
    while (!(name = possibles[list_index]).empty())
    {
        list_index++;

        // if found, assemble the entire keychain and return.
        if (name.find(entered) != string::npos)
        {
            keys.push_back(name);
            string complete = key_from(keys);
            return dupstr((char *)complete.c_str());
        }
    }

    return (char *)NULL;
}

/**
 * Handles the key completion for readline.
 *
 * @param text: the given text
 *
 * @param start: the column # on the line where this text starts
 *
 * @param end: the column # on the line where this text ends
 *
 * @return A list of possible matches.
 *
 */

static char **key_completion(const char *text , int start,  int end)
{
    char **matches;
    matches = (char **)NULL;

    if (start > 1)
    {
        matches = rl_completion_matches((char*)text, &key_generator);
    }
    else
    {
        rl_bind_key('\t', rl_abort);
    }

    return matches;
}

/**
 * Changes the key level. When the utility starts, the level is 'Root',
 * at the top. When the user wishes to go down [cd key1.key2] this
 * routine navigates 'current_node' down to the new level, recording the
 * current path "key1.key2", and requesting a new current_node from the
 * keymaster at this level. It also subscribes the new path from the
 * keymaster, so that any changes to the node at this new level is
 * automatically recorded.
 *
 * If a path is given with a leading period, i.e. ".foo.bar", the
 * command will attempt to switch to an absolute path "foo.bar" based on
 * "Root". If not, the command assumes this is a relative path.
 *
 * If ".." is given, the command will move up one level.
 *
 * @param level: The new desired level. "key" attempts to move down to
 * "key" from the current level. ".." moves up one level (unless already
 * at the top). "key1.key2" attempts to move to "key2" of the node at
 * "key1" in this level. ".key1.key2" attempts to move to the node at
 * "key2" in the node at "key1" from the top level.
 *
 */

void change_level(string level)
{
    string old_key;
    string new_key;

    old_key = key_from(current_path);

    // go up one level
    if (level == "..")
    {
        if (!old_key.empty()) // not at the top
        {
            keymaster->unsubscribe(old_key);
            current_path.pop_back();
            new_key = key_from(current_path);
            keymaster->subscribe(new_key, &km_cb);
            current_node = keymaster->get(new_key);
        }
        else
        {
            cout << "Already at top." << endl;
        }
    }
    else if (level.at(0) == '.') // absolute path
    {
        YAML::Node new_node;
        vector<string> nks;

        // a boost::split on "." would produce nks == {"", ""}, so
        // we must catch this and fix it up.
        if (level == ".")
        {
            nks = {""};
        }
        else
        {
            boost::split(nks, level, boost::is_any_of("."));
        }

        new_key = key_from(nks);
        new_node = keymaster->get(new_key);
        keymaster->subscribe(new_key, &km_cb);
        // now that all throwable keymaster stuff is done, make the
        // switch:
        current_path = nks;
        current_node = new_node;
    }
    else // relative path from here
    {
        yaml_result yr = get_yaml_node(current_node, level);

        if (yr.result == true)
        {
            current_node = YAML::Clone(yr.node);
            vector<string> nks;
            boost::split(nks, level, boost::is_any_of("."));
            current_path.insert(current_path.end(), nks.begin(), nks.end());
            new_key = key_from(current_path);
            keymaster->subscribe(new_key, &km_cb);
        }
        else
        {
            cout << "Could not switch to " << level << endl;
            cout << yr.err << endl;
        }
    }

    cout << "." << key_from(current_path) << endl;
}

/**
 * Program entry point.  Initializes and runs the program.
 *
 * @param int argc: Number of arguments
 * @param char *argv[]: The parameters.  1st param is the host that
 *                      is running MCBServer.
 *
 * @return 0 if all went well.
 *
 */

int main(int argc, char *argv[])

{
    if (!init(argc, argv))
    {
        return -1;
    }

    run();

    return 0;

}

/**
 * Run loop.  Uses the GNU readline library to provide command line
 *  services (edit, history, and tab-completion of keys)
 *
 */

void run()

{
    V_FP f;
    map<string, V_FP>::iterator i;
    string Exit("exit"), Quit("quit");
    char *line;


    rl_attempted_completion_function = key_completion;
    using_history();

    while (!quit)
    {
        string prompt = "-- ~." + key_from(current_path) + "~\n$ ";

        line = readline(prompt.c_str());

        if (line[0] != '\0' && !quit)  // quit might be set, and waiting for <CR>
        {
            rl_bind_key('\t', rl_complete);
            cmdline.new_list(line);

            if (Exit == cmdline.cmd() || Quit == cmdline.cmd())
            {
                break;
            }

            if ((i = cmds.find(cmdline.cmd())) != cmds.end())
            {
                f = i->second;

                if (f)
                {
                    try
                    {
                        f(cmdline);
                    }
                    catch (KeymasterException e)
                    {
                        cout << cmdline.cmd() << ": " << e.what() << endl;
                    }
                    catch (runtime_error e)
                    {
                        if (cmdline.cmd() == "read")
                        {
                            // TBF. Read doesn't work very well. May
                            // remove it.
                            f = cmds["tree"];
                            f(cmdline);
                        }
                        else
                        {
                            cout << cmdline.cmd() << ": " << e.what() << endl;
                        }
                    }
                }

                add_history(line);
            }
            else
            {
                cout << cmdline.cmd() << ": command not found" << endl;
            }
        }

        free(line);
    }
}

/**
 * Adds key/value pairs to the command map.  Each key represents a
 *  command that the user can type into the command line, and the
 *  value is a pointer to the function used to handle this command.
 *
 * @param const char *key: The key.
 * @param V_FP func: A pointer to the key handler.
 *        V_FP is void(*)(CmdParam &).
 *
 */

void add_cmd(string key, V_FP func)

{
    cmds[key] = func;

}

/**
 * Performs all startup operations here.
 *
 */

bool init(int argc, char *argv[])

{
    if (argc != 2)
    {
        cerr << "Need a URL to the Keymaster server!" << endl
             << "example:" << endl << "\ttcp://ajax.gb.nrao.edu:42000"
             << endl << "or" << endl << "\tipc://matrix.keymaster" << endl;
        return false;
    }

    try
    {
        string url = process_zmq_urn(argv[1]);

        if (url != argv[1]) // must be exact
        {
            return false;
        }

        keymaster.reset(new Keymaster(url));
        vector<string> km_pub_urls = keymaster->get_as<vector<string> >("Keymaster.URLS.AsConfigured.Pub");

        output_vector(km_pub_urls, cout);
        cout << endl;

        keymaster->subscribe("Root", &km_cb);
        current_node = keymaster->get("Root");

        signal(SIGINT, sig_handler);
        signal(SIGTERM, sig_handler);

        add_cmd("ls", ls);
        add_cmd("dir", ls);     // alias
        add_cmd("tree", tree);
        add_cmd("cd", cd);
        add_cmd("pwd", pwd);
        add_cmd("read", read);
        add_cmd("write", write);
        add_cmd("new", new_node);
        add_cmd("del", del_node);
        add_cmd("rm", del_node);
        add_cmd("help", help);
    }
    catch (KeymasterException e)
    {
        cerr << e.what() << endl;
        return false;
    }

    return true;

}

/**
 * Lists all the nodes at the current level, or at the provided key down
 * from the current level. ("ls .." is not supported.)
 *
 * @param p: The parameter object. If there is a parameter, use it as
 * the key to list.
 *
 */

void ls(CmdParam &p)
{
    static string help = "ls\n"
        "\tlists the key names at the current level, or at the given key:\n"
        "usage:\n"
        "\tls\n"
        "\tls <key>";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();
    YAML::iterator yi;
    YAML::Node n;

    if (p.count() > 0)
    {
        n = current_node[p[0]];
    }
    else
    {
        n = current_node;
    }

    if (n.Type() == YAML::NodeType::Map)
    {
        cout << setw(15) << left << "Type:" << setw(50) << "Name:" << endl;

        for (yi = n.begin(); yi != n.end(); ++yi)
        {
            ostringstream str;
            str << yi->first;

            cout << "  " << setw(15) << left << yaml_type_names[yi->second.Type()]
                 << setw(50) << str.str() << endl;
        }
    }
    else
    {
        cout << "Not a map." << endl;
    }
}

/**
 * Outputs the entire contents of the node in YAML format
 *
 * @param p: The command's parameters. If there is one, it is assumed to
 * be a key to the node to be output.
 *
 */

void tree(CmdParam &p)
{
    static string help = "tree\n"
        "\tprints the entire node at this level or at the level specified\n"
        "\tby the given key in tree form.\n"
        "usage:\n"
        "\ttree\n"
        "\ttree <key>\n";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();

    if (p.count() > 0)
    {
        cout << current_node[p[0]] << endl;
    }
    else
    {
        cout << current_node << endl;
    }
}

/**
 * Changes level. May go to any node, relative or absolute, or up one
 * step at a time.
 *
 * @param p: A parameter must be supplied, which is a relative or
 * absolute key, or ".." to indicate that it is to move up one level.
 *
 */

void cd(CmdParam &p)
{
    static string help = "cd <..>|<node_name>\n"
        "\tchanges into the named node, ascending if parameter is '..'.\n"
        "usage:\n"
        "\tcd .key # moves to absolute level 'key' at top level\n"
        "\tcd key  # moves to level 'key' relative from current location\n"
        "\tcd ..   # moves up one level\n";

    if (print_help(p, help))
    {
        return;
    }

    if (p.count() == 0)
    {
        cout << "Usage: " << help << endl;
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();
    change_level(p[0]);
}

/**
 * Outputs the current level.
 *
 * @param p: not used.
 *
 */

void pwd(CmdParam &p)
{
    static string help = "pwd\n"
        "\treports the current node level.";

    if (print_help(p, help))
    {
        return;
    }

    if (p.count() != 0)
    {
        cout << "Usage: " << help << endl;
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();

    cout << "." << key_from(current_path) << endl;
}

/**
 * Reads the current or specified node (relative to current), printing
 * out the node type and values of the node and any subordinate nodes.
 *
 * @param p: If it contains a parameter this is assumed to be a key to a
 * node to be read. Otherwise reads the current node.
 *
 */

void read(CmdParam &p)
{
    static string help = "read <node_name>\n"
        "\tReads and prints the value of the named node.\n";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();

    YAML::Node n;

    if (p.count() == 0)
    {
        n = current_node;
    }
    else
    {
        n = current_node[p[0]];
    }

    if (n)
    {
        string str_val, space(2, ' ');
        vector<string> str_seq;
        map<string, string> str_map;

        switch (n.Type())
        {
        case YAML::NodeType::Map:
            cout << space << "type: NodeType::Map" << endl;
            cout << space << n << endl;
            break;

        case YAML::NodeType::Sequence:
            cout << space << "type: NodeType::Sequence" << endl;
            str_seq = n.as<vector<string> >();
            cout << space << "value: ";
            output_vector(str_seq, cout);
            cout << endl;
            break;

        case YAML::NodeType::Scalar:
            cout << space << "type: NodeType::Scalar" << endl;
            cout << space << "value: " << n.as<string>() << endl;
            break;

        case YAML::NodeType::Null:
            cout << space << "Type is Null!" << endl;
            break;

        case YAML::NodeType::Undefined:
            cout << space << "Type is Undefined!" << endl;
            break;

        default:
            cout << space << "What?!?" << endl;
        }
    }
    else
    {
        cout << "No such node: " << p[0] << endl;
    }

}

void write(CmdParam &p)
{
    static string help = "write [node_name] <YAML_value>\n"
        "\twrites to the named YAML node. If 'node_name' is specified the node must already\n"
        "\texist and must be a child of the current node. If 'node_name' is not specified\n"
        "\t'YAML_value' will be written to the current node.\n"
        "\n\tThe 'YAML_value' must be a single-line ASCII representation of the value. Any\n"
        "\trepresentation that requires spaces should be enclosed in double quotes:\n"
        "\n\t  Scalar: 5, 43.2, frog, \"the quick brown fox\"\n"
        "\tSequence: \"[value, value, value, ...]\" where each value may be a Scalar,\n"
        "\t          another Sequence, or a Map.\n"
        "\t     Map: \"{key: value, key: value, ...}\". 'value' may be a Scalar, Sequence,\n"
        "\t          or another Map.\n"
       "Example:\n"
        "\twrite foo \"{bar: cat, baz: dog, quux: [1, 2, 3]}\"\n";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();

    yaml_result yr;
    string key, val;
    YAML::Node n;
    YAML::Node new_current_node = YAML::Clone(current_node);

    if (p.count() == 2) // key and val, current_node[key] = val
    {
        key = p[0];
        val = p[1];
    }
    else if (p.count() == 1) // only val given, current_node = val
    {
        key = "";
        val = p[0];
    }
    else
    {
        cout << help << endl;
    }

    n = YAML::Load(val);
    yr = put_yaml_node(new_current_node, key, n);

    if (yr.result)
    {
        keymaster->put(key_from(current_path), new_current_node);
        // current_node will be updated by the callback when the
        // Keymaster publishes the changes.
    }
    else
    {
        cout << p.cmd() << " " << key << " " << val << " failed: " << yr.err << endl;
    }
}

void new_node(CmdParam &p)
{
    static string help = "new <node_name> <YAML_value>\n"
        "\tcreates the named YAML node. The node will be a child of the current node.\n"
        "\n\tThe 'YAML_value' must be a single-line ASCII representation of the value. Any\n"
        "\trepresentation that requires spaces should be enclosed in double quotes:\n"
        "\n\t  Scalar: 5, 43.2, frog, \"the quick brown fox\"\n"
        "\tSequence: \"[value, value, value, ...]\" where each value may be a Scalar,\n"
        "\t          another Sequence, or a Map.\n"
        "\t     Map: \"{key: value, key: value, ...}\". 'value' may be a Scalar, Sequence,\n"
        "\t          or another Map.\n"
       "Example:\n"
        "\tnew foo \"{bar: cat, baz: dog, quux: [1, 2, 3]}\"\n";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();

    if (p.count() == 2) // key and val, current_node[key] = val
    {
        yaml_result yr;
        YAML::Node n = YAML::Load(p[1]);
        YAML::Node new_current_node = YAML::Clone(current_node);
        yr = put_yaml_node(new_current_node, p[0], n, true);

        if (yr.result)
        {
            keymaster->put(key_from(current_path), new_current_node, true);
        }
        else
        {
            cout << p.cmd_str() << " failed:\n\t" << yr.err
                 << endl << "Parent node must not be Scalar." << endl;
        }
    }
    else
    {
        cout << help << endl;
    }
}

void del_node(CmdParam &p)
{
    static string help = "del <node_name>\n"
        "\tdeletes the named YAML node. The node must be\n"
        "\ta child of the current node.\n"
        "Example:\n"
        "\tdel foo\n";

    if (print_help(p, help))
    {
        return;
    }

    if (p.count() != 1)
    {
        cout << "Need a node key to delete!\nUsage: " << help << endl;
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();

    string key = p[0];

    YAML::Node new_current_node = YAML::Clone(current_node);
    yaml_result yr = delete_yaml_node(new_current_node, key);

    if (yr.result)
    {
        keymaster->put(key_from(current_path), new_current_node);
        // current_node will be updated by the callback if the put succeeds.
    }
    else
    {
        cout << "Error deleting key " << key << ": " << yr.err << endl;
    }
}

/**
 * Handler for the 'help' command.  Shows all commands.
 *
 * @param CmdPram &p: The command and parameters that the user typed
 *        in.
 *
 */

void help(CmdParam &p)

{
    ostringstream cmd_line;
    map<string, V_FP>::const_iterator mi;
    char const *help = "help [cmdname]\n"
                       "\tPrints out a list of all mcbcmd's commands.  If provided with\n"
                       "\t'cmdname', prints out the help for that command.";


    if (print_help(p, help))
    {
        return;
    }

    if (p.count() == 0)
    {
        for (mi = cmds.begin(); mi != cmds.end(); ++mi)
        {
            cout << mi->first << endl;
        }

        cout << endl;
        cout << "type 'help <cmdname>' for help on that command." << endl;
    }
    else
    {
        V_FP f;
        CmdParam cl;
        cmd_line << p[0] << " help";

        cl.new_list(cmd_line.str());

        if ((mi = cmds.find(cl.cmd())) != cmds.end())
        {
            f = mi->second;

            if (f)
            {
                f(cl);
            }
        }
        else
        {
            cout << cl.cmd() << ": help for command not found" << endl;
        }
    }
}

/**
 * Prints a function's help string if requested.
 *
 * @param CmdParam &p: Used to see if help is requested.
 * @param char const *help: The help message.
 *
 * @return true if help printed, false otherwise.
 *
 */

bool print_help(CmdParam &p, string help)

{
    if (p.count() != 0)
    {
        if (p[0] == "help")
        {
            cout << help << endl;
            return true;
        }
    }

    return false;

}

/**
 * Handler to catch Ctrl-C input from the user.  Sets 'quit' to 'true',
 *  allowing 'run()' to exit its run loop.
 *
 * @param int sig: The signal received from the OS
 *
 */

void sig_handler(int /* sig */)

{
    quit = true;
    cout << "Press 'Enter' to exit" << endl;

}
