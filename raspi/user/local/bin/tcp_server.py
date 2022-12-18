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

def call(cmdStr):
    """Wrap subprocess.run(). """

    cmdArgs = shlex.split(cmdStr)
    logger.info("calling %s", cmdArgs)
    ret = subprocess.run(cmdArgs, timeout=15,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         check=True)
    if ret.stdout:
        logger.info("call output: %s", ret.stdout)
    if ret.stderr:
        logger.warning("call error: %s", ret.stderr)

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

def snapnames():
    rootDir = '/data/fms'
    now = datetime.datetime.now()
    dayDir = now.strftime("%Y-%m-%d")
    timestamp = now.strftime("%Y%m%d_%H%M%S")

    names = []
    for i in 0,1:
        filename = 'snap%d_%s.fits' % (i, timestamp)
        names.append(os.path.join(rootDir, dayDir, filename))

    return names

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
        elif cmdName == 'all':
            filenames = snapnames()
            retFiles = []
            try:
                call('allon')
                for c in 0,1:
                    cmdArgs['cam'] = c
                    cmdArgs['filename'] = filenames[c]
                    filename = snap(**cmdArgs)
                    retFiles.append(filename)
                response = 'OK %s' % (retFiles)
            except Exception as e:
                response = 'ERROR %s' % (e)
            finally:
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
