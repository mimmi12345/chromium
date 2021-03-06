#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import pyauto_functional
import pyauto
import test_utils


class SimpleTest(pyauto.PyUITest):

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Extra flag needed by scroll performance tests.
    fd, self._strace_log = tempfile.mkstemp()
    os.close(fd)
    extra_flags = ['--no-sandbox', '--renderer-clean-exit',
                   '--renderer-cmd-prefix=/usr/bin/strace -o %s' %
                    self._strace_log]
    return pyauto.PyUITest.ExtraChromeFlags(self) + extra_flags


  def testCleanExit(self):
    """Ensures the renderer process cleanly exits."""
    url = self.GetHttpURLForDataPath('title2.html')
    self.NavigateToURL(url)
    test_utils.CrashBrowser(self)
    strace_contents = open(self._strace_log).read()
    self.assertTrue('exit_group' in strace_contents)
    os.remove(self._strace_log)


if __name__ == '__main__':
  pyauto_functional.Main()
