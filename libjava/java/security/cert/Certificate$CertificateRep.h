
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_security_cert_Certificate$CertificateRep__
#define __java_security_cert_Certificate$CertificateRep__

#pragma interface

#include <java/lang/Object.h>
#include <gcj/array.h>

extern "Java"
{
  namespace java
  {
    namespace security
    {
      namespace cert
      {
          class Certificate$CertificateRep;
      }
    }
  }
}

class java::security::cert::Certificate$CertificateRep : public ::java::lang::Object
{

public: // actually protected
  Certificate$CertificateRep(::java::lang::String *, JArray< jbyte > *);
  virtual ::java::lang::Object * readResolve();
private:
  static const jlong serialVersionUID = -8563758940495660020LL;
  ::java::lang::String * __attribute__((aligned(__alignof__( ::java::lang::Object)))) type;
  JArray< jbyte > * data;
public:
  static ::java::lang::Class class$;
};

#endif // __java_security_cert_Certificate$CertificateRep__
