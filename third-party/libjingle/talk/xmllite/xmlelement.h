/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _xmlelement_h_
#define _xmlelement_h_

#include <iosfwd>
#include <string>
#include "talk/base/scoped_ptr.h"
#include "talk/xmllite/qname.h"

namespace buzz {

extern const QName QN_EMPTY;
extern const QName QN_XMLNS;


class XmlChild;
class XmlText;
class XmlElement;
class XmlAttr;

class XmlChild {
friend class XmlElement;

public:

  XmlChild * NextChild() { return pNextChild_; }
  const XmlChild * NextChild() const { return pNextChild_; }

  bool IsText() const { return IsTextImpl(); }

  XmlElement * AsElement() { return AsElementImpl(); }
  const XmlElement * AsElement() const { return AsElementImpl(); }

  XmlText * AsText() { return AsTextImpl(); }
  const XmlText * AsText() const { return AsTextImpl(); }


protected:

  XmlChild() :
    pNextChild_(NULL) {
  }

  virtual bool IsTextImpl() const = 0;
  virtual XmlElement * AsElementImpl() const = 0;
  virtual XmlText * AsTextImpl() const = 0;


  virtual ~XmlChild();

private:
  XmlChild(const XmlChild & noimpl);

  XmlChild * pNextChild_;

};

class XmlText : public XmlChild {
public:
  explicit XmlText(const std::string & text) :
    XmlChild(),
    text_(text) {
  }
  explicit XmlText(const XmlText & t) :
    XmlChild(),
    text_(t.text_) {
  }
  explicit XmlText(const char * cstr, size_t len) :
    XmlChild(),
    text_(cstr, len) {
  }
  virtual ~XmlText();

  const std::string & Text() const { return text_; }
  void SetText(const std::string & text);
  void AddParsedText(const char * buf, int len);
  void AddText(const std::string & text);

protected:
  virtual bool IsTextImpl() const;
  virtual XmlElement * AsElementImpl() const;
  virtual XmlText * AsTextImpl() const;

private:
  std::string text_;
};

class XmlAttr {
friend class XmlElement;

public:
  XmlAttr * NextAttr() const { return pNextAttr_; }
  const QName & Name() const { return name_; }
  const std::string & Value() const { return value_; }

private:
  explicit XmlAttr(const QName & name, const std::string & value) :
    pNextAttr_(NULL),
    name_(name),
    value_(value) {
  }
  explicit XmlAttr(const XmlAttr & att) :
    pNextAttr_(NULL),
    name_(att.name_),
    value_(att.value_) {
  }

  XmlAttr * pNextAttr_;
  QName name_;
  std::string value_;
};

class XmlElement : public XmlChild {
public:
  explicit XmlElement(const QName & name);
  explicit XmlElement(const QName & name, bool useDefaultNs);
  explicit XmlElement(const XmlElement & elt);

  virtual ~XmlElement();

  const QName & Name() const { return name_; }

  const std::string & BodyText() const;
  void SetBodyText(const std::string & text);

  const QName & FirstElementName() const;

  XmlAttr * FirstAttr();
  const XmlAttr * FirstAttr() const
    { return const_cast<XmlElement *>(this)->FirstAttr(); }

  //! Attr will return STR_EMPTY if the attribute isn't there:
  //! use HasAttr to test presence of an attribute. 
  const std::string & Attr(const QName & name) const;
  bool HasAttr(const QName & name) const;
  void SetAttr(const QName & name, const std::string & value);
  void ClearAttr(const QName & name);

  XmlChild * FirstChild();
  const XmlChild * FirstChild() const
    { return const_cast<XmlElement *>(this)->FirstChild(); }

  XmlElement * FirstElement();
  const XmlElement * FirstElement() const
    { return const_cast<XmlElement *>(this)->FirstElement(); }

  XmlElement * NextElement();
  const XmlElement * NextElement() const
    { return const_cast<XmlElement *>(this)->NextElement(); }

  XmlElement * FirstWithNamespace(const std::string & ns);
  const XmlElement * FirstWithNamespace(const std::string & ns) const
    { return const_cast<XmlElement *>(this)->FirstWithNamespace(ns); }
    
  XmlElement * NextWithNamespace(const std::string & ns);
  const XmlElement * NextWithNamespace(const std::string & ns) const
    { return const_cast<XmlElement *>(this)->NextWithNamespace(ns); }

  XmlElement * FirstNamed(const QName & name);
  const XmlElement * FirstNamed(const QName & name) const
    { return const_cast<XmlElement *>(this)->FirstNamed(name); }

  XmlElement * NextNamed(const QName & name);
  const XmlElement * NextNamed(const QName & name) const
    { return const_cast<XmlElement *>(this)->NextNamed(name); }

  const std::string & TextNamed(const QName & name) const;

  void InsertChildAfter(XmlChild * pPredecessor, XmlChild * pNewChild);
  void RemoveChildAfter(XmlChild * pPredecessor);

  void AddParsedText(const char * buf, int len);
  void AddText(const std::string & text);
  void AddText(const std::string & text, int depth);
  void AddElement(XmlElement * pelChild);
  void AddElement(XmlElement * pelChild, int depth);
  void AddAttr(const QName & name, const std::string & value);
  void AddAttr(const QName & name, const std::string & value, int depth);
  void ClearNamedChildren(const QName & name);
  void ClearChildren();

  static XmlElement * ForStr(const std::string & str);
  std::string Str() const;

  void Print(std::ostream * pout, std::string xmlns[], int xmlnsCount) const;

protected:
  virtual bool IsTextImpl() const;
  virtual XmlElement * AsElementImpl() const;
  virtual XmlText * AsTextImpl() const;

private:
  QName name_;
  XmlAttr * pFirstAttr_;
  XmlAttr * pLastAttr_;
  XmlChild * pFirstChild_;
  XmlChild * pLastChild_;

};



}

#endif
