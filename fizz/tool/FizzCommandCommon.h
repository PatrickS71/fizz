/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fizz/protocol/AsyncFizzBase.h>
#include <fizz/util/Parse.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventHandler.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fizz {
namespace tool {

inline uint16_t portFromString(const std::string& portStr, bool serverSide) {
  unsigned long converted = 0;
  try {
    converted = std::stoul(portStr);
  } catch (const std::exception&) {
    throw std::runtime_error(
        "Couldn't convert " + portStr + " to port number.");
  }
  if (converted <= std::numeric_limits<uint16_t>::max()) {
    if (converted == 0 && !serverSide) {
      throw std::runtime_error("Port 0 is not valid for client ports.");
    }
    return static_cast<uint16_t>(converted);
  } else {
    throw std::runtime_error(
        "Couldn't convert " + portStr + " to port number.");
  }
}

inline std::pair<std::string, uint16_t> hostPortFromString(
    const std::string& hostPortStr) {
  std::string host;
  uint16_t port;
  size_t colonIdx = hostPortStr.rfind(':');
  if (colonIdx == std::string::npos) {
    throw std::runtime_error("-connect requires a host:port pair.");
  }
  size_t start = 0;
  size_t length = colonIdx;
  // Handle IPv6. Force presence of [] to avoid using the last nibble
  // instead of a port.
  if (hostPortStr.rfind(':', colonIdx - 1) != std::string::npos) {
    if (hostPortStr.front() != '[' || hostPortStr.at(colonIdx - 1) != ']') {
      throw std::runtime_error(
          "IPv6 " + hostPortStr.substr(0, colonIdx) +
          " must be enclosed in square brackets.");
    }
    start++;
    length -= 2;
  }
  host = hostPortStr.substr(start, length);
  port = portFromString(hostPortStr.substr(colonIdx + 1), false);
  return {host, port};
}

// Argument handler function

typedef std::function<void(const std::string&)> FizzCommandArgHandler;
struct FizzCommandArgHandlerInfo {
  bool hasVariable;
  FizzCommandArgHandler handler;
};
typedef std::map<std::string, FizzCommandArgHandlerInfo> FizzArgHandlerMap;

int parseArguments(
    std::vector<std::string> argv,
    FizzArgHandlerMap handlers,
    std::function<void()> usageFunc);

// Utility to convert from comma-separated string to vector of T that has
// a parse() implementation in util/Parse.h
template <typename T>
inline std::vector<T> fromCSV(const std::string& arg) {
  std::vector<folly::StringPiece> pieces;
  std::vector<T> output;
  folly::split(",", arg, pieces);
  std::transform(
      pieces.begin(), pieces.end(), std::back_inserter(output), parse<T>);
  return output;
}

// Echo client/server classes

class InputHandlerCallback {
 public:
  virtual ~InputHandlerCallback() = default;
  virtual void write(std::unique_ptr<folly::IOBuf> msg) = 0;
  virtual void close() = 0;
  virtual bool connected() const = 0;
};

class TerminalInputHandler : public folly::EventHandler {
 public:
  explicit TerminalInputHandler(
      folly::EventBase* evb,
      InputHandlerCallback* cb);
  void handlerReady(uint16_t events) noexcept override;

 private:
  void hitEOF();

  InputHandlerCallback* cb_;
  folly::EventBase* evb_;
};

// Class that stores secrets for printing
class SecretCollector : public AsyncFizzBase::SecretCallback {
 public:
  void externalPskBinderAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    externalPskBinder_ = secret;
  }

  void resumptionPskBinderAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    resumptionPskBinder_ = secret;
  }

  void earlyExporterSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    earlyExporterSecret_ = secret;
  }

  void clientEarlyTrafficSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    clientEarlyTrafficSecret_ = secret;
  }

  void clientHandshakeTrafficSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    clientHandshakeTrafficSecret_ = secret;
  }

  void serverHandshakeTrafficSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    serverHandshakeTrafficSecret_ = secret;
  }

  void exporterMasterSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    exporterMasterSecret_ = secret;
  }

  void resumptionMasterSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    resumptionMasterSecret_ = secret;
  }

  void clientAppTrafficSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    clientAppTrafficSecret_ = secret;
  }

  void serverAppTrafficSecretAvailable(
      const std::vector<uint8_t>& secret) noexcept override {
    serverAppTrafficSecret_ = secret;
  }

 protected:
  using OptionalSecret = folly::Optional<std::vector<uint8_t>>;

  static std::string secretStr(const OptionalSecret& secret) {
    if (!secret) {
      return "(none)";
    }
    return folly::hexlify(*secret);
  }

  OptionalSecret externalPskBinder_;
  OptionalSecret resumptionPskBinder_;
  OptionalSecret earlyExporterSecret_;
  OptionalSecret clientEarlyTrafficSecret_;
  OptionalSecret clientHandshakeTrafficSecret_;
  OptionalSecret serverHandshakeTrafficSecret_;
  OptionalSecret exporterMasterSecret_;
  OptionalSecret resumptionMasterSecret_;
  OptionalSecret clientAppTrafficSecret_;
  OptionalSecret serverAppTrafficSecret_;
};

} // namespace tool
} // namespace fizz
