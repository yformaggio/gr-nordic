/* -*- c++ -*- */
/*
 * Copyright 2016 Bastille Networks.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/fxpt.h>
#include "nordic_tx_impl.h"
#include "nordictap.h"
#include "shockburst_packet.h"

namespace gr {
  namespace nordic {

    nordic_tx::sptr
    nordic_tx::make(uint8_t channel_count)
    {
      return gnuradio::get_initial_sptr
        (new nordic_tx_impl(channel_count));
    }

    /*
     * The private constructor
     */
    nordic_tx_impl::nordic_tx_impl(uint8_t channel_count)
      : gr::sync_block("nordic_tx",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1, channel_count, sizeof(uint8_t))),
        m_channel_count(channel_count)
    {
      // Register nordictap input, which accepts packets to transmit
      message_port_register_in(pmt::intern("nordictap_in"));
      set_msg_handler(pmt::intern("nordictap_in"), boost::bind(&nordic_tx_impl::nordictap_message_handler, this, _1));
    }

    /*
     * Our virtual destructor.
     */
    nordic_tx_impl::~nordic_tx_impl()
    {
    }

    // Incoming message handler
    void nordic_tx_impl::nordictap_message_handler(pmt::pmt_t msg)
    {
      m_tx_queue.push(msg);
    }

    int
    nordic_tx_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      // uint8_t *out = (uint8_t *) output_items[0];

      // Check for new input
      if(m_tx_queue.size())
      {
        // Get the blob
        std::vector<uint8_t> vec = pmt::u8vector_elements(m_tx_queue.front());
        uint8_t * blob = vec.data();

        // Read the channel index
        uint8_t channel = blob[0];

        // Read the nordictap header, address, and payload
        nordictap_header header;
        memcpy(&header, &blob[1], sizeof(nordictap_header));

        // Read the address and payload
        const uint8_t alen = header.address_length;
        const uint8_t plen = header.payload_length;
        uint8_t * address = new uint8_t[alen];
        uint8_t * payload = new uint8_t[plen];
        memcpy(address, &blob[sizeof(nordictap_header)+1], alen);
        memcpy(payload, &blob[sizeof(nordictap_header)+1 + alen], plen);

        // Build the packet
        shockburst_packet * packet =
          new shockburst_packet(header.address_length,
                                         header.payload_length,
//                                         header.sequence_number,
//                                         header.no_ack,
                                         header.crc_length,
                                         address,
                                         payload);

        // Remove the blob from the queue
        m_tx_queue.pop();

        // Write the output bytes
        uint8_t * out = (uint8_t *)output_items[channel];
        for(int b = 0; b < packet->bytes_length(); b++)
        {
          out[b] = packet->bytes()[b];
          out[packet->bytes_length()*2+b] = packet->bytes()[b];
        }

        // Write zeros to the other channels' buffers
        for(int c = 0; c < m_channel_count; c++)
        {
          if(c != channel)
          {
            memset(output_items[c], 0, packet->bytes_length()*2);
          }
        }

        // Cleanup
        delete[] address;
        delete[] payload;
        delete packet;

        // Return the number of bytes produced
        return packet->bytes_length()*2;
      }
      else
      {
        return 0;
      }
    }

    // Process a crc byte (or partial byte)
    uint16_t nordic_tx_impl::crc_update (uint16_t crc, uint8_t data, uint8_t bits)
    {
      crc = crc ^ ((uint16_t)data << 8);
      for (int x = 0; x < bits; x++)
      {
        if(crc & 0x8000) crc = (crc << 1) ^ 0x1021;
        else crc <<= 1;
      }
      return crc;
    }

  } /* namespace nordic */
} /* namespace gr */

