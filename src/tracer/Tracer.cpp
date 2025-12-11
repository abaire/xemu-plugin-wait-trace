#include "Tracer.h"

#include <cmath>
#include <iomanip>
#include <iostream>

extern "C" {
#include "TracerInterface.h"
}

namespace {
Tracer tracer_instance;
}

void Tracer::Insert(const std::string& function, uint32_t esp,
                    size_t num_params, const uint32_t* params) {
  std::lock_guard lock(tracer_mutex_);

  std::vector trace_params(params, params + num_params);
  CallKey key{
      .params = std::move(trace_params),
      .esp = esp,
  };
  auto entry = std::make_shared<TraceEntry>(next_id_++, std::move(key));
  entries_[function].insert(entry);
}

void Tracer::Remove(const std::string& function, uint32_t current_esp) {
  std::lock_guard lock(tracer_mutex_);
  auto it_map = entries_.find(function);
  if (it_map == entries_.end()) {
    return;
  }

  auto& entries = it_map->second;
  static constexpr uint32_t kStackThreshold = 256;

  auto best_it = entries.end();
  uint32_t min_diff = kStackThreshold;

  for (auto it = entries.begin(); it != entries.end(); ++it) {
    uint32_t entry_esp = (*it)->key.esp;

    uint32_t diff = (current_esp > entry_esp) ? (current_esp - entry_esp)
                                              : (entry_esp - current_esp);

    if (diff == 0) {
      entries.erase(it);
      return;
    }

    if (diff < min_diff) {
      min_diff = diff;
      best_it = it;
    }
  }

  if (best_it != entries.end()) {
    entries.erase(best_it);
    return;
  }

  std::cerr << "[WaitTrace] WARNING: Unmatched return for " << function
            << " at ESP 0x" << std::hex << current_esp << std::endl;
}

void Tracer::Count(const std::string& function, uint32_t esp, size_t num_params,
                   const uint32_t* params) {
  std::lock_guard lock(tracer_mutex_);
  std::vector trace_params(params, params + num_params);
  CallKey key{
      .params = trace_params,
      .esp = esp,
  };
  auto entry = std::make_shared<TraceEntry>(next_id_++, key);
  counters_[function].push_back(entry);
}

void Tracer::ProcessCommand(const std::string& args) {
  if (args == "dump" || args.empty()) {
    auto print_params = [](const std::vector<uint32_t>& params) {
      std::cerr << "{" << std::hex;
      std::string prefix;
      for (auto param : params) {
        std::cerr << prefix << param;
        prefix = ", ";
      }
      std::cerr << "}";
    };

    std::cerr << "[WaitTrace] Trace table:" << std::endl;
    std::lock_guard lock(tracer_mutex_);
    for (const auto& func_set : entries_) {
      std::cerr << "\t" << func_set.first << std::endl;
      for (const auto& entry : func_set.second) {
        std::cerr << "\t\t" << std::dec << entry->id << " params: ";
        print_params(entry->key.params);
        std::cerr << " $esp: " << entry->key.esp << std::endl;
      }
    }

    if (!counters_.empty()) {
      std::cerr << "Counters:" << std::endl;
      for (const auto& func_set : counters_) {
        std::cerr << "\t" << func_set.first << std::endl;

        std::map<CallKey, std::vector<uint32_t>> call_sites_to_ids;
        for (const auto& entry : func_set.second) {
          call_sites_to_ids[entry->key].push_back(entry->id);
        }

        for (const auto& entry : call_sites_to_ids) {
          std::cerr << "\t\t";
          print_params(entry.first.params);
          std::cerr << " esp:" << std::hex << entry.first.esp << std::dec
                    << std::endl;

          for (auto& id : entry.second) {
            std::cerr << "\t\t\t" << id << std::endl;
          }
        }
        std::cerr << std::endl;
      }
    }

    std::cerr << std::endl;
  } else if (args == "clear") {
    std::lock_guard lock(tracer_mutex_);
    entries_.clear();
    counters_.clear();
    std::cerr << "[WaitTrace] Traces cleared." << std::endl;
  } else {
    std::cerr << "[WaitTrace] Unknown command: " << args << std::endl;
    std::cerr << "Usage: plugin waittrace [dump|clear]" << std::endl;
  }
}

extern "C" void TracerPushEntry(const char* function, uint32_t esp,
                                size_t num_params, const uint32_t* params) {
  if (function) {
    tracer_instance.Insert(function, esp, num_params, params);
  }
}

extern "C" void TracerPopEntry(const char* function, uint32_t esp) {
  if (function) {
    tracer_instance.Remove(function, esp);
  }
}

extern "C" void TracerCmdCallback(const char* args) {
  // Robustness: ensure args is not null
  std::string safe_args = args ? args : "";
  tracer_instance.ProcessCommand(safe_args);
}

extern "C" void TracerCount(const char* function, uint32_t esp,
                            size_t num_params, const uint32_t* params) {
  if (function) {
    tracer_instance.Count(function, esp, num_params, params);
  }
}
