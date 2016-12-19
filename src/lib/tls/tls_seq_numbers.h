/*
* TLS Sequence Number Handling
* (C) 2012 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_TLS_SEQ_NUMBERS_H__
#define BOTAN_TLS_SEQ_NUMBERS_H__

#include <botan/types.h>

namespace Botan {

namespace TLS {

class Connection_Sequence_Numbers {
public:
  virtual ~Connection_Sequence_Numbers() {}

  virtual void new_read_cipher_state() = 0;
  virtual void new_write_cipher_state() = 0;

  virtual uint16_t current_read_epoch() const = 0;
  virtual uint16_t current_write_epoch() const = 0;

  virtual uint64_t next_write_sequence(uint16_t) = 0;
  virtual uint64_t next_read_sequence() = 0;

  virtual bool already_seen(uint64_t seq) const = 0;
  virtual void read_accept(uint64_t seq) = 0;
};

class Stream_Sequence_Numbers final : public Connection_Sequence_Numbers {
public:
  void new_read_cipher_state() override { m_read_seq_no = 0; m_read_epoch += 1; }
  void new_write_cipher_state() override { m_write_seq_no = 0; m_write_epoch += 1; }

  uint16_t current_read_epoch() const override { return m_read_epoch; }
  uint16_t current_write_epoch() const override { return m_write_epoch; }

  uint64_t next_write_sequence(uint16_t) override { return m_write_seq_no++; }
  uint64_t next_read_sequence() override { return m_read_seq_no; }

  bool already_seen(uint64_t) const override { return false; }
  void read_accept(uint64_t) override { m_read_seq_no++; }
private:
  uint64_t m_write_seq_no = 0;
  uint64_t m_read_seq_no = 0;
  uint16_t m_read_epoch = 0;
  uint16_t m_write_epoch = 0;
};

class Datagram_Sequence_Numbers final : public Connection_Sequence_Numbers {
public:
  Datagram_Sequence_Numbers() { m_write_seqs[0] = 0; }

  void new_read_cipher_state() override { m_read_epoch += 1; }

  void new_write_cipher_state() override {
    m_write_epoch += 1;
    m_write_seqs[m_write_epoch] = 0;
  }

  uint16_t current_read_epoch() const override { return m_read_epoch; }
  uint16_t current_write_epoch() const override { return m_write_epoch; }

  uint64_t next_write_sequence(uint16_t epoch) override {
    auto i = m_write_seqs.find(epoch);
    BOTAN_ASSERT(i != m_write_seqs.end(), "Found epoch");
    return (static_cast<uint64_t>(epoch) << 48) | i->second++;
  }

  uint64_t next_read_sequence() override {
    throw Exception("DTLS uses explicit sequence numbers");
  }

  bool already_seen(uint64_t sequence) const override {
    const size_t window_size = sizeof(m_window_bits) * 8;

    if (sequence > m_window_highest) {
      return false;
    }

    const uint64_t offset = m_window_highest - sequence;

    if (offset >= window_size) {
      return true;  // really old?
    }

    return (((m_window_bits >> offset) & 1) == 1);
  }

  void read_accept(uint64_t sequence) override {
    const size_t window_size = sizeof(m_window_bits) * 8;

    if (sequence > m_window_highest) {
      const size_t offset = sequence - m_window_highest;
      m_window_highest += offset;

      if (offset >= window_size) {
        m_window_bits = 0;
      }
      else {
        m_window_bits <<= offset;
      }

      m_window_bits |= 0x01;
    }
    else {
      const uint64_t offset = m_window_highest - sequence;
      m_window_bits |= (static_cast<uint64_t>(1) << offset);
    }
  }

private:
  std::map<uint16_t, uint64_t> m_write_seqs;
  uint16_t m_write_epoch = 0;
  uint16_t m_read_epoch = 0;
  uint64_t m_window_highest = 0;
  uint64_t m_window_bits = 0;
};

}

}

#endif
