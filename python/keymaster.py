import yaml
import zmq

class Keymaster:

    def __init__(self, url, ctx = None):
        self._km_url = url
        self._km = None

        if ctx:
            self._ctx = ctx
        else:
            self._ctx = zmq.Context(1)

    def __del__(self):
        if self._km:
            self._km.close()

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
        return {'key': key, 'result': False, 'err': 'Time-out when talking to Keymaster.'}

    def get(self, key):
        """Fetches a yaml node at 'key' from the Keymaster"""
        reply = self._call_keymaster('GET', key)

        if reply['result']:
            return reply['node']
        else:
            return {}
        
    def put(self, key, value, create = False):
        """Puts a value at 'key' on the Keymaster"""
        return self._call_keymaster('PUT', key, value, "create" if create else "")['result']
