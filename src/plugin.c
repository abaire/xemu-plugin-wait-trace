#include <glib.h>
#include <qemu-plugin.h>
#include <stdio.h>
#include <string.h>

#include "tracer/TracerInterface.h"

static const uint64_t KeWaitForSingleObjectAddr = 0x80022f5a;
static const uint64_t KeWaitForSingleObjectRet = 0x800231f9;
static const char KeWaitForSingleObjectTag[] = "KeWaitForSingleObject";

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

struct PluginContext {
  struct qemu_plugin_register *esp_handle;
};

static struct PluginContext context = {0};

static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu_index) {
  (void)id;
  (void)vcpu_index;

  if (context.esp_handle) {
    return;
  }

  GArray *regs = qemu_plugin_get_registers();
  if (!regs) {
    return;
  }

  for (guint i = 0; i < regs->len; i++) {
    qemu_plugin_reg_descriptor *desc =
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

static void on_target_exec(unsigned int vcpu_index, void *userdata) {
  (void)userdata;
  (void)vcpu_index;

  if (!context.esp_handle) {
    return;
  }

  const char *function_name = userdata;

  GByteArray *reg_data = g_byte_array_new();
  GByteArray *mem_data = g_byte_array_new();

  int read_len = qemu_plugin_read_register(context.esp_handle, reg_data);

  uint32_t esp_val = 0;
  if (read_len >= 4 && reg_data->len >= 4) {
    memcpy(&esp_val, reg_data->data, 4);
    uint64_t arg_vaddr = esp_val + 4;

    if (qemu_plugin_read_memory_vaddr(arg_vaddr, mem_data, 4)) {
      if (mem_data->len >= 4) {
        uint32_t tag_val = 0;
        memcpy(&tag_val, mem_data->data, 4);

        TracerPushEntry(function_name, tag_val, esp_val);
      }
    }
  }

  g_byte_array_free(reg_data, TRUE);
  g_byte_array_free(mem_data, TRUE);
}

static void on_target_return(unsigned int vcpu_index, void *userdata) {
  (void)vcpu_index;
  (void)userdata;
  if (!context.esp_handle) {
    return;
  }

  const char *function_name = userdata;

  GByteArray *reg_data = g_byte_array_new();

  qemu_plugin_read_register(context.esp_handle, reg_data);
  uint32_t current_esp = 0;
  if (reg_data->len >= 4) {
    memcpy(&current_esp, reg_data->data, 4);
    TracerPopEntry(function_name, current_esp);
  }

  g_byte_array_free(reg_data, TRUE);
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  (void)id;
  uint64_t vaddr = qemu_plugin_tb_vaddr(tb);

  struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, 0);

  if (vaddr == KeWaitForSingleObjectAddr) {
    qemu_plugin_register_vcpu_insn_exec_cb(insn, on_target_exec,
                                           QEMU_PLUGIN_CB_R_REGS,
                                           (void *)KeWaitForSingleObjectTag);
  } else if (vaddr == KeWaitForSingleObjectRet) {
    qemu_plugin_register_vcpu_insn_exec_cb(insn, on_target_return,
                                           QEMU_PLUGIN_CB_R_REGS,
                                           (void *)KeWaitForSingleObjectTag);
  }
}

static void plugin_exit(qemu_plugin_id_t id, void *p) {
  (void)id;
  (void)p;
  qemu_plugin_unregister_command("wait_trace");
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv) {
  (void)info;
  (void)argc;
  (void)argv;

  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  qemu_plugin_register_command("waittrace", TracerCmdCallback);
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
