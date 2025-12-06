#ifndef XEMU_WAIT_TRACE_PLUGIN_TRACER_H
#define XEMU_WAIT_TRACE_PLUGIN_TRACER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>

struct Tracer {
  typedef std::pair<uint32_t, uint32_t> ObjectEspPair;
  std::map<std::string, std::set<ObjectEspPair>> wait_entries;

  void Insert(const std::string &function, uint32_t object_param, uint32_t esp);
  void Remove(const std::string &function, uint32_t esp);

  void ProcessCommand(const std::string &args);

private:
  std::mutex tracer_mutex;
};

#endif // XEMU_WAIT_TRACE_PLUGIN_TRACER_H
