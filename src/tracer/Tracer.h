#ifndef XEMU_WAIT_TRACE_PLUGIN_TRACER_H
#define XEMU_WAIT_TRACE_PLUGIN_TRACER_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

struct Tracer {
  struct TraceEntry {
    uint32_t id;
    std::vector<uint32_t> params;
    uint32_t esp;

    auto operator<=>(const TraceEntry&) const = default;
  };

  void Insert(const std::string& function, uint32_t esp, size_t num_params,
              const uint32_t* params);
  void Remove(const std::string& function, uint32_t esp);
  void Count(const std::string& function, size_t num_params,
             const uint32_t* params);

  void ProcessCommand(const std::string& args);

 private:
  struct TraceEntryPtrCmp {
    bool operator()(const std::shared_ptr<TraceEntry>& lhs,
                    const std::shared_ptr<TraceEntry>& rhs) const {
      return *lhs < *rhs;
    }
  };

  std::mutex tracer_mutex_;
  std::map<std::string, std::set<std::shared_ptr<TraceEntry>, TraceEntryPtrCmp>>
      entries_;
  std::map<std::string, std::map<std::vector<uint32_t>, uint32_t>> counters_;

  uint32_t next_id_{0};
};

#endif  // XEMU_WAIT_TRACE_PLUGIN_TRACER_H
