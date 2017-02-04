// sodiumcrypter.cpp -- Symmetric encryption / decryption with MAC
//
// Copyright (C) 2017 Farid Hajji <farid@hajji.name>. All rights reserved.

#include "sodiumcrypter.h"
#include "sodiumkey.h"

#include <stdexcept>
#include <string>
#include <vector>

/**
 * Encrypt plaintext using key and nonce, returning ciphertext.
 *
 * Prior to encryption, a MAC of the plaintext is computed with key/nonce
 * and combined with the ciphertext.  This helps detect tampering of
 * the ciphertext and will also prevent decryption.
 *
 * This function will throw a std::runtime_error if the sizes of
 * the key and nonce don't make sense.
 *
 * To safely use this function, it is recommended that
 *   - NO value of nonce is EVER reused again with the same key
 * 
 * Nonces don't need to be kept secret from Eve/Oscar, and therefore
 * don't need to be stored in key_t memory. However, care MUST be
 * taken not to reuse a previously used nonce. When using a big
 * noncespace (24 bits here), generating them randomly e.g. with
 * libsodium's randombytes_buf() may be good enough... but be careful
 * nonetheless.
 *
 * The ciphertext is meant to be sent over the unsecure channel,
 * and it too won't be stored in protected key_t memory.
 **/

SodiumCrypter::data_t
SodiumCrypter::encrypt (const data_t      &plaintext,
		        const Sodium::Key &key,
		        const data_t      &nonce)
{
  // get the sizes
  std::size_t plaintext_size  = plaintext.size();
  std::size_t ciphertext_size = crypto_secretbox_MACBYTES + plaintext_size;
  std::size_t key_size        = Sodium::Key::KEYSIZE_SECRETBOX;
  std::size_t nonce_size      = crypto_secretbox_NONCEBYTES;

  // some sanity checks before we get started
  if (key.size() != key_size)
    throw std::runtime_error {"SodiumCrypter::encrypt() key has wrong size"};
  if (nonce.size() != nonce_size)
    throw std::runtime_error {"SodiumCrypter::encrypt() nonce has wrong size"};

  // make space for MAC and encrypted message
  data_t ciphertext(ciphertext_size);
  
  // let's encrypt now!
  crypto_secretbox_easy (ciphertext.data(),
			 plaintext.data(), plaintext.size(),
			 nonce.data(),
			 key.data());

  // return the encrypted bytes
  return ciphertext;
}

/**
 * Decrypt ciphertext using key and nonce, returing decrypted plaintext.
 * 
 * If the ciphertext has been tampered with, decryption will fail and
 * this function with throw a std::runtime_error.
 *
 * This function will also throw a std::runtime_error if the sizes of
 * the key, nonce and ciphertext don't make sense.
 **/

SodiumCrypter::data_t
SodiumCrypter::decrypt (const data_t      &ciphertext,
		        const Sodium::Key &key,
		        const data_t      &nonce)
{
  // get the sizes
  std::size_t ciphertext_size = ciphertext.size();
  std::size_t key_size        = key.size();
  std::size_t nonce_size      = nonce.size();
  std::size_t plaintext_size  = ciphertext_size - crypto_secretbox_MACBYTES;
  
  // some sanity checks before we get started
  if (key_size != Sodium::Key::KEYSIZE_SECRETBOX)
    throw std::runtime_error {"SodiumCrypter::decrypt() key has wrong size"};
  if (nonce_size != crypto_secretbox_NONCEBYTES)
    throw std::runtime_error {"SodiumCrypter::decrypt() nonce has wrong size"};
  if (plaintext_size <= 0)
    throw std::runtime_error {"SodiumCrypter::decrypt() plaintext negative size"};

  // make space for decrypted buffer
  data_t decryptedtext(plaintext_size);

  // and now decrypt!
  if (crypto_secretbox_open_easy (decryptedtext.data(),
				  ciphertext.data(), ciphertext_size,
				  nonce.data(),
				  key.data()) != 0)
    throw std::runtime_error {"SodiumCrypter::decrypt() message forged (sodium test)"};

  return decryptedtext;
}

/**
 * Convert the bytes of a ciphertext into a hex string,
 * and return that string.
 **/

std::string
SodiumCrypter::tohex (const data_t &ciphertext)
{
  std::size_t ciphertext_size = ciphertext.size();
  std::size_t hex_size        = ciphertext_size * 2 + 1;

  std::vector<char> hexbuf(hex_size);
  
  // convert [ciphertext, ciphertext + ciphertext_size] into hex:
  if (! sodium_bin2hex(hexbuf.data(), hex_size,
		       ciphertext.data(), ciphertext_size))
    throw std::runtime_error {"SodiumCrypter::tohex() overflowed"};

  // XXX: is copying hexbuf into a string really necessary here?
  
  // return hex output as a string:
  std::string outhex {hexbuf.data(), hexbuf.data() + hex_size};
  return outhex;
}