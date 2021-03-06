// keyvar.h -- Sodium Key Wrapper, key(s) with variable length at runtime
//
// ISC License
//
// Copyright (C) 2018 Farid Hajji <farid@hajji.name>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#pragma once

#include "allocator.h"
#include "common.h"
#include "key.h" // for KEYSIZE constants
#include "random.h"
#include <sodium.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif // ! NDEBUG

namespace sodium {

template<class BT = bytes_protected>
class keyvar
{
    /**
     * The class sodium::keyvar represents a Key used in various functions
     * of the libsodium library.  Key material, being particulary sensitive,
     * is stored in "protected memory" using a special allocator.
     *
     * A keyvar can be
     *   - default-constructed using random data,
     *   - default-constructed but left uninitialized
     *   - derived from a password string and a (hopefully random) salt.
     *
     * A keyvar can be made read-only or non-accessible when no longer
     * needed.  In general, it is a good idea to be as restrictive as
     * possible with key material.
     *
     * When a keyvar goes out of scope, it auto-destructs by zeroing its
     * memory, and eventually releasing the virtual pages too.
     **/

  public:
    /**
     * bytes_type is protected memory for bytes of key material (see:
     *allocator.h)
     *   * bytes_type memory will self-destruct/zero when out-of-scope / throws
     *   * bytes_type memory can be made readonly or temporarily non-accessible
     *   * bytes_type memory is stored in virtual pages protected by canary,
     *     guard pages, and access to those pages is granted with mprotect().
     **/

    using bytes_type =
      BT; // e.g. protected_bytes (std:vector<byte, sodium::allocator<byte>>)
    using byte_type =
      typename bytes_type::value_type; // e.g. byte (unsigned char)

    // refuse to compile when not instantiating with bytes_protected
    static_assert(std::is_same<bytes_type, bytes_protected>(),
                  "keyvar<> not in protected memory");

    // The strength of the key derivation efforts for setpass()
    using strength_type = enum class strength_enum { low, medium, high };

    /**
     * Construct a keyvar of size key_size.
     *
     * If bool is true, initialize the key, i.e. fill it with random data
     * generated by initialize(), and then make it readonly().
     *
     * If bool is false, leave the key uninitialized, i.e. in the state
     * as created by the special allocator for protected memory. Leave
     * the key in the readwrite() default for further setpass()...
     **/

    keyvar(std::size_t key_size, bool init = true)
      : keydata_(key_size)
    {
        if (init) {
            initialize();
            readonly();
        }
        // CAREFUL: read/write uninitialized key
    }

    /**
     * Copy constructor for keyvars
     *
     * Note that copying a keyvar can be expensive, as the underlying keydata
     * needs to be copied as well, i.e. new key_t virtual pages need to
     * be allocated, mprotect()ed etc.
     *
     * Consider using move semantics/constructor when passing keyvar(s)
     * along for better performance (see below).
     *
     * Note that the copied keyvar will be readwrite(), even if the
     * source was readonly(). If you want a read-only copy, you'll have
     * manually set it to readonly() after it was copy-constructed.
     *
     * If the source keyvar was noaccess(), this copy c'tor will
     * terminate the program.
     **/

    keyvar(const keyvar& other) = default;
    keyvar& operator=(const keyvar& other) = default;

    /**
     * A keyvar can be move-constructed and move-assigned from another
     * existing keyvar, thereby destroying that other keyvar along the
     * way.
     *
     * Move semantics for a keyvar means that the underlying keydata
     * key_t representation won't be unnecessarily duplicated / copied
     * around; saving us from creating virtual pages and mprotect()-ing
     * them when passing keyvar(s) around.
     *
     * For move semantics to take effect, don't forget to use
     * either r-values for keyvar(s) when passing them to functions,
     * or convert keyvar l-values to r-values with std::move().
     *
     * The following constructor / members implement move semantics for
     * keyvars.
     **/

    keyvar()
      : keydata_(0)
    {
        // leave readwrite()
    }

    keyvar(keyvar&& other) noexcept
      : keyvar{}
    {
        std::swap(this->keydata_, other.keydata_);
    }

    keyvar& operator=(keyvar&& other)
    {
        this->keydata_ = std::move(other.keydata_);
        return *this;
    }

    /**
     * Various libsodium functions used either directly or in
     * the wrappers need access to the bytes stored in the keyvar.
     *
     * data() gives const access to those bytes of which
     * size() bytes are stored in the keyvar.
     *
     * We don't provide mutable access to the bytes by design
     * with this data()/size() interface.
     *
     * The only functions that change those bytes are:
     *   initialize(), destroy(), setpass().
     **/

    const byte_type* data() const { return keydata_.data(); }
    std::size_t size() const { return keydata_.size(); }

    /**
     * Provide mutable access to the bytes of the keyvar, so that users
     * can change / set them from the outside.
     *
     * It is the responsibility of the user to ensure that
     *   - the keyvar is set to readwrite(), if data is to be changed
     *   - no more than [setdata(), setdata()+size()) bytes are changed
     *
     * This function is primarily provided for the classes whose
     * underlying libsodium functions write the bytes of a keyvar directly,
     * like:
     *   - KeyPair
     *   - CryptorMultiPK
     *
     * CAVEAT: Currently, KeyPair and CryptorMultiPK use Key<...> instead
     *         of keyvar. But we leave this functionality in place in case
     *         it is needed.
     **/

    byte_type* setdata() { return keydata_.data(); }

    /**
     * Derive key material from the string password, and the salt
     * (where salt.size() == KEYSIZE_SALT) and store that key material
     * into this key object's protected readonly() memory.
     *
     * The strength parameter determines how much effort is to be
     * put into the derivation of the key. It can be one of
     *    keyvar::strength_type::{low,medium,high}.
     *
     * This function throws a std::runtime_error if the strength parameter
     * or the salt size don't make sense, or if the underlying libsodium
     * derivation function crypto_pwhash() runs out of memory.
     **/

    void setpass(const std::string& password,
                 const bytes& salt,
                 const strength_type strength = strength_type::high)
    {
        // check strength and set appropriate parameters
        std::size_t strength_mem;
        unsigned long long strength_cpu;
        switch (strength) {
            case strength_type::low:
                strength_mem = crypto_pwhash_MEMLIMIT_INTERACTIVE;
                strength_cpu = crypto_pwhash_OPSLIMIT_INTERACTIVE;
                break;
            case strength_type::medium:
                strength_mem = crypto_pwhash_MEMLIMIT_MODERATE;
                strength_cpu = crypto_pwhash_OPSLIMIT_MODERATE;
                break;
            case strength_type::high:
                strength_mem = crypto_pwhash_MEMLIMIT_SENSITIVE;
                strength_cpu = crypto_pwhash_OPSLIMIT_SENSITIVE;
                break;
            default:
                throw std::runtime_error{
                    "sodium::keyvar::setpass() wrong strength"
                };
        }

        // check salt length
        if (salt.size() != KEYSIZE_SALT)
            throw std::runtime_error{
                "sodium::keyvar::setpass() wrong salt size"
            };

        // derive a key from the hash of the password, and store it!
        readwrite(); // temporarily unlock the key (if not already)
        if (crypto_pwhash(keydata_.data(),
                          keydata_.size(),
                          password.data(),
                          password.size(),
                          salt.data(),
                          strength_cpu,
                          strength_mem,
                          crypto_pwhash_ALG_DEFAULT) != 0)
            throw std::runtime_error{
                "sodium::keyvar::setpass() crypto_pwhash()"
            };
        readonly(); // relock the key
    }

    /**
     * Initialize, i.e. fill with random data generated with libsodium's
     * function randombytes_buf() the number of bytes already allocated
     * to this keyvar upon construction.
     *
     * You normally don't need to call this function yourself, as it is
     * called by keyvar's constructor. It is provided as a public function
     * nonetheless, should you need to rescramble the key, keeping its
     * size (a rare case).
     *
     * This function will terminate the program if the keyvar is readonly()
     * or noaccess() on systems that enforce mprotect().
     **/

    void initialize() { sodium::randombytes_buf_inplace(keydata_); }

    /**
     * Destroy the bytes stored in protected memory of this key by
     * attempting to zeroing them.
     *
     * A keyvar that has been destroy()ed still holds size() zero-bytes in
     * protected memory, and can thus be reused, i.e. reset by calling
     * e.g. setpass().
     *
     * The keyvar will be destroyed, even if it has been set readonly()
     * or noaccess() previously.
     *
     * You normally don't need to explicitely zero a keyvar, because
     * keyvars self-destruct (including zeroing their bytes) when they
     * go out of scope. This function is provided in case you need to
     * immediately erase a keyvar anyway (think: Panic Button).
     **/

    void destroy()
    {
        readwrite();
        sodium_memzero(keydata_.data(), keydata_.size());
    }

    /**
     * Mark this keyvar as non-accessible. All attempts to read or write
     * to this key will be caught by the CPU / operating system and
     * will result in abnormal program termination.
     *
     * The protection mechanism works by mprotect()ing the virtual page
     * containing the key bytes accordingly.
     *
     * Note that the key bytes are still available, even when noaccess()
     * has been called. Restore access by calling readonly() or readwrite().
     **/

    void noaccess() { keydata_.get_allocator().noaccess(keydata_.data()); }

    /**
     * Mark this keyvar as read-only. All attemps to write to this key will
     * be caught by the CPU / operating system and will result in abnormal
     * program termination.
     *
     * The protection mechanism works by mprotect()ing the virtual page
     * containing the key bytes accordingly.
     *
     * Note that the key bytes can be made writable by calling readwrite().
     **/

    void readonly() { keydata_.get_allocator().readonly(keydata_.data()); }

    /**
     * Mark this keyvar as read/writable. Useful after it has been
     * previously marked readonly() or noaccess().
     **/

    void readwrite() { keydata_.get_allocator().readwrite(keydata_.data()); }

  private:
    bytes_type keydata_; // the bytes of the key are stored in protected memory
};

} // namespace sodium

template<class BT>
bool
operator==(const sodium::keyvar<BT>& k1, const sodium::keyvar<BT>& k2)
{
    // Don't do this (side channel attack):
    // return (k1.size() == k2.size())
    //     &&
    //   std::equal(k1.data(), k1.data() + k1.size(),
    // 	     k2.data());

#ifndef NDEBUG
    std::cerr << "DEBUG: sodium::keyvar::operator==() called" << std::endl;
#endif // ! NDEBUG

    // Compare in constant time instead:
    return (k1.size() == k2.size()) &&
           (sodium_memcmp(k1.data(), k2.data(), k1.size()) == 0);
}

template<class BT>
bool
operator!=(const sodium::keyvar<BT>& k1, const sodium::keyvar<BT>& k2)
{
#ifndef NDEBUG
    std::cerr << "DEBUG: sodium::keyvar::operator!=() called" << std::endl;
#endif // ! NDEBUG

    return (!(k1 == k2));
}
