#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tracer/TracerInterface.h"
#include "xemu/qemu-plugin-ext.h"

typedef struct {
  const char* function_name;
  size_t num_arguments;
  unsigned int argument_stack_offsets[16];
  bool count_only;
} TraceConfig;

static const uint64_t KeWaitForSingleObjectAddr = 0x80022f5a;
static const uint64_t KeWaitForSingleObjectRetAddr = 0x800231f9;
static const uint64_t KeWaitForSingleObjectRaiseAddr = 0x80022fe6;

static const uint64_t KeWaitForMultipleObjectsAddr = 0x80022c05;
static const uint64_t KeWaitForMultipleObjectsRetAddr = 0x80022d37;
static const uint64_t KeWaitForMultipleObjectsRaiseAddr = 0x80022f57;

static const uint64_t KeSetEventAddr = 0x80023baa;

static const TraceConfig g_trace_ke_wait_for_single_object = {
    .function_name = "KeWaitForSingleObject",
    .num_arguments = 1,
    .argument_stack_offsets = {4},  // Object is param 0
    .count_only = false,
};

static const TraceConfig g_trace_ke_wait_for_multiple_objects = {
    .function_name = "KeWaitForMultipleObjects",
    .num_arguments = 1,
    .argument_stack_offsets = {8},  // Objects array is param 1
    .count_only = false,
};

static const TraceConfig g_trace_ke_set_event = {
    .function_name = "KeSetEvent",
    .num_arguments = 1,
    .argument_stack_offsets = {4},  // Event object is param 0
    .count_only = true,
};

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

struct PluginContext {
  struct qemu_plugin_register* esp_handle;
};

static struct PluginContext context = {0};

static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu_index) {
  (void)id;
  (void)vcpu_index;

  if (context.esp_handle) {
    return;
  }

  GArray* regs = qemu_plugin_get_registers();
  if (!regs) {
    return;
  }

  for (guint i = 0; i < regs->len; i++) {
    qemu_plugin_reg_descriptor* desc =
        &g_array_index(regs, qemu_plugin_reg_descriptor, i);

    if (g_strcmp0(desc->name, "esp") == 0) {
      context.esp_handle = desc->handle;
      break;
    }
  }

  g_array_free(regs, TRUE);

  if (!context.esp_handle) {
    fprintf(stderr, "[WaitTrace] Error: Failed to resolve ESP register.\n");
  }
}

static bool get_stack_argument(uint32_t offset, uint32_t* out_arg,
                               uint32_t* out_esp) {
  assert(out_arg && out_esp);
  if (!context.esp_handle) {
    return false;
  }

  GByteArray* reg_data = g_byte_array_new();
  GByteArray* mem_data = g_byte_array_new();
  bool success = false;

  int read_len = qemu_plugin_read_register(context.esp_handle, reg_data);
  uint32_t esp_val = 0;
  if (read_len >= 4 && reg_data->len >= 4) {
    memcpy(&esp_val, reg_data->data, 4);
    *out_esp = esp_val;
    uint64_t arg_vaddr = esp_val + offset;

    if (qemu_plugin_read_memory_vaddr(arg_vaddr, mem_data, 4)) {
      if (mem_data->len >= 4) {
        memcpy(out_arg, mem_data->data, 4);
        success = true;
      }
    }
  }

  g_byte_array_free(reg_data, TRUE);
  g_byte_array_free(mem_data, TRUE);
  return success;
}

static void trace_entry_callback(unsigned int vcpu_index, void* userdata) {
  (void)vcpu_index;
  const TraceConfig* config = (const TraceConfig*)userdata;

  uint32_t esp;
  uint32_t params[16];
  for (size_t i = 0; i < config->num_arguments; ++i) {
    get_stack_argument(config->argument_stack_offsets[i], params + i, &esp);
  }

  if (config->count_only) {
    TracerCount(config->function_name, esp, config->num_arguments, params);
  } else {
    TracerPushEntry(config->function_name, esp, config->num_arguments, params);
  }
}

static void pop_entry(unsigned int vcpu_index, void* userdata) {
  (void)vcpu_index;
  (void)userdata;
  if (!context.esp_handle) {
    return;
  }

  const char* function_name = userdata;

  GByteArray* reg_data = g_byte_array_new();

  qemu_plugin_read_register(context.esp_handle, reg_data);
  uint32_t current_esp = 0;
  if (reg_data->len >= 4) {
    memcpy(&current_esp, reg_data->data, 4);
    TracerPopEntry(function_name, current_esp);
  }

  g_byte_array_free(reg_data, TRUE);
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb* tb) {
  (void)id;

  size_t n = qemu_plugin_tb_n_insns(tb);

  for (size_t i = 0; i < n; ++i) {
    struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, i);
    uint64_t vaddr = qemu_plugin_insn_vaddr(insn);

    if (vaddr == KeWaitForSingleObjectAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(
          insn, trace_entry_callback, QEMU_PLUGIN_CB_R_REGS,
          (void*)&g_trace_ke_wait_for_single_object);
    } else if (vaddr == KeWaitForSingleObjectRetAddr ||
               vaddr == KeWaitForSingleObjectRaiseAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(
          insn, pop_entry, QEMU_PLUGIN_CB_R_REGS,
          (void*)g_trace_ke_wait_for_single_object.function_name);
    } else if (vaddr == KeSetEventAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(insn, trace_entry_callback,
                                             QEMU_PLUGIN_CB_R_REGS,
                                             (void*)&g_trace_ke_set_event);
    } else if (vaddr == KeWaitForMultipleObjectsAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(
          insn, trace_entry_callback, QEMU_PLUGIN_CB_R_REGS,
          (void*)&g_trace_ke_wait_for_multiple_objects);
    } else if (vaddr == KeWaitForMultipleObjectsRetAddr ||
               vaddr == KeWaitForMultipleObjectsRaiseAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(
          insn, pop_entry, QEMU_PLUGIN_CB_R_REGS,
          (void*)g_trace_ke_wait_for_multiple_objects.function_name);
    }
  }
}

static void plugin_exit(qemu_plugin_id_t id, void* p) {
  (void)id;
  (void)p;
  qemu_plugin_unregister_command("wait_trace");
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t* info, int argc,
                                           char** argv) {
  (void)info;
  (void)argc;
  (void)argv;

  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  qemu_plugin_register_command("waittrace", TracerCmdCallback);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
