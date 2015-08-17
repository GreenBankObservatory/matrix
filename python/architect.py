######################################################################
#  architect.py - A Python Matrix Architect proxy. Performs the same
#  functions as the C++ Architect, except for creating components,
#  which are assumed to already exist, and controlling the lifetime of
#  the program.
#
#  Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Correspondence concerning GBT software should be addressed as follows:
#  GBT Operations
#  National Radio Astronomy Observatory
#  P. O. Box 2
#  Green Bank, WV 24944-0002 USA
#
######################################################################

import zmq
import keymaster
import Queue
import time
import weakref

class Architect(object):

    def __init__(self, name, km_url, context = None):
        self._keymaster = keymaster.Keymaster(km_url, context)
        self._name = name
        self._timeout = 4.0
        self._components = self._keymaster.get('components')
        self._architect = self._keymaster.get('architect')
        self.ArchitectCallback('components')
        self.ArchitectCallback('architect')
        print 'Architect created'

    def __del__(self):
        self._keymaster.unsubscribe('components')
        self._keymaster.unsubscribe('architect')
        self._keymaster._kill_subscriber_thread()
        print 'Architect terminated'

    def ArchitectCallback(self, sub_key):
        """
        Requests that the internal Keymaster client create a callback for 'sub_key'.

        *sub_key*: the Keymaster key of interest.
        """
        
        # prevent circular reference
        arch = weakref.ref(self)

        def cb(key, val):
            if key == sub_key:
                arch().set_data(key, val)

        self._keymaster.subscribe(sub_key, cb)

    def set_data(self, key, val):
        if key == 'components':
            self._components = val
        elif key == 'architect':
            self._architect = val

    def check_all_in_state(self, state):
        """check that all component states are in the state specified.

        *state*: The state to check.

        """
        comps = self._keymaster.get('components')
        states = [comps[i]['state'] for i in comps if comps[i]['active']]
        return all(x == state for x in states)

    def kget(self, key):
        return self._keymaster.get(key)

    def kput(self, key, value, create = False):
        return self._keymaster.put(key, value, create)

    def wait_all_in_state(self, statename, timeout):
        """wait until component states are all in the state specified.

        *statename*: The state to wait for, a string.

        *timeout*: The length of time to wait for the state, in S (float).
        """

        rval = False
        total_wait_time = 0.0

        while True:
            if self.check_all_in_state(statename):
                rval = True
                break

            total_wait_time += 0.1
            time.sleep(0.1)

            if total_wait_time > timeout:
                break

        return rval

    def send_event(self, event):
        """Issue an arbitrary user-defined event to the FSM.

        *event*: The event.

        """
        self._keymaster.put('architect.control.command', event)
        return True

    def get_active_components(self):
        """Returns a list of components selected for this mode.
        """

        return [i for i in self._components if self._components[i]['active']]

    def get_system_modes(self):
        """returns a list of supported modes. Modes are specified in the
        'connections' section of the YAML configuration file.

        """

        connections = self._keymaster.get('connections')
        return connections.keys()

    def get_system_mode(self):
        """returns the currently set mode.
        """

        return self._architect['control']['configuration']

    def set_system_mode(self, mode):
        """Set a specific mode. The mode name should be defined in the
        "connections" section of the configuration file. The system
        state *must* be 'Standby' to change the mode. If it is not,
        the function returns 'False'.

        *mode*: The mode, or "connections", key name, a string.

        """
        arch_state = self._architect['control']['state']
        modes = self.get_system_modes()

        # system must be in 'Standby' for mode changes to take place
        if arch_state != 'Standby':
            return (False,
                    "System state must be 'Standby' for this operation. It is currently '%s'"
                    % arch_state)

        # 'mode' must exist in 'connections'
        if mode not in modes:
            return (False, "Unknown mode '%s'" % mode)

        # all good, change modes
        rval = self._keymaster.put("architect.control.configuration", mode)

        if not rval:
            return (False, "Could not change mode, Keymaster error.")

        return (True, mode)

    def get_state(self):
        """
        Returns the current Architect state.
        """
        self._architect = self._keymaster.get('architect')
        return self._architect['control']['state']


    def ready(self):
        """Create DataSink connections and enter the Ready state

        """
        self.send_event('get_ready')
        return self.wait_all_in_state('Ready', self._timeout)

    def standby(self):
        """
        Close DataSink connections and enter the Standby state
        """
        self.send_event('do_standby')
        return self.wait_all_in_state('Standby', self._timeout)

    def start(self):
        """From the Ready state, prepare to run and enter the Running state.

        """
        self.send_event('start')
        return self.wait_all_in_state('Running', self._timeout)

    def stop(self):
        """
        From the Running state, shutdown and enter the Ready state.
        """
        self.send_event('stop')
        return self.wait_all_in_state('Ready', self._timeout)
