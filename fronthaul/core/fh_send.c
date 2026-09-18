/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_send.h"
#include <stdio.h>
#include <stdlib.h>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_pause.h>

#define FH_TX_RETRY_US 100

int fh_send_init(fh_send_t *send, fh_timer_t *timer, uint16_t port_id, uint16_t queue_id)
{
  send->timer = timer;
  send->port_id = port_id;
  send->queue_id = queue_id;
  send->tx_retry_cycles = rte_get_timer_hz() * FH_TX_RETRY_US / 1000000;
  rte_spinlock_init(&send->tx_lock);

  return 0;
}

uint16_t fh_send_immediate(fh_send_t *send, struct rte_mbuf **mbufs, uint32_t n)
{
  if (!send || n == 0 || n > UINT16_MAX)
    return 0;
  rte_spinlock_lock(&send->tx_lock);
  const uint64_t start_cycles = rte_get_timer_cycles();
  uint16_t sent = 0;
  do {
    // DPDK rte_eth_tx_burst() returns the accepted prefix; retry only the unsent suffix.
    sent += rte_eth_tx_burst(send->port_id, send->queue_id, mbufs + sent, (uint16_t)n - sent);
    if (sent == n)
      break;
    rte_pause();
  } while (rte_get_timer_cycles() - start_cycles < send->tx_retry_cycles);
  rte_spinlock_unlock(&send->tx_lock);
  return sent;
}
