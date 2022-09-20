#ifndef COMMON_MAP_H
#define COMMON_MAP_H

// NOTE(ljre): Little HashMap: linear probing, immutable entries, exponencial growth.

#ifndef Map_HashFunc
#   define Map_HashFunc Map_DefaultHashFunc

static inline uint64
Map_DefaultHashFunc(String mem)
{
	uint64 hash = (uint64)2166136261ull;
	uintsize i = 0;
	
	while (i < mem.size) {
		uint64 value = (uint64)mem.data[i++];
		
		hash ^= value;
		hash *= (uint64)16777619ull;
	}
	
	return hash;
}
#endif //Map_HashFunc

struct Map_Entry
{
	uint64 hash;
	String key;
	void* value;
}
typedef Map_Entry;

struct Map typedef Map;
struct Map
{
	Map* next;
	Arena* arena;
	
	uint32 cap; // always a power of 2
	uint32 len;
	
	Map_Entry entries[];
};

static inline bool
Map_IsEntryDead_(const Map_Entry* entry)
{ return entry->hash == UINT64_MAX && entry->value == NULL; }

static inline bool
Map_IsEntryEmpty_(const Map_Entry* entry)
{ return entry->hash == 0 && entry->value == NULL; }

static inline uint64
Map_MaxLenForBlock_(uint64 cap)
{ return cap>>1 | cap>>2; }

static inline uint64
Map_IndexFromHash_(uint64 cap, uint64 hash)
{ return hash * (uint64)11400714819323198485llu & cap-1; }

static Map*
Map_Create(Arena* arena, uint32 cap)
{
	Assert(IsPowerOf2(cap));
	
	Map* map = Arena_Push(arena, sizeof(*map) + sizeof(*map->entries) * cap);
	Assert(map);
	
	map->next = NULL;
	map->arena = arena;
	map->cap = cap;
	map->len = 0;
	
	return map;
}

static void
Map_InsertWithHash(Map* map, String key, void* value, uint64 hash)
{
	//TraceName(key);
	
	while (map && map->len >= Map_MaxLenForBlock_(map->cap))
	{
		if (!map->next)
			map->next = Map_Create(map->arena, map->cap << 1);
		
		map = map->next;
	}
	
	uint32 start_index = Map_IndexFromHash_(map->cap, hash);
	uint32 index = start_index;
	
	while ((index+1 & map->cap-1) != start_index)
	{
		if (map->entries[index].hash == hash && String_Equals(key, map->entries[index].key))
			Unreachable();
		
		if (!map->entries[index].value)
		{
			map->entries[index].key = key;
			map->entries[index].value = value;
			map->entries[index].hash = hash;
			
			++map->len;
			return;
		}
		
		index = (index + 1) & map->cap-1;
	}
	
	Unreachable();
}

static void*
Map_FetchWithHash(const Map* map, String key, uint64 hash)
{
	//TraceName(key);
	
	for (; map; map = map->next)
	{
		uint32 start_index = Map_IndexFromHash_(map->cap, hash);
		uint32 index = start_index;
		
		for (;
			 (index+1 & map->cap-1) != start_index;
			 index = (index + 1) & map->cap-1)
		{
			if (map->entries[index].hash == hash)
			{
				if (String_Equals(map->entries[index].key, key))
					return map->entries[index].value;
				else
					continue;
			}
			
			if (Map_IsEntryDead_(&map->entries[index]))
				continue;
			if (!map->entries[index].value)
				break;
		}
	}
	
	return NULL;
}

static bool
Map_RemoveWithHash(Map* map, String key, uint64 hash)
{
	//TraceName(key);
	
	for (; map; map = map->next)
	{
		uint32 start_index = Map_IndexFromHash_(map->cap, hash);
		uint32 index = start_index;
		
		for (;
			 (index+1 & map->cap-1) != start_index;
			 index = (index + 1) & map->cap-1)
		{
			if (map->entries[index].hash == hash)
			{
				if (String_Equals(map->entries[index].key, key))
				{
					map->entries[index].value = NULL;
					map->entries[index].hash = UINT64_MAX;
					
					return true;
				}
				else
					continue;
			}
			
			if (Map_IsEntryDead_(&map->entries[index]))
				continue;
			if (!map->entries[index].value)
				break;
		}
	}
	
	return false;
}

static inline void
Map_Insert(Map* map, String key, void* value)
{ Map_InsertWithHash(map, key, value, Map_HashFunc(key)); }

static inline void*
Map_Fetch(const Map* map, String key)
{ return Map_FetchWithHash(map, key, Map_HashFunc(key)); }

static inline bool
Map_Remove(Map* map, String key)
{ return Map_RemoveWithHash(map, key, Map_HashFunc(key)); }

//~ NOTE(ljre): InternedString
#ifdef Map_INTERNED_STRING
struct InternedString
{
	uintsize size;
	char data[];
}
typedef InternedString;

static InternedString*
Map_InternString(Map* map, String str)
{
	uint64 hash = Map_HashFunc(str);
	InternedString* result = Map_FetchWithHash(map, str, hash);
	
	if (!result)
	{
		result = Arena_Push(map->arena, sizeof(InternedString) + str.size);
		
		result->size = str.size;
		MemCopy(result->data, str.data, str.size);
		
		Map_InsertWithHash(map, StrMake(result->data, result->size), result, hash);
	}
	
	return result;
}
#endif //Map_INTERNED_STRING

//~ NOTE(ljre): Map_Iterator
#ifdef Map_ITERATOR
struct Map_Iterator
{
	Map* map;
	uintsize index;
}
typedef Map_Iterator;

static bool
Map_Next(Map_Iterator* iter, String* out_key, void** out_value)
{
	beginning:;
	
	if (!iter->map || iter->index >= iter->map->cap)
		return false;
	
	Map_Entry* cur = &iter->map->entries[iter->index];
	Map_Entry* end = &iter->map->entries[iter->map->cap];
	
	while (cur->hash == 0 && cur->key.size == 0 || Map_IsEntryDead_(cur))
	{
		if (++cur >= end)
		{
			iter->map = iter->map->next;
			iter->index = 0;
			goto beginning;
		}
	}
	
	if (out_key)
		*out_key = cur->key;
	if (out_value)
		*out_value = cur->value;
	
	return true;
}
#endif //Map_ITERATOR

#endif //COMMON_MAP_H
