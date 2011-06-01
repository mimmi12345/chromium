// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_CONFIG_H_
#define REMOTING_CLIENT_CLIENT_CONFIG_H_

#include <string>

#include "base/basictypes.h"

namespace remoting {

struct ClientConfig {
  ClientConfig();
  ~ClientConfig();

  bool xmpp_signalling;

  // Set only with |xmpp_signalling| set to true.
  std::string xmpp_username;
  std::string xmpp_auth_token;
  std::string xmpp_auth_service;  // Do we still need it?

  // Set only with |xmpp_signalling| set to false.
  std::string local_jid;

  std::string host_jid;
  std::string host_public_key;

  std::string access_code;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_CONFIG_H_
