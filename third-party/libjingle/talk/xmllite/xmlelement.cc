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

#include <string>
#include <iostream>
#include <vector>
#include <sstream>

#include "talk/base/common.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlparser.h"
#include "talk/xmllite/xmlbuilder.h"
#include "talk/xmllite/xmlprinter.h"
#include "talk/xmllite/xmlconstants.h"

namespace buzz {

const QName QN_EMPTY(true, STR_EMPTY, STR_EMPTY);
const QName QN_XMLNS(true, STR_EMPTY, STR_XMLNS);


XmlChild::~XmlChild() {
}

bool
XmlText::IsTextImpl() const {
  return true;
}

XmlElement *
XmlText::AsElementImpl() const {
  return NULL;
}

XmlText *
XmlText::AsTextImpl() const {
  return const_cast<XmlText *>(this);
}

void
XmlText::SetText(const std::string & text) {
  text_ = text;
}

void
XmlText::AddParsedText(const char * buf, int len) {
  text_.append(buf, len);
}

void
XmlText::AddText(const std::string & text) {
  text_ += text;
}

XmlText::~XmlText() {
}

XmlElement::XmlElement(const QName & name) :
    name_(name),
    pFirstAttr_(NULL),
    pLastAttr_(NULL),
    pFirstChild_(NULL),
    pLastChild_(NULL) {
}

XmlElement::XmlElement(const XmlElement & elt) :
    XmlChild(),
    name_(elt.name_),
    pFirstAttr_(NULL),
    pLastAttr_(NULL),
    pFirstChild_(NULL),
    pLastChild_(NULL) {

  // copy attributes
  XmlAttr * pAttr;
  XmlAttr ** ppLastAttr = &pFirstAttr_;
  XmlAttr * newAttr = NULL;
  for (pAttr = elt.pFirstAttr_; pAttr; pAttr = pAttr->NextAttr()) {
    newAttr = new XmlAttr(*pAttr);
    *ppLastAttr = newAttr;
    ppLastAttr = &(newAttr->pNextAttr_);
  }
  pLastAttr_ = newAttr;

  // copy children
  XmlChild * pChild;
  XmlChild ** ppLast = &pFirstChild_;
  XmlChild * newChild = NULL;

  for (pChild = elt.pFirstChild_; pChild; pChild = pChild->NextChild()) {
    if (pChild->IsText()) {
      newChild = new XmlText(*(pChild->AsText()));
    } else {
      newChild = new XmlElement(*(pChild->AsElement()));
    }
    *ppLast = newChild;
    ppLast = &(newChild->pNextChild_);
  }
  pLastChild_ = newChild;

}

XmlElement::XmlElement(const QName & name, bool useDefaultNs) :
  name_(name),
  pFirstAttr_(useDefaultNs ? new XmlAttr(QN_XMLNS, name.Namespace()) : NULL),
  pLastAttr_(pFirstAttr_),
  pFirstChild_(NULL),
  pLastChild_(NULL) {
}

bool
XmlElement::IsTextImpl() const {
  return false;
}

XmlElement *
XmlElement::AsElementImpl() const {
  return const_cast<XmlElement *>(this);
}

XmlText *
XmlElement::AsTextImpl() const {
  return NULL;
}

const std::string &
XmlElement::BodyText() const {
  if (pFirstChild_ && pFirstChild_->IsText() && pLastChild_ == pFirstChild_) {
    return pFirstChild_->AsText()->Text();
  }

  return STR_EMPTY;
}

void
XmlElement::SetBodyText(const std::string & text) {
  if (text == STR_EMPTY) {
    ClearChildren();
  } else if (pFirstChild_ == NULL) {
    AddText(text);
  } else if (pFirstChild_->IsText() && pLastChild_ == pFirstChild_) {
    pFirstChild_->AsText()->SetText(text);
  } else {
    ClearChildren();
    AddText(text);
  }
}

const QName &
XmlElement::FirstElementName() const {
  const XmlElement * element = FirstElement();
  if (element == NULL)
    return QN_EMPTY;
  return element->Name();
}

XmlAttr *
XmlElement::FirstAttr() {
  return pFirstAttr_;
}

const std::string &
XmlElement::Attr(const QName & name) const {
  XmlAttr * pattr;
  for (pattr = pFirstAttr_; pattr; pattr = pattr->pNextAttr_) {
    if (pattr->name_ == name)
      return pattr->value_;
  }
  return STR_EMPTY;
}

bool
XmlElement::HasAttr(const QName & name) const {
  XmlAttr * pattr;
  for (pattr = pFirstAttr_; pattr; pattr = pattr->pNextAttr_) {
    if (pattr->name_ == name)
      return true;
  }
  return false;
}

void
XmlElement::SetAttr(const QName & name, const std::string & value) {
  XmlAttr * pattr;
  for (pattr = pFirstAttr_; pattr; pattr = pattr->pNextAttr_) {
    if (pattr->name_ == name)
      break;
  }
  if (!pattr) {
    pattr = new XmlAttr(name, value);
    if (pLastAttr_)
      pLastAttr_->pNextAttr_ = pattr;
    else
      pFirstAttr_ = pattr;
    pLastAttr_ = pattr;
    return;
  }
  pattr->value_ = value;
}

void
XmlElement::ClearAttr(const QName & name) {
  XmlAttr * pattr;
  XmlAttr *pLastAttr = NULL;
  for (pattr = pFirstAttr_; pattr; pattr = pattr->pNextAttr_) {
    if (pattr->name_ == name)
      break;
    pLastAttr = pattr;
  }
  if (!pattr)
    return;
  if (!pLastAttr)
    pFirstAttr_ = pattr->pNextAttr_;
  else
    pLastAttr->pNextAttr_ = pattr->pNextAttr_;
  if (pLastAttr_ == pattr)
    pLastAttr_ = pLastAttr;
  delete pattr;
}

XmlChild *
XmlElement::FirstChild() {
  return pFirstChild_;
}

XmlElement *
XmlElement::FirstElement() {
  XmlChild * pChild;
  for (pChild = pFirstChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText())
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::NextElement() {
  XmlChild * pChild;
  for (pChild = pNextChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText())
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::FirstWithNamespace(const std::string & ns) {
  XmlChild * pChild;
  for (pChild = pFirstChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText() && pChild->AsElement()->Name().Namespace() == ns)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::NextWithNamespace(const std::string & ns) {
  XmlChild * pChild;
  for (pChild = pNextChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText() && pChild->AsElement()->Name().Namespace() == ns)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::FirstNamed(const QName & name) {
  XmlChild * pChild;
  for (pChild = pFirstChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::NextNamed(const QName & name) {
  XmlChild * pChild;
  for (pChild = pNextChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement();
  }
  return NULL;
}

const std::string &
XmlElement::TextNamed(const QName & name) const {
  XmlChild * pChild;
  for (pChild = pFirstChild_; pChild; pChild = pChild->pNextChild_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement()->BodyText();
  }
  return STR_EMPTY;
}

void
XmlElement::InsertChildAfter(XmlChild * pPredecessor, XmlChild * pNext) {
  if (pPredecessor == NULL) {
    pNext->pNextChild_ = pFirstChild_;
    pFirstChild_ = pNext;
  }
  else {
    pNext->pNextChild_ = pPredecessor->pNextChild_;
    pPredecessor->pNextChild_ = pNext;
  }
}

void
XmlElement::RemoveChildAfter(XmlChild * pPredecessor) {
  XmlChild * pNext;

  if (pPredecessor == NULL) {
    pNext = pFirstChild_;
    pFirstChild_ = pNext->pNextChild_;
  }
  else {
    pNext = pPredecessor->pNextChild_;
    pPredecessor->pNextChild_ = pNext->pNextChild_;
  }

  if (pLastChild_ == pNext)
    pLastChild_ = pPredecessor;

  delete pNext;
}

void
XmlElement::AddAttr(const QName & name, const std::string & value) {
  ASSERT(!HasAttr(name));

  XmlAttr ** pprev = pLastAttr_ ? &(pLastAttr_->pNextAttr_) : &pFirstAttr_;
  pLastAttr_ = (*pprev = new XmlAttr(name, value));
}

void
XmlElement::AddAttr(const QName & name, const std::string & value,
                         int depth) {
  XmlElement * element = this;
  while (depth--) {
    element = element->pLastChild_->AsElement();
  }
  element->AddAttr(name, value);
}

void
XmlElement::AddParsedText(const char * cstr, int len) {
  if (len == 0)
    return;

  if (pLastChild_ && pLastChild_->IsText()) {
    pLastChild_->AsText()->AddParsedText(cstr, len);
    return;
  }
  XmlChild ** pprev = pLastChild_ ? &(pLastChild_->pNextChild_) : &pFirstChild_;
  pLastChild_ = *pprev = new XmlText(cstr, len);
}

void
XmlElement::AddText(const std::string & text) {
  if (text == STR_EMPTY)
    return;

  if (pLastChild_ && pLastChild_->IsText()) {
    pLastChild_->AsText()->AddText(text);
    return;
  }
  XmlChild ** pprev = pLastChild_ ? &(pLastChild_->pNextChild_) : &pFirstChild_;
  pLastChild_ = *pprev = new XmlText(text);
}

void
XmlElement::AddText(const std::string & text, int depth) {
  // note: the first syntax is ambigious for msvc 6
  // XmlElement * pel(this);
  XmlElement * element = this;
  while (depth--) {
    element = element->pLastChild_->AsElement();
  }
  element->AddText(text);
}

void
XmlElement::AddElement(XmlElement *pelChild) {
  if (pelChild == NULL)
    return;

  XmlChild ** pprev = pLastChild_ ? &(pLastChild_->pNextChild_) : &pFirstChild_;
  pLastChild_ = *pprev = pelChild;
  pelChild->pNextChild_ = NULL;
}

void
XmlElement::AddElement(XmlElement *pelChild, int depth) {
  XmlElement * element = this;
  while (depth--) {
    element = element->pLastChild_->AsElement();
  }
  element->AddElement(pelChild);
}

void
XmlElement::ClearNamedChildren(const QName & name) {
  XmlChild * prev_child = NULL;
  XmlChild * next_child;
  XmlChild * child;
  for (child = FirstChild(); child; child = next_child) {
    next_child = child->NextChild();
    if (!child->IsText() && child->AsElement()->Name() == name)
    {
      RemoveChildAfter(prev_child);
      continue;
    }
    prev_child = child;
  }
}

void
XmlElement::ClearChildren() {
  XmlChild * pchild;
  for (pchild = pFirstChild_; pchild; ) {
    XmlChild * pToDelete = pchild;
    pchild = pchild->pNextChild_;
    delete pToDelete;
  }
  pFirstChild_ = pLastChild_ = NULL;
}

std::string
XmlElement::Str() const {
  std::stringstream ss;
  Print(&ss, NULL, 0);
  return ss.str();
}

XmlElement *
XmlElement::ForStr(const std::string & str) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, str);
  return builder.CreateElement();
}

void
XmlElement::Print(
    std::ostream * pout, std::string xmlns[], int xmlnsCount) const {
  XmlPrinter::PrintXml(pout, this, xmlns, xmlnsCount);
}

XmlElement::~XmlElement() {
  XmlAttr * pattr;
  for (pattr = pFirstAttr_; pattr; ) {
    XmlAttr * pToDelete = pattr;
    pattr = pattr->pNextAttr_;
    delete pToDelete;
  }

  XmlChild * pchild;
  for (pchild = pFirstChild_; pchild; ) {
    XmlChild * pToDelete = pchild;
    pchild = pchild->pNextChild_;
    delete pToDelete;
  }
}

}
