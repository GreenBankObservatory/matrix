######################################################################
# keymaster.py -- A python Matrix Keymaster client.
######################################################################

import yaml
import zmq
import threading
import string
import random
import inspect
from time import sleep

def gen_random_string(rand_len = 10, chars = string.ascii_uppercase + string.ascii_lowercase + string.digits):
    """Generates a random sequence of characters of size 'rand_len' from
       the characters provided in 'char'

    """
    return ''.join(random.choice(chars) for _ in range(rand_len))

class Keymaster:

    class subscriber_task(threading.Thread):
        """Thread for subscriber services."""

        def __init__(self, keymaster):
            threading.Thread.__init__(self)
            self._keymaster = keymaster
            km_url = keymaster._km_url
            # Get the as-configured publication URLs
            pub_urls = self._keymaster.get('Keymaster.URLS.AsConfigured.Pub')
            # choose the one who's transport is the same as that of our
            # keymaster's request URL. i.e., if tcp, then use the pub tcp, etc.
            self._pub_url = [p for p in pub_urls if p.split(':')[0] == km_url.split(':')[0]][0]
            # main thread communicates to this thread via ZMQ inproc REQ/REP.
            self.pipe_url = "inproc://" + gen_random_string(20)
            self._callbacks = {}

        def run(self):
            """The thread code.

            Runs the subscriber thread. Receives messages over a ZMQ
            pipe and a ZMQ subscriber socket for events both from the
            client and the KeymasterServer.

            """
            try:
                ctx = self._keymaster._ctx
                sub_sock = ctx.socket(zmq.SUB)
                pipe = ctx.socket(zmq.REP)
                poller = zmq.Poller()
                sub_sock.connect(self._pub_url)
                pipe.bind(self.pipe_url);
                poller.register(sub_sock, flags = zmq.POLLIN)
                poller.register(pipe, flags = zmq.POLLIN)
                end_thread = False

                while not end_thread:
                    event = poller.poll()

                    for e in event:
                        sock = e[0] # e[1] always POLLIN

                        if sock == pipe:
                            msg = pipe.recv_pyobj()

                            if msg == self._keymaster.SUBSCRIBE:
                                key = pipe.recv_pyobj()
                                fcb = pipe.recv_pyobj()

                                if key == "":
                                    key = 'Root'

                                if not self._keymaster.get(key):
                                    pipe.send_pyobj(False, zmq.SNDMORE)
                                    pipe.send_pyobj("'%s' does not exist on the Keymaster." % key)
                                elif key in self._callbacks:
                                    # there is already a callback, fail.
                                    pipe.send_pyobj(False, zmq.SNDMORE)
                                    pipe.send_pyobj("'%s' is already registered for a callback." % key)
                                else:
                                    # Key exists on Keymaster, and no
                                    # callback, so register one. First, test the function
                                    bad_function = False

                                    try:
                                        x = inspect.getargspec(fcb)

                                        # Should be a function that takes two parameters.
                                        if len(x.args) == 2:
                                            self._callbacks[key] = fcb
                                            sub_sock.setsockopt(zmq.SUBSCRIBE, key)
                                            pipe.send_pyobj(True, zmq.SNDMORE)
                                            pipe.send_pyobj(key)
                                        else:
                                            pipe.send_pyobj(False, zmq.SNDMORE)
                                            pipe.send_pyobj('Callback function must take 2 arguments')
                                    except TypeError:
                                        # not a function at all!
                                        pipe.send_pyobj(False, zmq.SNDMORE)
                                        pipe.send_pyobj('Callback object is not a function!')

                            elif msg == self._keymaster.UNSUBSCRIBE:
                                key = pipe.recv_pyobj()
                                sub_sock.setsockopt(zmq.UNSUBSCRIBE, key)

                                if key in self._callbacks:
                                    self._callbacks.pop(key)
                                    pipe.send_pyobj(True, zmq.SNDMORE)
                                    pipe.send_pyobj("'%s' unsubscribed." % key)
                                else:
                                    pipe.send_pyobj(False, zmq.SNDMORE)
                                    pipe.send_pyobj("'%s': No such key is subscribed." % key)

                            elif msg == self._keymaster.QUIT:
                                pipe.send_pyobj(True)
                                end_thread = True

                            elif msg == self._keymaster.PING:
                                pipe.send_pyobj(True)

                        if sock == sub_sock:
                            msg = sub_sock.recv_multipart()
                            key = msg[0]

                            if len(msg) > 1:
                                if key in self._callbacks:
                                    n = msg[1]
                                    mci = self._callbacks[key]
                                    mci(key, n)
                pipe.close()
                sub_sock.close()

            except zmq.core.error.ZMQError, e:
                print 'In subscriber thread, exception zmq.core.error.ZMQError:', e
                print "pipe_url:", self.pipe_url
                print " pub_url:", self._pub_url
            finally:
                print "Keymaster: Ending subscriber thread."


    def __init__(self, url, ctx = None):
        self.SUBSCRIBE = 10
        self.UNSUBSCRIBE = 11
        self.QUIT = 12
        self.PING = 13
        self._km_url = url
        self._km = None
        self._sub_task = None

        if ctx:
            self._ctx = ctx
        else:
            self._ctx = zmq.Context(1)

    def __del__(self):
        if self._km:
            self._km.close()

        if self._sub_task:
            pipe = self._ctx.socket(zmq.REQ)
            pipe.connect(self._sub_task.pipe_url)
            pipe.send_pyobj(self.QUIT)
            pipe.recv_pyobj()
            pipe.close()
            self._sub_task.join()
            del self._sub_task

    def _keymaster_socket(self):
        """returns the keymaster socket, creating one if needed."""
        if not self._km:
            self._km = self._ctx.socket(zmq.REQ)
            self._km.connect(self._km_url)
        return self._km

    def _call_keymaster(self, cmd, key, val = None, flag = None):
        """atomically calls the keymaster."""
        km = self._keymaster_socket()
        parts = [p for p in [cmd, key, val, flag] if p]
        km.send_multipart(parts)

        if km.poll(5000):
            response = km.recv()
            return yaml.load(response)
        return {'key': key, 'result': False, 'err': 'Time-out when talking to Keymaster.', 'node': {}}

    def get(self, key):
        """Fetches a yaml node at 'key' from the Keymaster"""
        reply = self._call_keymaster('GET', key)

        if reply['result']:
            return reply['node']
        else:
            return {}

    def put(self, key, value, create = False):
        """Puts a value at 'key' on the Keymaster"""
        return self._call_keymaster('PUT', key, yaml.dump(value), "create" if create else "")['result']

    def delete(self, key):
        """Deletes a key from the Keymaster"""
        return self._call_keymaster('DEL', key)

    def subscribe(self, key, cb_fun):
        """Subscribes to a key on the Keymaster

        *key:*
          the key of interest. Must be a full path, i.e. 'foo.bar.baz'

        *cb_fun:*
          the callback function, which must take 2 args: the key, and a yaml node

        returns 'True' if the subscription was successful, 'False'
        otherwise. The function will fail if 'key' is already
        subscribed.

        """

        # start the subscriber task if not already running
        if not self._sub_task:
            self._sub_task = Keymaster.subscriber_task(self)
            self._sub_task.start()
            sleep(1) # give it time to start

        pipe = self._ctx.socket(zmq.REQ)
        pipe.connect(self._sub_task.pipe_url)
        pipe.send_pyobj(self.SUBSCRIBE, zmq.SNDMORE)
        pipe.send_pyobj(key, zmq.SNDMORE)
        pipe.send_pyobj(cb_fun)
        rval = pipe.recv_pyobj()
        msg = pipe.recv_pyobj()
        return (rval, msg)

    def unsubscribe(self, key):
        """Unsubscribes from a key on the Keymaster

        *key:*
          the key of interest. Must be a full path, i.e. 'foo.bar.baz'

        returns 'True' if the key was unsubscribed, 'False' if
        not. The function will fail if the key was not previously
        subscribed.

        """

        if self._sub_task:
            pipe = self._ctx.socket(zmq.REQ)
            pipe.connect(self._sub_task.pipe_url)
            pipe.send_pyobj(self.UNSUBSCRIBE, zmq.SNDMORE)
            pipe.send_pyobj(key)
            rval = pipe.recv_pyobj()
            msg = pipe.recv_pyobj()
            return (rval, msg)
        return (False, 'No subscriber thread running!')


def my_callback(key, val):
    print key
    print val

