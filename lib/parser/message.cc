// Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "message.h"
#include "char-set.h"
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

namespace Fortran {
namespace parser {

std::ostream &operator<<(std::ostream &o, const MessageFixedText &t) {
  for (std::size_t j{0}; j < t.size(); ++j) {
    o << t.str()[j];
  }
  return o;
}

std::string MessageFixedText::ToString() const {
  return std::string{str_, /*not in std::*/ strnlen(str_, bytes_)};
}

MessageFormattedText::MessageFormattedText(MessageFixedText text, ...)
  : isFatal_{text.isFatal()} {
  const char *p{text.str()};
  std::string asString;
  if (p[text.size()] != '\0') {
    // not NUL-terminated
    asString = text.ToString();
    p = asString.data();
  }
  char buffer[256];
  va_list ap;
  va_start(ap, text);
  vsnprintf(buffer, sizeof buffer, p, ap);
  va_end(ap);
  string_ = buffer;
}

void Message::Incorporate(Message &that) {
  if (provenanceRange_.start() == that.provenanceRange_.start() &&
      cookedSourceRange_.begin() == that.cookedSourceRange_.begin() &&
      !expected_.empty()) {
    expected_ = expected_.Union(that.expected_);
  }
}

std::string Message::ToString() const {
  std::string s{string_};
  bool isExpected{isExpected_};
  if (string_.empty()) {
    if (fixedText_ != nullptr) {
      if (fixedBytes_ > 0 && fixedBytes_ < std::string::npos) {
        s = std::string(fixedText_, fixedBytes_);
      } else {
        s = std::string{fixedText_};  // NUL-terminated
      }
    } else {
      SetOfChars expect{expected_};
      if (expect.Has('\n')) {
        expect = expect.Difference('\n');
        if (expect.empty()) {
          return "expected end of line"_err_en_US.ToString();
        } else {
          s = expect.ToString();
          if (s.size() == 1) {
            return MessageFormattedText(
                "expected end of line or '%s'"_err_en_US, s.data())
                .MoveString();
          } else {
            return MessageFormattedText(
                "expected end of line or one of '%s'"_err_en_US, s.data())
                .MoveString();
          }
        }
      }
      s = expect.ToString();
      if (s.size() != 1) {
        return MessageFormattedText("expected one of '%s'"_err_en_US, s.data())
            .MoveString();
      }
      isExpected = true;
    }
  }
  if (isExpected) {
    return MessageFormattedText("expected '%s'"_err_en_US, s.data())
        .MoveString();
  }
  return s;
}

ProvenanceRange Message::Emit(
    std::ostream &o, const CookedSource &cooked, bool echoSourceLine) const {
  ProvenanceRange provenanceRange{provenanceRange_};
  if (cookedSourceRange_.begin() != nullptr) {
    provenanceRange = cooked.GetProvenance(cookedSourceRange_);
  }
  if (!context_ || context_->Emit(o, cooked, false) != provenanceRange) {
    cooked.allSources().Identify(o, provenanceRange, "", echoSourceLine);
  }
  o << "   ";
  if (isFatal_) {
    o << "ERROR: ";
  }
  o << ToString() << '\n';
  return provenanceRange;
}

void Messages::Incorporate(Messages &that) {
  if (messages_.empty()) {
    *this = std::move(that);
  } else if (!that.messages_.empty()) {
    last_->Incorporate(*that.last_);
  }
}

void Messages::Copy(const Messages &that) {
  for (const Message &m : that.messages_) {
    Message copy{m};
    Put(std::move(copy));
  }
}

void Messages::Emit(std::ostream &o, const CookedSource &cooked,
    const char *prefix, bool echoSourceLines) const {
  for (const auto &msg : messages_) {
    if (prefix) {
      o << prefix;
    }
    if (msg.context()) {
      o << "In the context ";
    }
    msg.Emit(o, cooked, echoSourceLines);
  }
}

bool Messages::AnyFatalError() const {
  for (const auto &msg : messages_) {
    if (msg.isFatal()) {
      return true;
    }
  }
  return false;
}
}  // namespace parser
}  // namespace Fortran
