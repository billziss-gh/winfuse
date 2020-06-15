/**
 * @file shared/km/cache.c
 *
 * @copyright 2019-2020 Bill Zissimopoulos
 */
/*
 * This file is part of WinFuse.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * Affero General Public License version 3 as published by the Free
 * Software Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the AGPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <shared/km/shared.h>

/*
 * FUSE "entry" cache
 *
 * The cache is implemented as a hash table that maps parent inode number, child name
 * tuples to FUSE protocol "entries":
 *     <parent_ino, child_name> -> <child_ino, child_attr> ("entry")
 *
 * There are two important complications:
 *
 * (1) Entries have an expiration time after which they may no longer be valid.
 * Entries that have expired must not be returned to users of this class and must eventually
 * be "forgotten" (i.e. a FUSE FORGET message must be sent to the user mode file system).
 *
 * (2) Entries may be in use when they become expired/invalid. In this case the
 * corresponding FORGET message must be delayed to ensure that the user mode file system
 * keeps the relevant inode information around (even if the associated information is
 * expired).
 *
 * To accommodate complication (1) this implementation maintains cached entries in an LRU
 * (least-recently-used) list and expires them periodically. It should be noted that this
 * implementation does not sort entries by expiration time; so it is possible for an entry
 * that has not expired but was not recently used to be purged prior to an entry that has
 * expired, but was recently used.
 *
 * To accommodate complication (2) this implementation maintains a list of monotonic
 * "generations" that control when entries are actually "forgotten". As file system
 * operations arrive, the current generation is computed from the "interrupt time"
 * and a reference is taken on that generation (i.e. a reference count is incremented).
 * When the operation is complete the reference on the associated generation is released
 * (i.e. the reference count is decremented). If there are no remaining references on the
 * generation (i.e. the reference count is zero) the generation can be "forgotten", which
 * means that all entries that have expired and have not been used since the generation's
 * time can now be "forgotten" (i.e. the corresponding FUSE messages can be sent).
 *
 * These two primary complications together with the fact that the implementation must
 * deal with failures and re-setting existing entries make the code rather complicated.
 */

NTSTATUS FuseCacheCreate(ULONG Capacity, BOOLEAN CaseInsensitive, FUSE_CACHE **PCache);
VOID FuseCacheDelete(FUSE_CACHE *Cache);
VOID FuseCacheExpirationRoutine(FUSE_CACHE *Cache,
    FUSE_INSTANCE *Instance, UINT64 ExpirationTime);
NTSTATUS FuseCacheReferenceGen(FUSE_CACHE *Cache, PVOID *PGen);
VOID FuseCacheDereferenceGen(FUSE_CACHE *Cache, PVOID Gen);
BOOLEAN FuseCacheGetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry, PVOID *PItem);
VOID FuseCacheSetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry, PVOID *PItem);
VOID FuseCacheRemoveEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name);
VOID FuseCacheReferenceItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheDereferenceItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheQuickExpireItem(FUSE_CACHE *Cache, PVOID Item);
VOID FuseCacheDeleteForgotten(PLIST_ENTRY ForgetList);
BOOLEAN FuseCacheForgetOne(PLIST_ENTRY ForgetList, FUSE_PROTO_FORGET_ONE *PForgetOne);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseCacheCreate)
#pragma alloc_text(PAGE, FuseCacheDelete)
#pragma alloc_text(PAGE, FuseCacheExpirationRoutine)
#pragma alloc_text(PAGE, FuseCacheReferenceGen)
#pragma alloc_text(PAGE, FuseCacheDereferenceGen)
#pragma alloc_text(PAGE, FuseCacheGetEntry)
#pragma alloc_text(PAGE, FuseCacheSetEntry)
#pragma alloc_text(PAGE, FuseCacheRemoveEntry)
#pragma alloc_text(PAGE, FuseCacheReferenceItem)
#pragma alloc_text(PAGE, FuseCacheDereferenceItem)
#pragma alloc_text(PAGE, FuseCacheQuickExpireItem)
#pragma alloc_text(PAGE, FuseCacheDeleteForgotten)
#pragma alloc_text(PAGE, FuseCacheForgetOne)
#endif

typedef struct _FUSE_CACHE_ITEM FUSE_CACHE_ITEM;

struct _FUSE_CACHE
{
    ULONG Capacity;
    BOOLEAN CaseInsensitive;
    FAST_MUTEX Mutex;
    LIST_ENTRY GenList;
    LIST_ENTRY ItemList;
    LIST_ENTRY ForgetList;
    ULONG ItemCount;
    ULONG ItemBucketCount;
    PVOID ItemBuckets[];
};

struct _FUSE_CACHE_GEN
{
    LIST_ENTRY ListEntry;
    LONG RefCount;
    UINT64 InterruptTime;
};

struct _FUSE_CACHE_ITEM
{
    struct _FUSE_CACHE_ITEM *DictNext;
    LIST_ENTRY ListEntry;
    BOOLEAN NoForget;
    ULONG Hash;
    UINT64 ParentIno;
    STRING Name;
    UINT64 NLookup;
    UINT64 ExpirationTime;
    UINT64 LastUsedTime;
    FUSE_PROTO_ENTRY Entry;
    LONG QuickExpiry;
    LONG RefCount;
    CHAR NameBuf[];
};

static inline UINT64 FuseCacheForgetTime(FUSE_CACHE *Cache, UINT64 InterruptTime)
{
    if (!IsListEmpty(&Cache->GenList))
    {
        FUSE_CACHE_GEN *Gen = CONTAINING_RECORD(Cache->GenList.Flink, FUSE_CACHE_GEN, ListEntry);
        if (InterruptTime >= Gen->InterruptTime)
            InterruptTime = Gen->InterruptTime - 1;
    }
    return InterruptTime;
}

static inline BOOLEAN FuseCacheForgetNextItem(FUSE_CACHE *Cache,
    UINT64 ExpirationTime, PLIST_ENTRY ForgetList)
{
    if (!IsListEmpty(&Cache->ForgetList))
    {
        FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Cache->ForgetList.Flink, FUSE_CACHE_ITEM, ListEntry);
        if (FuseCacheForgetTime(Cache, ExpirationTime) >= Item->LastUsedTime)
        {
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(ForgetList, &Item->ListEntry);
            return TRUE;
        }
    }
    return FALSE;
}

static inline BOOLEAN FuseCacheExpireItem(FUSE_CACHE *Cache,
    FUSE_CACHE_ITEM *Item)
{
    ULONG HashIndex = Item->Hash % Cache->ItemBucketCount;
    for (FUSE_CACHE_ITEM **P = (PVOID)&Cache->ItemBuckets[HashIndex]; *P; P = &(*P)->DictNext)
        if (*P == Item)
        {
            *P = (*P)->DictNext;
            RemoveEntryList(&Item->ListEntry);
            Cache->ItemCount--;
            if (0 == InterlockedDecrement(&Item->RefCount))
                InsertTailList(&Cache->ForgetList, &Item->ListEntry);
            return TRUE;
        }
    return FALSE;
}

static inline BOOLEAN FuseCacheExpireNextItem(FUSE_CACHE *Cache,
    UINT64 ExpirationTime)
{
    if (!IsListEmpty(&Cache->ItemList))
    {
        FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Cache->ItemList.Flink, FUSE_CACHE_ITEM, ListEntry);
        if (ExpirationTime >= Item->ExpirationTime ||
            InterlockedCompareExchange(&Item->QuickExpiry, 1, 1))
            return FuseCacheExpireItem(Cache, Item);
    }
    return FALSE;
}

static inline size_t hash_chars(const char *s, size_t length)
{
    /* djb2: see http://www.cse.yorku.ca/~oz/hash.html */
    size_t h = 5381;
    for (const char *t = s + length; t > s; ++s)
        h = 33 * h + *s;
    return h;
}

static inline size_t hash_upper_chars(const char *s, size_t length)
{
    /* djb2: see http://www.cse.yorku.ca/~oz/hash.html */
    size_t h = 5381;
    for (const char *t = s + length; t > s; ++s)
        h = 33 * h + RtlUpperChar(*s);
    return h;
}

static inline ULONG FuseCacheHash(UINT64 ParentIno, PSTRING Name, BOOLEAN CaseInsensitive)
{
    return (ULONG)FuseHashMix64(ParentIno) ^
        (0 != Name ? (ULONG)(CaseInsensitive ?
            hash_upper_chars(Name->Buffer, Name->Length) : hash_chars(Name->Buffer, Name->Length)) : 0);
}

static inline FUSE_CACHE_ITEM *FuseCacheLookupHashedItem(FUSE_CACHE *Cache,
    ULONG Hash, UINT64 ParentIno, PSTRING Name)
{
    FUSE_CACHE_ITEM *Item = 0;
    ULONG HashIndex = Hash % Cache->ItemBucketCount;
    for (FUSE_CACHE_ITEM *ItemX = Cache->ItemBuckets[HashIndex]; ItemX; ItemX = ItemX->DictNext)
        if (ItemX->Hash == Hash &&
            ItemX->ParentIno == ParentIno &&
            RtlEqualString(&ItemX->Name, Name, Cache->CaseInsensitive))
        {
            Item = ItemX;
            break;
        }
    return Item;
}

static inline VOID FuseCacheAddItem(FUSE_CACHE *Cache,
    FUSE_CACHE_ITEM *Item)
{
    ULONG HashIndex = Item->Hash % Cache->ItemBucketCount;
#if DBG
    for (FUSE_CACHE_ITEM *ItemX = Cache->ItemBuckets[HashIndex]; ItemX; ItemX = ItemX->DictNext)
        if (ItemX->Hash == Item->Hash &&
            ItemX->ParentIno == Item->ParentIno &&
            RtlEqualString(&ItemX->Name, &Item->Name, Cache->CaseInsensitive))
        {
            ASSERT(0);
        }
#endif
    Item->DictNext = Cache->ItemBuckets[HashIndex];
    Cache->ItemBuckets[HashIndex] = Item;
    /* mark as most-recently used */
    InsertTailList(&Cache->ItemList, &Item->ListEntry);
    Cache->ItemCount++;
}

static inline FUSE_CACHE_ITEM *FuseCacheUpdateHashedItem(FUSE_CACHE *Cache,
    ULONG Hash, UINT64 ParentIno, PSTRING Name,
    UINT64 ExpirationTime, UINT64 LastUsedTime, FUSE_PROTO_ENTRY *Entry)
{
    FUSE_CACHE_ITEM *Item = FuseCacheLookupHashedItem(Cache, Hash, ParentIno, Name);
    if (0 != Item)
    {
        if (Entry->nodeid == Item->Entry.nodeid &&
            !InterlockedCompareExchange(&Item->QuickExpiry, 1, 1))
        {
            Item->NLookup++;
            Item->ExpirationTime = ExpirationTime;
            Item->LastUsedTime = LastUsedTime;
            RtlCopyMemory(&Item->Entry, Entry, sizeof Item->Entry);

            /* mark as most-recently used */
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ItemList, &Item->ListEntry);
        }
        else
        {
            FuseCacheExpireItem(Cache, Item);
            Item = 0;
        }
    }
    return Item;
}

NTSTATUS FuseCacheCreate(ULONG Capacity, BOOLEAN CaseInsensitive, FUSE_CACHE **PCache)
{
    PAGED_CODE();

    FUSE_CACHE *Cache;
    ULONG CacheSize;

    *PCache = 0;

    if (0 == Capacity)
        Capacity = ((PAGE_SIZE - sizeof *Cache) / sizeof Cache->ItemBuckets[0]) * 3 / 4;

    CacheSize = (Capacity * 4 / 3) * sizeof Cache->ItemBuckets[0] + sizeof *Cache;
    CacheSize = FSP_FSCTL_ALIGN_UP(CacheSize, PAGE_SIZE);

    Cache = FuseAllocNonPaged(CacheSize);
        /* FAST_MUTEX's must be in non-paged memory */
    if (0 == Cache)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(Cache, CacheSize);
    Cache->Capacity = Capacity;
    Cache->CaseInsensitive = CaseInsensitive;
    ExInitializeFastMutex(&Cache->Mutex);
    InitializeListHead(&Cache->GenList);
    InitializeListHead(&Cache->ItemList);
    InitializeListHead(&Cache->ForgetList);
    Cache->ItemBucketCount = (CacheSize - sizeof *Cache) / sizeof Cache->ItemBuckets[0];

    *PCache = Cache;

    return STATUS_SUCCESS;
}

VOID FuseCacheDelete(FUSE_CACHE *Cache)
{
    PAGED_CODE();

    for (PLIST_ENTRY Entry = Cache->GenList.Flink; &Cache->GenList != Entry;)
    {
        FUSE_CACHE_GEN *Gen = CONTAINING_RECORD(Entry, FUSE_CACHE_GEN, ListEntry);
        Entry = Entry->Flink;
        FuseFree(Gen);
    }

    FuseCacheDeleteForgotten(&Cache->ItemList);
    FuseCacheDeleteForgotten(&Cache->ForgetList);

    FuseFree(Cache);
}

VOID FuseCacheExpirationRoutine(FUSE_CACHE *Cache,
    FUSE_INSTANCE *Instance, UINT64 ExpirationTime)
{
    PAGED_CODE();

    LIST_ENTRY ForgetList;

    InitializeListHead(&ForgetList);

    ExAcquireFastMutex(&Cache->Mutex);

    while (FuseCacheExpireNextItem(Cache, ExpirationTime))
        ;

    while (FuseCacheForgetNextItem(Cache, ExpirationTime, &ForgetList))
        ;

    ExReleaseFastMutex(&Cache->Mutex);

    for (PLIST_ENTRY Entry = ForgetList.Flink; &ForgetList != Entry;)
    {
        FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Entry, FUSE_CACHE_ITEM, ListEntry);
        Entry = Entry->Flink;
        if (Item->NoForget)
        {
            RemoveEntryList(&Item->ListEntry);
            FuseFree(Item);
        }
    }

    if (!IsListEmpty(&ForgetList))
    {
        BOOLEAN Success = DEBUGTEST(90) &&
            NT_SUCCESS(FuseProtoPostForget(Instance, &ForgetList));
        if (!Success)
        {
            ASSERT(!IsListEmpty(&ForgetList));

            ExAcquireFastMutex(&Cache->Mutex);

            /* re-add forgotten items in the "forget list" */
            RemoveEntryList(&ForgetList);
                /* see AppendTailList comments */
#if 0
            /* append */
            AppendTailList(&Cache->ForgetList, &ForgetList);
#else
            /* prepend */
            PLIST_ENTRY ListToPrepend = ForgetList.Flink;
            PLIST_ENTRY ListHead = &Cache->ForgetList;
            PLIST_ENTRY ListBegin = ListHead->Flink;
            ListBegin->Blink = ListToPrepend->Blink;
            ListHead->Flink = ListToPrepend;
            ListToPrepend->Blink->Flink = ListBegin;
            ListToPrepend->Blink = ListHead;
#endif

            ExReleaseFastMutex(&Cache->Mutex);
        }
    }
}

NTSTATUS FuseCacheReferenceGen(FUSE_CACHE *Cache, PVOID *PGen)
{
    PAGED_CODE();

    UINT64 InterruptTime = KeQueryInterruptTime() / 10000000 * 10000000;
    FUSE_CACHE_GEN *Gen = 0, *NewGen = 0;

    *PGen = 0;

    ExAcquireFastMutex(&Cache->Mutex);

    if (!IsListEmpty(&Cache->GenList))
    {
        Gen = CONTAINING_RECORD(Cache->GenList.Blink, FUSE_CACHE_GEN, ListEntry);
        if (InterruptTime <= Gen->InterruptTime)
            Gen->RefCount++;
        else
            Gen = 0;
    }

    ExReleaseFastMutex(&Cache->Mutex);

    if (0 == Gen)
    {
        NewGen = FuseAlloc(sizeof *NewGen);
        if (0 == NewGen)
            return STATUS_INSUFFICIENT_RESOURCES;

        RtlZeroMemory(NewGen, sizeof *NewGen);
        NewGen->RefCount = 1;
        NewGen->InterruptTime = InterruptTime;

        ExAcquireFastMutex(&Cache->Mutex);

        if (!IsListEmpty(&Cache->GenList))
        {
            Gen = CONTAINING_RECORD(Cache->GenList.Blink, FUSE_CACHE_GEN, ListEntry);
            if (InterruptTime <= Gen->InterruptTime)
                Gen->RefCount++;
            else
                Gen = 0;
        }
        if (0 == Gen)
        {
            InsertTailList(&Cache->GenList, &NewGen->ListEntry);

            Gen = NewGen;
            NewGen = 0;
        }

        ExReleaseFastMutex(&Cache->Mutex);
    }

    *PGen = Gen;

    if (0 != NewGen)
        FuseFree(NewGen);

    return STATUS_SUCCESS;
}

VOID FuseCacheDereferenceGen(FUSE_CACHE *Cache, PVOID Gen0)
{
    PAGED_CODE();

    FUSE_CACHE_GEN *Gen = Gen0;
    LONG RefCount;

    if (0 == Gen)
        return;

    ExAcquireFastMutex(&Cache->Mutex);
    RefCount = --Gen->RefCount;
    if (0 == RefCount)
        RemoveEntryList(&Gen->ListEntry);
    ExReleaseFastMutex(&Cache->Mutex);

    if (0 == RefCount)
        FuseFree(Gen);
}

BOOLEAN FuseCacheGetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry, PVOID *PItem)
{
    PAGED_CODE();

    UINT64 InterruptTime = KeQueryInterruptTime();
    FUSE_CACHE_ITEM *Item;
    ULONG Hash = FuseCacheHash(ParentIno, Name, Cache->CaseInsensitive);

    ExAcquireFastMutex(&Cache->Mutex);

    Item = FuseCacheLookupHashedItem(Cache, Hash, ParentIno, Name);
    if (0 != Item)
    {
        if (InterruptTime < Item->ExpirationTime &&
            !InterlockedCompareExchange(&Item->QuickExpiry, 1, 1))
        {
            Item->LastUsedTime = InterruptTime;
            RtlCopyMemory(Entry, &Item->Entry, sizeof Item->Entry);

            /* mark as most-recently used */
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ItemList, &Item->ListEntry);
        }
        else
        {
            FuseCacheExpireItem(Cache, Item);
            Item = 0;
        }
    }

    ExReleaseFastMutex(&Cache->Mutex);

    *PItem = Item;
    return 0 != Item;
}

VOID FuseCacheSetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry, PVOID *PItem)
{
    PAGED_CODE();

    UINT64 InterruptTime = KeQueryInterruptTime();
    UINT64 EntryTimeout = Entry->entry_valid * 10000000 + Entry->entry_valid_nsec / 100;
    UINT64 AttrTimeout = Entry->attr_valid * 10000000 + Entry->attr_valid_nsec / 100;
    UINT64 ExpirationTime = InterruptTime +
        (EntryTimeout < AttrTimeout ? EntryTimeout : AttrTimeout);
    FUSE_CACHE_ITEM *Item = 0, *NewItem = 0;
    ULONG Hash = FuseCacheHash(ParentIno, Name, Cache->CaseInsensitive);

    ExAcquireFastMutex(&Cache->Mutex);

    Item = FuseCacheUpdateHashedItem(Cache,
        Hash, ParentIno, Name, ExpirationTime, InterruptTime, Entry);

    ExReleaseFastMutex(&Cache->Mutex);

    if (0 == Item)
    {
        NewItem = FuseAllocMustSucceed(FIELD_OFFSET(FUSE_CACHE_ITEM, NameBuf) + Name->Length);

        RtlZeroMemory(NewItem, FIELD_OFFSET(FUSE_CACHE_ITEM, NameBuf));
        NewItem->NoForget =
            /* the root is not LOOKUP'ed; free without FORGET */
            ParentIno == FUSE_PROTO_ROOT_INO && 1 == Name->Length && '/' == Name->Buffer[0];
        NewItem->Hash = Hash;
        NewItem->ParentIno = ParentIno;
        NewItem->Name.Length = NewItem->Name.MaximumLength = Name->Length;
        NewItem->Name.Buffer = NewItem->NameBuf;
        NewItem->NLookup = 1;
        NewItem->ExpirationTime = ExpirationTime;
        NewItem->LastUsedTime = InterruptTime;
        NewItem->RefCount = 1;
        RtlCopyMemory(&NewItem->Entry, Entry, sizeof NewItem->Entry);
        RtlCopyMemory(&NewItem->NameBuf, Name->Buffer, Name->Length);

        ExAcquireFastMutex(&Cache->Mutex);

        Item = FuseCacheUpdateHashedItem(Cache,
            Hash, ParentIno, Name, ExpirationTime, InterruptTime, Entry);
        if (0 == Item)
        {
            if (Cache->ItemCount >= Cache->Capacity)
                FuseCacheExpireNextItem(Cache, (UINT64)-1LL);

            FuseCacheAddItem(Cache, NewItem);

            Item = NewItem;
            NewItem = 0;
        }

        ExReleaseFastMutex(&Cache->Mutex);
    }

    if (0 != NewItem)
        FuseFree(NewItem);

    *PItem = Item;
}

VOID FuseCacheRemoveEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name)
{
    PAGED_CODE();

    FUSE_CACHE_ITEM *Item;
    ULONG Hash = FuseCacheHash(ParentIno, Name, Cache->CaseInsensitive);

    ExAcquireFastMutex(&Cache->Mutex);

    Item = FuseCacheLookupHashedItem(Cache, Hash, ParentIno, Name);
    if (0 != Item)
        FuseCacheExpireItem(Cache, Item);

    ExReleaseFastMutex(&Cache->Mutex);
}

VOID FuseCacheReferenceItem(FUSE_CACHE *Cache, PVOID Item0)
{
    PAGED_CODE();

    FUSE_CACHE_ITEM *Item = Item0;

    InterlockedIncrement(&Item->RefCount);
}

VOID FuseCacheDereferenceItem(FUSE_CACHE *Cache, PVOID Item0)
{
    PAGED_CODE();

    FUSE_CACHE_ITEM *Item = Item0;
    LONG RefCount;

    if (0 == Item)
        return;

    RefCount = InterlockedDecrement(&Item->RefCount);
    if (0 == RefCount)
    {
        ExAcquireFastMutex(&Cache->Mutex);
        InsertTailList(&Cache->ForgetList, &Item->ListEntry);
        ExReleaseFastMutex(&Cache->Mutex);
    }
}

VOID FuseCacheQuickExpireItem(FUSE_CACHE *Cache, PVOID Item0)
{
    PAGED_CODE();

    FUSE_CACHE_ITEM *Item = Item0;

    InterlockedExchange(&Item->QuickExpiry, 1);
}

VOID FuseCacheDeleteForgotten(PLIST_ENTRY ForgetList)
{
    PAGED_CODE();

    for (PLIST_ENTRY Entry = ForgetList->Flink; ForgetList != Entry;)
    {
        FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Entry, FUSE_CACHE_ITEM, ListEntry);
        Entry = Entry->Flink;
        FuseFree(Item);
    }
}

BOOLEAN FuseCacheForgetOne(PLIST_ENTRY ForgetList, FUSE_PROTO_FORGET_ONE *PForgetOne)
{
    PAGED_CODE();

    PLIST_ENTRY Entry = RemoveHeadList(ForgetList);
    if (ForgetList == Entry)
        return FALSE;

    FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Entry, FUSE_CACHE_ITEM, ListEntry);
    ASSERT(!Item->NoForget);
    PForgetOne->nodeid = Item->Entry.nodeid;
    PForgetOne->nlookup = Item->NLookup;
    FuseFree(Item);

    return TRUE;
}
