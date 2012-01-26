// Copyright 2010-2012, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/jsonpath.h"

#include <sstream>
#include <string>
#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace net {
namespace {

struct JsonPathNode {
  enum Type {
    UNDEFINED_INDEX,
    OBJECT_INDEX,
    ARRAY_INDEX,
    SLICE_INDEX
  };
  Type   type;
  string object_index;
  int    array_index;
  int    slice_start;
  int    slice_end;
  int    slice_step;

  static const int kSliceUndef = kint32max;

  static bool IsUndef(int n) {
    return n == kSliceUndef;
  }

  string DebugString() const {
    ostringstream os;
    os << "{" << type << ":" << array_index << ":" << object_index
       << ":(" << slice_start << ":" << slice_end << ":" << slice_step << ")}";
    return os.str();
  }

  JsonPathNode() :
      type(UNDEFINED_INDEX), array_index(0),
      slice_start(0), slice_end(0), slice_step(0) {}
  ~JsonPathNode() {}
};

bool GetDigit(const string &str, int *output) {
  DCHECK(output);
  if (str.empty()) {
    return false;
  }

  const char *begin = str.data();
  const char *end = str.data() + str.size();
  if (str[0] == '-') {
    if (str.size() == 1) {
      return false;
    }
    ++begin;
  }

  while (begin < end) {
    if (!isdigit(*begin)) {
      return false;
    }
    ++begin;
  }

  *output = atoi32(str.c_str());

  return true;
}

bool GetSliceDigit(const string &str, int *output) {
  if (str.empty()) {
    *output = JsonPathNode::kSliceUndef;
    return true;
  }
  return GetDigit(str, output);
}

bool GetQuotedString(const string &str, const char c,
                     string *output) {
  if (str.size() >= 2 &&
      str[0] == c && str[str.size() - 1] == c) {
    *output = str.substr(1, str.size() - 2);
    return true;
  }
  return false;
}

typedef vector<JsonPathNode> JsonPathNodes;

class JsonPathExp : public vector<vector<JsonPathNode> > {
 public:
  bool Parse(const string &jsonpath) {
    clear();
    if (jsonpath.size() <= 1 || jsonpath[0] != '$') {
      LOG(ERROR) << "Not starts with $";
      return false;
    }

    if (Util::EndsWith(jsonpath, ".") ||
        jsonpath.find("...") != string::npos) {
      LOG(ERROR) << "Parse error: " << jsonpath;
      return false;
    }

    if (jsonpath.find("(") != string::npos ||
        jsonpath.find(")") != string::npos ||
        jsonpath.find("@") != string::npos ||
        jsonpath.find("?") != string::npos) {
      LOG(ERROR) << "script expression/current node are not supported: "
                 << jsonpath;
      return false;
    }

    const char *begin = jsonpath.data() + 1;
    const char *end = jsonpath.data() + jsonpath.size();

    string item;
    for (; begin < end; ++begin) {
      if (*begin == ']') {
        return false;
      }
      if (*begin == '.' || *begin == '[') {
        if (!item.empty()) {
          if (!AddNodes(item, OUT_BRACKET)) {
            return false;
          }
          item.clear();
        }
        if (*begin == '.' && begin + 1 < end && *(begin + 1) == '.') {
          ++begin;
          if (!AddNodes(".", OUT_BRACKET)) {
            return false;
          }
        } else if (*begin == '[') {
          ++begin;
          string exp;
          while (*begin != ']') {
            if (begin == end) {
              LOG(ERROR) << "Cannot find closing \"]\"";
              return false;
            }
            exp += *begin;
            ++begin;
          }
          if (!AddNodes(exp, IN_BRACKET)) {
            return false;
          }
        }
      } else {
        item += *begin;
      }
    }

    if (!item.empty()) {
      if (!AddNodes(item, OUT_BRACKET)) {
        return false;
      }
    }

    return !empty();
  }

  string DebugString() const {
    ostringstream os;
    for (size_t i = 0; i < size(); ++i) {
      os << "[";
      for (size_t j = 0; j < (*this)[i].size(); ++j) {
        os << (*this)[i][j].DebugString();
      }
      os << "]";
    }
    return os.str();
  }

 private:
  enum NodesType {
    IN_BRACKET,
    OUT_BRACKET
  };

  bool AddNodes(const string &nodes_exp, NodesType nodes_type) {
    if (nodes_exp.empty()) {
      return false;
    }

    resize(size() + 1);
    JsonPathNodes *nodes = &(back());
    DCHECK(nodes);

    if (nodes_type == OUT_BRACKET) {
      JsonPathNode node;
      node.type = JsonPathNode::OBJECT_INDEX;
      node.object_index = nodes_exp;
      nodes->push_back(node);
    } else if (nodes_type == IN_BRACKET) {
      vector<string> nodes_exps;
      Util::SplitStringUsing(nodes_exp, ",", &nodes_exps);
      for (size_t i = 0; i < nodes_exps.size(); ++i) {
        JsonPathNode node;
        node.type = JsonPathNode::UNDEFINED_INDEX;
        string in_nodes_exp;
        if (GetQuotedString(nodes_exps[i], '\'', &in_nodes_exp) ||
            GetQuotedString(nodes_exps[i], '\"', &in_nodes_exp)) {
          node.type = JsonPathNode::OBJECT_INDEX;
          node.object_index = in_nodes_exp;
        } else if (nodes_exps[i] == "*") {
          node.type = JsonPathNode::OBJECT_INDEX;
          node.object_index = "*";
        } else {
          vector<string> slice;
          Util::SplitStringAllowEmpty(nodes_exps[i], ":", &slice);
          if (slice.size() == 1) {
            if (GetDigit(slice[0], &node.array_index)) {
              node.type = JsonPathNode::ARRAY_INDEX;
            } else {
              // fallback to OBJECT_INDEX.
              node.type = JsonPathNode::OBJECT_INDEX;
              node.object_index = slice[0];
            }
          } else if (slice.size() == 2 &&
                     GetSliceDigit(slice[0], &node.slice_start) &&
                     GetSliceDigit(slice[1], &node.slice_end)) {
            node.slice_step = JsonPathNode::kSliceUndef;
            node.type = JsonPathNode::SLICE_INDEX;
          } else if (slice.size() == 3 &&
                     GetSliceDigit(slice[0], &node.slice_start) &&
                     GetSliceDigit(slice[1], &node.slice_end) &&
                     GetSliceDigit(slice[2], &node.slice_step)) {
            node.type = JsonPathNode::SLICE_INDEX;
          }
        }

        if (node.type == JsonPathNode::UNDEFINED_INDEX) {
          return false;
        }

        nodes->push_back(node);
      }
    } else {
      LOG(FATAL) << "Unknown nodes type";
    }

    return true;
  }
};

// Find all Json::Value(s) from root node |value|,
//  which matches to |nodes|.
void CollectValuesRecursively(const Json::Value &value,
                              const JsonPathNodes &nodes,
                              vector<const Json::Value *> *output) {
  DCHECK(output);
  for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
    if (nodes[node_index].type != JsonPathNode::OBJECT_INDEX) {
      continue;
    }
    if (value.isObject()) {
      const Json::Value::Members members = value.getMemberNames();
      const string &object_index = nodes[node_index].object_index;
      if (object_index != "*" && value.isMember(object_index)) {
        output->push_back(&value[object_index]);
      }
      for (size_t i = 0; i < members.size(); ++i) {
        const Json::Value &v = value[members[i]];
        if (object_index == "*") {
          output->push_back(&v);
        }
        CollectValuesRecursively(v, nodes, output);
      }
    } else if (value.isArray()) {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        CollectValuesRecursively(value[i], nodes, output);
      }
    }
  }
}

void CollectNodesFromJson(const Json::Value &value,
                          const JsonPathExp &jsonpathexp,
                          size_t depth,
                          vector<const Json::Value *> *output) {
  if (depth >= jsonpathexp.size()) {
    output->push_back(&value);
    return;
  }

  const JsonPathNodes &nodes = jsonpathexp[depth];

  for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
    const JsonPathNode &node = nodes[node_index];
    if (node.type == JsonPathNode::OBJECT_INDEX) {
      if (node.object_index == "*") {
        if (value.isObject()) {
          const Json::Value::Members members = value.getMemberNames();
          for (size_t i = 0; i < members.size(); ++i) {
            CollectNodesFromJson(value[members[i]],
                                 jsonpathexp, depth + 1, output);
          }
        } else if (value.isArray()) {
          for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
            CollectNodesFromJson(value[i], jsonpathexp, depth + 1, output);
          }
        } else {
          CollectNodesFromJson(value, jsonpathexp, depth + 1, output);
        }
      } else if (node.object_index == "." && depth + 1 < jsonpathexp.size()) {
        vector<const Json::Value *> matched_values;
        CollectValuesRecursively(value, jsonpathexp[depth + 1],
                                 &matched_values);
        for (size_t i = 0; i < matched_values.size(); ++i) {
          CollectNodesFromJson(*(matched_values[i]),
                               jsonpathexp, depth + 2, output);
        }
      } else if (value.isObject() && value.isMember(node.object_index)) {
        CollectNodesFromJson(value[node.object_index],
                             jsonpathexp, depth + 1, output);
      }
    } else if (node.type == JsonPathNode::ARRAY_INDEX) {
      const int i = node.array_index >= 0 ?
          node.array_index : value.size() + node.array_index;
      if (value.isArray() && value.isValidIndex(i)) {
        CollectNodesFromJson(value[i], jsonpathexp, depth + 1, output);
      }
    } else if (node.type == JsonPathNode::SLICE_INDEX) {
      if (value.isArray()) {
        const int size = static_cast<int>(value.size());
        int start = JsonPathNode::IsUndef(node.slice_start) ?
            0 : node.slice_start;
        int end = JsonPathNode::IsUndef(node.slice_end) ?
            size : node.slice_end;
        const int step = JsonPathNode::IsUndef(node.slice_step) ?
            1 : node.slice_step;
        start = (start < 0) ? max(0, start + size) : min(size, start);
        end   = (end < 0) ?   max(0, end + size)   : min(size, end);
        if (step > 0 && end > start) {
          for (int i = start; i < end; i += step) {
            if (value.isValidIndex(i)) {
              CollectNodesFromJson(value[i], jsonpathexp, depth + 1, output);
            }
          }
        } else if (step < 0 && end < start) {
          for (int i = start; i > end; i += step) {
            if (value.isValidIndex(i)) {
              CollectNodesFromJson(value[i], jsonpathexp, depth + 1, output);
            }
          }
        }
      }
    } else {
      LOG(FATAL) << "Unknown type";
    }
  }
}
}  // namespace

// static
bool JsonPath::Parse(const Json::Value &root,
                     const string &jsonpath,
                     vector<const Json::Value *> *output) {
  JsonPathExp jsonpathexp;
  if (!jsonpathexp.Parse(jsonpath)) {
    LOG(WARNING) << "Parsing JsonPath error: " << jsonpath;
    return false;
  }

  VLOG(1) << jsonpathexp.DebugString();

  DCHECK(output);
  CollectNodesFromJson(root, jsonpathexp, 0, output);
  return true;
}
}   // net
}   // mozc
