
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_nio_VMDirectByteBuffer__
#define __java_nio_VMDirectByteBuffer__

#pragma interface

#include <java/lang/Object.h>
#include <gcj/array.h>

extern "Java"
{
  namespace gnu
  {
    namespace gcj
    {
        class RawData;
    }
  }
  namespace java
  {
    namespace nio
    {
        class VMDirectByteBuffer;
    }
  }
}

class java::nio::VMDirectByteBuffer : public ::java::lang::Object
{

public: // actually package-private
  VMDirectByteBuffer();
  static ::gnu::gcj::RawData * allocate(jint);
  static void free(::gnu::gcj::RawData *);
  static jbyte get(::gnu::gcj::RawData *, jint);
  static void get(::gnu::gcj::RawData *, jint, JArray< jbyte > *, jint, jint);
  static void put(::gnu::gcj::RawData *, jint, jbyte);
  static ::gnu::gcj::RawData * adjustAddress(::gnu::gcj::RawData *, jint);
  static void shiftDown(::gnu::gcj::RawData *, jint, jint, jint);
public:
  static ::java::lang::Class class$;
};

#endif // __java_nio_VMDirectByteBuffer__
