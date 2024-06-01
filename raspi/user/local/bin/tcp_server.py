#!/usr/bin/env python3

import datetime
import logging
import os
import shlex
import socketserver
import subprocess
import threading

logging.basicConfig(level=logging.DEBUG)

logger = logging.getLogger('fms')

# The bindings between the cameras and the Cable B SM bundles.

cams = {0:'cab3', 1:'cab4',
        2:'cab2', 3:'cab1'}

def call(cmdStr):
    """Wrap subprocess.run(). """

    cmdArgs = shlex.split(cmdStr)
    logger.info("calling %s", cmdArgs)
    try:
        ret = subprocess.run(cmdArgs, timeout=15,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             check=True)
        if ret.stdout:
            logger.info("call output: %s", ret.stdout)
        if ret.stderr:
            logger.warning("call error: %s", ret.stderr)
    except Exception as e:
        logger.error('cmd botch: %s' % (e))

def snap(cam=0, filename=None, exptime=100, gain=0):
    """Take a single exposure with one camera.
    """
    if filename is None:
        filename = '/tmp/snap%s.fits' % (cam)
    dirname = os.path.dirname(filename)
    os.makedirs(dirname, exist_ok=True)

    snapCmd = 'snap -c %s -F %s %s %s' % (cam, filename, exptime, gain)
    call(snapCmd)

    return filename

def snappath(cam, now=None):
    if now is None:
        now = datetime.datetime.now()
    try:
        rootDir = '/data/fms'
        dayDir = now.strftime("%Y-%m-%d")
        timestamp = now.strftime("%Y%m%d_%H%M%S")
        cable = cams[cam]

        filename = 'snap%d_%s_%s.fits' % (cam, cable, timestamp)
        path = os.path.join(rootDir, dayDir, filename)
    except Exception as e:
        logger.error('path boom: %s' % (e))
    return path

class FmsRequestHandler(socketserver.BaseRequestHandler):
    def setup(self):
        pass

    def parseCommand(self, cmdStr):
        cmd = cmdStr.split()
        logger.info("rawCmd: %s", cmd)

        cmdName = cmd[0]
        cmdArgs = dict()
        for kv in cmd[1:]:
            k,v = kv.split('=')
            cmdArgs[k] = v
        logger.debug("cmd args: %s", cmdArgs)

        return cmdName, cmdArgs

    def respond(self, text):
        response = text + '\n'
        self.request.sendall(response.encode('latin-1'))

    def handle(self):
        rawCmd = str(self.request.recv(1024), 'latin-1')

        try:
            cmdName, cmdArgs = self.parseCommand(rawCmd)
        except Exception as e:
            logger.error('parsing failed: %s', e)
            response = 'ERROR bad command: %s\n' % (e)
            self.respond(response)
            return

        logger.info('new cmd: %s (%s)', cmdName, cmdArgs)
        if cmdName == 'exit':
            response = 'OK done'
            self.respond(response)
            self.server.shutdown()
            return

        elif cmdName == 'on':
            call('allon')
            response = 'OK'
        elif cmdName == 'off':
            call('alloff')
            response = 'OK'
        elif cmdName == 'snap':
            try:
                call('allon')
                filename = snap(**cmdArgs)
                response = 'OK %s' % (filename)
            except Exception as e:
                response = 'ERROR %s' % (e)
            finally:
                call('alloff')
        elif cmdName in {'all', 'dark'}:
            retFiles = []
            ts = datetime.datetime.now()
            try:
                if cmdName == 'all':
                    call('allon')
                for c in cams.keys():
                    cmdArgs['cam'] = c
                    cmdArgs['filename'] = snappath(c, now=ts)
                    filename = snap(**cmdArgs)
                    retFiles.append(filename)
                response = 'OK %s' % (retFiles)
            except Exception as e:
                response = 'ERROR %s' % (e)
            finally:
                if cmdName == 'all':
                    call('alloff')
        else:
            response = 'ERROR unknown command %s' % (cmdName)
        logger.info("response: %s", response)

        self.respond(response)

class FmsServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    name = 'fms'

def run():
    ip, port = '', 1024
    server = FmsServer((ip, port), FmsRequestHandler)

    # Start a thread with the server -- that thread will then start one
    # more thread for each request
    server_thread = threading.Thread(target=server.serve_forever)

    # Exit the server thread when our thread terminates
    server_thread.daemon = True
    print("Server loop starting in thread:", server_thread.name)
    server_thread.start()
    server_thread.join()
    print("Server loop done in thread:", server_thread.name)

def main(argv=None):
    if isinstance(argv, str):
        import shlex
        argv = shlex.split(argv)

    run()

if __name__ == "__main__":
    main()
