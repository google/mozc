# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""A utility tool to run NaCl Pepper HTTPClient test.

Example usage:
  The following command starts HTTP server and launch Chrome with the extension
  and open test.html. When '/TEST_FIN?result=success' is accessed from the page
  or the extension, this command successfully exit.
  When '/TEST_FIN?result=SOME_ERROR' is accessed, it exit showing SOME_ERROR.
  When the timeout specified has expired, it exit showing timeout error.

  python nacl_net_test_server.py --browser_path=/usr/bin/google-chrome \
    --serving_dir=/PATH/TO/FILE_DIR \
    --serving_dir=/PATH/TO/ADDITIONAL_FILE_DIR \
    --load_extension=/PATH/TO/EXTENSION_DIR \
    --url=test.html \
    --timeout=20
"""

import BaseHTTPServer
import optparse
import os
import os.path
import shutil
import SocketServer
import subprocess
import sys
import tempfile
import thread
import time
import urlparse


class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """Handle the HTTP requests that arrive at the server."""

  def do_GET(self):
  # pylint: disable=g-bad-name
    """Handles GET request."""
    parsed_path = urlparse.urlparse(self.path)
    options = {'response': 200,
               'result': '',
               'before_response_sleep': 0.0,
               'before_head_sleep': 0.0,
               'after_head_sleep': 0.0,
               'before_data_sleep': 0.0,
               'after_data_sleep': 0.0,
               'content_length': '',
               'data': 'DEFAULT_DATA',
               'times': 1,
               'redirect_location': ''}
    query = urlparse.parse_qsl(parsed_path.query)
    for params in query:
      options[params[0]] = params[1]

    if parsed_path.path.startswith('/TEST_FIN'):
      if 'result' in options:
        if options['result'] == 'success':
          self.server.result = 0
        else:
          self.server.result = options['result']
      self.server.finished = True
      return

    # For retry test.
    if parsed_path.path.startswith('/RETRY_TEST'):
      if 'action' in options:
        self.send_response(200)
        self.end_headers()
        if options['action'] == 'set_counter':
          self.wfile.write('OK')
          self.server.retry_test_counter = int(options['value'])
        elif options['action'] == 'get_counter':
          self.wfile.write(str(self.server.retry_test_counter))
        return
      else:
        self.server.retry_test_counter += 1
        if self.server.retry_test_counter <= 0:
          self.send_response(404)
          self.end_headers()
          self.wfile.write('NG')
          return

    for extra_dir in self.server.serving_dirs:
      full_path = os.path.join(extra_dir, os.path.basename(parsed_path.path))
      if os.path.isfile(full_path):
        try:
          data = open(full_path).read()
          self.send_response(200)
          self.send_header('Content-Length', len(data))
          self.end_headers()
          self.wfile.write(data)
        except IOError, (errno, strerror):
          print 'I/O error(%s): %s' % (errno, strerror)
        return

    try:
      time.sleep(float(options['before_response_sleep']))
      self.send_response(int(options['response']))
      time.sleep(float(options['before_head_sleep']))
      self.send_header('command', '%s' % self.command)
      self.send_header('path', '%s' % self.path)
      self.send_header('parsed_path', '%s' % parsed_path.path)
      for name, value in sorted(self.headers.items()):
        self.send_header('CLIENT_HEADER_%s' % name, '%s' % value)
      if options['content_length']:
        self.send_header('Content-Length',
                         options['content_length'])
      if options['redirect_location']:
        self.send_header('Location',
                         options['redirect_location'])
      self.end_headers()
      time.sleep(float(options['after_head_sleep']))
      for _ in range(int(options['times'])):
        time.sleep(float(options['before_data_sleep']))
        self.wfile.write(options['data'])
        time.sleep(float(options['after_data_sleep']))
    except IOError, (errno, strerror):
      print 'I/O error(%s): %s' % (errno, strerror)
    return

  def do_POST(self):
  # pylint: disable=g-bad-name
    """Handles POST request."""
    parsed_path = urlparse.urlparse(self.path)
    options = {'response': 200,
               'result': '',
               'before_response_sleep': 0.0,
               'before_head_sleep': 0.0,
               'after_head_sleep': 0.0,
               'after_data_sleep': 0.0,
               'content_length': '',
               'redirect_location': ''}
    query = urlparse.parse_qsl(parsed_path.query)
    for params in query:
      options[params[0]] = params[1]

    try:
      content_len = int(self.headers.getheader('content-length'))
      post_data = self.rfile.read(content_len)
      time.sleep(float(options['before_response_sleep']))
      self.send_response(int(options['response']))
      time.sleep(float(options['before_head_sleep']))
      self.send_header('command', '%s' % self.command)
      self.send_header('path', '%s' % self.path)
      self.send_header('parsed_path', '%s' % parsed_path.path)
      for name, value in sorted(self.headers.items()):
        self.send_header('CLIENT_HEADER_%s' % name, '%s' % value)
      if options['content_length']:
        self.send_header('Content-Length',
                         options['content_length'])
      if options['redirect_location']:
        self.send_header('Location',
                         options['redirect_location'])
      self.end_headers()
      time.sleep(float(options['after_head_sleep']))
      self.wfile.write(post_data)
      time.sleep(float(options['after_data_sleep']))
      return
    except IOError, (errno, strerror):
      print 'I/O error(%s): %s' % (errno, strerror)
    return

  def do_HEAD(self):
  # pylint: disable=g-bad-name
    """Handles HEAD request."""
    parsed_path = urlparse.urlparse(self.path)
    options = {'response': 200,
               'before_response_sleep': 0.0,
               'before_head_sleep': 0.0}
    query = urlparse.parse_qsl(parsed_path.query)
    for params in query:
      options[params[0]] = params[1]

    try:
      time.sleep(float(options['before_response_sleep']))
      self.send_response(options['response'])
      time.sleep(float(options['before_head_sleep']))
      self.send_header('command', '%s' % self.command)
      self.send_header('path', '%s' % self.path)
      self.send_header('parsed_path', '%s' % parsed_path.path)
      for name, value in sorted(self.headers.items()):
        self.send_header('CLIENT_HEADER_%s' % name, '%s' % value)
      self.end_headers()
    except IOError, (errno, strerror):
      print 'I/O error(%s): %s' % (errno, strerror)
    return


class TestServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
  def Configure(self, serving_dirs):
    self.serving_dirs = serving_dirs
    self.finished = False
    self.result = 'Not Finished.'
    self.retry_test_counter = 0


def main():
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('--timeout', dest='timeout', action='store', type='float',
                    default=5.0, help='Timeout in seconds.')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default='/usr/bin/google-chrome',
                    help='The browser path.')
  parser.add_option('--serving_dir', dest='serving_dirs', action='append',
                    type='string', default=[],
                    help='File directory to be served by HTTP server.')
  parser.add_option('--load_extension', dest='load_extension', action='store',
                    type='string', default='', help='The extension path.')
  parser.add_option('-u', '--url', dest='url', action='store',
                    type='string', default=None, help='The webpage to load.')
  (options, _) = parser.parse_args()
  # TODO(horo) Don't use the fixed port number. Find the way to pass the port
  # number to the test module.
  server = TestServer(('localhost', 9999), RequestHandler)
  server.Configure(options.serving_dirs)
  host, port = server.socket.getsockname()
  print 'Starting server %s:%s' % (host, port)

  def Serve():
    while not server.finished:
      server.handle_request()
  thread.start_new_thread(Serve, ())

  temp_dir = tempfile.mkdtemp()
  if options.browser_path:
    cmd = [options.browser_path, '--user-data-dir=%s' % temp_dir]
    if options.load_extension:
      cmd.append('--load-extension=%s' % options.load_extension)
    if options.url:
      cmd.append('http://%s:%s/%s' % (host, port, options.url))
    print cmd
    browser_handle = subprocess.Popen(cmd)

  time_started = time.time()
  result = 0

  while True:
    if time.time() - time_started >= options.timeout:
      result = 'Timeout!: %s' %  (time.time() - time_started)
      break
    if server.finished:
      result = server.result
      break
    time.sleep(0.5)

  if options.browser_path:
    browser_handle.kill()
    browser_handle.wait()
  shutil.rmtree(temp_dir)
  sys.exit(result)

if __name__ == '__main__':
  main()
