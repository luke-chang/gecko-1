/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGElementFactory.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FromParser.h"

using namespace mozilla;
using namespace mozilla::dom;

// Hash table that maps nsAtom* SVG tags to an offset index
// within the array sSVGContentCreatorFunctions (offset by TABLE_VALUE_OFFSET)
static PLHashTable* sTagAtomTable = nullptr;

// We don't want to store 0 in the hash table as a return value of 0 from
// PL_HashTableLookupConst indicates that the value is not found
#define TABLE_VALUE_OFFSET 1

#define SVG_TAG(_tag, _classname)                                              \
  nsresult NS_NewSVG##_classname##Element(                                     \
    nsIContent** aResult,                                                      \
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);                     \
                                                                               \
  nsresult NS_NewSVG##_classname##Element(                                     \
    nsIContent** aResult,                                                      \
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,                      \
    FromParser aFromParser)                                                    \
  {                                                                            \
    return NS_NewSVG##_classname##Element(aResult, mozilla::Move(aNodeInfo));  \
  }

#define SVG_FROM_PARSER_TAG(_tag, _classname)

#include "SVGTagList.h"
#undef SVG_TAG
#undef SVG_FROM_PARSER_TAG

nsresult
NS_NewSVGElement(Element** aResult,
                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

static const SVGContentCreatorFunction sSVGContentCreatorFunctions[] = {
#define SVG_TAG(_tag, _classname) NS_NewSVG##_classname##Element,
#define SVG_FROM_PARSER_TAG(_tag, _classname)  NS_NewSVG##_classname##Element,
#include "SVGTagList.h"
#undef SVG_TAG
#undef SVG_FROM_PARSER_TAG
};

enum SVGTag {
#define SVG_TAG(_tag, _classname) eSVGTag_##_tag,
#define SVG_FROM_PARSER_TAG(_tag, _classname) eSVGTag_##_tag,
#include "SVGTagList.h"
#undef SVG_TAG
#undef SVG_FROM_PARSER_TAG
  eSVGTag_Count
};

// nsAtom* -> id hash
static PLHashNumber
SVGTagsHashCodeAtom(const void* key)
{
  return NS_PTR_TO_INT32(key) >> 2;
}

void
SVGElementFactory::Init()
{
  sTagAtomTable = PL_NewHashTable(64, SVGTagsHashCodeAtom,
                                  PL_CompareValues, PL_CompareValues,
                                  nullptr, nullptr);

#define SVG_TAG(_tag, _classname) \
  PL_HashTableAdd(sTagAtomTable, nsGkAtoms::_tag,\
                  NS_INT32_TO_PTR(eSVGTag_##_tag + TABLE_VALUE_OFFSET));
#define SVG_FROM_PARSER_TAG(_tag, _classname) \
  PL_HashTableAdd(sTagAtomTable, nsGkAtoms::_tag,\
                  NS_INT32_TO_PTR(eSVGTag_##_tag + TABLE_VALUE_OFFSET));
#include "SVGTagList.h"
#undef SVG_TAG
#undef SVG_FROM_PARSER_TAG
}

void
SVGElementFactory::Shutdown()
{
  if (sTagAtomTable) {
    PL_HashTableDestroy(sTagAtomTable);
    sTagAtomTable = nullptr;
  }
}

nsresult
NS_NewSVGElement(Element** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                 FromParser aFromParser)
{
  NS_ASSERTION(sTagAtomTable, "no lookup table, needs SVGElementFactory::Init");

  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  nsAtom* name = ni->NameAtom();

  NS_ASSERTION(ni->NamespaceEquals(kNameSpaceID_SVG),
               "Trying to create SVG elements that aren't in the SVG namespace");

  void* tag = PL_HashTableLookupConst(sTagAtomTable, name);
  if (tag) {
    int32_t index = NS_PTR_TO_INT32(tag) - TABLE_VALUE_OFFSET;
    if (index < 0 || index >= eSVGTag_Count) {
      NS_WARNING("About to index out of array bounds - crashing instead");
      MOZ_CRASH();
    }

    SVGContentCreatorFunction cb = sSVGContentCreatorFunctions[index];

    nsCOMPtr<nsIContent> content;
    nsresult rv = cb(getter_AddRefs(content), ni.forget(), aFromParser);
    *aResult = content.forget().take()->AsElement();
    return rv;
  }

  // if we don't know what to create, just create a standard svg element:
  return NS_NewSVGElement(aResult, ni.forget());
}

nsresult
NS_NewSVGUnknownElement(nsIContent** aResult,
                        already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                        FromParser aFromParser)
{
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  nsCOMPtr<Element> element;
  nsresult rv = NS_NewSVGElement(getter_AddRefs(element), ni.forget());
  element.forget(aResult);
  return rv;
}
