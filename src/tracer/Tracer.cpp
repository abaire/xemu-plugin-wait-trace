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

void Tracer::Insert(const std::string &function, uint32_t object_param,
                    uint32_t esp) {
  std::lock_guard<std::mutex> lock(tracer_mutex);
  wait_entries[function].insert(std::make_pair(object_param, esp));
}

void Tracer::Remove(const std::string &function, uint32_t current_esp) {
  std::lock_guard<std::mutex> lock(tracer_mutex);
  auto it_map = wait_entries.find(function);
  if (it_map == wait_entries.end()) {
    return;
  }

  std::set<ObjectEspPair> &entries = it_map->second;
  static constexpr uint32_t kStackThreshold = 256;

  for (auto it = entries.begin(); it != entries.end(); ++it) {
    uint32_t entry_esp = it->second;

    uint32_t diff = (current_esp > entry_esp) ? (current_esp - entry_esp)
                                              : (entry_esp - current_esp);

    if (diff < kStackThreshold) {
      entries.erase(it);
      return;
    }
  }

  std::cerr << "[Trace] WARNING: Unmatched return for " << function
            << " at ESP 0x" << std::hex << current_esp << std::endl;
}

void Tracer::ProcessCommand(const std::string &args) {

  if (args == "dump" || args.empty()) {
    std::cerr << "[WaitTrace] Wait table:" << std::endl;
    std::lock_guard<std::mutex> lock(tracer_mutex);
    for (const auto &func_set : wait_entries) {
      auto &func = func_set.first;
      std::cerr << "\t" << func_set.first << std::endl;
      for (auto &entry : func_set.second) {
        auto object = entry.first;
        auto esp = entry.second;

        std::cerr << "\t\t" << object << " $esp: " << esp << std::endl;
      }
    }

    std::cerr << std::endl;
  } else if (args == "clear") {
    std::lock_guard<std::mutex> lock(tracer_mutex);
    wait_entries.clear();
    // Use qemu_plugin_outs if you want to verify it worked in the log,
    // or just print to stderr
    std::cerr << "[WaitTrace] Table cleared." << std::endl;
  } else {
    std::cerr << "[WaitTrace] Unknown command: " << args << std::endl;
    std::cerr << "Usage: plugin waittrace [dump|clear]" << std::endl;
  }
}

extern "C" void TracerPushEntry(const char *function, uint32_t object_param,
                                uint32_t esp) {
  if (function) {
    tracer_instance.Insert(function, object_param, esp);
  }
}

extern "C" void TracerPopEntry(const char *function, uint32_t esp) {
  if (function) {
    tracer_instance.Remove(function, esp);
  }
}

extern "C" void TracerCmdCallback(const char *args) {
  // Robustness: ensure args is not null
  std::string safe_args = args ? args : "";
  tracer_instance.ProcessCommand(safe_args);
}
