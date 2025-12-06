#ifndef XEMU_WAIT_TRACE_PLUGIN_TRACER_H
#define XEMU_WAIT_TRACE_PLUGIN_TRACER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>

struct Tracer {
  struct WaitEntry {
    uint32_t id;
    uint32_t wait_object;
    uint32_t esp;

    auto operator<=>(const WaitEntry&) const = default;
  };

  void Insert(const std::string& function, uint32_t object_param, uint32_t esp);
  void Remove(const std::string& function, uint32_t esp);
  void Signal(const std::string& function, uint32_t object_param, uint32_t esp);

  void ProcessCommand(const std::string& args);

 private:
  std::mutex tracer_mutex_;
  std::map<std::string, std::set<WaitEntry>> wait_entries_;
  std::map<uint32_t, uint32_t> signal_calls_;
  uint32_t next_id_{0};
};

#endif  // XEMU_WAIT_TRACE_PLUGIN_TRACER_H
