/* MollenOS
 *
 * Copyright 2019, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 * System Memory Allocator (Based on SLAB design)
 */
#define __MODULE "HEAP"
//#define __TRACE

#include <arch/utils.h>
#include <memoryspace.h>
#include <machine.h>
#include <debug.h>
#include <heap.h>
#include <assert.h>
#include <string.h>

#define MEMORY_OVERRUN_PATTERN                      0xA5A5A5A5
#define MEMORY_SLAB_ONSITE_THRESHOLD                512
#define MEMORY_ATOMIC_CACHE(Cache, Core)            (MemoryAtomicCache_t*)(Cache->AtomicCaches + (Core * (sizeof(MemoryAtomicCache_t) + (Cache->ObjectCount * sizeof(void*)))))
#define MEMORY_ATOMIC_ELEMENT(AtomicCache, Element) ((uintptr_t**)((uintptr_t)AtomicCache + sizeof(MemoryAtomicCache_t) + (Element * sizeof(void*))))
#define MEMORY_SLAB_ELEMENT(Cache, Slab, Element)   (void*)((uintptr_t)Slab->Address + (Element * (Cache->ObjectSize + Cache->ObjectPadding)))

// All the standard caches DO not use contigious memory
static MemoryCache_t InitialCache = { 0 };
static struct FixedCache {
    size_t         ObjectSize;
    const char*    Name;
    MemoryCache_t* Cache;
    Flags_t        InitializationFlags;
} DefaultCaches[] = {
    { 32,     "size32_cache",     NULL, HEAP_CACHE_DEFAULT },
    { 64,     "size64_cache",     NULL, HEAP_CACHE_DEFAULT },
    { 128,    "size128_cache",    NULL, HEAP_CACHE_DEFAULT },
    { 256,    "size256_cache",    NULL, HEAP_CACHE_DEFAULT },
    { 512,    "size512_cache",    NULL, HEAP_CACHE_DEFAULT },
    { 1024,   "size1024_cache",   NULL, HEAP_CACHE_DEFAULT },
    { 2048,   "size2048_cache",   NULL, HEAP_CACHE_DEFAULT },
    { 4096,   "size4096_cache",   NULL, HEAP_CACHE_DEFAULT },
    { 8192,   "size8192_cache",   NULL, HEAP_CACHE_DEFAULT },
    { 16384,  "size16384_cache",  NULL, HEAP_CACHE_DEFAULT },
    { 32768,  "size32768_cache",  NULL, HEAP_CACHE_DEFAULT },
    { 65536,  "size65536_cache",  NULL, HEAP_CACHE_DEFAULT },
    { 131072, "size131072_cache", NULL, HEAP_CACHE_DEFAULT },
    { 262144, "size262144_cache", NULL, HEAP_CACHE_DEFAULT },
    { 0,      NULL,               NULL, 0 }
};

static uintptr_t
allocate_virtual_memory(Flags_t Flags, size_t PageCount)
{
    uintptr_t  Address;
    size_t     PageSize = GetMemorySpacePageSize();
    Flags_t    AllocationFlags = MAPPING_VIRTUAL_GLOBAL;
    OsStatus_t Status;
    
    if (Flags & HEAP_CONTIGIOUS) {
        AllocationFlags |= MAPPING_PHYSICAL_CONTIGIOUS;
    }
    else {
        AllocationFlags |= MAPPING_PHYSICAL_DEFAULT;
    }
    
    Status = CreateMemorySpaceMapping(GetCurrentMemorySpace(), NULL, &Address, PageSize * PageCount, 
        MAPPING_COMMIT | MAPPING_DOMAIN, MAPPING_PHYSICAL_DEFAULT | MAPPING_VIRTUAL_GLOBAL, __MASK);
    if (Status != OsSuccess) {
        ERROR("Ran out of memory for allocation in the heap");
        return 0;
    }
    return Address;
}

static void
free_virtual_memory(uintptr_t Address, size_t PageCount)
{
    size_t     PageSize = GetMemorySpacePageSize();
    OsStatus_t Status   = RemoveMemorySpaceMapping(GetCurrentMemorySpace(), Address, PageSize * PageCount);
    if (Status != OsSuccess) {
        ERROR("Failed to free allocation 0x%" PRIxIN " of size 0x%" PRIxIN "", Address, PageSize * PageCount);
    }
}

static inline struct FixedCache*
cache_find_fixed_size(
    _In_ size_t Size)
{
    struct FixedCache* Selected = NULL;
    int                i = 0;

    // Find a cache to do the allocation in
    while (DefaultCaches[i].ObjectSize != 0) {
        if (Size <= DefaultCaches[i].ObjectSize) {
            Selected = &DefaultCaches[i];
            break;
        }
        i++;
    }
    return Selected;
}

static int
slab_allocate_index(MemoryCache_t* Cache, MemorySlab_t* Slab)
{
    int i;
    assert(Slab->NumberOfFreeObjects <= Cache->ObjectCount);
    for (i = 0; i < (int)Cache->ObjectCount; i++) {
        unsigned Block  = i / 8;
        unsigned Offset = i % 8;
        uint8_t  Bit    = (1u << Offset);
        if (!(Slab->FreeBitmap[Block] & Bit)) {
            Slab->FreeBitmap[Block] |= Bit;
            Slab->NumberOfFreeObjects--;
            return i;
        }
    }
    return -1;
}

static void
slab_free_index(MemoryCache_t* Cache, MemorySlab_t* Slab, int Index)
{
    unsigned Block  = Index / 8;
    unsigned Offset = Index % 8;
    uint8_t  Bit    = (1u << Offset);
    assert(Slab->NumberOfFreeObjects < Cache->ObjectCount);
    if (Index < (int)Cache->ObjectCount) {
        Slab->FreeBitmap[Block] &= ~(Bit);
        Slab->NumberOfFreeObjects++;
    }
}

static int
slab_contains_address(MemoryCache_t* Cache, MemorySlab_t* Slab, uintptr_t Address)
{
    size_t    ObjectSize = Cache->ObjectSize + Cache->ObjectPadding;
    uintptr_t Base       = (uintptr_t)Slab->Address;
    uintptr_t End        = Base + (Cache->ObjectCount * ObjectSize);
    int       Index      = -1;
    if (Address >= Base && Address < End) {
        Index = (int)((Address - Base) / ObjectSize);
        assert(Index >= 0);
        assert(Index < Cache->ObjectCount);
    }
    return Index;
}

static void
slab_initalize_objects(MemoryCache_t* Cache, MemorySlab_t* Slab)
{
    uintptr_t Address = (uintptr_t)Slab->Address;
    size_t    i;

    for (i = 0; i < Cache->ObjectCount; i++) {
        if (Cache->ObjectConstructor) {
            Cache->ObjectConstructor(Cache, (void*)Address);
        }

        Address += Cache->ObjectSize;
        if (Cache->Flags & HEAP_DEBUG_OVERRUN) {
            *((uint32_t*)Address) = MEMORY_OVERRUN_PATTERN;
        }
        Address += Cache->ObjectPadding;
    }
}

static void 
slab_destroy_objects(
    MemoryCache_t* Cache, MemorySlab_t* Slab)
{
    uintptr_t Address = (uintptr_t)Slab->Address;
    size_t    i;

    for (i = 0; i < Cache->ObjectCount; i++) {
        if (Cache->ObjectDestructor) {
            Cache->ObjectDestructor(Cache, (void*)Address);
        }
        Address += Cache->ObjectSize + Cache->ObjectPadding;
    }
}

static MemorySlab_t* 
slab_create(
    _In_ MemoryCache_t* Cache)
{
    MemorySlab_t* Slab;
    uintptr_t     ObjectAddress;
    uintptr_t     DataAddress = allocate_virtual_memory(Cache->Flags, Cache->PageCount);
    TRACE("slab_create(%s): 0x%" PRIxIN "", Cache->Name, DataAddress);

    if (Cache->SlabOnSite) {
        Slab          = (MemorySlab_t*)DataAddress;
        ObjectAddress = DataAddress + Cache->SlabStructureSize;
        if (Cache->ObjectAlignment != 0 && (ObjectAddress % Cache->ObjectAlignment)) {
            ObjectAddress += Cache->ObjectAlignment - (ObjectAddress % Cache->ObjectAlignment);
        }
    }
    else {
        // Protect against recursive allocations here, if we are default cache
        // make sure we don't allocate from the same size as us
        if (Cache->Flags & HEAP_CACHE_DEFAULT) {
            struct FixedCache* Fixed = cache_find_fixed_size(Cache->SlabStructureSize);
            if (Fixed->ObjectSize == Cache->ObjectSize) {
                FATAL(FATAL_SCOPE_KERNEL, "Recursive allocation %u for default cache %u", 
                    Cache->SlabStructureSize, Cache->ObjectSize);
            }
        }
        
        Slab          = (MemorySlab_t*)kmalloc(Cache->SlabStructureSize);
        ObjectAddress = DataAddress;
    }

    // Handle debug flags
    if (Cache->Flags & HEAP_DEBUG_USE_AFTER_FREE) {
        memset((void*)DataAddress, MEMORY_OVERRUN_PATTERN, (Cache->PageCount * GetMemorySpacePageSize()));
    }
    memset(Slab, 0, Cache->SlabStructureSize);

    // Initialize slab
    Slab->NumberOfFreeObjects = Cache->ObjectCount;
    Slab->FreeBitmap          = (uint8_t*)((uintptr_t)Slab + sizeof(MemorySlab_t));
    Slab->Address             = (uintptr_t*)ObjectAddress;
    slab_initalize_objects(Cache, Slab);
    return Slab;
}

static void slab_destroy(MemoryCache_t* Cache, MemorySlab_t* Slab)
{
    slab_destroy_objects(Cache, Slab);
    if (!Cache->SlabOnSite) {
        free_virtual_memory((uintptr_t)Slab->Address, Cache->PageCount);
        kfree(Slab);
    }
    else {
        free_virtual_memory((uintptr_t)Slab, Cache->PageCount);
    }
}

static void
slab_dump_information(
    _In_ MemoryCache_t* Cache,
    _In_ MemorySlab_t*  Slab)
{
    uintptr_t StartAddress = (uintptr_t)Slab->Address;
    uintptr_t EndAddress   = StartAddress + (Cache->ObjectCount * (Cache->ObjectSize + Cache->ObjectPadding));
    
    // Write slab information
    WRITELINE(" -- slab: 0x%" PRIxIN " => 0x%" PRIxIN ", FreeObjects %" PRIuIN "", StartAddress, EndAddress, Slab->NumberOfFreeObjects);
}

static void
cache_dump_information(
    _In_ MemoryCache_t* Cache)
{
    CollectionItem_t* Node;
    
    // Write cache information
    WRITELINE("%s: Object Size %" PRIuIN ", Alignment %" PRIuIN ", Padding %" PRIuIN ", Count %" PRIuIN ", FreeObjects %" PRIuIN "",
        Cache->Name, Cache->ObjectSize, Cache->ObjectAlignment, Cache->ObjectPadding,
        Cache->ObjectCount, Cache->NumberOfFreeObjects);
        
    // Dump slabs
    WRITELINE("* full slabs");
    _foreach(Node, &Cache->FullSlabs) {
        slab_dump_information(Cache, (MemorySlab_t*)Node);
    }
    
    WRITELINE("* partial slabs");
    _foreach(Node, &Cache->PartialSlabs) {
        slab_dump_information(Cache, (MemorySlab_t*)Node);
    }
    
    WRITELINE("* free slabs");
    _foreach(Node, &Cache->FreeSlabs) {
        slab_dump_information(Cache, (MemorySlab_t*)Node);
    }
    WRITELINE("");
}

static int
cache_contains_address(
    _In_ MemoryCache_t* Cache,
    _In_ uintptr_t      Address)
{
    CollectionItem_t* Node;
    int               Found = 0;
    TRACE("cache_contains_address(%s, 0x%" PRIxIN ")", Cache->Name, Address);

    // Check partials first, we have to lock the cache as slabs can 
    // get rearranged as we go, which is not ideal
    dslock(&Cache->SyncObject);
    _foreach(Node, &Cache->PartialSlabs) {
        int Index = slab_contains_address(Cache, (MemorySlab_t*)Node, Address);
        if (Index != -1) {
            Found = 1;
            break;
        }
    }

    // Otherwise move on to the full slabs
    if (!Found) {
        _foreach(Node, &Cache->FullSlabs) {
            int Index = slab_contains_address(Cache, (MemorySlab_t*)Node, Address);
            if (Index != -1) {
                Found = 1;
                break;
            }
        }
    }
    dsunlock(&Cache->SyncObject);
    return Found;
}

static inline size_t
cache_calculate_slab_structure_size(
    _In_ size_t ObjectsPerSlab)
{
    size_t SlabStructure = sizeof(MemorySlab_t);
    // Calculate how many bytes the slab metadata will need
    SlabStructure += (ObjectsPerSlab / 8);
    if (ObjectsPerSlab % 8) {
        SlabStructure++;
    }
    return SlabStructure;
}

static size_t
cache_calculate_atomic_cache(
    _In_ MemoryCache_t* Cache)
{
    // number of cpu * cpu_cache_objects + number of cpu * objects per slab * pointer size
    size_t BytesRequired = 0;
    if (GetMachine()->NumberOfCores > 1) {
        BytesRequired = (GetMachine()->NumberOfCores * 
            (sizeof(MemoryAtomicCache_t) + (Cache->ObjectCount * sizeof(void*))));
        if (cache_find_fixed_size(BytesRequired) == NULL) {
            BytesRequired = 0;
        }
    }
    return BytesRequired;
}

static void
cache_initialize_atomic_cache(
    _In_ MemoryCache_t* Cache)
{
    size_t AtomicCacheSize = cache_calculate_atomic_cache(Cache);
    int    i;
    if (AtomicCacheSize) {
        Cache->AtomicCaches = (uintptr_t)kmalloc(AtomicCacheSize);
        memset((void*)Cache->AtomicCaches, 0, AtomicCacheSize);
        for (i = 0; i < GetMachine()->NumberOfCores; i++) {
            MemoryAtomicCache_t* AtomicCache = MEMORY_ATOMIC_CACHE(Cache, i);
            AtomicCache->Available           = 0;
            AtomicCache->Limit               = Cache->ObjectCount;
        }
    }
}

static void
cache_drain_atomic_cache(MemoryCache_t* Cache)
{
    // Send out IPI to all cores to empty their caches and put them into the gloal
    // cache, this should be done when attempting to free up memory.
    // @todo
}

// Object size is the size of the actual object
// Object alignment is the required alignment before an object starts
// Object padding is a combined size of extra data after each object including alingment
static void
cache_calculate_slab_size(
    _In_ MemoryCache_t* Cache,
    _In_ size_t         ObjectSize,
    _In_ size_t         ObjectAlignment,
    _In_ size_t         ObjectPadding)
{
    // We only ever accept 1/8th of a page of wasted bytes
    size_t PageSize        = GetMemorySpacePageSize();
    size_t AcceptedWastage = (PageSize >> 4);
    size_t ReservedSpace   = 0;
    int    SlabOnSite      = 0;
    size_t PageCount       = 1;
    int    i               = 0;
    size_t ObjectsPerSlab;
    size_t Wastage;
    TRACE("cache_calculate_slab_size(%s, %" PRIuIN ", %" PRIuIN ", %" PRIuIN ")",
        (Cache == NULL ? "null" : Cache->Name), ObjectSize, ObjectAlignment, ObjectPadding);

    if ((ObjectSize + ObjectPadding) < MEMORY_SLAB_ONSITE_THRESHOLD) {
        ObjectsPerSlab = PageSize / (ObjectSize + ObjectPadding);
        SlabOnSite     = 1;
        ReservedSpace  = cache_calculate_slab_structure_size(ObjectsPerSlab) + ObjectAlignment;
    }
    ObjectsPerSlab = (PageSize - ReservedSpace) / (ObjectSize + ObjectPadding);
    Wastage        = (PageSize - (ObjectsPerSlab * (ObjectSize + ObjectPadding))) - ReservedSpace;
    TRACE(" * %" PRIuIN " Objects (%" PRIiIN "), On %" PRIuIN " Pages, %" PRIuIN " Bytes of Waste (%" PRIuIN " Bytes Reserved)", 
        ObjectsPerSlab, SlabOnSite, PageCount, Wastage, ReservedSpace);
    
    // Make sure we always have atleast 1 element
    while (ObjectsPerSlab == 0 || Wastage > AcceptedWastage) {
        assert(i != 9); // 8 = 256 pages, allow for no more
        i++;
        PageCount      = (1 << i);
        ObjectsPerSlab = (PageSize * PageCount) / (ObjectSize + ObjectPadding);
        if (SlabOnSite) {
            ReservedSpace = cache_calculate_slab_structure_size(ObjectsPerSlab) + ObjectAlignment;
        }
        ObjectsPerSlab = ((PageSize * PageCount) - ReservedSpace) / (ObjectSize + ObjectPadding);
        Wastage        = ((PageSize * PageCount) - (ObjectsPerSlab * (ObjectSize + ObjectPadding)) - ReservedSpace);
    }

    // We do, detect if there is enough room for us to keep the slab on site
    // and still provide proper alignment
    if (!SlabOnSite && (Wastage >= (cache_calculate_slab_structure_size(ObjectsPerSlab) + ObjectAlignment))) {
        SlabOnSite = 1;
    }

    // We have two special case, as this is an operating system memory allocator, and we use
    // 4096/8192 pretty frequently, we don't want just 1 page per slab, we instead increase this
    // to 16
    if (ObjectSize == 4096) {
        ObjectsPerSlab = 16;
        PageCount      = 16;
        SlabOnSite     = 0;
        ReservedSpace  = 0;
    }
    else if (ObjectSize == 8192) {
        ObjectsPerSlab = 8;
        PageCount      = 16;
        SlabOnSite     = 0;
        ReservedSpace  = 0;
    }

    // Make sure we calculate the size of the slab in the case it's not allocated on site
    if (ReservedSpace == 0) {
        ReservedSpace = cache_calculate_slab_structure_size(ObjectsPerSlab);
    }

    if (Cache != NULL) {
        Cache->ObjectCount       = ObjectsPerSlab;
        Cache->SlabOnSite        = SlabOnSite;
        Cache->SlabStructureSize = ReservedSpace;
        Cache->PageCount         = PageCount;
    }
    TRACE(" => %" PRIuIN " Objects (%" PRIiIN "), On %" PRIuIN " Pages", ObjectsPerSlab, SlabOnSite, PageCount);
}

void
MemoryCacheConstruct(
    _In_ MemoryCache_t* Cache,
    _In_ const char*    Name,
    _In_ size_t         ObjectSize,
    _In_ size_t         ObjectAlignment,
    _In_ Flags_t        Flags,
    _In_ void(*ObjectConstructor)(struct MemoryCache*, void*),
    _In_ void(*ObjectDestructor)(struct MemoryCache*, void*))
{
    size_t ObjectPadding = 0;

    // Calculate padding
    if (Flags & HEAP_DEBUG_OVERRUN) {
        ObjectPadding += 4;
    }

    if (ObjectAlignment != 0 && ((ObjectSize + ObjectPadding) % ObjectAlignment)) {
        ObjectPadding += ObjectAlignment - ((ObjectSize + ObjectPadding) % ObjectAlignment);
    }
    memset(Cache, 0, sizeof(MemoryCache_t));

    Cache->Name              = Name;
    Cache->Flags             = Flags;
    Cache->ObjectSize        = ObjectSize;
    Cache->ObjectAlignment   = ObjectAlignment;
    Cache->ObjectPadding     = ObjectPadding;
    Cache->ObjectConstructor = ObjectConstructor;
    Cache->ObjectDestructor  = ObjectDestructor;
    cache_calculate_slab_size(Cache, ObjectSize, ObjectAlignment, ObjectPadding);
    
    // We only perform this if it has been requested and is not a default cache
    if (!(Cache->Flags & (HEAP_CACHE_DEFAULT | HEAP_SLAB_NO_ATOMIC_CACHE))) {
        cache_initialize_atomic_cache(Cache);
    }
}

MemoryCache_t*
MemoryCacheCreate(
    _In_ const char* Name,
    _In_ size_t      ObjectSize,
    _In_ size_t      ObjectAlignment,
    _In_ Flags_t     Flags,
    _In_ void(*ObjectConstructor)(struct MemoryCache*, void*),
    _In_ void(*ObjectDestructor)(struct MemoryCache*, void*))
{
    MemoryCache_t* Cache = (MemoryCache_t*)MemoryCacheAllocate(&InitialCache);
    MemoryCacheConstruct(Cache, Name, ObjectSize, ObjectAlignment, Flags, ObjectConstructor, ObjectDestructor);
    return Cache;
}

static void
cache_destroy_list(
    _In_ MemoryCache_t* Cache,
    _In_ Collection_t*  List)
{
    MemorySlab_t* Slab;
    DataKey_t Key = { 0 };

    // Iterate all the slab lists, unlink the slab and then destroy it
    Slab = (MemorySlab_t*)CollectionGetNodeByKey(List, Key, 0);
    while (Slab != NULL) {
        CollectionRemoveByNode(List, &Slab->Header);
        slab_destroy(Cache, Slab);
    }
}

void
MemoryCacheDestroy(
    _In_ MemoryCache_t* Cache)
{
    // If there are any cpu caches, free them, there is no need to drain the cpu caches
    // here at this point as we assume that when destroying a cache we do it for good reason
    if (Cache->AtomicCaches != 0) {
        kfree((void*)Cache->AtomicCaches);
    }
    cache_destroy_list(Cache, &Cache->FreeSlabs);
    cache_destroy_list(Cache, &Cache->PartialSlabs);
    cache_destroy_list(Cache, &Cache->FullSlabs);
    MemoryCacheFree(&InitialCache, Cache);
}

void*
MemoryCacheAllocate(
    _In_ MemoryCache_t* Cache)
{
    MemorySlab_t* Slab;
    void*         Allocated = NULL;
    int           Index;
    TRACE("MemoryCacheAllocate(%s)", Cache->Name);

    // Can we allocate from cpu cache?
    if (Cache->AtomicCaches != 0) {
        MemoryAtomicCache_t* AtomicCache = MEMORY_ATOMIC_CACHE(Cache, ArchGetProcessorCoreId());
        if (AtomicCache->Available > 0) {
            Allocated = MEMORY_ATOMIC_ELEMENT(AtomicCache, (--AtomicCache->Available))[0];
            return Allocated;
        }
    }

    // Otherwise allocate from global cache  0x407018
    dslock(&Cache->SyncObject);
    if (Cache->NumberOfFreeObjects != 0) {
        if (CollectionLength(&Cache->PartialSlabs) != 0) {
            Slab = (MemorySlab_t*)CollectionBegin(&Cache->PartialSlabs);
            assert(Slab->NumberOfFreeObjects != 0);
        }
        else {
            Slab = (MemorySlab_t*)CollectionPopFront(&Cache->FreeSlabs);
            CollectionAppend(&Cache->PartialSlabs, &Slab->Header);
        }
        
        Index = slab_allocate_index(Cache, Slab);
        assert(Index != -1);
        if (!Slab->NumberOfFreeObjects) {
            // Last index, push to full
            CollectionRemoveByNode(&Cache->PartialSlabs, &Slab->Header);
            CollectionAppend(&Cache->FullSlabs, &Slab->Header);
        }
        Cache->NumberOfFreeObjects--;
    }
    else {
        dsunlock(&Cache->SyncObject);
        // allocate and build new slab, put it into partial list right away
        // as we are allocating a new object immediately
        Slab = slab_create(Cache);
        assert(Slab != NULL);
        Index = slab_allocate_index(Cache, Slab);
        assert(Index != -1);
        
        dslock(&Cache->SyncObject);
        if (!Slab->NumberOfFreeObjects) {
            CollectionAppend(&Cache->FullSlabs, &Slab->Header);
        }
        else {
            Cache->NumberOfFreeObjects += (Cache->ObjectCount - 1);
            CollectionAppend(&Cache->PartialSlabs, &Slab->Header);
        }
    }
    dsunlock(&Cache->SyncObject);
    
    Allocated = MEMORY_SLAB_ELEMENT(Cache, Slab, Index);
    TRACE(" => 0x%" PRIxIN "", Allocated);
    return Allocated;
}

void
MemoryCacheFree(
    _In_ MemoryCache_t* Cache,
    _In_ void*          Object)
{
    CollectionItem_t* Node;
    int CheckFull = 1;
    TRACE("MemoryCacheFree(%s, 0x%" PRIxIN ")", Cache->Name, Object);

    // Handle debug flags
    if (Cache->Flags & HEAP_DEBUG_USE_AFTER_FREE) {
        memset(Object, MEMORY_OVERRUN_PATTERN, Cache->ObjectSize);
    }

    // Can we push to cpu cache?
    if (Cache->AtomicCaches != 0) {
        MemoryAtomicCache_t* AtomicCache = MEMORY_ATOMIC_CACHE(Cache, ArchGetProcessorCoreId());
        if (AtomicCache->Available < AtomicCache->Limit) {
            MEMORY_ATOMIC_ELEMENT(AtomicCache, (AtomicCache->Available++))[0] = Object;
            return;
        }
    }

    dslock(&Cache->SyncObject);

    // Check partials first, and move to free if neccessary
    _foreach(Node, &Cache->PartialSlabs) {
        MemorySlab_t* Slab  = (MemorySlab_t*)Node;
        int           Index = slab_contains_address(Cache, Slab, (uintptr_t)Object);
        if (Index != -1) {
            slab_free_index(Cache, Slab, Index);
            if (Slab->NumberOfFreeObjects == Cache->ObjectCount) {
                CollectionRemoveByNode(&Cache->PartialSlabs, Node);
                CollectionAppend(&Cache->FreeSlabs, Node);
            }
            CheckFull = 0;
            Cache->NumberOfFreeObjects++;
            break;
        }
    }

    // Otherwise move on to the full slabs, and move to partial if neccessary
    if (CheckFull) {
        _foreach(Node, &Cache->FullSlabs) {
            MemorySlab_t* Slab = (MemorySlab_t*)Node;
            int           Index = slab_contains_address(Cache, Slab, (uintptr_t)Object);
            if (Index != -1) {
                slab_free_index(Cache, Slab, Index);
                CollectionRemoveByNode(&Cache->FullSlabs, Node);
                
                // A slab can go directly from full to free if the count is 1
                if (Cache->ObjectCount == 1) {
                    CollectionAppend(&Cache->FreeSlabs, Node);
                }
                else {
                    CollectionAppend(&Cache->PartialSlabs, Node);
                }
                Cache->NumberOfFreeObjects++;
                break;
            }
        }
    }
    dsunlock(&Cache->SyncObject);
}

int MemoryCacheReap(void)
{
    // Iterate the caches in the system and drain their cpu caches
    // Then start looking at the entirely free slabs, and free them.
    return 0;
}

void* kmalloc(size_t Size)
{
    TRACE("kmalloc(%" PRIuIN ")", Size);
    struct FixedCache* Selected = cache_find_fixed_size(Size);
    if (Selected == NULL) {
        ERROR("Could not find a cache for size %" PRIuIN "", Size);
        MemoryCacheDump(NULL);
        assert(0);   
    }

    // If the cache does not exist, we must create it
    if (Selected->Cache == NULL) {
        Selected->Cache = MemoryCacheCreate(Selected->Name, Selected->ObjectSize,
            Selected->ObjectSize, Selected->InitializationFlags, NULL, NULL);
        
        // We must initialize this after updating the cache variable to ensure
        // we don't end up in an enternal loop
        cache_initialize_atomic_cache(Selected->Cache);
    }
    return MemoryCacheAllocate(Selected->Cache);
}

void* kmalloc_p(size_t Size, uintptr_t* DmaOut)
{
    void* Allocation = kmalloc(Size);
    if (Allocation != NULL && DmaOut != NULL) {
        *DmaOut = GetMemorySpaceMapping(GetCurrentMemorySpace(), (VirtualAddress_t)Allocation);
    }
    return Allocation;
}

void kfree(void* Object)
{
    struct FixedCache* Selected = NULL;
    int                i = 0;

    // Find the cache that the allocation was done in
    while (DefaultCaches[i].ObjectSize != 0) {
        if (DefaultCaches[i].Cache != NULL) {
            if (cache_contains_address(DefaultCaches[i].Cache, (uintptr_t)Object)) {
                Selected = &DefaultCaches[i];
                break;
            }
        }
        i++;
    }
    if (Selected == NULL) {
        ERROR("Could not find a cache for object 0x%" PRIxIN "", Object);
        MemoryCacheDump(NULL);
        assert(0);   
    }
    MemoryCacheFree(Selected->Cache, Object);
}

void
MemoryCacheDump(
    _In_ MemoryCache_t* Cache)
{
    int i = 0;
    
    if (Cache != NULL) {
        cache_dump_information(Cache);
        return;
    }
    
    // Otherwise dump default caches
    while (DefaultCaches[i].ObjectSize != 0) {
        if (DefaultCaches[i].Cache != NULL) {
            cache_dump_information(DefaultCaches[i].Cache);
        }
        i++;
    }
    
    // Dump memory information
    WRITELINE("\nMemory Stats: %" PRIuIN "/%" PRIuIN " Bytes, %" PRIuIN "/%" PRIuIN " Blocks",
        GetMachine()->PhysicalMemory.BlocksAllocated * GetMemorySpacePageSize(), 
        GetMachine()->PhysicalMemory.BlockCount * GetMemorySpacePageSize(),
        GetMachine()->PhysicalMemory.BlocksAllocated, GetMachine()->PhysicalMemory.BlockCount);
}

void
MemoryCacheInitialize(void)
{
    MemoryCacheConstruct(&InitialCache, "cache_cache", sizeof(MemoryCache_t), 0, 0, NULL, NULL);
}
