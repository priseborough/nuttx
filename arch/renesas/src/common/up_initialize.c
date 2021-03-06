/****************************************************************************
 * arch/renesas/src/common/up_initialize.c
 *
 *   Copyright (C) 2008-2010, 2012-2013, 2015-2017 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/sched_note.h>
#include <nuttx/mm/iob.h>
#include <nuttx/drivers/drivers.h>
#include <nuttx/fs/loop.h>
#include <nuttx/net/loopback.h>
#include <nuttx/net/tun.h>
#include <nuttx/net/telnet.h>
#include <nuttx/syslog/syslog.h>
#include <nuttx/syslog/syslog_console.h>
#include <nuttx/serial/pty.h>
#include <nuttx/crypto/crypto.h>
#include <nuttx/power/pm.h>

#include "up_arch.h"
#include "up_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Define to enable timing loop calibration */

#undef CONFIG_ARCH_CALIBRATION

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_calibratedelay
 *
 * Description:
 *   Delay loops are provided for short timing loops.  This function, if
 *   enabled, will just wait for 100 seconds.  Using a stopwatch, you can
 *   can then determine if the timing loops are properly calibrated.
 *
 ****************************************************************************/

#if defined(CONFIG_ARCH_CALIBRATION) & defined(CONFIG_DEBUG_FEATURES)
static void up_calibratedelay(void)
{
  int i;

  swarn("Beginning 100s delay\n");
  for (i = 0; i < 100; i++)
    {
      up_mdelay(1000);
    }

  swarn("End 100s delay\n");
}
#else
# define up_calibratedelay()
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_initialize
 *
 * Description:
 *   up_initialize will be called once during OS initialization after the
 *   basic OS services have been initialized.  The architecture specific
 *   details of initializing the OS will be handled here.  Such things as
 *   setting up interrupt service routines, starting the clock, and
 *   registering device drivers are some of the things that are different
 *   for each processor and hardware platform.
 *
 *   up_initialize is called after the OS initialized but before the user
 *   initialization logic has been started and before the libraries have
 *   been initialized.  OS services and driver services are available.
 *
 ****************************************************************************/

void up_initialize(void)
{
  /* Initialize global variables */

  g_current_regs = NULL;

  /* Calibrate the timing loop */

  up_calibratedelay();

  /* Initialize the interrupt subsystem */

  up_irqinitialize();

#ifdef CONFIG_PM
  /* Initialize the power management subsystem.  This MCU-specific function
   * must be called *very* early in the initialization sequence *before* any
   * other device drivers are initialized (since they may attempt to register
   * with the power management subsystem).
   */

  up_pminitialize();
#endif

#if !defined(CONFIG_SUPPRESS_INTERRUPTS) && !defined(CONFIG_SUPPRESS_TIMER_INTS)
  /* Initialize the system timer interrupt */

  renesas_timer_initialize();
#endif

#ifdef CONFIG_MM_IOB
  /* Initialize IO buffering */

  iob_initialize();
#endif

#if CONFIG_NFILE_DESCRIPTORS > 0
  /* Register devices */

#if defined(CONFIG_DEV_NULL)
  devnull_register();   /* Standard /dev/null */
#endif

#if defined(CONFIG_DEV_RANDOM)
  devrandom_register(); /* Standard /dev/random */
#endif

#if defined(CONFIG_DEV_URANDOM)
  devurandom_register();   /* Standard /dev/urandom */
#endif

#if defined(CONFIG_DEV_ZERO)
  devzero_register();   /* Standard /dev/zero */
#endif

#if defined(CONFIG_DEV_LOOP)
  loop_register();      /* Standard /dev/loop */
#endif
#endif /* CONFIG_NFILE_DESCRIPTORS */

#if defined(CONFIG_SCHED_INSTRUMENTATION_BUFFER) && \
    defined(CONFIG_DRIVER_NOTE)
  note_register();      /* Non-standard /dev/note */
#endif

  /* Initialize the serial device driver */

#ifdef USE_SERIALDRIVER
  up_serialinit();
#endif

  /* Initialize the console device driver (if it is other than the standard
   * serial driver). NOTE that the naming implies that the console is a serial
   * driver.  That is usually the case, however, if no UARTs are enabled, the
   * console could als be provided through some other device, such as an LCD.
   * Architecture-specific logic will have to detect that case.
   */

#if defined(CONFIG_DEV_LOWCONSOLE)
  lowconsole_init();
#elif defined(CONFIG_CONSOLE_SYSLOG)
  syslog_console_init();
#elif defined(CONFIG_RAMLOG_CONSOLE)
  ramlog_consoleinit();
#endif

#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_PSEUDOTERM_SUSV1)
  /* Register the master pseudo-terminal multiplexor device */

  (void)ptmx_register();
#endif

  /* Early initialization of the system logging device.  Some SYSLOG channel
   * can be initialized early in the initialization sequence because they
   * depend on only minimal OS initialization.
   */

  syslog_initialize(SYSLOG_INIT_EARLY);

#if defined(CONFIG_CRYPTO)
  /* Initialize the HW crypto and /dev/crypto */

  up_cryptoinitialize();
#endif

#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_CRYPTO_CRYPTODEV)
  devcrypto_register();
#endif

#ifndef CONFIG_NETDEV_LATEINIT
  /* Initialize the network */

  up_netinitialize();
#endif

#ifdef CONFIG_NETDEV_LOOPBACK
  /* Initialize the local loopback device */

  (void)localhost_initialize();
#endif

#ifdef CONFIG_NET_TUN
  /* Initialize the TUN device */

  (void)tun_initialize();
#endif

#ifdef CONFIG_NETDEV_TELNET
  /* Initialize the Telnet session factory */

  (void)telnet_initialize();
#endif

  /* Initialize USB */

  up_usbinitialize();

  board_autoled_on(LED_IRQSENABLED);
}
