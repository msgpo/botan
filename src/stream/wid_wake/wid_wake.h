/*
* WiderWake
* (C) 1999-2008 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#ifndef BOTAN_WIDER_WAKE_H__
#define BOTAN_WIDER_WAKE_H__

#include <botan/stream_cipher.h>

namespace Botan {

/**
* WiderWake4+1-BE
*
* Note: quite old and possibly not safe; use XSalsa20 or a block
* cipher in counter mode.
*/
class BOTAN_DLL WiderWake_41_BE : public StreamCipher
   {
   public:
      void cipher(const byte[], byte[], size_t);
      void set_iv(const byte[], size_t);

      bool valid_iv_length(size_t iv_len) const
         { return (iv_len == 8); }

      Key_Length_Specification key_spec() const
         {
         return Key_Length_Specification(16);
         }

      void clear();
      std::string name() const { return "WiderWake4+1-BE"; }
      StreamCipher* clone() const { return new WiderWake_41_BE; }

   private:
      void key_schedule(const byte[], size_t);

      void generate(size_t);

      secure_vector<u32bit> T;
      secure_vector<u32bit> state;
      secure_vector<u32bit> t_key;
      secure_vector<byte> buffer;
      size_t position;
   };

}

#endif
