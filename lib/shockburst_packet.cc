/* -*- c++ -*- */
/*
 * Copyright 2017 Istuary Innovations Labs, Inc.
 * Based on original work from Marc Newlin for Bastille Networks
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

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "shockburst_packet.h"

shockburst_packet::shockburst_packet(uint8_t address_length,
		    uint8_t payload_length,
		    uint8_t crc_length,
		    uint8_t *address,
		    uint8_t *payload
		    ) :
  m_address_length(address_length),
  m_payload_length(payload_length),
  m_crc_length(crc_length)
{
  // Allocate buffers
  m_address = new uint8_t[m_address_length];
  m_payload = new uint8_t[m_payload_length];
  m_crc = new uint8_t[m_crc_length];

  // Copy over address and payload
  memcpy(m_address, address, m_address_length);
  memcpy(m_payload, payload, m_payload_length);

  // Build packet bytes
  const int blen = 1 /* preamble */ +
    m_crc_length +
    m_address_length +
    m_payload_length;

  m_packet_length_bytes = blen;
  m_packet_length_bits = blen * 8;
  m_packet_bytes = new uint8_t[blen];

  // Preamble
  if ((address[0] & 0x80) == 0x80)
    m_packet_bytes[0] = 0xAA;
  else
    m_packet_bytes[0] = 0x55;

  // Address
  memcpy(&m_packet_bytes[1], address, m_address_length);

  // Payload
  for (int b = 0; b < m_payload_length; b++)
    m_packet_bytes[1 + m_address_length + b] = payload[b];

  // Calculate the CRC
  uint16_t crc = 0xFFFF;
  for(int b = 1; b < 7 + m_payload_length; b++)
    crc = crc_update(crc, m_packet_bytes[b]);
  crc = crc_update(crc, m_packet_bytes[6 + m_payload_length] & 0x80, 1);
  memcpy(m_crc, &crc, m_crc_length);

  m_packet_bytes[1 + m_address_length + m_payload_length] = m_crc[0];
  m_packet_bytes[2 + m_address_length + m_payload_length] = m_crc[1];
}

// Destructor
shockburst_packet::~shockburst_packet()
{
  delete[] m_address;
  delete[] m_payload;
  delete[] m_crc;
}


// Attempt to parse a packet from some incoming bytes
bool shockburst_packet::try_parse(const uint8_t *bytes,
		      const uint8_t *bytes_shifted,
		      uint8_t address_length,
		      uint8_t crc_length,
		      shockburst_packet *& packet)
{
  // Read the payload length
  uint8_t payload_length = 11; // TODO: find a way to calculate payload length

  // Read the address
  uint8_t *address = new uint8_t[address_length];
  memcpy(address, &bytes[1], address_length);

  // Read the RX crc
  uint16_t crc_rx;
  memcpy(&crc_rx, &bytes[1 + address_length + payload_length], 2);

  // Calculate the CRC
  uint16_t crc = 0xFFFF;
  for (int b = 1; b < 1 + address_length + payload_length; b++) {
    crc = crc_update(crc, bytes[b]);
  }
  crc = htons(crc);

  // Validate the CRC
  if (memcmp(&crc, &crc_rx, 2) != 0){
    delete[] address;
    return false;
  }

  // Read the payload
  uint8_t payload[32];
  memcpy(payload, &bytes[1 + address_length], payload_length);

  // Updates the fields
  packet = new shockburst_packet(address_length, payload_length, crc_length, address, payload);

  // Cleanup
  delete[] address;

  return true;
}


// Print the packet details on standard output
void shockburst_packet::print(){
  printf("Address: ");
  for (int x = 0; x < m_address_length; x++)
    printf("%02X ", m_address[x]);
  printf("\n");

  printf("Payload: ");
  for (int x = 0; x < m_payload_length; x++)
    printf("%02X ", m_payload[x]);
  printf("\n");

  printf("CRC:     ");
  for (int x = 0; x < m_crc_length; x++)
    printf("%02X ", m_crc[x]);
  printf("\n");

  printf("Bytes:   ");
  for (int x = 0; x < m_packet_length_bytes; x++)
    printf("%02X ", m_packet_bytes[x]);
  printf("\n");

  printf("\n");
}


// Process a CRC byte
uint16_t shockburst_packet::crc_update(uint16_t crc, uint8_t data, uint8_t bits) {
  crc = crc ^ ((uint16_t)data << 8);

  for (int x = 0; x < bits; x++) {
    if (crc & 0x8000){
      crc = (crc << 1) ^ 0x1021;
    }
    else {
      crc <<= 1;
    }
  }

  return crc;
}
