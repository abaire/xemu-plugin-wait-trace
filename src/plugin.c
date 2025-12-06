#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "tracer/TracerInterface.h"
#include "xemu/qemu-plugin-ext.h"

static const uint64_t KeWaitForSingleObjectAddr = 0x80022f5a;
static const uint64_t KeWaitForSingleObjectRetAddr = 0x800231f9;
static const uint64_t KeWaitForSingleObjectRaiseAddr = 0x80022fe6;
static const char KeWaitForSingleObjectTag[] = "KeWaitForSingleObject";

static const uint64_t KeWaitForMultipleObjectsAddr = 0x80022c05;
static const uint64_t KeWaitForMultipleObjectsRetAddr = 0x80022d37;
static const uint64_t KeWaitForMultipleObjectsRaiseAddr = 0x80022f57;
static const char KeWaitForMultipleObjectsTag[] = "KeWaitForMultipleObjects";

static const uint64_t KeSetEventAddr = 0x80023baa;
static const char KeSetEventTag[] = "KeSetEvent";

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
    if (out_esp) {
      *out_esp = esp_val;
    }
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

static void wait_for_single_object_enter(unsigned int vcpu_index,
                                         void* userdata) {
  (void)userdata;
  (void)vcpu_index;

  uint32_t esp;
  uint32_t object_param;

  if (get_stack_argument(4, &object_param, &esp)) {
    const char* function_name = userdata;
    TracerPushEntry(function_name, object_param, esp);
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

static void wait_for_multiple_objects_enter(unsigned int vcpu_index,
                                            void* userdata) {
  (void)userdata;
  (void)vcpu_index;

  uint32_t esp;
  uint32_t object_array_param;

  if (get_stack_argument(8, &object_array_param, &esp)) {
    const char* function_name = userdata;
    TracerPushEntry(function_name, object_array_param, esp);
  }
}

static void ke_set_event_enter(unsigned int vcpu_index, void* userdata) {
  (void)userdata;
  (void)vcpu_index;
  uint32_t esp;
  uint32_t object_param;

  if (get_stack_argument(4, &object_param, &esp)) {
    const char* function_name = userdata;

    TracerSignal(function_name, object_param, esp);
  }
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb* tb) {
  (void)id;

  // Iterate over every instruction in the block
  size_t n = qemu_plugin_tb_n_insns(tb);

  for (size_t i = 0; i < n; ++i) {
    struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, i);
    uint64_t vaddr = qemu_plugin_insn_vaddr(insn);

    // Check individual instruction addresses
    if (vaddr == KeWaitForSingleObjectAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(insn, wait_for_single_object_enter,
                                             QEMU_PLUGIN_CB_R_REGS,
                                             (void*)KeWaitForSingleObjectTag);
    } else if (vaddr == KeWaitForSingleObjectRetAddr ||
               vaddr == KeWaitForSingleObjectRaiseAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(insn, pop_entry,
                                             QEMU_PLUGIN_CB_R_REGS,
                                             (void*)KeWaitForSingleObjectTag);
    } else if (vaddr == KeSetEventAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(insn, ke_set_event_enter,
                                             QEMU_PLUGIN_CB_R_REGS,
                                             (void*)KeSetEventTag);
    } else if (vaddr == KeWaitForMultipleObjectsAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(
          insn, wait_for_multiple_objects_enter, QEMU_PLUGIN_CB_R_REGS,
          (void*)KeWaitForMultipleObjectsTag);
    } else if (vaddr == KeWaitForMultipleObjectsRetAddr ||
               vaddr == KeWaitForMultipleObjectsRaiseAddr) {
      qemu_plugin_register_vcpu_insn_exec_cb(
          insn, pop_entry, QEMU_PLUGIN_CB_R_REGS,
          (void*)KeWaitForMultipleObjectsTag);
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
