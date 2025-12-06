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

void Tracer::Insert(const std::string& function, uint32_t object_param,
                    uint32_t esp) {
  std::lock_guard<std::mutex> lock(tracer_mutex_);
  WaitEntry entry{
      .id = next_id_++,
      .wait_object = object_param,
      .esp = esp,
  };
  wait_entries_[function].insert(entry);
}

void Tracer::Remove(const std::string& function, uint32_t current_esp) {
  std::lock_guard<std::mutex> lock(tracer_mutex_);
  auto it_map = wait_entries_.find(function);
  if (it_map == wait_entries_.end()) {
    return;
  }

  std::set<WaitEntry>& entries = it_map->second;
  static constexpr uint32_t kStackThreshold = 256;

  auto best_it = entries.end();
  uint32_t min_diff = kStackThreshold;

  for (auto it = entries.begin(); it != entries.end(); ++it) {
    uint32_t entry_esp = it->esp;

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

void Tracer::Signal(const std::string&, uint32_t object_param, uint32_t) {
  std::lock_guard<std::mutex> lock(tracer_mutex_);
  signal_calls_[object_param]++;
}

void Tracer::ProcessCommand(const std::string& args) {
  if (args == "dump" || args.empty()) {
    std::cerr << "[WaitTrace] Wait table:" << std::endl;
    std::lock_guard<std::mutex> lock(tracer_mutex_);
    for (const auto& func_set : wait_entries_) {
      std::cerr << "\t" << func_set.first << std::endl;
      for (const auto& entry : func_set.second) {
        std::cerr << "\t\t" << std::dec << entry.id << " obj: " << std::hex
                  << entry.wait_object << " $esp: " << entry.esp << std::endl;
      }
    }

    if (!signal_calls_.empty()) {
      std::cerr << "Signal calls:" << std::endl;
      for (const auto& entry : signal_calls_) {
        std::cerr << "\t" << std::hex << entry.first << ": " << std::dec
                  << entry.second << std::endl;
      }
    }

    std::cerr << std::endl;
  } else if (args == "clear") {
    std::lock_guard<std::mutex> lock(tracer_mutex_);
    wait_entries_.clear();
    signal_calls_.clear();
    std::cerr << "[WaitTrace] Table cleared." << std::endl;
  } else {
    std::cerr << "[WaitTrace] Unknown command: " << args << std::endl;
    std::cerr << "Usage: plugin waittrace [dump|clear]" << std::endl;
  }
}

extern "C" void TracerPushEntry(const char* function, uint32_t object_param,
                                uint32_t esp) {
  if (function) {
    tracer_instance.Insert(function, object_param, esp);
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

extern "C" void TracerSignal(const char* function, uint32_t object_param,
                             uint32_t esp) {
  if (function) {
    tracer_instance.Signal(function, object_param, esp);
  }
}
