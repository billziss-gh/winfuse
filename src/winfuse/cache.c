/**
 * @file winfuse/cache.c
 *
 * @copyright 2019 Bill Zissimopoulos
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

#include <winfuse/driver.h>

/*
 * FUSE "entry" cache
 *
 * The cache is implemented as a hash table that maps inode number, name tuples to
 * FUSE protocol "entries":
 *     <ino, name> -> <ino, attr> ("entry").
 *
 * Cached entries are also maintained in an LRU (least-recently-used) list and are
 * expired periodically. It should be noted that this implementation does not sort
 * entries by expiration time; so it is possible for an entry that has not expired
 * but was not recently used to be purged prior to an entry that has expired, but
 * was recently used.
 */

/*
 * FUSE_CACHE_REMOVE_HASHED_ITEM
 *
 * Define this macro to include code to remove hashed items.
 *
 * (Untested: please test prior to using.)
 */
//#define FUSE_CACHE_REMOVE_HASHED_ITEM

NTSTATUS FuseCacheCreate(ULONG Capacity, BOOLEAN CaseInsensitive, FUSE_CACHE **PCache);
VOID FuseCacheDelete(FUSE_CACHE *Cache);
VOID FuseCacheDeleteItems(PLIST_ENTRY ItemList);
BOOLEAN FuseCacheForgetNextItem(PLIST_ENTRY ItemList, PUINT64 PIno);
VOID FuseCacheInvalidateExpired(FUSE_CACHE *Cache, UINT64 ExpirationTime,
    PDEVICE_OBJECT DeviceObject);
BOOLEAN FuseCacheGetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry);
NTSTATUS FuseCacheSetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FuseCacheCreate)
#pragma alloc_text(PAGE, FuseCacheDelete)
#pragma alloc_text(PAGE, FuseCacheDeleteItems)
#pragma alloc_text(PAGE, FuseCacheForgetNextItem)
#pragma alloc_text(PAGE, FuseCacheInvalidateExpired)
#pragma alloc_text(PAGE, FuseCacheGetEntry)
#pragma alloc_text(PAGE, FuseCacheSetEntry)
#endif

struct _FUSE_CACHE
{
    FAST_MUTEX Mutex;
    BOOLEAN CaseInsensitive;
    ULONG Capacity, ItemCount;
    LIST_ENTRY ItemList, ForgetList;
    ULONG ItemBucketCount;
    PVOID ItemBuckets[];
};

typedef struct _FUSE_CACHE_ITEM
{
    LIST_ENTRY ListEntry;
    struct _FUSE_CACHE_ITEM *DictNext;
    ULONG Hash;
    UINT64 ParentIno;
    STRING Name;
    UINT64 ExpirationTime;
    FUSE_PROTO_ENTRY Entry;
    CHAR NameBuf[];
} FUSE_CACHE_ITEM;

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
            RtlEqualString(&Item->Name, Name, Cache->CaseInsensitive))
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
                ASSERT(0);
#endif
    Item->DictNext = Cache->ItemBuckets[HashIndex];
    Cache->ItemBuckets[HashIndex] = Item;
    /* mark as most-recently used */
    InsertTailList(&Cache->ItemList, &Item->ListEntry);
    Cache->ItemCount++;
}

static inline FUSE_CACHE_ITEM *FuseCacheRemoveItem(FUSE_CACHE *Cache,
    FUSE_CACHE_ITEM *Item)
{
    ULONG HashIndex = Item->Hash % Cache->ItemBucketCount;
    for (FUSE_CACHE_ITEM **P = (PVOID)&Cache->ItemBuckets[HashIndex]; *P; P = &(*P)->DictNext)
        if (*P == Item)
        {
            *P = (*P)->DictNext;
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ForgetList, &Item->ListEntry);
            Cache->ItemCount--;
            break;
        }
    return Item;
}

static inline FUSE_CACHE_ITEM *FuseCacheRemoveExpiredItem(FUSE_CACHE *Cache,
    UINT64 ExpirationTime)
{
    PLIST_ENTRY Head = &Cache->ItemList;
    PLIST_ENTRY Entry = Head->Flink;
    if (Head == Entry)
        return 0;
    FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Entry, FUSE_CACHE_ITEM, ListEntry);
    if (FuseExpirationTimeValid2(Item->ExpirationTime, ExpirationTime))
        return 0;
    return FuseCacheRemoveItem(Cache, Item);
}

#if defined(FUSE_CACHE_REMOVE_HASHED_ITEM)
static inline FUSE_CACHE_ITEM *FuseCacheRemoveHashedItem(FUSE_CACHE *Cache,
    ULONG Hash, UINT64 ParentIno, PSTRING Name)
{
    FUSE_CACHE_ITEM *Item = 0;
    ULONG HashIndex = Item->Hash % Cache->ItemBucketCount;
    for (FUSE_CACHE_ITEM **P = (PVOID)&Cache->ItemBuckets[HashIndex]; *P; P = &(*P)->DictNext)
        if ((*P)->Hash == Hash &&
            (*P)->ParentIno == ParentIno &&
            RtlEqualString(&Item->Name, Name, Cache->CaseInsensitive))
        {
            Item = *P;
            *P = (*P)->DictNext;
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ForgetList, &Item->ListEntry);
            Cache->ItemCount--;
            break;
        }
    return Item;
}
#endif

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
    ExInitializeFastMutex(&Cache->Mutex);
    InitializeListHead(&Cache->ItemList);
    InitializeListHead(&Cache->ForgetList);
    Cache->CaseInsensitive = CaseInsensitive;
    Cache->Capacity = Capacity;
    Cache->ItemBucketCount = (CacheSize - sizeof *Cache) / sizeof Cache->ItemBuckets[0];

    *PCache = Cache;

    return STATUS_SUCCESS;
}

VOID FuseCacheDelete(FUSE_CACHE *Cache)
{
    PAGED_CODE();

    FuseCacheInvalidateExpired(Cache, (UINT64)-1LL, 0);
    FuseFree(Cache);
}

VOID FuseCacheDeleteItems(PLIST_ENTRY ItemList)
{
    PAGED_CODE();

    for (PLIST_ENTRY Entry = ItemList->Flink; ItemList != Entry; Entry = Entry->Flink)
    {
        FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Entry, FUSE_CACHE_ITEM, ListEntry);
        FuseFree(Item);
    }
}

BOOLEAN FuseCacheForgetNextItem(PLIST_ENTRY ItemList, PUINT64 PIno)
{
    PAGED_CODE();

    PLIST_ENTRY Entry = RemoveHeadList(ItemList);
    if (ItemList == Entry)
        return FALSE;

    FUSE_CACHE_ITEM *Item = CONTAINING_RECORD(Entry, FUSE_CACHE_ITEM, ListEntry);
    *PIno = Item->Entry.nodeid;
    FuseFree(Item);

    return TRUE;
}

VOID FuseCacheInvalidateExpired(FUSE_CACHE *Cache, UINT64 ExpirationTime,
    PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    LIST_ENTRY ForgetList;
    NTSTATUS Result;

    ExAcquireFastMutex(&Cache->Mutex);

    while (FuseCacheRemoveExpiredItem(Cache, ExpirationTime))
        ;

    ForgetList = Cache->ForgetList;
    InitializeListHead(&Cache->ForgetList);

    ExReleaseFastMutex(&Cache->Mutex);

    if (&ForgetList != ForgetList.Flink)
    {
        if (0 != DeviceObject)
        {
            Result = FuseProtoPostForget(DeviceObject, &ForgetList);
            if (!NT_SUCCESS(Result))
            {
                ExAcquireFastMutex(&Cache->Mutex);
                AppendTailList(&Cache->ForgetList, &ForgetList);
                ExReleaseFastMutex(&Cache->Mutex);
            }
        }
        else
            FuseCacheDeleteItems(&ForgetList);
    }
}

BOOLEAN FuseCacheGetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry)
{
    PAGED_CODE();

    FUSE_CACHE_ITEM *Item;
    ULONG Hash = FuseCacheHash(ParentIno, Name, Cache->CaseInsensitive);

    ExAcquireFastMutex(&Cache->Mutex);

    Item = FuseCacheLookupHashedItem(Cache, Hash, ParentIno, Name);
    if (0 != Item)
    {
        if (FuseExpirationTimeValid(Item->ExpirationTime))
        {
            RtlCopyMemory(Entry, &Item->Entry, sizeof Item->Entry);

            /* mark as most-recently used */
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ItemList, &Item->ListEntry);
        }
        else
        {
            FuseCacheRemoveItem(Cache, Item);
            Item = 0;
        }
    }

    ExReleaseFastMutex(&Cache->Mutex);

    return 0 != Item;
}

NTSTATUS FuseCacheSetEntry(FUSE_CACHE *Cache, UINT64 ParentIno, PSTRING Name,
    FUSE_PROTO_ENTRY *Entry)
{
    PAGED_CODE();

    UINT64 ExpirationTime = 0;
    if (0 != Entry)
    {
        UINT64 EntryTimeout = Entry->entry_valid * 10000000 + Entry->entry_valid_nsec / 100;
        UINT64 AttrTimeout = Entry->attr_valid * 10000000 + Entry->attr_valid_nsec / 100;
        ExpirationTime = FuseExpirationTimeFromTimeout(EntryTimeout < AttrTimeout ?
            EntryTimeout : AttrTimeout);
    }

    FUSE_CACHE_ITEM *Item = 0, *NewItem = 0;
    ULONG Hash = FuseCacheHash(ParentIno, Name, Cache->CaseInsensitive);

    ExAcquireFastMutex(&Cache->Mutex);

    if (0 != Entry)
    {
        Item = FuseCacheLookupHashedItem(Cache, Hash, ParentIno, Name);
        if (0 != Item)
        {
            Item->ExpirationTime = ExpirationTime;
            RtlCopyMemory(&Item->Entry, Entry, sizeof Item->Entry);

            /* mark as most-recently used */
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ItemList, &Item->ListEntry);
        }
    }
    else
    {
#if defined(FUSE_CACHE_REMOVE_HASHED_ITEM)
        FuseCacheRemoveHashedItem(Cache, Hash, ParentIno, Name);
#else
        ASSERT(0);
#endif
    }

    ExReleaseFastMutex(&Cache->Mutex);

    if (0 != Entry && 0 == Item)
    {
        NewItem = FuseAlloc(FIELD_OFFSET(FUSE_CACHE_ITEM, NameBuf) + Name->Length);
        if (0 == NewItem)
            return STATUS_INSUFFICIENT_RESOURCES;

        RtlZeroMemory(NewItem, FIELD_OFFSET(FUSE_CACHE_ITEM, NameBuf));
        NewItem->Hash = Hash;
        NewItem->ParentIno = ParentIno;
        NewItem->Name.Length = NewItem->Name.MaximumLength = Name->Length;
        NewItem->Name.Buffer = NewItem->NameBuf;
        NewItem->ExpirationTime = ExpirationTime;
        RtlCopyMemory(&NewItem->Entry, Entry, sizeof NewItem->Entry);
        RtlCopyMemory(&NewItem->NameBuf, Name->Buffer, Name->Length);

        ExAcquireFastMutex(&Cache->Mutex);

        Item = FuseCacheLookupHashedItem(Cache, Hash, ParentIno, Name);
        if (0 != Item)
        {
            Item->ExpirationTime = ExpirationTime;
            RtlCopyMemory(&Item->Entry, Entry, sizeof Item->Entry);

            /* mark as most-recently used */
            RemoveEntryList(&Item->ListEntry);
            InsertTailList(&Cache->ItemList, &Item->ListEntry);
        }
        else
        {
            if (Cache->ItemCount >= Cache->Capacity)
                FuseCacheRemoveExpiredItem(Cache, (UINT64)-1LL);

            FuseCacheAddItem(Cache, NewItem);

            NewItem = 0;
        }

        ExReleaseFastMutex(&Cache->Mutex);
    }

    if (0 != NewItem)
        FuseFree(NewItem);

    return STATUS_SUCCESS;
}
