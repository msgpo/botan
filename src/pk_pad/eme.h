/*
* EME Classes
* (C) 1999-2007 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#ifndef BOTAN_PUBKEY_EME_ENCRYPTION_PAD_H__
#define BOTAN_PUBKEY_EME_ENCRYPTION_PAD_H__

#include <botan/secmem.h>
#include <botan/rng.h>

namespace Botan {

/**
* Encoding Method for Encryption
*/
class BOTAN_DLL EME
   {
   public:
      /**
      * Return the maximum input size in bytes we can support
      * @param keybits the size of the key in bits
      * @return upper bound of input in bytes
      */
      virtual size_t maximum_input_size(size_t keybits) const = 0;

      /**
      * Encode an input
      * @param in the plaintext
      * @param in_length length of plaintext in bytes
      * @param key_length length of the key in bits
      * @param rng a random number generator
      * @return encoded plaintext
      */
      secure_vector<byte> encode(const byte in[],
                                size_t in_length,
                                size_t key_length,
                                RandomNumberGenerator& rng) const;

      /**
      * Encode an input
      * @param in the plaintext
      * @param key_length length of the key in bits
      * @param rng a random number generator
      * @return encoded plaintext
      */
      secure_vector<byte> encode(const secure_vector<byte>& in,
                                size_t key_length,
                                RandomNumberGenerator& rng) const;

      /**
      * Decode an input
      * @param in the encoded plaintext
      * @param in_length length of encoded plaintext in bytes
      * @param key_length length of the key in bits
      * @return plaintext
      */
      secure_vector<byte> decode(const byte in[],
                                size_t in_length,
                                size_t key_length) const;

      /**
      * Decode an input
      * @param in the encoded plaintext
      * @param key_length length of the key in bits
      * @return plaintext
      */
      secure_vector<byte> decode(const secure_vector<byte>& in,
                                size_t key_length) const;

      virtual ~EME() {}
   private:
      /**
      * Encode an input
      * @param in the plaintext
      * @param in_length length of plaintext in bytes
      * @param key_length length of the key in bits
      * @param rng a random number generator
      * @return encoded plaintext
      */
      virtual secure_vector<byte> pad(const byte in[],
                                     size_t in_length,
                                     size_t key_length,
                                     RandomNumberGenerator& rng) const = 0;

      /**
      * Decode an input
      * @param in the encoded plaintext
      * @param in_length length of encoded plaintext in bytes
      * @param key_length length of the key in bits
      * @return plaintext
      */
      virtual secure_vector<byte> unpad(const byte in[],
                                       size_t in_length,
                                       size_t key_length) const = 0;
   };

}

#endif
