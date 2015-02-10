
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_security_auth_callback_ConfirmationCallback__
#define __javax_security_auth_callback_ConfirmationCallback__

#pragma interface

#include <java/lang/Object.h>
#include <gcj/array.h>

extern "Java"
{
  namespace javax
  {
    namespace security
    {
      namespace auth
      {
        namespace callback
        {
            class ConfirmationCallback;
        }
      }
    }
  }
}

class javax::security::auth::callback::ConfirmationCallback : public ::java::lang::Object
{

public:
  ConfirmationCallback(jint, jint, jint);
  ConfirmationCallback(jint, JArray< ::java::lang::String * > *, jint);
  ConfirmationCallback(::java::lang::String *, jint, jint, jint);
  ConfirmationCallback(::java::lang::String *, jint, JArray< ::java::lang::String * > *, jint);
  virtual ::java::lang::String * getPrompt();
  virtual jint getMessageType();
  virtual jint getOptionType();
  virtual JArray< ::java::lang::String * > * getOptions();
  virtual jint getDefaultOption();
  virtual void setSelectedIndex(jint);
  virtual jint getSelectedIndex();
private:
  void setMessageType(jint);
  void setOptionType(jint, jint);
  void setOptions(JArray< ::java::lang::String * > *, jint);
  void setPrompt(::java::lang::String *);
public:
  static const jint UNSPECIFIED_OPTION = -1;
  static const jint YES_NO_OPTION = 0;
  static const jint YES_NO_CANCEL_OPTION = 1;
  static const jint OK_CANCEL_OPTION = 2;
  static const jint YES = 0;
  static const jint NO = 1;
  static const jint CANCEL = 2;
  static const jint OK = 3;
  static const jint INFORMATION = 0;
  static const jint WARNING = 1;
  static const jint ERROR = 2;
private:
  ::java::lang::String * __attribute__((aligned(__alignof__( ::java::lang::Object)))) prompt;
  jint messageType;
  jint optionType;
  jint defaultOption;
  JArray< ::java::lang::String * > * options;
  jint selection;
public:
  static ::java::lang::Class class$;
};

#endif // __javax_security_auth_callback_ConfirmationCallback__
