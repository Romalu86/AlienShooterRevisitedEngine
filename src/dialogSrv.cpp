#include <cstdio>
#include <new>
#include "engine_globals.h"
#include "myerror.h"
#include "zm/Utils/zLogConsole.h"
#include "list.h"
#include "basetype.h"

#include "mylib.h"
#include <stdexcept>
#include "dialogSrv.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int zDlgSrv::GetDlgKey(int group, int var2)
{
    if (var2 >= 1000)
        DebugAssert(4u, "var2 < 1000", "..\\src\\dialogSrv.cpp", 308u);
    return BitsToSigned32(static_cast<unsigned int>(MultiplySigned32Wrap(group, 1000))+static_cast<unsigned int>(var2));
}
#include "graph.h"
#include "map.h"
#include "resread.h"
#include "unit.h"
#include "zm/RPGHero/zRPGFrameWork.h"

int  zDlgSrv_ReleaseData(void* self);
void  Dialog_Dtor(void* self);
void  zDlgSrv_LoadDialogs(void* self, const STRING& source);

static DialogTreeNode* DialogTreeAllocateNode()
{
    void* const storage = CrtNew(sizeof(DialogTreeNode));
    if (!storage)
        throw std::bad_alloc();
    return ::new (storage) DialogTreeNode;
}

static void DialogTreeInit(DialogTreeControl* tree)
{
    DialogTreeNode* const head = DialogTreeAllocateNode();
    head->m_left = head;
    head->m_parent = head;
    head->m_right = head;
    head->m_color = 1u;
    head->m_isNil = 1u;
    tree->m_head = head;
    tree->m_size = 0u;
}

static DialogTreeIterator DialogTreeBegin(DialogTreeControl* tree)
{
    return DialogTreeIterator{tree->m_head->m_left};
}

static DialogTreeIterator DialogTreeEnd(DialogTreeControl* tree)
{
    return DialogTreeIterator{tree->m_head};
}

static bool DialogTreeEqual(DialogTreeIterator first, DialogTreeIterator last)
{
    return first.m_node == last.m_node;
}

static DialogTreeNode* DialogTreeDereference(DialogTreeIterator position)
{
    return position.m_node;
}

static DialogTreeNode* DialogTreeMinimum(DialogTreeNode* node)
{
    while (!node->m_left->m_isNil)
        node = node->m_left;
    return node;
}

static DialogTreeNode* DialogTreeMaximum(DialogTreeNode* node)
{
    while (!node->m_right->m_isNil)
        node = node->m_right;
    return node;
}

void DialogTreeConstIterator_Inc(DialogTreeIterator* iterator)
{
    DialogTreeIterator& position = *iterator;
    if (position.m_node->m_isNil)
        return;
    if (!position.m_node->m_right->m_isNil)
    {
        position.m_node = DialogTreeMinimum(position.m_node->m_right);
        return;
    }
    DialogTreeNode* parent = position.m_node->m_parent;
    while (!parent->m_isNil && position.m_node == parent->m_right)
    {
        position.m_node = parent;
        parent = parent->m_parent;
    }
    position.m_node = parent;
}

void DialogTreeConstIterator_Dec(DialogTreeIterator* iterator)
{
    DialogTreeIterator& position = *iterator;
    if (position.m_node->m_isNil)
    {
        position.m_node = position.m_node->m_right;
        return;
    }
    if (!position.m_node->m_left->m_isNil)
    {
        position.m_node = DialogTreeMaximum(position.m_node->m_left);
        return;
    }
    DialogTreeNode* parent = position.m_node->m_parent;
    while (!parent->m_isNil && position.m_node == parent->m_left)
    {
        position.m_node = parent;
        parent = parent->m_parent;
    }
    if (!position.m_node->m_isNil)
        position.m_node = parent;
}

static void DialogTreeRotateLeft(DialogTreeControl* tree, DialogTreeNode* node)
{
    DialogTreeNode* const pivot = node->m_right;
    node->m_right = pivot->m_left;
    if (!pivot->m_left->m_isNil)
        pivot->m_left->m_parent = node;
    pivot->m_parent = node->m_parent;
    if (node == tree->m_head->m_parent)
        tree->m_head->m_parent = pivot;
    else if (node == node->m_parent->m_left)
        node->m_parent->m_left = pivot;
    else
        node->m_parent->m_right = pivot;
    pivot->m_left = node;
    node->m_parent = pivot;
}

static void DialogTreeRotateRight(DialogTreeControl* tree, DialogTreeNode* node)
{
    DialogTreeNode* const pivot = node->m_left;
    node->m_left = pivot->m_right;
    if (!pivot->m_right->m_isNil)
        pivot->m_right->m_parent = node;
    pivot->m_parent = node->m_parent;
    if (node == tree->m_head->m_parent)
        tree->m_head->m_parent = pivot;
    else if (node == node->m_parent->m_right)
        node->m_parent->m_right = pivot;
    else
        node->m_parent->m_left = pivot;
    pivot->m_right = node;
    node->m_parent = pivot;
}

static DialogTreeNode* DialogTreeLowerBound(DialogTreeControl* tree, int key)
{
    DialogTreeNode* candidate = tree->m_head;
    DialogTreeNode* node = tree->m_head->m_parent;
    while (!node->m_isNil)
    {
        if (node->m_key < key)
            node = node->m_right;
        else
        {
            candidate = node;
            node = node->m_left;
        }
    }
    return candidate;
}


static DialogTreeNode* DialogTreeCreateNode(DialogTreeControl* tree, int key, DIALOG* value)
{
    DialogTreeNode* const node = DialogTreeAllocateNode();
    node->m_left = tree->m_head;
    node->m_parent = tree->m_head;
    node->m_right = tree->m_head;
    node->m_color = 0u;
    node->m_isNil = 0u;
    node->m_key = key;
    node->m_dialog = value;
    return node;
}

static DialogTreeIterator DialogTreeInsertNode(DialogTreeControl* tree, bool putLeft,
    DialogTreeNode* parent, DialogTreeNode* node)
{
    if (tree->m_size >= 0x1FFFFFFEu)
    {
        CrtFreePointer(node);
        throw std::length_error("map/set<T> too long");
    }
    ++tree->m_size;
    node->m_parent = parent;
    if (parent == tree->m_head)
    {
        tree->m_head->m_parent = node;
        tree->m_head->m_left = node;
        tree->m_head->m_right = node;
    }
    else if (putLeft)
    {
        parent->m_left = node;
        if (parent == tree->m_head->m_left)
            tree->m_head->m_left = node;
    }
    else
    {
        parent->m_right = node;
        if (parent == tree->m_head->m_right)
            tree->m_head->m_right = node;
    }
    DialogTreeNode* current = node;
    while (current->m_parent->m_color == 0u)
    {
        DialogTreeNode* parentNode = current->m_parent;
        DialogTreeNode* grandparent = parentNode->m_parent;
        if (parentNode == grandparent->m_left)
        {
            DialogTreeNode* const uncle = grandparent->m_right;
            if (uncle->m_color == 0u)
            {
                parentNode->m_color = 1u;
                uncle->m_color = 1u;
                grandparent->m_color = 0u;
                current = grandparent;
            }
            else
            {
                if (current == parentNode->m_right)
                {
                    current = parentNode;
                    DialogTreeRotateLeft(tree, current);
                    parentNode = current->m_parent;
                    grandparent = parentNode->m_parent;
                }
                parentNode->m_color = 1u;
                grandparent->m_color = 0u;
                DialogTreeRotateRight(tree, grandparent);
            }
        }
        else
        {
            DialogTreeNode* const uncle = grandparent->m_left;
            if (uncle->m_color == 0u)
            {
                parentNode->m_color = 1u;
                uncle->m_color = 1u;
                grandparent->m_color = 0u;
                current = grandparent;
            }
            else
            {
                if (current == parentNode->m_left)
                {
                    current = parentNode;
                    DialogTreeRotateRight(tree, current);
                    parentNode = current->m_parent;
                    grandparent = parentNode->m_parent;
                }
                parentNode->m_color = 1u;
                grandparent->m_color = 0u;
                DialogTreeRotateLeft(tree, grandparent);
            }
        }
    }
    tree->m_head->m_parent->m_color = 1u;
    return DialogTreeIterator{node};
}

static DialogTreeInsertResult DialogTreeInsertUniqueNode(DialogTreeControl* tree,
    DialogTreeNode* candidate)
{
    const int key = candidate->m_key;
    DialogTreeNode* node = tree->m_head->m_parent;
    DialogTreeNode* parent = tree->m_head;
    bool insertLeft = true;
    while (!node->m_isNil)
    {
        parent = node;
        insertLeft = key < node->m_key;
        node = insertLeft ? node->m_left : node->m_right;
    }
    DialogTreeIterator position = {parent};
    if (insertLeft)
    {
        if (parent == tree->m_head->m_left)
        {
            DialogTreeInsertResult result;
            result.m_position = DialogTreeInsertNode(tree, true, parent, candidate);
            result.m_inserted = 1u;
            return result;
        }
        DialogTreeConstIterator_Dec(&position);
    }
    DialogTreeInsertResult result;
    if (position.m_node->m_key < key)
    {
        result.m_position = DialogTreeInsertNode(tree, insertLeft, parent, candidate);
        result.m_inserted = 1u;
    }
    else
    {
        CrtFreePointer(candidate);
        result.m_position = position;
        result.m_inserted = 0u;
    }
    return result;
}

static DialogTreeIterator DialogTreeEraseNode(DialogTreeControl* tree, DialogTreeIterator position)
{
    DialogTreeNode* const eraseNode = position.m_node;
    if (eraseNode->m_isNil)
        throw std::out_of_range("invalid map/set<T> iterator");
    DialogTreeConstIterator_Inc(&position);
    DialogTreeNode* releaseNode = eraseNode;
    DialogTreeNode* replacement;
    DialogTreeNode* replacementParent;
    if (eraseNode->m_left->m_isNil)
        replacement = eraseNode->m_right;
    else if (eraseNode->m_right->m_isNil)
        replacement = eraseNode->m_left;
    else
    {
        releaseNode = position.m_node;
        replacement = releaseNode->m_right;
    }
    if (releaseNode == eraseNode)
    {
        replacementParent = eraseNode->m_parent;
        if (!replacement->m_isNil)
            replacement->m_parent = replacementParent;
        if (tree->m_head->m_parent == eraseNode)
            tree->m_head->m_parent = replacement;
        else if (replacementParent->m_left == eraseNode)
            replacementParent->m_left = replacement;
        else
            replacementParent->m_right = replacement;
        if (tree->m_head->m_left == eraseNode)
            tree->m_head->m_left = replacement->m_isNil ? replacementParent : DialogTreeMinimum(replacement);
        if (tree->m_head->m_right == eraseNode)
            tree->m_head->m_right = replacement->m_isNil ? replacementParent : DialogTreeMaximum(replacement);
    }
    else
    {
        eraseNode->m_left->m_parent = releaseNode;
        releaseNode->m_left = eraseNode->m_left;
        if (releaseNode == eraseNode->m_right)
            replacementParent = releaseNode;
        else
        {
            replacementParent = releaseNode->m_parent;
            if (!replacement->m_isNil)
                replacement->m_parent = replacementParent;
            replacementParent->m_left = replacement;
            releaseNode->m_right = eraseNode->m_right;
            eraseNode->m_right->m_parent = releaseNode;
        }
        if (tree->m_head->m_parent == eraseNode)
            tree->m_head->m_parent = releaseNode;
        else if (eraseNode->m_parent->m_left == eraseNode)
            eraseNode->m_parent->m_left = releaseNode;
        else
            eraseNode->m_parent->m_right = releaseNode;
        releaseNode->m_parent = eraseNode->m_parent;
        const unsigned char oldColor = eraseNode->m_color;
        eraseNode->m_color = releaseNode->m_color;
        releaseNode->m_color = oldColor;
    }
    if (eraseNode->m_color == 1u)
    {
        while (replacement != tree->m_head->m_parent && replacement->m_color == 1u)
        {
            if (replacement == replacementParent->m_left)
            {
                DialogTreeNode* sibling = replacementParent->m_right;
                if (sibling->m_color == 0u)
                {
                    sibling->m_color = 1u;
                    replacementParent->m_color = 0u;
                    DialogTreeRotateLeft(tree, replacementParent);
                    sibling = replacementParent->m_right;
                }
                if (sibling->m_isNil)
                {
                    replacement = replacementParent;
                    replacementParent = replacementParent->m_parent;
                }
                else if (sibling->m_left->m_color == 1u && sibling->m_right->m_color == 1u)
                {
                    sibling->m_color = 0u;
                    replacement = replacementParent;
                    replacementParent = replacementParent->m_parent;
                }
                else
                {
                    if (sibling->m_right->m_color == 1u)
                    {
                        sibling->m_left->m_color = 1u;
                        sibling->m_color = 0u;
                        DialogTreeRotateRight(tree, sibling);
                        sibling = replacementParent->m_right;
                    }
                    sibling->m_color = replacementParent->m_color;
                    replacementParent->m_color = 1u;
                    sibling->m_right->m_color = 1u;
                    DialogTreeRotateLeft(tree, replacementParent);
                    break;
                }
            }
            else
            {
                DialogTreeNode* sibling = replacementParent->m_left;
                if (sibling->m_color == 0u)
                {
                    sibling->m_color = 1u;
                    replacementParent->m_color = 0u;
                    DialogTreeRotateRight(tree, replacementParent);
                    sibling = replacementParent->m_left;
                }
                if (sibling->m_isNil)
                {
                    replacement = replacementParent;
                    replacementParent = replacementParent->m_parent;
                }
                else if (sibling->m_right->m_color == 1u && sibling->m_left->m_color == 1u)
                {
                    sibling->m_color = 0u;
                    replacement = replacementParent;
                    replacementParent = replacementParent->m_parent;
                }
                else
                {
                    if (sibling->m_left->m_color == 1u)
                    {
                        sibling->m_right->m_color = 1u;
                        sibling->m_color = 0u;
                        DialogTreeRotateLeft(tree, sibling);
                        sibling = replacementParent->m_left;
                    }
                    sibling->m_color = replacementParent->m_color;
                    replacementParent->m_color = 1u;
                    sibling->m_left->m_color = 1u;
                    DialogTreeRotateRight(tree, replacementParent);
                    break;
                }
            }
        }
        replacement->m_color = 1u;
    }
    CrtFreePointer(eraseNode);
    if (tree->m_size != 0u)
        --tree->m_size;
    return position;
}

static void DialogTreeDestroySubtree(DialogTreeNode* node)
{
    while (!node->m_isNil)
    {
        DialogTreeDestroySubtree(node->m_right);
        DialogTreeNode* const left = node->m_left;
        CrtFreePointer(node);
        node = left;
    }
}

static DialogTreeIterator DialogTreeEraseRange(DialogTreeControl* tree,
    DialogTreeIterator first, DialogTreeIterator last)
{
    if (DialogTreeEqual(first, DialogTreeBegin(tree)) && DialogTreeEqual(last, DialogTreeEnd(tree)))
    {
        DialogTreeDestroySubtree(tree->m_head->m_parent);
        tree->m_head->m_parent = tree->m_head;
        tree->m_size = 0u;
        tree->m_head->m_left = tree->m_head;
        tree->m_head->m_right = tree->m_head;
        return DialogTreeBegin(tree);
    }
    while (!DialogTreeEqual(first, last))
    {
        const DialogTreeIterator old = first;
        DialogTreeConstIterator_Inc(&first);
        (void)DialogTreeEraseNode(tree, old);
    }
    return first;
}

static void DialogTreeClear(DialogTreeControl* tree)
{
    (void)DialogTreeEraseRange(tree, DialogTreeBegin(tree), DialogTreeEnd(tree));
}

static void DialogTreeReleaseStorage(DialogTreeControl* tree)
{
    DialogTreeClear(tree);
    CrtFreePointer(tree->m_head);
}


DialogTreeNode* __cdecl DialogTree_Min(DialogTreeNode* start)
{
    return DialogTreeMinimum(start);
}

void DialogTree_EraseSubtree(void*, void* node)
{
    DialogTreeDestroySubtree(static_cast<DialogTreeNode*>(node));
}

DialogTreeControl::DialogTreeControl()
{
    DialogTreeInit(this);
}

DialogTreeControl::~DialogTreeControl() noexcept(false)
{
    DialogTreeReleaseStorage(this);
}

static DialogTreeIterator DialogTreeInsertHint(DialogTreeControl* tree,
    DialogTreeIterator position, int key, DIALOG* dialog)
{
    DialogTreeNode* const candidate = DialogTreeCreateNode(tree, key, dialog);
    if (tree->m_size == 0u)
        return DialogTreeInsertNode(tree, true, tree->m_head, candidate);
    if (DialogTreeEqual(position, DialogTreeBegin(tree)))
    {
        if (key < position.m_node->m_key)
            return DialogTreeInsertNode(tree, true, position.m_node, candidate);
    }
    else if (DialogTreeEqual(position, DialogTreeEnd(tree)))
    {
        if (tree->m_head->m_right->m_key < key)
            return DialogTreeInsertNode(tree, false, tree->m_head->m_right, candidate);
    }
    else
    {
        if (key < position.m_node->m_key)
        {
            DialogTreeIterator before = position;
            DialogTreeConstIterator_Dec(&before);
            if (before.m_node->m_key < key)
            {
                if (before.m_node->m_right->m_isNil)
                    return DialogTreeInsertNode(tree, false, before.m_node, candidate);
                return DialogTreeInsertNode(tree, true, position.m_node, candidate);
            }
        }
        if (position.m_node->m_key < key)
        {
            DialogTreeIterator after = position;
            DialogTreeConstIterator_Inc(&after);
            if (DialogTreeEqual(after, DialogTreeEnd(tree)) || key < after.m_node->m_key)
            {
                if (position.m_node->m_right->m_isNil)
                    return DialogTreeInsertNode(tree, false, position.m_node, candidate);
                return DialogTreeInsertNode(tree, true, after.m_node, candidate);
            }
        }
    }
    return DialogTreeInsertUniqueNode(tree, candidate).m_position;
}

static DIALOG*& DialogTreeValue(DialogTreeControl* tree, int key)
{
    DialogTreeIterator position = {DialogTreeLowerBound(tree, key)};
    if (position.m_node == tree->m_head || key < position.m_node->m_key)
        position = DialogTreeInsertHint(tree, position, key, 0);
    return DialogTreeDereference(position)->m_dialog;
}

PHRASE::PHRASE() noexcept
{
}

void*  zDlgSrv_DeletingDtor(void* self, unsigned int flags)
{
    zDlgSrv_Dtor(self);
    if (flags & 1u) CrtFreePointer( self);
    return self;
}

void DialogIteratorBoolPair_Ctor(void* self, const void* nodeSource, const void* insertedSource)
{
    DialogTreeInsertResult* result = static_cast<DialogTreeInsertResult*>(self);
    result->m_position = *static_cast<const DialogTreeIterator*>(nodeSource);
    result->m_inserted = *static_cast<const unsigned char*>(insertedSource);
}

void __cdecl DialogComment_Construct(COMMENT* destination, const COMMENT* source);


PHRASE& PHRASE::operator=(const PHRASE& source)
{
    m_text = source.m_text;
    m_voiceFile = source.m_voiceFile;
    m_pauseType = source.m_pauseType;
    m_sfx = source.m_sfx;
    m_exp = source.m_exp;
    m_money = source.m_money;
    m_duration = source.m_duration;
    return *this;
}


void __stdcall DialogCommentVector_Ufill(void* begin, unsigned int count, const void* sourceElement)
{
    COMMENT* destination = static_cast<COMMENT*>(begin);
    const COMMENT* source = static_cast<const COMMENT*>(sourceElement);
    COMMENT* current = destination;
    try
    {
        for (unsigned int elementIndex = 0; elementIndex<count; ++elementIndex, ++current)
            DialogComment_Construct(current, source);
    }
    catch (...)
    {
        DialogCommentVector_Destroy(destination, current);
        throw;
    }
}

void __cdecl DialogComment_Fill(void* begin, void* end, const void* sourceElement)
{
    COMMENT* current = static_cast<COMMENT*>(begin);
    COMMENT* finish = static_cast<COMMENT*>(end);
    const COMMENT* source = static_cast<const COMMENT*>(sourceElement);
    while (current != finish)
    {
        current->m_ownedString = source->m_ownedString;
        current->m_dialogKey = source->m_dialogKey;
        ++current;
    }
}

unsigned char* __stdcall DialogCommentVector_Ucopy(unsigned char* sourceBegin, unsigned char* sourceEnd, unsigned char* destinationBegin)
{
    COMMENT* source = reinterpret_cast<COMMENT*>(sourceBegin);
    COMMENT* finish = reinterpret_cast<COMMENT*>(sourceEnd);
    COMMENT* destination = reinterpret_cast<COMMENT*>(destinationBegin);
    COMMENT* const start = destination;
    try
    {
        while (source != finish)
        {
            DialogComment_Construct(destination, source);
            ++destination;
            ++source;
        }
    }
    catch (...)
    {
        DialogCommentVector_Destroy(start, destination);
        throw;
    }
    return reinterpret_cast<unsigned char*>(destination);
}

unsigned char* __cdecl DialogComment_CopyBackward(unsigned char* sourceBegin, unsigned char* sourceEnd, unsigned char* destinationEnd)
{
    COMMENT* begin = reinterpret_cast<COMMENT*>(sourceBegin);
    COMMENT* source = reinterpret_cast<COMMENT*>(sourceEnd);
    COMMENT* destination = reinterpret_cast<COMMENT*>(destinationEnd);
    if (begin == source) return destinationEnd;
    do
    {
        --source;
        --destination;
        destination->m_ownedString = source->m_ownedString;
        destination->m_dialogKey = source->m_dialogKey;
    }
    while (source != begin);
    return reinterpret_cast<unsigned char*>(destination);
}


DialogVector8Control::DialogVector8Control() noexcept
    : m_begin(0), m_end(0), m_capEnd(0)
{
}

DialogVector8Control::~DialogVector8Control()
{
    DialogCommentVector_Dtor(this);
}

zDlgSrv::zDlgSrv()
{
    m_textDurationPercent = 100;
    m_showDialogSubtitles = 1;
    m_activeRadioDialogKey = 0;
    m_oneLineDuration = m_twoLineDuration = m_threeLineDuration = m_fourLineDuration = m_fiveLineDuration = 0;
    if (!::gpLogCons) GetNew_zLogConsole(&::gpLogCons);
}

zDlgSrv::~zDlgSrv() noexcept(false)
{
    ReleaseData();
}

void* zDlgSrv_Ctor(void* self)
{
    return ::new (self) zDlgSrv();
}

void zDlgSrv_Dtor(void* self)
{
    static_cast<zDlgSrv*>(self)->~zDlgSrv();
}

unsigned int __cdecl DialogSrv_StaticCleanupRegister02()
{
    return RegisterCrtAtExit(&DialogServiceGlobalStringCleanup);
}

unsigned int __cdecl DialogSrv_StaticCleanupRegister03()
{
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}

unsigned int __cdecl DialogSrv_StaticCleanupRegister04()
{
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}

unsigned int __cdecl DialogSrv_StaticCleanupRegister05()
{
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}

unsigned int __cdecl DialogSrv_StaticCleanupRegister06()
{
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}


void  DialogCommentVector_Dtor(void* self)
{
    DialogVector8Control* vector = static_cast<DialogVector8Control*>(self);
    if (vector->m_begin)
    {
        COMMENT* current = vector->m_begin;
        COMMENT* const finish = vector->m_end;
        while (current != finish)
            (current++)->~COMMENT();
        CrtFreePointer(vector->m_begin);
    }
    vector->m_begin = 0;
    vector->m_end = 0;
    vector->m_capEnd = 0;
}

int  DialogCommentVector_Size(const void* self)
{
    const DialogVector8Control* vector = static_cast<const DialogVector8Control*>(self);
    if (!vector->m_begin) return 0;
    return static_cast<int>(vector->m_end-vector->m_begin);
}

void __stdcall DialogCommentVector_Destroy(void* begin, void* end)
{
    COMMENT* current = static_cast<COMMENT*>(begin);
    const COMMENT* finish = static_cast<const COMMENT*>(end);
    while (current != finish)
    {
        current->~COMMENT();
        ++current;
    }
}


DialogVectorIterator DialogCommentVector_Erase(DialogVector8Control* vector,
    DialogVectorIterator first, DialogVectorIterator last)
{
    if (!first.m_owner || first.m_owner != last.m_owner)
        ::_invalid_parameter_noinfo();
    if (first.m_position == last.m_position)
        return first;
    COMMENT* destination = first.m_position;
    COMMENT* source = last.m_position;
    COMMENT* const copyEnd = vector->m_end;
    while (source != copyEnd)
    {
        destination->m_ownedString = source->m_ownedString;
        destination->m_dialogKey = source->m_dialogKey;
        ++source;
        ++destination;
    }
    COMMENT* const newEnd = destination;
    COMMENT* const destroyEnd = vector->m_end;
    while (destination != destroyEnd)
        (destination++)->~COMMENT();
    vector->m_end = newEnd;
    return first;
}

void __cdecl DialogSrvRegisterStaticCleanupSet()
{
    typedef unsigned int (__cdecl *CleanupRegistration)();
    const CleanupRegistration registrations[4] = {
        &DialogSrv_StaticCleanupRegister03,
        &DialogSrv_StaticCleanupRegister04,
        &DialogSrv_StaticCleanupRegister05,
        &DialogSrv_StaticCleanupRegister06
    };
    for (unsigned int registrationIndex = 0u; registrationIndex<4u; ++registrationIndex)
    {
        const unsigned char guard = CrtStaticInitializationGuards[registrationIndex];
        if ((guard&1u) == 0u)
            CrtStaticInitializationGuards[registrationIndex] = static_cast<unsigned char>(guard | 1u);
        (void) registrations[registrationIndex]();
    }
}

STRING FGets(std::FILE* file)
{
    char buffer[1024];
    memset(buffer, 0, sizeof(unsigned int));
    char* const readBuffer = std::fgets(buffer, 1024, file);
    const char* lineText = readBuffer?buffer:StringGlobals::EmptyText();
    return STRING(lineText);
}

template<> void LIST<PHRASE>::Insert(PHRASE value)
{
    if (m_count >= m_capacity)
    {
        const unsigned int grown = static_cast<unsigned int>(m_capacity)*2u+4u;
        const int newCapacity = BitsToSigned32(grown);
        if (newCapacity > m_capacity)
        {
            PHRASE* const oldPhrases = m_data;
            unsigned int* const allocation = static_cast<unsigned int*>(
                CrtNew(grown*sizeof(PHRASE)+sizeof(unsigned int)));
            PHRASE* newPhrases = 0;
            if (allocation)
            {
                *allocation = grown;
                newPhrases = reinterpret_cast<PHRASE*>(allocation+1);
                as2::core::constructArray(newPhrases, sizeof(PHRASE), grown,
                    [](void* element){ ::new (element) PHRASE(); },
                    [](void* element){ static_cast<PHRASE*>(element)->~PHRASE(); });
            }
            m_data = newPhrases;
            if (!newPhrases) ReportAllocationFailure(grown);
            if (oldPhrases)
            {
                for (int index = 0; index < m_capacity; ++index)
                    m_data[index] = oldPhrases[index];
                (void) PhraseVectorDeletingDestructorAlias(oldPhrases, 3u);
            }
            m_capacity = newCapacity;
        }
    }
    PHRASE* const destination = m_data+m_count;
    m_count = BitsToSigned32(static_cast<unsigned int>(m_count)+1u);
    *destination = value;
}

template<> LIST<PHRASE>::~LIST()
{
    if (m_data)
        (void) PhraseVectorDeletingDestructorAlias(m_data, 3u);
    m_data = 0;
    m_count = 0;
}

int zDlgSrv::ReleaseData()
{
    DialogTreeControl* tree = &m_dialogTree;
    DialogTreeIterator position = DialogTreeBegin(tree);
    while (!DialogTreeEqual(position, DialogTreeEnd(tree)))
    {
        DIALOG* const dialog = DialogTreeDereference(position)->m_dialog;
        if (dialog)
        {
            dialog->~DIALOG();
            CrtFreePointer(dialog);
        }
        DialogTreeConstIterator_Inc(&position);
    }
    DialogTreeClear(tree);
    return static_cast<int>(PtrToU32(tree->m_head));
}

bool zDlgSrv::IsDlgNumValid(int key)
{
    return DialogTreeValue(&m_dialogTree, key) != 0;
}

DIALOG* zDlgSrv::GetDialog(int _iDlgKey)
{
    if (!IsDlgNumValid(_iDlgKey))
    {
        if (!IsDlgNumValid(_iDlgKey))
            DebugAssert(5u, "IsDlgNumValid(_iDlgKey)", "..\\src\\dialogSrv.cpp", 289u);
        (void) ::gpLogCons->LogStr(LT_ERR, "invalid dialog %d in GetDialog", _iDlgKey);
    }
    return DialogTreeValue(&m_dialogTree, _iDlgKey);
}


DialogVectorIterator DialogCommentVector_Insert(DialogVector8Control* vector,
    DialogVectorIterator position, const COMMENT& value)
{
    const unsigned int oldCount = static_cast<unsigned int>(DialogCommentVector_Size(vector));
    unsigned int index = 0u;
    if (oldCount != 0u)
    {
        if (vector->m_begin > vector->m_end)
            ::_invalid_parameter_noinfo();
        if (!position.m_owner || position.m_owner != vector)
            ::_invalid_parameter_noinfo();
        index = static_cast<unsigned int>(position.m_position-vector->m_begin);
    }

    COMMENT* const insertionPosition = position.m_position;
    const COMMENT valueCopy(value);
    const COMMENT* const sourceValue = &valueCopy;
    COMMENT* const end = vector->m_end;
    COMMENT* const begin = vector->m_begin;
    const unsigned int capacity = begin != 0
        ? static_cast<unsigned int>(vector->m_capEnd-begin) : 0u;
    const unsigned int maxCount = 0x1FFFFFFFu;
    if (maxCount-oldCount < 1u)
        throw std::length_error("vector<T> too long");

    if (capacity < oldCount+1u)
    {
        unsigned int newCapacity = maxCount-capacity/2u < capacity
            ? 0u : capacity+capacity/2u;
        if (newCapacity < oldCount+1u)
            newCapacity = oldCount+1u;
        COMMENT* const newBegin = static_cast<COMMENT*>(CrtNew(newCapacity*sizeof(COMMENT)));
        if (!newBegin)
            throw std::bad_alloc();
        COMMENT* constructedEnd = newBegin;
        try
        {
            constructedEnd = reinterpret_cast<COMMENT*>(DialogCommentVector_Ucopy(
                reinterpret_cast<unsigned char*>(begin),
                reinterpret_cast<unsigned char*>(insertionPosition),
                reinterpret_cast<unsigned char*>(constructedEnd)));
            DialogCommentVector_Ufill(constructedEnd, 1u, sourceValue);
            ++constructedEnd;
            (void) DialogCommentVector_Ucopy(reinterpret_cast<unsigned char*>(insertionPosition),
                reinterpret_cast<unsigned char*>(end), reinterpret_cast<unsigned char*>(constructedEnd));
        }
        catch (...)
        {
            DialogCommentVector_Destroy(newBegin, constructedEnd);
            CrtFreePointer(newBegin);
            throw;
        }
        DialogCommentVector_Destroy(begin, end);
        CrtFreePointer(begin);
        vector->m_capEnd = newBegin+newCapacity;
        vector->m_begin = newBegin;
        vector->m_end = newBegin+oldCount+1u;
    }
    else if (insertionPosition == end)
    {
        DialogCommentVector_Ufill(end, 1u, sourceValue);
        vector->m_end = end+1;
    }
    else
    {
        (void) DialogCommentVector_Ucopy(reinterpret_cast<unsigned char*>(end-1),
            reinterpret_cast<unsigned char*>(end), reinterpret_cast<unsigned char*>(end));
        vector->m_end = end+1;
        (void) DialogComment_CopyBackward(reinterpret_cast<unsigned char*>(insertionPosition),
            reinterpret_cast<unsigned char*>(end-1), reinterpret_cast<unsigned char*>(end));
        DialogComment_Fill(insertionPosition, insertionPosition+1, sourceValue);
    }
    if (vector->m_begin > vector->m_end)
        ::_invalid_parameter_noinfo();
    COMMENT* const result = vector->m_begin+index;
    if (result > vector->m_end || result < vector->m_begin)
        ::_invalid_parameter_noinfo();
    return DialogVectorIterator{vector, result};
}

static void DialogCommentVectorPushBack(DialogVector8Control* vector, const COMMENT& value)
{
    if (vector->m_begin != 0 && vector->m_end < vector->m_capEnd)
    {
        DialogCommentVector_Ufill(vector->m_end, 1u, &value);
        ++vector->m_end;
        return;
    }
    if (vector->m_begin > vector->m_end)
        ::_invalid_parameter_noinfo();
    (void) DialogCommentVector_Insert(vector, DialogVectorIterator{vector, vector->m_end}, value);
}

namespace {
static int DialogParseInteger(const char* text)
{
    if (text[1] == 'x')
    {
        int value;
        (void) sscanf(text, "%i", &value);
        return value;
    }
    return atoi(text);
}

static DIALOG* DialogRecordForKey(DialogSrv* service, int key)
{
    return DialogTreeValue(&service->m_dialogTree, key);
}

static PHRASE::PAUSE_TYPE DialogLastPauseType(DIALOG* dialog)
{
    if (dialog->m_phrases.m_count == 0) return PHRASE::PAUSE;
    unsigned int index = static_cast<unsigned int>(dialog->m_phrases.m_count-1);

    if (dialog->m_phrases.m_count>999999) index = 999999u;
    return dialog->m_phrases.m_data[index].m_pauseType;
}

static void DialogInsertPhrase(DIALOG* dialog, const STRING& text,
    PHRASE::PAUSE_TYPE pauseType, const STRING& voiceFile, int pauseDuration,
    int sfx, int exp, int money)
{
    dialog->m_phrases.Insert(PHRASE(text, pauseType, voiceFile, sfx, pauseDuration, exp, money));
}

static void DialogInsertComment(DialogSrv* service, const STRING& text, int dialogKey)
{
    COMMENT comment(text, dialogKey);
    DialogCommentVectorPushBack(&service->m_dialogVector, comment);
}

static void DialogLogOpenFailure(const STRING& source)
{
    STRING category("DIALOG");
    (void)::Error->Error(category, E_LOAD, source, 0u);
}

static void DialogLogDeprecatedPauseDuration()
{
    iLogConsole* const logger = ::gpLogCons;
    (void) logger->LogStr(LT_WARN, "default <PauseDuration> not used any more");
}
}

void zDlgSrv::LoadDialogs(const STRING& source)
{
    zDlgSrv* service = this;

    ReleaseData();
    (void) DialogCommentVector_Erase(&service->m_dialogVector,
        DialogVectorIterator{&service->m_dialogVector, service->m_dialogVector.m_begin},
        DialogVectorIterator{&service->m_dialogVector, service->m_dialogVector.m_end});

    std::FILE* file = 0;
    {
        STRING mode("rb");
        if (source.m_value[0] != 0)
            file = std::fopen(source.m_value, mode.m_value);
    }
    if (!file)
    {
        if (source.m_value[0] != 0) DialogLogOpenFailure(source);
        return;
    }

    STRING phraseText(StringGlobals::EmptyText());
    STRING voiceFile(StringGlobals::EmptyText());
    STRING lastLine;

    service->m_dialogRecord.m_coor.x = 0u;
    service->m_dialogRecord.m_coor.y = 0u;
    service->m_dialogRecord.m_coor.z = 0.0f;
    service->m_dialogRecord.m_radio = 0u;
    service->m_dialogRecord.m_faceDirection = 0u;
    service->m_dialogRecord.m_unitVid[0] = 2u;

    int pauseDuration = 0;
    int sfx = 0;
    int exp = 0;
    int money = 0;
    int currentDialogKey = 0;
    int pendingBlankLines = 0;

    for (; ;)
    {
        if (std::feof(file) != 0) break;

        STRING fetchedLine = FGets(file);
        STRING line(fetchedLine);
        lastLine = line;
        line.RemoveEndChars(" \n\r\t");

        const char* text = line.m_value;
        const bool numericDialogTag = (text[0] == '<' && CrtIsDigit(static_cast<unsigned int>(static_cast<int>(static_cast<signed char>(text[1])))) != 0u && strstr(text, ">") != 0);
        const bool commentLine = (text[0] == ';');

        if (text[0] == 0)
        {
            ++pendingBlankLines;

        }
        else if (numericDialogTag || commentLine)
        {
            while (pendingBlankLines>0)
            {
                STRING empty;
                DialogInsertComment(service, empty, currentDialogKey);
                --pendingBlankLines;
            }
            pendingBlankLines = 0;

            if (commentLine)
            {
                DialogInsertComment(service, line, currentDialogKey);
                continue;
            }
        }
        else
        {
            pendingBlankLines = 0;
        }

        if (numericDialogTag)
        {
            phraseText.RemoveEndChars(" \n\r\t");
            if (currentDialogKey != 0 && phraseText.m_value[0] != 0)
            {
                const PHRASE::PAUSE_TYPE pauseType = DialogLastPauseType(DialogRecordForKey(service, currentDialogKey));
                DialogInsertPhrase(DialogRecordForKey(service, currentDialogKey), phraseText, pauseType, voiceFile, pauseDuration, sfx, exp, money);
                voiceFile = "";
            }

            STRING tagBody = line.After("<").Before(">");

            if (tagBody.m_value[0] != 0)
            {
                const int groupValue = DialogParseInteger(tagBody.Before("_").m_value);
                const int indexValue = DialogParseInteger(tagBody.After("_").m_value);

                currentDialogKey = (groupValue != 0 && indexValue != 0)
                    ? GetDlgKey(groupValue, indexValue)
                    : GetDlgKey(DialogParseInteger(tagBody.m_value), 0);

                DIALOG* newDialog = static_cast<DIALOG*>(CrtNew(sizeof(DIALOG)));
                if (newDialog)(void) DialogCtor(newDialog);
                DialogTreeValue(&service->m_dialogTree, currentDialogKey) = newDialog;
            }

            const int radioFont = service->m_dialogRecord.m_unitVid[1];
            DialogRecordForKey(service, currentDialogKey)->m_unitVid[1] = radioFont;
            const int normalFont = service->m_dialogRecord.m_unitVid[0];
            DialogRecordForKey(service, currentDialogKey)->m_unitVid[0] = normalFont;
            phraseText = "";
            voiceFile = "";
            sfx = 0;
            exp = 0;
            money = 0;
            pauseDuration = 0;
            DIALOG* currentDialog = DialogRecordForKey(service, currentDialogKey);
            currentDialog->m_coor = service->m_dialogRecord.m_coor;
            const unsigned int faceDirection = service->m_dialogRecord.m_faceDirection;
            const unsigned int radio = service->m_dialogRecord.m_radio;
            currentDialog = DialogRecordForKey(service, currentDialogKey);
            currentDialog->m_radio = radio;
            currentDialog->m_faceDirection = faceDirection;
            continue;
        }

        if (strcmp(text, "<pause>") == 0 || strcmp(text, "<click or pause>") == 0)
        {
            phraseText.RemoveEndChars(" \n\r\t");
            if (currentDialogKey != 0)
            {
                DIALOG* currentDialog = DialogRecordForKey(service, currentDialogKey);
                DialogInsertPhrase(currentDialog, phraseText,
                    strcmp(text, "<pause>") == 0?PHRASE::PAUSE:PHRASE::CLICK_OR_PAUSE,
                    voiceFile, pauseDuration, sfx, exp, money);
                pauseDuration = 0u;
                sfx = 0u;
                exp = 0;
                money = 0;
                voiceFile = "";
            }
            phraseText = "";
            continue;
        }

        if (strncmp(text, "<Speech=", 8u) == 0)
        {
            voiceFile = line.After("<Speech=").Before(">");
            continue;
        }

        if (strncmp(text, "<Font=", 6u) == 0)
        {
            if (currentDialogKey != 0)
            {
                const int font = DialogParseInteger(line.After("<Font=").Before(">").m_value);
                DialogRecordForKey(service, currentDialogKey)->m_unitVid[0] = font;
                DialogRecordForKey(service, currentDialogKey)->m_unitVid[1] = font;
            }
            else service->m_dialogRecord.m_unitVid[0] = DialogParseInteger(line.After("<Font=").Before(">").m_value);
            continue;
        }

        if (strncmp(text, "<DefaultFontForRadio=", 21u) == 0)
        {
            service->m_dialogRecord.m_unitVid[1] = DialogParseInteger(line.After("<DefaultFontForRadio=").Before(">").m_value);
            continue;
        }

        if (strncmp(text, "<SFX=", 5u) == 0)
        {
            sfx = DialogParseInteger(line.After("<SFX=").Before(">").m_value);
            continue;
        }

        if (strncmp(text, "<Exp=", 5u) == 0)
        {
            exp = DialogParseInteger(line.After("<Exp=").Before(">").m_value);
            continue;
        }

        if (strncmp(text, "<Money=", 7u) == 0)
        {
            money = DialogParseInteger(line.After("<Money=").Before(">").m_value);
            continue;
        }

        if (strncmp(text, "<PauseDuration=", 15u) == 0)
        {
            pauseDuration = DialogParseInteger(line.After("<PauseDuration=").Before(">").m_value);
            if (currentDialogKey == 0) DialogLogDeprecatedPauseDuration();
            continue;
        }

        if (strncmp(text, "<Coor=", 6u) == 0)
        {
            float x, y, z;
            (void) sscanf(line.After("<Coor=").Before(">").m_value, "%f, %f, %f", &x, &y, &z);
            if (currentDialogKey != 0)
            {
                DIALOG* currentDialog = DialogRecordForKey(service, currentDialogKey);
                currentDialog->m_coor.x = x;
                currentDialog->m_coor.y = y;
                currentDialog->m_coor.z = z;
            }
            else
            {
                service->m_dialogRecord.m_coor.x = x;
                service->m_dialogRecord.m_coor.y = y;
                service->m_dialogRecord.m_coor.z = z;
            }
            continue;
        }

        if (strncmp(text, "<Icon=", 6u) == 0)
        {
            int groupText, indexText;
            (void) sscanf(line.After("<Icon=").Before(">").m_value, "%d, %d", &groupText, &indexText);
            if (currentDialogKey != 0)
            {
                DIALOG* currentDialog = DialogRecordForKey(service, currentDialogKey);
                currentDialog->m_radio = static_cast<unsigned int>(groupText);
                currentDialog->m_faceDirection = static_cast<unsigned int>(indexText);
            }
            else
            {
                service->m_dialogRecord.m_radio = static_cast<unsigned int>(groupText);
                service->m_dialogRecord.m_faceDirection = static_cast<unsigned int>(indexText);
            }
            continue;
        }

        phraseText += fetchedLine;
    }

    phraseText.RemoveEndChars(" \n\r\t");
    if (currentDialogKey != 0 && phraseText.m_value[0] != 0)
    {
        const PHRASE::PAUSE_TYPE pauseType = DialogLastPauseType(DialogRecordForKey(service, currentDialogKey));
        DialogInsertPhrase(DialogRecordForKey(service, currentDialogKey), phraseText, pauseType, voiceFile, pauseDuration, sfx, exp, money);
    }

    (void) std::fclose(file);
}

void __cdecl DialogComment_Construct(
    COMMENT* destination, const COMMENT* source)
{
    if (destination != 0)
        ::new (static_cast<void*>(destination)) COMMENT(*source);
}


void Dialog_Dtor(void* self)
{
    static_cast<DIALOG*>(self)->~DIALOG();
}

PHRASE::PHRASE(const STRING& text, PAUSE_TYPE pauseType,
    const STRING& voiceFile, int sfx, int pauseDuration, int exp, int money)
{
    m_sfx = sfx;
    m_exp = exp;
    m_money = money;
    m_text = text;
    m_text.RemoveEndChars(" \n\r\t");
    m_pauseType = pauseType;
    m_voiceFile = voiceFile;
    m_duration = pauseDuration;
}

COMMENT::COMMENT(STRING text, int dialogKey)
    : m_ownedString(text), m_dialogKey(dialogKey)
{
}

int  zDlgSrv_GetDlgKey(void* self, int group, int index)
{
    return static_cast<zDlgSrv*>(self)->GetDlgKey(group, index);
}
int  zDlgSrv_ReleaseData(void* self)
{
    return static_cast<zDlgSrv*>(self)->ReleaseData();
}
bool  zDlgSrv_IsDlgNumValid(void* self, int key)
{
    return static_cast<zDlgSrv*>(self)->IsDlgNumValid(key);
}
DIALOG*  zDlgSrv_GetDialog(void* self, int key)
{
    return static_cast<zDlgSrv*>(self)->GetDialog(key);
}
void  zDlgSrv_LoadDialogs(void* self, const STRING& source)
{
    static_cast<zDlgSrv*>(self)->LoadDialogs(source);
}
