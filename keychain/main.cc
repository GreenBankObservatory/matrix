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

#include <sys/select.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <map>
#include <set>
#include <list>
#include <iostream>

using namespace std;
using namespace mxutils;

typedef void (*V_FP)(CmdParam &);

void add_cmd(string key, V_FP func);

void ls(CmdParam &);
void tree(CmdParam &);
void down(CmdParam &);
void up(CmdParam &);
void read(CmdParam &);
void write(CmdParam &);
void help(CmdParam &);

bool init(int argc, char *argv[]);
void run();
void sig_handler(int sig);
bool print_help(CmdParam &p, string help);

map<string, V_FP> cmds;
volatile bool quit = false;
CmdParam cmdline;
bool output = true;

YAML::Node root_node;
YAML::Node current_node;
Mutex node_mtx;
list<string> current_path;

shared_ptr<Keymaster> keymaster;

// Any updates on the keymaster will be reflected here
struct KeychainCallback : public KeymasterCallbackBase
{
private:
    void _call(string key, YAML::Node val)
    {
        ThreadLock<Mutex> l(node_mtx);
        l.lock();
        root_node = val;
    }
};

KeychainCallback km_cb;

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
 * Run loop for mcbcmd.  Uses the GNU readline library to provide
 *  command line services (edit and history.)
 *
 */

void run()

{
    V_FP f;
    map<string, V_FP>::iterator i;
    string Exit("exit"), Quit("quit");
    string in_buf;


    using_history();

    while (!quit)
    {
        in_buf = readline("km > ");

        if (in_buf[0] != '\0' && !quit)  // quit might be set, and waiting for <CR>
        {
            cmdline.new_list(in_buf);

            if (Exit == cmdline.cmd() || Quit == cmdline.cmd())
            {
                break;
            }

            if ((i = cmds.find(cmdline.cmd())) != cmds.end())
            {
                f = i->second;

                if (f)
                {
                    f(cmdline);
                }

                add_history(in_buf.c_str());
            }
            else
            {
                cout << cmdline.cmd() << ": command not found" << endl;
            }
        }
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
        keymaster->subscribe("Root", &km_cb);
        root_node = keymaster->get("Root");
        current_node = root_node;

        signal(SIGINT, sig_handler);
        signal(SIGTERM, sig_handler);

        add_cmd("ls", ls);
        add_cmd("tree", tree);
        add_cmd("down", down);
        add_cmd("up", up);
        add_cmd("read", read);
        add_cmd("write", write);
        add_cmd("help", help);
    }
    catch (KeymasterException e)
    {
        cerr << e.what() << endl;
        return false;
    }

    return true;

}

void ls(CmdParam &p)
{
    static string help = "ls\n"
        "\tlists the keys at the current level.\n";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();
    YAML::iterator yi;

    for (yi = current_node.begin(); yi != current_node.end(); ++yi)
    {
        cout << yi->first << endl;
    }
}

void tree(CmdParam &p)
{
    static string help = "tree\n"
        "\tprints the entire node at this level in tree form.\n";

    if (print_help(p, help))
    {
        return;
    }

    ThreadLock<Mutex> l(node_mtx);
    l.lock();
    cout << current_node << endl;
}

void down(CmdParam &p)
{
    static string help = "down <node_name>\n"
        "\tdescends into the named node.\n";

    if (print_help(p, help))
    {
        return;
    }

    if (p.count() == 0)
    {
        cout << "Usage: " << help << endl;
    }

    if (current_node.find(p[0]))
    {
        current_path.push_back(p[0]);
        fn_string_join fn(".");
        string keychain = fn(current_path);
        current_node = keymaster->get(keychain);
    }
}

void up(CmdParam &p)
{
    static string help = "up\n"
        "\tAscends the node tree one level.\n";

    if (print_help(p, help))
    {
        return;
    }
}

void read(CmdParam &p)
{
    static string help = "read <node_name>\n"
        "\tReads and prints the value of the named node.\n";

    if (print_help(p, help))
    {
        return;
    }
}

void write(CmdParam &p)
{
    static string help = "write <node_name> <YAML_value>\n"
        "\twrites to the named YAML node. The node must be\n"
        "a child of the current node.\n"
        "Example:\n"
        "\twrite foo {bar: cat, baz: dog}\n";

    if (print_help(p, help))
    {
        return;
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

// /**
//  * Same as monitor, but from an SIB context: SIB id and offset instead
//  *  of address.
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void read(CmdParam &p)

// {
//     unsigned short id, offs, data;
//     SIB *sib;
//     char const *help = "read <sib> <offs>\n"
//         "\tReads a value from offset 'offs' on SIB 'sib''.\n"
//         "\tThe value 'offs' is an unsigned 16-bit value. All parameters\n"
//         "\tare given as hexadecimal values.";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     if (p.Count() != 2)
//     {
//         cerr << "read requires 2 parameters: SIB id and offset, in hex" << endl;
//         return;
//     }

//     if (!convert(p[0], id, 16, true))
//     {
//         return;
//     }

//     if (!convert(p[1], offs, 16, true))
//     {
//         return;
//     }

//     try
//     {
//         if (sibs.find(id) == sibs.end())
//         {
//             sib =  new SIB(id);
//             sibs[id] = sib;
//         }
//         else
//         {
//             sib = sibs[id];
//         }

//         data = sib->read(offs);
//     }
//     catch (MCB::Exception e)
//     {
//         cerr << "read exception: " << e.what() << endl;
//         return;
//     }

//     if (output)
//     {
//         cout << hex << data << endl;
//     }

// }

// /**
//  * Writes data to a SIB offset.  Same as 'control()', but from the
//  *  SIB perspective.
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void write(CmdParam &p)

// {
//     unsigned short id, offs, data, mask = 0;
//     SIB *sib;
//     char const *help = "write <sib> <offs> <data>\n"
//         "\tWrites value 'data' to the offset 'offs' of SIB 'sib'.\n"
//         "\tThe values 'offs' and 'data' are both unsigned 16-bit\n"
//         "\tvalues. All parameters are given as hexadecimal values.";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     if (p.Count() < 3 || p.Count() > 4)
//     {
//         cerr << "write requires 3 or 4 parameters: SIB id, offset, data [, mask]" << endl;
//         return;
//     }

//     if (!convert(p[0], id, 16, true))
//     {
//         return;
//     }

//     if (!convert(p[1], offs, 16, true))
//     {
//         return;
//     }

//     if (!convert(p[2], data, 16, true))
//     {
//         return;
//     }

//     if (p.Count() == 4)
//     {
//         if (!convert(p[3], mask, 16, true))
//         {
//             return;
//         }
//     }

//     try
//     {
//         if (sibs.find(id) == sibs.end())
//         {
//             sib = new SIB(id);
//             sibs[id] = sib;
//         }
//         else
//         {
//             sib = sibs[id];
//         }

//         sib->write(offs, data, mask);
//     }
//     catch (MCB::Exception e)
//     {
//         cerr << "write exception: " << e.what() << endl;
//         return;
//     }

//     if (output)
//     {
//         cout << "OK" << endl;
//     }

// }

// /**
//  * Lists the MCB devices controlled on the host.
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void mcblist(CmdParam &p)

// {
//     set<String> mcb_set;
//     set<String>::iterator si;
//     char const *help = "mcblist\n"
//         "\tPrints a tree listing of all MCBs on the host.";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     try
//     {
//         MCBMap::Instance()->get_mcb_set(host_name.c_str(), mcb_set);
//         cout << host_name << ":" << endl;

//         for (si = mcb_set.begin(); si != mcb_set.end(); ++si)
//         {
//             cout << "\t|" << endl;
//             cout << "\t->" << *si << endl;
//         }
//     }
//     catch (MCB::Exception e)
//     {
//         cerr << "read exception: " << e.what() << endl;
//         return;
//     }

// }

// /**
//  * Same as mcblist, but also includes SIBs for each MCB
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void siblist(CmdParam &p)

// {
//     set<String> mcb_set;
//     set<String>::iterator si;
//     MCB *mcb;
//     MCB::sib_list sl;
//     MCB::sib_list::iterator sli;
//     char const *help = "siblist\n"
//         "\tPrints a tree listing of all SIBs, branching from the\n"
//         "\tMCBs supporting the SIBs.";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     try
//     {
// 	// get the MCBs on a host (/dev/ttyS0, /dev/ttyS1, etc.
//         MCBMap::Instance()->get_mcb_set(host_name.c_str(), mcb_set);
//         cout << host_name << ":" << endl;

// 	// for each MCB, do:
//         for (si = mcb_set.begin(); si != mcb_set.end(); ++si)
//         {
//             cout << "\t|" << endl;
//             cout << "\t->" << *si << endl;

// 	    // get the MCB object associated with that MCB name ("/dev/ttyS0", etc.)
//             mcb = MCBMap::Instance()->get_mcb_by_name(si->c_str(), host_name.c_str());
// 	    // get a list of the SIBs connected to that MCB
//             mcb->sibs(sl);

//             for (sli = sl.begin(); sli != sl.end(); ++sli)
//             {
//                 cout << "\t\t|" << endl;
//                 cout << "\t\t->id: " << hex << sli->id << " addr: " << sli->addr
//                      << " len: " << sli->len << endl;
//             }
//         }
//     }
//     catch (MCB::Exception e)
//     {
//         cerr << "read exception: " << e.what() << endl;
//         return;
//     }
// }

// /**
//  * Handler for the 'help' command.  Shows all commands.
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void help(CmdParam &p)

// {
//     char buf[100];
//     map<String, V_FP>::const_iterator mi;
//     char const *help = "help [cmdname]\n"
//                        "\tPrints out a list of all mcbcmd's commands.  If provided with\n"
//                        "\t'cmdname', prints out the help for that command.";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     if (p.Count() == 0)
//     {
//         for (mi = cmds.begin(); mi != cmds.end(); ++mi)
//         {
//             cout << mi->first << endl;
//         }

//         cout << endl;
//         cout << "type 'help <cmdname>' for help on that command." << endl;
//     }
//     else
//     {
//         V_FP f;
//         CmdParam cl(' ');
//         sprintf(buf, "%s %s", p[0], "help");
//         cl.NewList(buf);

//         if ((mi = cmds.find(cl.Cmd())) != cmds.end())
//         {
//             f = mi->second;

//             if (f)
//             {
//                 f(cl);
//             }
//         }
//         else
//         {
//             cout << cl.Cmd() << ": help for command not found" << endl;
//         }
//     }

// }

// /**
//  * Prints out the map of SIB name to number and MCB
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void sibmap(CmdParam &p)

// {
//     size_t max_name = 0, max_host = 0, max_port = 0;
//     size_t id_col, host_col, port_col, line_len;
//     char line[128], buf[32];
//     const char *sib_name = "Name", *sib_id = "ID";
//     const char *sib_host = "Host", *sib_port = "MCB";
//     set<MCBMap::SIBKeys> sibs;
//     set<MCBMap::SIBKeys>::iterator si;
//     char const *help = "map\n"
//         "\tPrints out a list of all SIBs and their mapping data.\n"
//         "\tFormat: <sibname> <id> <host> <mcb>.\n"
//         "\twhere:\t'sibname' is the name as found in SIBMap.conf;\n"
//         "\t\t'id' is the SIB ID, in hex;\n"
//         "\t\t'host' is the computer hosting the MCB;\n"
//         "\t\t'port' is the name of the MCB on that computer.";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     MCBMap::Instance()->get_sib_set(sibs);

//     for (si = sibs.begin(); si != sibs.end(); ++si)
//     {
//         if (si->name.length() > max_name)
//         {
//             max_name = si->name.length();
//         }

//         if (si->host.length() > max_host)
//         {
//             max_host = si->host.length();
//         }

//         if (si->port.length() > max_port)
//         {
//             max_port = si->port.length();
//         }
//     }

//     id_col = max_name + 2;
//     host_col = id_col + 4;
//     port_col = host_col + max_host + 2;
//     memset(line, 0x20, sizeof line);
//     memcpy(line, sib_name, strlen(sib_name));
//     memcpy(&line[id_col], sib_id, strlen(sib_id));
//     memcpy(&line[host_col], sib_host, strlen(sib_host));
//     sprintf(&line[port_col], sib_port);
//     cout << line << endl;
//     line_len = strlen(line) + max_port - 3;
//     memset(line, '-', line_len);
//     line[line_len] = 0;
//     cout << line << endl;

//     for (si = sibs.begin(); si != sibs.end(); ++si)
//     {
//         memset(line, 0x20, sizeof line);
//         memcpy(line, si->name.c_str(), si->name.length());
//         sprintf(buf, "%02hX", si->ID);
//         memcpy(&line[id_col], buf, strlen(buf));
//         memcpy(&line[host_col], si->host.c_str(), si->host.length());
//         sprintf(&line[port_col], si->port.c_str());
//         cout << line << endl;
//     }
// }

// /**
//  * Runs the given command a specified number of times, returning timing
//  *  stats
//  *
//  * @param CmdPram &p: The command and parameters that the user typed
//  *        in.
//  *
//  */

// void repeat(CmdParam &p)

// {
//     unsigned int k, n;
//     CmdParam mycmd(' ');
//     V_FP f;
//     TimeStamp start, end;
//     map<String, V_FP>::iterator i;
//     char const *help = "repeat n \"<cmd> ...\"\n"
//         "\tAssumes the first parameter given (n) is the number of times to\n"
//         "\trepeat the following command line.  For example:\n"
//         "\trepeat 10 read 7 FF";


//     if (print_help(p, help))
//     {
//         return;
//     }

//     if (p.Count() != 2)
//     {
//         cerr << "repeat requires at least 2 parameters: iterations and command string" << endl;
//         return;
//     }

//     if (!convert(p[0], n, 10, true))
//     {
//         return;
//     }

//     mycmd.NewList(p[1]);
//     start = getTimeOfDay();

//     if ((i = cmds.find(mycmd.Cmd())) != cmds.end())
//     {
//         f = i->second;

//         if (!f)
//         {
//             cout << "Command " << mycmd.Cmd() << " not found.  Aborting..." << endl;
//             return;
//         }
//     }

//     output = false;

//     for (k = 0; k < n; ++k)
//     {
//         f(mycmd);
//     }

//     output = true;
//     end = getTimeOfDay();
//     cout << mycmd.Cmd() << " executed in " << (end - start).Sec() / double(n) << " seconds" << endl;

// }

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
