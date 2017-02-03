// main.cpp -- Test libsodium library with custom C++ wrappers
//
// Copyright (C) 2017 Farid Hajji <farid@hajji.name>. All rights reserved.
//
// Use CMake w/ CMakeLists.txt like this:
//   $ cmake .
//   $ make

#include "sodiumtester.h"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <iostream>

int main()
{
  try {
    SodiumTester st {};

    std::string plaintext;
    std::string cyphertext;
    
    std::cout << "Enter plaintext: ";
    std::getline(std::cin, plaintext);
    
    cyphertext = st.test0(plaintext);
    std::cout << "crypto_secretbox_easy(): " << cyphertext << std::endl;

    bool res1 = st.test1(plaintext);
    std::cout << "crypto_auth()/crypto_auth_verify(): " << res1 << std::endl;
  }
  catch (std::runtime_error e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}
