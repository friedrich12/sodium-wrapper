This is a set of C++11 wrappers to the libsodium library.

!!! EXPERIMENTAL AND INCOMPLETE -- VERY EARLY ALPHA -- DON'T USE YET !!!

>>> Interfaces are incomplete and subject to change <<<

I'm still figuring out how to best map libsodium's C-API to C++
classes. Therefore, the following C++ API is subject to change
at any time. Don't use yet for production code or anything serious.
This is an (self-)educational repo/project for now.

USE AT YOUR OWN RISK. YOU'VE BEEN WARNED.

Criticism and pull requests welcome, of course.

Dependencies:
  - libsodium (duh!)
  - boost

Build dependencies:
  - a C++11 compliant compiler
  - CMake

I'm developing on:
  - FreeBSD/amd64 11.0-STABLE
  - clang++ 3.8.0 / llvm 3.8.0
  - cmake version 3.7.2
  
Thanks,

Farid Hajji.
