/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowRoot_h
#define ShadowRoot_h

#include "core/CoreExport.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "core/dom/shadow/SlotAssignment.h"

namespace blink {

class Document;
class ElementShadow;
class ExceptionState;
class HTMLShadowElement;
class InsertionPoint;
class ShadowRootRareData;
class ShadowRootRareDataV0;
class StyleSheetList;

enum class ShadowRootType {
    UserAgent,
    V0,
    Open,
    Closed
};

class CORE_EXPORT ShadowRoot final : public DocumentFragment, public TreeScope {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(ShadowRoot);
public:
    // FIXME: Current implementation does not work well if a shadow root is dynamically created.
    // So multiple shadow subtrees in several elements are prohibited.
    // See https://github.com/w3c/webcomponents/issues/102 and http://crbug.com/234020
    static ShadowRoot* create(Document& document, ShadowRootType type)
    {
        return new ShadowRoot(document, type);
    }

    void recalcStyle(StyleRecalcChange);

    // Disambiguate between Node and TreeScope hierarchies; TreeScope's implementation is simpler.
    using TreeScope::document;
    using TreeScope::getElementById;

    // TODO(kochi): crbug.com/507413 In non-Oilpan, host() may return null during queued
    // event handling (e.g. during execCommand()).
    Element* host() const { return toElement(parentOrShadowHostNode()); }
    ElementShadow* owner() const { return host() ? host()->shadow() : 0; }

    ShadowRoot* youngerShadowRoot() const;
    ShadowRoot* olderShadowRoot() const;
    ShadowRoot* olderShadowRootForBindings() const;

    void setYoungerShadowRoot(ShadowRoot&);
    void setOlderShadowRoot(ShadowRoot&);

    String mode() const { return (type() == ShadowRootType::V0 || type() == ShadowRootType::Open) ? "open" : "closed"; };

    bool isOpenOrV0() const { return type() == ShadowRootType::V0 || type() == ShadowRootType::Open; }

    bool isV1() const { return type() == ShadowRootType::Open || type() == ShadowRootType::Closed; }

    bool isYoungest() const { return !youngerShadowRoot(); }
    bool isOldest() const { return !olderShadowRoot(); }

    void attach(const AttachContext& = AttachContext()) override;

    InsertionNotificationRequest insertedInto(ContainerNode*) override;
    void removedFrom(ContainerNode*) override;

    void registerScopedHTMLStyleChild();
    void unregisterScopedHTMLStyleChild();

    bool containsShadowElements() const;
    bool containsContentElements() const;
    bool containsInsertionPoints() const { return containsShadowElements() || containsContentElements(); }
    bool containsShadowRoots() const;

    unsigned descendantShadowElementCount() const;

    // For Internals, don't use this.
    unsigned childShadowRootCount() const;
    unsigned numberOfStyles() const { return m_numberOfStyles; }

    HTMLShadowElement* shadowInsertionPointOfYoungerShadowRoot() const;
    void setShadowInsertionPointOfYoungerShadowRoot(HTMLShadowElement*);

    void didAddInsertionPoint(InsertionPoint*);
    void didRemoveInsertionPoint(InsertionPoint*);
    const HeapVector<Member<InsertionPoint>>& descendantInsertionPoints();

    ShadowRootType type() const { return static_cast<ShadowRootType>(m_type); }

    void didAddSlot();
    void didRemoveSlot();
    const HeapVector<Member<HTMLSlotElement>>& descendantSlots();

    void assignV1();
    void distributeV1();

    HTMLSlotElement* assignedSlotFor(const Node& node) const
    {
        DCHECK(m_slotAssignment);
        return m_slotAssignment->assignedSlotFor(node);
    }

    // Make protected methods from base class public here.
    using TreeScope::setDocument;
    using TreeScope::setParentTreeScope;

    Element* activeElement() const;

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionState&);

    Node* cloneNode(bool, ExceptionState&);

    StyleSheetList* styleSheets();

    void setDelegatesFocus(bool flag) { m_delegatesFocus = flag; }
    bool delegatesFocus() const { return m_delegatesFocus; }

    DECLARE_VIRTUAL_TRACE();

private:
    ShadowRoot(Document&, ShadowRootType);
    ~ShadowRoot() override;

    void childrenChanged(const ChildrenChange&) override;

    ShadowRootRareData& ensureShadowRootRareData();
    ShadowRootRareDataV0& ensureShadowRootRareDataV0();

    void addChildShadowRoot();
    void removeChildShadowRoot();
    void invalidateDescendantInsertionPoints();

    // ShadowRoots should never be cloned.
    Node* cloneNode(bool) override { return nullptr; }

    // FIXME: This shouldn't happen. https://bugs.webkit.org/show_bug.cgi?id=88834
    bool isOrphan() const { return !host(); }

    void invalidateDescendantSlots();
    unsigned descendantSlotCount() const;

    Member<ShadowRootRareData> m_shadowRootRareData;
    Member<ShadowRootRareDataV0> m_shadowRootRareDataV0;
    Member<SlotAssignment> m_slotAssignment;
    unsigned m_numberOfStyles : 26;
    unsigned m_type : 2;
    unsigned m_registeredWithParentShadowRoot : 1;
    unsigned m_descendantInsertionPointsIsValid : 1;
    unsigned m_delegatesFocus : 1;
    unsigned m_descendantSlotsIsValid : 1;
};

inline Element* ShadowRoot::activeElement() const
{
    return adjustedFocusedElement();
}

inline ShadowRoot* Element::shadowRootIfV1() const
{
    ShadowRoot* root = this->shadowRoot();
    if (root && root->isV1())
        return root;
    return nullptr;
}

DEFINE_NODE_TYPE_CASTS(ShadowRoot, isShadowRoot());
DEFINE_TYPE_CASTS(ShadowRoot, TreeScope, treeScope, treeScope->rootNode().isShadowRoot(), treeScope.rootNode().isShadowRoot());
DEFINE_TYPE_CASTS(TreeScope, ShadowRoot, shadowRoot, true, true);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ShadowRootType&);

} // namespace blink

#endif // ShadowRoot_h
