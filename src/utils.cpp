// utils.cpp -- Common functions.
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

#include "common.h"

#include <sodium.h>
#include <string>
#include <vector>

std::string
sodium::tohex (const sodium::bytes &in)
{
	const std::size_t hexbuf_size = in.size() * 2 + 1;
	sodium::chars     hexbuf(hexbuf_size);
  
	// convert bytes in in into hex using sodium_bin2hex():
	if (! sodium_bin2hex(hexbuf.data(), hexbuf_size, in.data(), in.size()))
		throw std::runtime_error {"sodium::tohex(bytes) overflowed"};

	// In C++17, we could construct a std::string with hexbuf_size chars,
	// and modify it directly through non-const data(). Unfortunately,
	// in C++11 and C++14, std::string's data() is const only, so we need
	// to copy the data over from sodium::chars to std::string for now.
  
	// return hex output as a string:
	std::string outhex {hexbuf.cbegin(), hexbuf.cend()};
	return outhex;
}

std::string
sodium::tohex (const sodium::chars &in)
{
	const std::size_t hexbuf_size = in.size() * 2 + 1;
	sodium::chars     hexbuf(hexbuf_size);
  
	// convert chars in in, interpreted as bytes, into hex
	// using sodium_bin2hex():
	if (! sodium_bin2hex(hexbuf.data(), hexbuf_size,
			reinterpret_cast<const unsigned char *>(in.data()),
			in.size()))
		throw std::runtime_error {"sodium::tohex(chars) overflowed"};

	// In C++17, we could construct a std::string with hexbuf_size chars,
	// and modify it directly through non-const data(). Unfortunately,
	// in C++11 and C++14, std::string's data() is const only, so we need
	// to copy the data over from sodium::chars to std::string for now.
  
	// return hex output as a string:
	std::string outhex {hexbuf.cbegin(), hexbuf.cend()};
	return outhex;
}