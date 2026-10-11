// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2023 The Android Open Source Project
 */

#include <fastboot.h>
#include <net.h>
#include <net/fastboot_tcp.h>
#include <net/tcp.h>
#include <linux/kernel.h>

#define FASTBOOT_TCP_PORT	5554

static const unsigned short handshake_length = 4;
static const uchar *handshake = "FB01";

static char rxbuf[sizeof(u64) + FASTBOOT_COMMAND_LEN + 1];
static char txbuf[sizeof(u64) + FASTBOOT_RESPONSE_LEN + 1];

/* stream offset of the first byte not consumed yet (commands and data) */
static u32 data_read;
static u32 tx_last_offs, tx_last_len;

/*
 * Download data phase: after "download:<size>" the client sends the image as
 * length prefixed packets (8-byte big-endian length, then that many bytes),
 * normally a single packet holding the whole image.
 */
static u64 dl_pkt_remaining;
static u8 dl_hdr[sizeof(u64)];
static int dl_hdr_len;

static void queue_response(void)
{
	__be64 len_be;
	int len;

	len = strlen(txbuf + sizeof(u64));
	len_be = __cpu_to_be64(len);
	memcpy(txbuf, &len_be, sizeof(u64));

	tx_last_offs += tx_last_len;
	tx_last_len = len + sizeof(u64);
}

/**
 * dl_consume() - Feed in-order bytes of the data phase to the download
 *
 * @buf: Received bytes, starting at stream offset data_read
 * @len: Number of bytes
 * Return: number of bytes consumed by the download (the rest belong to the
 *	next command), or -1 on a protocol error
 */
static int dl_consume(const char *buf, int len)
{
	int left = len;

	while (left && fastboot_data_remaining()) {
		u64 n;

		if (!dl_pkt_remaining) {
			/* packet header */
			n = min_t(u64, left, sizeof(dl_hdr) - dl_hdr_len);
			memcpy(dl_hdr + dl_hdr_len, buf, n);
			dl_hdr_len += n;
			buf += n;
			left -= n;
			if (dl_hdr_len == sizeof(dl_hdr)) {
				memcpy(&dl_pkt_remaining, dl_hdr, sizeof(u64));
				dl_pkt_remaining = __be64_to_cpu(dl_pkt_remaining);
				dl_hdr_len = 0;
				if (!dl_pkt_remaining ||
				    dl_pkt_remaining > fastboot_data_remaining()) {
					printf("fastboot: bad data packet length\n");
					return -1;
				}
			}
			continue;
		}

		n = min3((u64)left, dl_pkt_remaining, (u64)fastboot_data_remaining());
		fastboot_data_download(buf, n, txbuf + sizeof(u64));
		if (!strncmp("FAIL", txbuf + sizeof(u64), 4)) {
			queue_response();
			return -1;
		}
		dl_pkt_remaining -= n;
		buf += n;
		left -= n;
	}

	if (!fastboot_data_remaining()) {
		fastboot_data_complete(txbuf + sizeof(u64));
		queue_response();
		dl_pkt_remaining = 0;
		dl_hdr_len = 0;
	}

	return len - left;
}

static void tcp_stream_on_rcv_nxt_update(struct tcp_stream *tcp, u32 rx_offs)
{
	u64	cmd_size;
	char	saved;
	int	fastboot_command_id, avail;

	if (!data_read && rx_offs >= handshake_length) {
		if (memcmp(rxbuf, handshake, handshake_length)) {
			printf("fastboot: bad handshake\n");
			tcp_stream_close(tcp);
			return;
		}

		tx_last_offs = 0;
		tx_last_len = handshake_length;
		memcpy(txbuf, handshake, handshake_length);

		data_read += handshake_length;
		avail = rx_offs - data_read;
		if (avail > 0)
			memmove(rxbuf, rxbuf + handshake_length, avail);
		return;
	}

	/* data phase: consumed in tcp_stream_rx() */
	if (fastboot_data_remaining())
		return;

	avail = rx_offs - data_read;
	if (avail < sizeof(u64))
		return;

	memcpy(&cmd_size, rxbuf, sizeof(u64));
	cmd_size = __be64_to_cpu(cmd_size);
	if (cmd_size > FASTBOOT_COMMAND_LEN) {
		printf("fastboot: command too long\n");
		tcp_stream_close(tcp);
		return;
	}
	if (avail < sizeof(u64) + cmd_size)
		return;

	saved = rxbuf[sizeof(u64) + cmd_size];
	rxbuf[sizeof(u64) + cmd_size] = '\0';
	fastboot_command_id = fastboot_handle_command(rxbuf + sizeof(u64),
						      txbuf + sizeof(u64));
	fastboot_handle_boot(fastboot_command_id,
			     strncmp("OKAY", txbuf + sizeof(u64), 4) != 0);
	rxbuf[sizeof(u64) + cmd_size] = saved;

	queue_response();

	data_read += sizeof(u64) + cmd_size;
	avail -= sizeof(u64) + cmd_size;
	if (avail > 0)
		memmove(rxbuf, rxbuf + sizeof(u64) + cmd_size, avail);

	/* "download:" accepted: what follows is the image data */
	if (fastboot_data_remaining() && avail > 0) {
		int n = dl_consume(rxbuf, avail);

		if (n < 0) {
			tcp_stream_close(tcp);
			return;
		}
		data_read += n;
		avail -= n;
		if (avail > 0)
			memmove(rxbuf, rxbuf + n, avail);
	}
}

static int tcp_stream_rx(struct tcp_stream *tcp, u32 rx_offs, void *buf, int len)
{
	int n = 0;

	if (fastboot_data_remaining()) {
		/*
		 * Image data goes straight to the download buffer, in order:
		 * a segment arriving early is not accepted, the peer
		 * retransmits it after the gap is filled.
		 */
		if (rx_offs != data_read)
			return 0;
		n = dl_consume(buf, len);
		if (n < 0)
			return -1;
		data_read += n;
		rx_offs += n;
		buf += n;
		len -= n;
	}

	/* command bytes wait in rxbuf until they are complete */
	if (rx_offs - data_read + len > sizeof(rxbuf))
		return -1;
	memcpy(rxbuf + rx_offs - data_read, buf, len);

	return n + len;
}

static int tcp_stream_tx(struct tcp_stream *tcp, u32 tx_offs, void *buf, int maxlen)
{
	/* by design: tx_offs >= tx_last_offs */
	if (tx_offs >= tx_last_offs + tx_last_len)
		return 0;

	maxlen = tx_last_offs + tx_last_len - tx_offs;
	memcpy(buf, txbuf + (tx_offs - tx_last_offs), maxlen);

	return maxlen;
}

static int tcp_stream_on_create(struct tcp_stream *tcp)
{
	if (tcp->lport != FASTBOOT_TCP_PORT)
		return 0;

	data_read = 0;
	tx_last_offs = 0;
	tx_last_len = 0;
	dl_pkt_remaining = 0;
	dl_hdr_len = 0;

	tcp->on_rcv_nxt_update = tcp_stream_on_rcv_nxt_update;
	tcp->rx = tcp_stream_rx;
	tcp->tx = tcp_stream_tx;

	return 1;
}

void fastboot_tcp_start_server(void)
{
	memset(net_server_ethaddr, 0, 6);
	tcp_stream_set_on_create_handler(tcp_stream_on_create);

	printf("Using %s device\n", eth_get_name());
	printf("Listening for fastboot command on tcp %pI4\n", &net_ip);
}
