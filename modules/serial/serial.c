/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck/common/basic_defs.h>
#include <tilck/common/printk.h>
#include <tilck/common/atomics.h>

#include <tilck/kernel/modules.h>
#include <tilck/kernel/hal.h>
#include <tilck/kernel/irq.h>
#include <tilck/kernel/tasklet.h>
#include <tilck/kernel/cmdline.h>
#include <tilck/kernel/tty.h>
#include <tilck/kernel/sched.h>

#include <tilck/mods/serial.h>

/* NOTE: hw-specific stuff in generic code. TODO: fix that. */
static const u16 com_ports[] = {COM1, COM2, COM3, COM4};

static int serial_port_tasklet_runner;
static struct tty *serial_ttys[4];
static ATOMIC(int) tasklets_per_port[4];

static void serial_con_bh_handler(u16 portn)
{
   struct tty *const t = serial_ttys[portn];
   const u16 p = com_ports[portn];
   char c;

   while (serial_read_ready(p)) {

      c = serial_read(p);
      tty_send_keyevent(t, make_key_event(0, c, true), true);
   }

   tasklets_per_port[portn]--;
}

static enum irq_action serial_con_irq_handler(void *ctx)
{
   const u32 portn = (u32)ctx;

   if (!serial_read_ready(com_ports[portn]))
      return IRQ_UNHANDLED; /* Not an IRQ from this "device" [irq sharing] */

   if (tasklets_per_port[portn] >= 2)
      return IRQ_FULLY_HANDLED;

   if (!enqueue_tasklet1(serial_port_tasklet_runner,
                         &serial_con_bh_handler, portn))
   {
      panic("[serial] hit tasklet queue limit");
   }

   tasklets_per_port[portn]++;
   return IRQ_REQUIRES_BH;
}

void early_init_serial_ports(void)
{
   init_serial_port(COM1);
   init_serial_port(COM2);
   init_serial_port(COM3);
   init_serial_port(COM4);
}

DEFINE_IRQ_HANDLER_NODE(com1, serial_con_irq_handler, (void *)0);
DEFINE_IRQ_HANDLER_NODE(com2, serial_con_irq_handler, (void *)1);
DEFINE_IRQ_HANDLER_NODE(com3, serial_con_irq_handler, (void *)2);
DEFINE_IRQ_HANDLER_NODE(com4, serial_con_irq_handler, (void *)3);

static void init_serial_comm(void)
{
   disable_preemption();
   {
      serial_port_tasklet_runner =
         create_tasklet_thread(1 /* priority */, KB_TASKLETS_QUEUE_SIZE);

      if (serial_port_tasklet_runner < 0)
         panic("Serial: Unable to create a tasklet runner thread for IRQs");
   }
   enable_preemption();

   for (int i = 0; i < 4; i++)
      serial_ttys[i] = get_serial_tty(i);

   irq_install_handler(X86_PC_COM1_COM3_IRQ, &com1);
   irq_install_handler(X86_PC_COM1_COM3_IRQ, &com3);
   irq_install_handler(X86_PC_COM2_COM4_IRQ, &com2);
   irq_install_handler(X86_PC_COM2_COM4_IRQ, &com4);
}

static struct module serial_module = {

   .name = "serial",
   .priority = 400,
   .init = &init_serial_comm,
};

REGISTER_MODULE(&serial_module);
