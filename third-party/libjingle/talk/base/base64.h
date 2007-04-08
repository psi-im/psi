
//*********************************************************************
//* C_Base64 - a simple base64 encoder and decoder.
//*
//*     Copyright (c) 1999, Bob Withers - bwit@pobox.com
//*
//* This code may be freely used for any purpose, either personal
//* or commercial, provided the authors copyright notice remains
//* intact.
//*********************************************************************

#ifndef Base64_H
#define Base64_H

#include <string>
using std::string;  // comment if your compiler doesn't use namespaces

class Base64
{
public:
  static string encode(const string & data);
  static string decode(const string & data);
  static string encodeFromArray(const char * data, size_t len);
private:
  static const string Base64::Base64Table;
  static const string::size_type Base64::DecodeTable[];
};

#endif
