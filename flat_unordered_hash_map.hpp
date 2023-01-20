#pragma once
#ifndef KABLUNK_UTILITIES_CONTAINER_FLAT_HASH_MAP_HPP
#define KABLUNK_UTILITIES_CONTAINER_FLAT_HASH_MAP_HPP

#include <stdint.h>
#include <string>
#include <utility>
#include <type_traits>

// include for Kablunk Engine core code
#ifdef KB_PLATFORM_WINDOWS
#   include <Kablunk/Core/Core.h>
#else
#   if defined(_MSC_VER)
#       define KB_CORE_ASSERT(x, ...) { if (!(x)) __debugbreak(); }
#		include <emmintrin.h> // see2 instructions
#   else
#       define KB_CORE_ASSERT(x, ...) { }
#       warning "msvc is the only supported compiler!"
#   endif
#endif

/*
 * documentation for sse2 instructions http://const.me/articles/simd/simd.pdf 
 */

namespace Kablunk::util::container
{ // start namespace Kablunk::util::container

namespace hash
{ // start namespace ::hash

	// algorithm from https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
	template <typename T> 
	inline uint64_t generate_u64_fnv1a_hash(const T& value)
	{
		static_assert(false, "value_t does not support hashing!");
	}

	// template specialization for uint64_t
	template <>
	inline uint64_t generate_u64_fnv1a_hash(const uint64_t& value)
	{
		static constexpr const uint64_t FNV_offset_basis = 0xcbf29ce484222325;
		// FNV prime is large prime
		static constexpr const uint64_t FNV_prime = 0x100000001b3;
		uint64_t hashed_value = FNV_offset_basis;

		// iterate through each byte of data to hash
		for (size_t i = 0; i < sizeof(value); ++i)
		{
			// xor the lower 8 bits of the hash with the current byte of data
			hashed_value = (hashed_value & 0xFFFFFFFFFFFFFF00) | ((hashed_value >> 8) & 0xFF) ^ ((value >> (i * 8)) & 0xFF);
			// multiply hashed value with prime constant
			hashed_value = hashed_value * FNV_prime;
		}

		return hashed_value;
	}

	// #TODO research how to specialize void* template so arbitrary types can work
	// template specialization for void*
	/*template <>
	inline uint64_t generate_u64_fnv1a_hash(const void*& value)
	{
		// cast to uint64_t and use that specialization
		return generate_u64_fnv1a_hash<uint64_t>(static_cast<uint64_t>(value));
	}*/

	template <>
	inline uint64_t generate_u64_fnv1a_hash<std::string>(const std::string& value)
	{
		static constexpr const uint64_t FNV_offset_basis = 14695981039346656037ull;
		// FNV prime is large prime
		static constexpr const uint64_t FNV_prime = 1099511628211ull;
		uint64_t hashed_value = FNV_offset_basis;

		// iterate through each byte of data to hash
		for (size_t i = 0; i < value.size(); ++i)
		{
			// xor the lower 8 bits of the hash with the current byte of data
			hashed_value = (hashed_value & 0xFFFFFFFFFFFFFF00) | (hashed_value & 0xFF) ^ static_cast<uint8_t>(value[i]);
			// multiply hashed value with prime constant
			hashed_value = hashed_value * FNV_prime;
		}

		return hashed_value;
	}

} // end namespace ::hash

namespace details
{ // start namespace ::details

	// metadata design from absl's swiss table implementation https://abseil.io/about/design/swisstables
	// 1 byte of overhead for the 
	struct swiss_table_metadata
	{
		static constexpr const uint8_t empty_bit_flag{ 0b10000000 };
		static constexpr const uint8_t occupied_bit_flag{ 0b00000000 };
		static constexpr const uint8_t deleted_bit_flag{ 0b10000000 };
		// h1 mask is the lowest 57 bits of a hash
		static constexpr const size_t h1_hash_mask = 0x01FFFFFFFFFFFFFF;
		// h2 mask is the highest 7 bits of a hash
		static constexpr const size_t h2_hash_mask = 0xFE00000000000000;
		// bits that can be used as metadata flags to optimize lookup and insertion
		// lowest bit stores a flag for whether an entry is empty (1), full (0), or deleted (1)
		// highest 7 bits store an "h2" hash (highest 7 bits of a hash)
		uint8_t m_data{ empty_bit_flag };

		swiss_table_metadata() = default;
		swiss_table_metadata(const swiss_table_metadata&) = default;
		swiss_table_metadata(swiss_table_metadata&&) = default;
		~swiss_table_metadata() = default;
		
		swiss_table_metadata& operator=(const swiss_table_metadata&) = default;
		swiss_table_metadata& operator=(swiss_table_metadata&&) = default;

		// helper function to check whether the metadata slot is occupied
		inline bool is_slot_occupied() const { return (m_data & empty_bit_flag) == occupied_bit_flag; }
		// helper function to check whether the metadata slot is empty
		inline bool is_slot_empty() const { return (m_data & 0xFF) == empty_bit_flag; }
		// helper function to check whether the metadata slot is deleted
		inline bool is_slot_deleted() const { return (m_data & deleted_bit_flag) == deleted_bit_flag; }
	};

	template <typename K, typename V>
	struct hash_map_pair
	{
		using key_t = K;
		using value_t = V;

		// key that is used to hash and store the pair
		key_t key{};
		// value associated with key
		value_t value{};

		hash_map_pair() = default;
		~hash_map_pair() = default;
		hash_map_pair(const key_t& key, const value_t& value)
			: key{ key }, value{ value }
		{ }
		hash_map_pair(key_t&& key, value_t&& value)
			: key{ std::move(key) }, value{ std::move(value) }
		{ }
		hash_map_pair(const hash_map_pair& other)
			: key{ other.key }, value{ other.value }
		{ }
		hash_map_pair(hash_map_pair&& other) noexcept
			: key{ std::move(other.key) }, value{ std::move(other.value) }
		{ 
			other.key = {};
			other.value = {};
		}

		// copy assign operator
		hash_map_pair& operator=(const hash_map_pair& other)
		{
			// copy
			key = other.key;
			value = other.value;

			return *this;
		}

		// move assign operator
		hash_map_pair& operator=(hash_map_pair&& other) noexcept
		{
			std::swap(key, other.key);
			std::swap(value, other.value);
			
			return *this;
		}

		// overload for structured binding
		// returns reference
		template <std::size_t _index>
		std::tuple_element_t<_index, hash_map_pair>& get()
		{
			if constexpr (_index == 0) { return key; }
			if constexpr (_index == 1) { return value; }
		}

		// overload for structured binding
		// returns const reference
		template <std::size_t _index>
		const std::tuple_element_t<_index, hash_map_pair>& get() const
		{
			if constexpr (_index == 0) { return key; }
			if constexpr (_index == 1) { return value; }
		}

		// overload for structured binding
		// returns rvalue overload
		template <std::size_t _index>
		std::tuple_element_t<_index, hash_map_pair>&& get() const
		{
			if constexpr (_index == 0) { return std::move(key); }
			if constexpr (_index == 1) { return std::move(value); }
		}

		// equality comparison operator
		inline bool operator==(const hash_map_pair& other) const { return key == other.key && value == other.value; }
		// inequality comparison operator
		inline bool operator!=(const hash_map_pair& other) const { return !(*this == other); }
	};
} // end namespace ::details

template <typename K, typename V>
class flat_unordered_hash_map
{
public:
	using key_t = K;
	using value_t = V;
	using hash_map_pair_t = details::hash_map_pair<key_t, value_t>;
	using hash_t = uint64_t;
	using metadata_t = details::swiss_table_metadata;
	using mask_t = uint16_t;
	using h2_t = uint8_t;
public:

	// iterator class for flat_unordered_hash_map
	// essentially it is a wrapper for a pointer to a pair in the map
	class iterator
	{
	public:
		// default constructor
		iterator() = default;
		// constructor that takes a hash map pair
		iterator(hash_map_pair_t* pair_ptr, const flat_unordered_hash_map* map_ptr)
			: m_pair_ptr{ pair_ptr }, m_map_ptr{ map_ptr }
		{ 
			// make sure we point to a valid pair
			const size_t index = m_map_ptr ? pair_ptr - m_map_ptr->m_bucket : 0;
			if (m_pair_ptr && m_map_ptr && !m_map_ptr->is_slot_occupied(m_map_ptr->m_metadata_bucket[index]))
				find_next_valid_pair();
		}
		// copy constructor
		iterator(const iterator&) = default;
		// move constructor
		iterator(iterator&&) noexcept = default;
		// destructor
		~iterator() = default;

		// copy assignment operator
		iterator& operator=(const iterator& other)
		{
			m_pair_ptr = other.m_pair_ptr;
			m_map_ptr = other.m_map_ptr;
		}

		// dereferencing operator
		hash_map_pair_t& operator*() 
		{
			KB_CORE_ASSERT(m_pair_ptr, "invalid pointer");

			return *m_pair_ptr;
		}
		
		// member access operator
		hash_map_pair_t* operator->() 
		{
			KB_CORE_ASSERT(m_pair_ptr, "invalid pointer");

			return m_pair_ptr;
		}

		// equality comparison operator
		bool operator==(const iterator& other) const { return m_pair_ptr == other.m_pair_ptr && m_map_ptr == other.m_map_ptr; }
		// inequality comparison operator
		bool operator!=(const iterator& other) const { return !(*this == other); }

		// prefix increment operator
		iterator& operator++()
		{
			if (!m_pair_ptr)
				return *this;

			find_next_valid_pair();

			return *this;
		}
	private:
		// increment the pointer so it points to a valid pair
		// sets to nullptr if it exceeds the end of the map or the original pointer is invalid
		// #TODO use sse2 instructions to speed up
		void find_next_valid_pair()
		{
			// #TODO this should probably be an assertion
			if (!m_pair_ptr || !m_map_ptr)
			{
				// invalidate and return
				m_pair_ptr = nullptr;
				m_map_ptr = nullptr;
				return;
			}

			// increment pointer until it finds a valid slot, or is out of bounds
			const hash_map_pair_t* end = m_map_ptr->m_bucket + m_map_ptr->m_max_elements;
			while (++m_pair_ptr < end && !m_map_ptr->is_slot_occupied(m_map_ptr->m_metadata_bucket[m_pair_ptr - m_map_ptr->m_bucket]))
				continue;

			// out of bounds check
			if (m_pair_ptr >= end)
				m_pair_ptr = nullptr;
		}
	private:
		// pointer to a pair in the hash map
		hash_map_pair_t* m_pair_ptr = nullptr;
		// pointer to the underlying map, used when finding occupied slots and the end iterator
		const flat_unordered_hash_map* m_map_ptr = nullptr;
	};


	// const iterator class for the map
	// operates the same as a normal iterator
	class citerator
	{
	public:
		// default constructor
		citerator() = default;
		// constructor that takes a hash map pair
		citerator(hash_map_pair_t* pair_ptr, const flat_unordered_hash_map* map_ptr)
			: m_pair_ptr{ pair_ptr }, m_map_ptr{ map_ptr }
		{
			// make sure we point to a valid pair
			const size_t index = m_map_ptr ? pair_ptr - m_map_ptr->m_bucket : 0;
			if (m_pair_ptr && m_map_ptr && !m_map_ptr->is_slot_occupied(m_map_ptr->m_metadata_bucket[index]))
				find_next_valid_pair();
		}
		// copy constructor
		citerator(const citerator&) = default;
		// move constructor
		citerator(citerator&&) noexcept = default;
		// destructor
		~citerator() = default;

		// copy assignment operator
		citerator& operator=(const citerator& other)
		{
			m_pair_ptr = other.m_pair_ptr;
			m_map_ptr = other.m_map_ptr;
		}

		// dereferencing operator
		hash_map_pair_t& operator*()
		{
			KB_CORE_ASSERT(m_pair_ptr, "invalid pointer");

			return *m_pair_ptr;
		}

		// member access operator
		hash_map_pair_t* operator->()
		{
			KB_CORE_ASSERT(m_pair_ptr, "invalid pointer");

			return m_pair_ptr;
		}

		// equality comparison operator
		bool operator==(const citerator& other) const { return m_pair_ptr == other.m_pair_ptr && m_map_ptr == other.m_map_ptr; }
		// inequality comparison operator
		bool operator!=(const citerator& other) const { return !(*this == other); }

		// prefix increment operator
		citerator& operator++()
		{
			if (!m_pair_ptr)
				return *this;

			find_next_valid_pair();

			return *this;
		}
	private:
		// increment the pointer so it points to a valid pair
		// sets to nullptr if it exceeds the end of the map or the original pointer is invalid
		// #TODO use sse2 instructions to speed up
		void find_next_valid_pair()
		{
			// #TODO this should probably be an assertion
			if (!m_pair_ptr || !m_map_ptr)
			{
				// invalidate and return
				m_pair_ptr = nullptr;
				m_map_ptr = nullptr;
				return;
			}

			// increment pointer until it finds a valid slot, or is out of bounds
			const hash_map_pair_t* end = m_map_ptr->m_bucket + m_map_ptr->m_max_elements;
			while (++m_pair_ptr < end && !m_map_ptr->is_slot_occupied(m_map_ptr->m_metadata_bucket[m_pair_ptr - m_map_ptr->m_bucket]))
				continue;

			// out of bounds check
			if (m_pair_ptr >= end)
				m_pair_ptr = nullptr;
		}
	private:
		// pointer to a pair in the hash map
		const hash_map_pair_t* m_pair_ptr = nullptr;
		// pointer to the underlying map, used when finding occupied slots and the end iterator
		const hash_map_pair_t* m_map_ptr = nullptr;
	};
public:
	// default constructor
	flat_unordered_hash_map();
	// copy constructor
	flat_unordered_hash_map(const flat_unordered_hash_map& other);
	// move constructor
	flat_unordered_hash_map(flat_unordered_hash_map&& other) noexcept;
	// destructor
	~flat_unordered_hash_map();

	// copy assign operator
	flat_unordered_hash_map& operator=(const flat_unordered_hash_map& other);
	// move assign operator
	flat_unordered_hash_map& operator=(flat_unordered_hash_map&& other) noexcept;

	// ========
	// capacity
	// ========

	// check whether the map is empty
	inline bool empty() const { return m_element_count == 0; }
	// returns the number of key-value pairs in the map
	inline size_t size() const { return m_element_count; };
	// returns the maximum number of elements that can be in the map before re-allocation of underlying bucket(s)
	inline size_t max_size() const { return m_max_elements; };
	// return the default max element count of a map
	inline constexpr size_t get_default_max_size() { return s_default_max_elements; }

	// =========
	// modifiers
	// =========

	// clear all entries and free memory, map is now considered an invalid object unless it is re-initialized
	void destroy();
	// clear all the entries from the map and resize to default map size
	void clear();
	// clear all the entries from the map
	void clear_entries();
	// insert element into the map
	void insert(const hash_map_pair_t& pair);
	// insert an element into the map
	void insert(hash_map_pair_t&& pair);
	// insert an element into the map via key and pair
	inline void insert(const key_t& key, const value_t& value) { insert(std::move(hash_map_pair_t{ key, value })); }
	// insert an element or assign if it already exists
	void insert_or_assign();
	// construct element in-place
	void emplace(hash_map_pair_t&& pair);
	// construct element in-place
	void emplace(key_t&& key, value_t&& value);
	// construct element in-place with a hint
	void emplace_hint();
	// insert in-place if the key does not exist, otherwise do nothing
	void try_emplace(hash_map_pair_t&& pair);
	// erase element(s) from the map
	void erase(const key_t& key);
	// swap the contents
	void swap(flat_unordered_hash_map& other);
	// extract nodes from the container, removing the pair from the map and copying to a new address
	// returns an owning pointer
	hash_map_pair_t* extract(const key_t& key);
	// splices nodes from another container
	void merge(const flat_unordered_hash_map& other);
	
	// splices nodes from another container
	// is there an optimization to be made where moving a container is faster?
	// void merge(flag_unordered_hash_map&& other);
	
	// reserve *more* memory for the map, throws an error if the operation tries to make the map smaller
	void reserve(size_t new_size);
	// resize the map to a specified size
	// can make the map smaller, but does not guarantee which keys will be kept
	void resize(size_t new_size);

	// ======
	// lookup
	// ======

	// access a specific element with bounds checking
	value_t& at(const key_t& key);
	// access a specific element with bounds checking
	const value_t& at(const key_t& key) const;
	// access or insert a specific element
	value_t& operator[](const key_t& key);
	// return the number of elements matching a certain key
	size_t count(const key_t& key) const;
	// finds the element with a certain key
	iterator find(const key_t& key)
	{
		KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

		const size_t index = find_index_of(key);
		if (is_slot_occupied(m_metadata_bucket[index]))
			return iterator{ m_bucket + index, this };

		return end();
	}
	// finds the element with a certain key
	citerator find(const key_t& key) const
	{
		KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

		const size_t index = find_index_of(key);
		if (is_slot_occupied(m_metadata_bucket[index]))
			return citerator{ m_bucket + index, this };

		return cend();
	}
	// check if a key is contained within the map
	bool contains(const key_t& key) const;

	// =========
	// iterators
	// =========

	// iterator pointing to the beginning of the map
	iterator begin() { return iterator{ m_bucket, this }; }
	// iterator pointing to the end of the map
	iterator end() { return iterator{ nullptr, this }; }
	// const iterator pointing to the beginning of the map
	citerator cbegin() const { return citerator{ m_bucket, this }; }
	// iterator pointing to the end of the map
	citerator cend() const { return citerator{ nullptr, this }; }
private:
	// find the index of the bucket where a key lives if present
	inline size_t find_index_of(const key_t& key) const;
	// find the index into the pair bucket where a key lives
	inline size_t find_index_of(const hash_t h1_hash, const h2_t h2_hash, const K& key) const;
	// check if a metadata slot is occupied
	inline bool is_slot_occupied(const metadata_t metadata) const { return metadata.is_slot_occupied(); }
	// check if a metadata slot is empty
	inline bool is_slot_empty(const metadata_t metadata) const { return metadata.is_slot_empty(); }
	// re-allocate a larger array, move old map's values, and free old map
	inline void rebuild();
	// checks if the load factor has been reached, and a rebuild is necessary
	inline void check_if_needs_rebuild() 
	{ 
		// #TODO should load factor and casting be doubles so we don't overflow?
		if (m_element_count + 1 >= static_cast<uint64_t>(static_cast<float>(m_max_elements) * m_load_factor))
		{
#ifdef KB_DEBUG
			KB_CORE_INFO(
				"[flat_unordered_hash_map]: triggering rebuild, {} >= {}",
				m_element_count + 1,
				static_cast<uint64_t>(static_cast<float>(m_max_elements) * m_load_factor)
			);
#endif
			rebuild();
		}
	}
	// use sse2 instructions to perform 16 masked lookups at once
	// based on swiss table implementation details https://abseil.io/about/design/swisstables
	// the metadata buffer is a pointer to 16 metadata elements (each being 8 bytes, making up a total of 128 bits)
	inline mask_t find_matches_sse2(const h2_t h2_hash, const metadata_t* metadata_buffer) const
	{
		// 16 metadata elements are loaded into register
		// for _mm_load_si128(), the data needs to be 16 byte aligned 
		// _mm_loadu_si128 has potentially worse performance but data does not need to be aligned
		__m128i metadata = _mm_loadu_si128((__m128i*)metadata_buffer);
		// #TODO document what this does
		__m128i match = _mm_set1_epi8(h2_hash);
		// #TODO document what this does
		return static_cast<mask_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(match, metadata)));
	}
private:
	// default size of map
	static constexpr const size_t s_default_max_elements = 1024ull;
	// count of metadata that simd instructions can simultaneously check
	static constexpr const size_t s_metadata_count_to_check = 16ull;
	// count of elements in the map
	size_t m_element_count = 0ull;
	// maximum size of the bucket before re-allocation
	size_t m_max_elements = s_default_max_elements;
	// percentage the bucket can be filled before re-allocation
	float m_load_factor = 0.875f;
	// contiguous array of hash map pairs
	hash_map_pair_t* m_bucket = nullptr;
	// contiguous array of hash map metadata
	metadata_t* m_metadata_bucket = nullptr;
	// 16 byte array to store contiguous metadata when lookup index >= m_max_elements - 15
	metadata_t* m_temporary_metadata_bucket = nullptr;
	// friend declarations
	friend class iterator;
	friend class citerator;
};

// ============================
// start implementation details
// ============================

// default constructor
// pair bucket is not initialized, while the metadata bucket is
// metadata_t has a default constructor which initializes the metadata to an "empty" state
template <typename K, typename V>
flat_unordered_hash_map<K, V>::flat_unordered_hash_map()
	: m_bucket{ new hash_map_pair_t[m_max_elements]{} }, m_metadata_bucket{ new metadata_t[m_max_elements]{} }, 
	m_temporary_metadata_bucket{ new metadata_t[s_metadata_count_to_check] }
{

}

// copy constructor for hash map with the same key and value type
template <typename K, typename V>
flat_unordered_hash_map<K, V>::flat_unordered_hash_map(const flat_unordered_hash_map& other)
	: m_bucket{ new hash_map_pair_t[m_max_elements]{} }, m_metadata_bucket{ new metadata_t[m_max_elements]{} },
	m_temporary_metadata_bucket{ new metadata_t[s_metadata_count_to_check] }
{
	// reserve more space if needed
	if (other.max_size() > max_size())
		reserve(other.max_size());

	// do we need to go through every element and copy? could we just iterate through valid keys?
	// copy pairs and metadata
	for (size_t i = 0; i < m_max_elements; ++i)
	{
		m_bucket[i] = other.m_bucket[i];
		m_metadata_bucket = other.m_metadata_bucket[i];
	}

	m_max_elements = other.m_max_elements;
	m_element_count = other.m_element_count;
	m_load_factor = other.m_load_factor;
}

// move constructor for hash map with the same key and value type
template <typename K, typename V>
flat_unordered_hash_map<K, V>::flat_unordered_hash_map(flat_unordered_hash_map&& other) noexcept
{
	// swap contents with other map
	swap(other);
}

// destructor
template <typename K, typename V>
flat_unordered_hash_map<K, V>::~flat_unordered_hash_map()
{
	if (m_bucket)
		delete[] m_bucket;
#if KB_DEBUG
	// we should never have an invalid pointer
	else
		KB_CORE_ASSERT(false, "tried deleting invalid bucket pointer?");
#endif

	if (m_metadata_bucket)
		delete[] m_metadata_bucket;
#if KB_DEBUG
	// we should never have an invalid pointer
	else
		KB_CORE_ASSERT(false, "tried deleting invalid metadata pointer?");
#endif

	if (m_temporary_metadata_bucket)
		delete[] m_temporary_metadata_bucket;
}

// copy assign operator
template <typename K, typename V>
flat_unordered_hash_map<K, V>& flat_unordered_hash_map<K, V>::operator=(const flat_unordered_hash_map& other)
{
	*this = flat_unordered_hash_map<K, V>{ other };
}

// move assign operator
template <typename K, typename V>
flat_unordered_hash_map<K, V>& flat_unordered_hash_map<K, V>::operator=(flat_unordered_hash_map&& other) noexcept
{
	swap(other);
}

// free memory and invalid the map
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::destroy()
{
	if (m_bucket)
		delete[] m_bucket;

	if (m_metadata_bucket)
		delete[] m_metadata_bucket;

	m_bucket = nullptr;
	m_element_count = 0;
	m_max_elements = 0;
}

// clear all the entries from the map
// frees current bucket, resizing to default size
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::clear()
{
	// #TODO there are most likely optimizations to be made here...

	// free pairs
	if (m_bucket)
		delete[] m_bucket;

	// free metadata
	if (m_metadata_bucket)
		delete[] m_metadata_bucket;

	// resize arrays to default size
	m_bucket = new hash_map_pair_t[s_default_max_elements]{};
	m_metadata_bucket = new metadata_t[s_default_max_elements]{};
	m_element_count = 0;
	m_max_elements = s_default_max_elements;
}

// clear all the entries from the map
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::clear_entries()
{
	// #TODO like the clear function above, there are optimizations to be made...

	// clear pair data
	if (m_bucket)
		for (size_t i = 0; i < m_max_elements; ++i)
			m_bucket[i] = hash_map_pair_t{};

	// clear metadata
	if (m_bucket)
		for (size_t i = 0; i < m_max_elements; ++i)
			m_metadata_bucket[i] = metadata_t{};

	m_element_count = 0;
}

// insert element into the map. *safely* fails if the key is already present
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::insert(const hash_map_pair_t& pair)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// rebuild the map if we are getting too full
	check_if_needs_rebuild();
	
	// compute general hash, and mask out h1 and h2 hashes
	const hash_t hash_value = hash::generate_u64_fnv1a_hash(key);
	const hash_t h1_hash = hash_value & metadata_t::h1_hash_mask;
	const h2_t h2_hash = static_cast<h2_t>((hash_value & metadata_t::h2_hash_mask) >> 0x39);
	const size_t index = find_index_of(h1_hash, h2_hash, key);

	metadata_t& metadata = m_metadata_bucket[index];
	// *safely* fail if the slot is occupied
	if (metadata.is_slot_occupied())
	{
#ifdef KB_DEBUG
		KB_CORE_ASSERT(false, "tried inserting but key was already present")
#endif
		return;
	}

	// copy pair data
	m_bucket[index] = pair;
	// set metadata
	m_metadata_bucket[index] = (metadata_t::occupied_bit_flag | h2_hash);
	++m_element_count;

#if 0
	hash_map_pair_t& found_pair = m_bucket[find_index_of(pair.key)];
	if (is_slot_occupied(found_pair))
	{
#ifdef KB_DEBUG
		KB_CORE_ASSERT(false, "tried inserting but key was already present")
#endif
		return;
	}
	
	// #TODO should these values be moved?
	found_pair = pair;
	found_pair.flags |= hash_map_pair_t::occupied_bit_flag;
	++m_element_count;
#endif
}

// insert element into the map. *safely* fails if the key is already present
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::insert(hash_map_pair_t&& pair)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// rebuild the map if we are getting too full
	check_if_needs_rebuild();

	// compute general hash, and mask out h1 and h2 hashes
	const hash_t hash_value = hash::generate_u64_fnv1a_hash(pair.key);
	const hash_t h1_hash = hash_value & metadata_t::h1_hash_mask;
	const h2_t h2_hash = static_cast<h2_t>((hash_value & metadata_t::h2_hash_mask) >> 0x39);
	const size_t index = find_index_of(h1_hash, h2_hash, pair.key);

	metadata_t& metadata = m_metadata_bucket[index];
	// *safely* fail if the slot is occupied
	if (metadata.is_slot_occupied())
	{
#ifdef KB_DEBUG
		KB_CORE_ASSERT(false, "tried inserting but key was already present")
#endif
		return;
	}

	// pointer to where the pair will be move constructed
	hash_map_pair_t* pair_ptr = m_bucket + index;
	// move construct in bucket memory
	// new (pair_ptr) hash_map_pair_t{ pair };
	*pair_ptr = std::move(pair);
	// set metadata
	m_metadata_bucket[index] = metadata_t{ static_cast<uint8_t>(metadata_t::occupied_bit_flag | h2_hash) };
	++m_element_count;
#if 0
	// rebuild the map if we are getting too full
	check_if_needs_rebuild();

	hash_map_pair_t* found_pair_ptr = m_bucket + find_index_of(pair.key);
	if (is_slot_occupied(*found_pair_ptr))
	{
#ifdef KB_DEBUG
		KB_CORE_ASSERT(false, "tried inserting but key was already present")
#endif
		return;
	}

	pair.flags |= hash_map_pair_t::occupied_bit_flag;
	new (found_pair_ptr) hash_map_pair_t{ std::move(pair) }; // move construct in memory
	++m_element_count;
#endif
}

// helper function to compute an index from a key, when callee does not need to know h1 or h2 hash
template <typename K, typename V>
inline size_t flat_unordered_hash_map<K, V>::find_index_of(const K& key) const
{
	// compute general hash, and mask out h1 and h2 hashes
	const hash_t hash_value = hash::generate_u64_fnv1a_hash(key);
	const hash_t h1_hash = hash_value & metadata_t::h1_hash_mask;
	const h2_t h2_hash = static_cast<h2_t>((hash_value & metadata_t::h2_hash_mask) >> 0x39);
	return find_index_of(h1_hash, h2_hash, key);
}

// find the index of the bucket where a key lives if present using open addressing. 
// this uses a naive implementation of linear probing open addressing from https://en.wikipedia.org/wiki/Open_addressing
// the steps of this swiss table lookup is as follows
//   1. use the *h1 hash* to find the start of a "bucket chain" for that specific hash
//   2. use the *h2 hash* to create a mask
//   3. use sse2 instructions and the mask to find candidate slots
//   4. perform equality checks on all candidates
//   5. if the check fails, start performing linear probing to generate a new "bucket chain" and repeat
//      a. an empty element stops probing
//      b. a deleted element does not
template <typename K, typename V>
inline size_t flat_unordered_hash_map<K, V>::find_index_of(const hash_t h1_hash, const h2_t h2_hash, const K& key) const
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// normal hash map indexing using the h1 hash
	size_t index = h1_hash % m_max_elements;

	// #TODO this is subject to infinite looping if the map is completely full, though we should never get to that point...
	while (true)
	{
		const metadata_t* metadata_ptr;
		
		// the normal case is when the index <= max_elements - 16
		// this means we can just pass a pointer to the metadata bucket
		if (index <= m_max_elements - s_metadata_count_to_check) // #TODO see if msvc has likely branch attribute
		{
			metadata_ptr = m_metadata_bucket + index;
		}
		else
		{
			// since sse2 needs the memory to be 16 bytes, we use a cached, contiguous array
			// copy metadata into bucket cache
			for (size_t i = 0; i < s_metadata_count_to_check; ++i)
			{
				const size_t metadata_index = (index + i) % m_max_elements;
				// copy data to temporary bucket
				// since it's only 16 bytes, copies *should* be fine
				m_temporary_metadata_bucket[i] = m_metadata_bucket[metadata_index];
			}

			metadata_ptr = m_temporary_metadata_bucket;
		}

		// use sse2 instructions to search for 16 potential candidates at once
		const mask_t candidates = find_matches_sse2(h2_hash, metadata_ptr);

		// equality check on all candidates
		// #TODO this could probably be optimized
		for (size_t i = 0ull; i < s_metadata_count_to_check; ++i)
		{
			if (is_slot_empty(metadata_ptr[i]))
				return (index + i) % m_max_elements;
			
			const bool candidate = candidates & (0b1 << i);
			if (!candidate)
				continue;

			// since we check 16 elements at once, another modulus is required 
			// so we don't accidentally overflow the pair bucket
			const size_t bucket_index = (index + i) % m_max_elements;
			// check whether the key matches and the slot is occupied
			const bool is_occupied_and_key_matches = is_slot_occupied(metadata_ptr[i]) && m_bucket[bucket_index].key == key;
			if (is_occupied_and_key_matches)
				return (index + i) % m_max_elements;
		}

		// otherwise continue probing
		index = (index + s_metadata_count_to_check) % m_max_elements;
	}
#if 0
	const hash_t hash_value = hash::generate_u64_fnv1a_hash(key);
	size_t index = hash_value % m_max_elements;
	// this can be subject to infinite loop if load balance == 1, though we should never get to that point...
	while (is_slot_occupied(m_bucket[index]) && m_bucket[index].key != key)
		index = (index + 1) % m_max_elements;

	return index;
#endif
}

// rebuild the map, doubling its max element count
// moves old map entries to the new bucket
template <typename K, typename V>
inline void flat_unordered_hash_map<K, V>::rebuild()
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	hash_map_pair_t* old_bucket = m_bucket;
	metadata_t* old_metadata_bucket = m_metadata_bucket;
	const size_t old_element_count = m_max_elements;

	// double size of new bucket
	m_max_elements *= 2;
	m_bucket = new hash_map_pair_t[m_max_elements]{};
	m_metadata_bucket = new metadata_t[m_max_elements]{};

	// move old elements to new map
	for (size_t i = 0; i < old_element_count; ++i)
	{
		if (!is_slot_occupied(old_metadata_bucket[i]))
			continue;

		// compute general hash, and mask out h1 and h2 hashes
		const key_t& key = old_bucket[i].key;
		const hash_t hash_value = hash::generate_u64_fnv1a_hash(key);
		const hash_t h1_hash = hash_value & metadata_t::h1_hash_mask;
		const h2_t h2_hash = static_cast<h2_t>((hash_value & metadata_t::h2_hash_mask) >> 0x39);
		const size_t index = find_index_of(h1_hash, h2_hash, key);

		hash_map_pair_t* slot_ptr = m_bucket + index;
		// move construct in bucket memory
		//new (slot_ptr) hash_map_pair_t{ std::move(old_bucket[i]) };
		m_bucket[index] = std::move(old_bucket[i]);
		m_metadata_bucket[index] = metadata_t{ static_cast<uint8_t>(metadata_t::occupied_bit_flag | h2_hash) };
	}
		
	if (old_bucket)
		delete[] old_bucket;

	if (old_metadata_bucket)
		delete[] old_metadata_bucket;
}

// try inserting a value if the key does not exist in the map, otherwise assign the value at the key
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::insert_or_assign()
{
	KB_CORE_ASSERT(false, "not implemented!");
}

// emplace a value in the map, does not care whether the key already exists or not
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::emplace(hash_map_pair_t&& pair)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// rebuild the map if we are getting too full
	check_if_needs_rebuild();

	// compute general hash, and mask out h1 and h2 hashes
	const hash_t hash_value = hash::generate_u64_fnv1a_hash(pair.key);
	const hash_t h1_hash = hash_value & metadata_t::h1_hash_mask;
	const h2_t h2_hash = static_cast<h2_t>((hash_value & metadata_t::h2_hash_mask) >> 0x39);
	const size_t index = find_index_of(h1_hash, h2_hash, pair.key);

	// pointer to where the pair will be move constructed
	hash_map_pair_t* pair_ptr = m_bucket + index;
	// move construct in bucket memory
	//new (pair_ptr) hash_map_pair_t{ std::move(pair) };
	*pair_ptr = std::move(pair);
	// set metadata
	m_metadata_bucket[index] = metadata_t{ static_cast<uint8_t>(metadata_t::occupied_bit_flag | h2_hash) };
	++m_element_count;
#if 0
	check_if_needs_rebuild();

	pair.flags |= details::hash_map_pair<K, V>::occupied_bit_flag;
	hash_map_pair_t* found_pair_ptr = m_bucket + find_index_of(pair.key);
	new (found_pair_ptr) hash_map_pair_t{ std::move(pair) };
	m_element_count++;
#endif
}

// emplace a value in the map, does not care whether the key already exists or not
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::emplace(K&& key, V&& value)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// rebuild the map if we are getting too full
	check_if_needs_rebuild();

	// compute general hash, and mask out h1 and h2 hashes
	const hash_t hash_value = hash::generate_u64_fnv1a_hash(key);
	const hash_t h1_hash = hash_value & metadata_t::h1_hash_mask;
	const h2_t h2_hash = static_cast<h2_t>((hash_value & metadata_t::h2_hash_mask) >> 0x39);
	const size_t index = find_index_of(h1_hash, h2_hash, key);

	// pointer to where the pair will be move constructed
	hash_map_pair_t* pair_ptr = m_bucket + index;
	// move construct in bucket memory
	//new (pair_ptr) hash_map_pair_t{ std::move(key), std::move(value) };
	pair_ptr->key = std::move(key);
	pair_ptr->value = std::move(value);
	// set metadata
	m_metadata_bucket[index] = metadata_t{ static_cast<uint8_t>(metadata_t::occupied_bit_flag | h2_hash) };
	++m_element_count;

#if 0
	check_if_needs_rebuild();

	hash_map_pair_t* found_pair_ptr = m_bucket + find_index_of(key);
	new (found_pair_ptr) hash_map_pair_t{ std::move(key), std::move(value), details::hash_map_pair<K, V>::occupied_bit_flag };
	m_element_count++;
#endif
}

template <typename K, typename V>
void flat_unordered_hash_map<K, V>::emplace_hint()
{
	KB_CORE_ASSERT(false, "not implemented!");
}

// try emplace a value in the map if the key does not exist, otherwise do nothing
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::try_emplace(hash_map_pair_t&& pair)
{
	KB_CORE_ASSERT(false, "invalid implementation!");
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	hash_map_pair_t& found_pair = m_bucket[find_index_of(pair.key)];
	if (is_slot_occupied(found_pair))
	{
		return;
	}

	// is this correct? should we instead get a pointer to the pair and move with de-referencing?
	found_pair = std::move(pair);
}

// erase an entry from the map via key
// uses tombstone deletion, where the metadata flag for "delete" is set
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::erase(const key_t& key)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// make sure we don't try to delete from an empty map
	if (m_element_count == 0)
		return;

	const size_t index = find_index_of(key);
	if (!is_slot_occupied(m_metadata_bucket[index]))
	{
#ifdef KB_DEBUG
		KB_CORE_ASSERT(false, "key does not exist in map!");
#endif
		return;
	}

	// tombstone deletion
	m_metadata_bucket[index] |= metadata_t::deleted_bit_flag;
	--m_element_count;
	// std::memset(m_bucket + index, 0, sizeof(hash_map_pair_t));
}

template <typename K, typename V>
void flat_unordered_hash_map<K, V>::swap(flat_unordered_hash_map& other)
{
	// swap bucket pointers
	std::swap(m_bucket, other.m_bucket);
	// swap element count
	std::swap(m_element_count, other.m_element_count);
	// swap max load
	std::swap(m_max_elements, other.m_max_elements);
	// swap load factor
	std::swap(m_load_factor, other.m_load_factor);
	// swap metadata
	std::swap(m_metadata_bucket, other.m_metadata_bucket);
	// swap contiguous metadata cache
	std::swap(m_temporary_metadata_bucket, other.m_temporary_metadata_bucket);
}

// extract a pair from the map
// allocates new memory for the pair and returns an owning pointer
// zeros out the original entry in the map
template <typename K, typename V>
details::hash_map_pair<K, V>* flat_unordered_hash_map<K, V>::extract(const K& key)
{
	KB_CORE_ASSERT(false, "invalid implementation!");
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	const size_t index = find_index_of(key);
	KB_CORE_ASSERT(is_slot_occupied(m_bucket[index]), "tried extracting a pair that does not exist in the map!");
	
	// copy pair to new memory address
	hash_map_pair_t* new_pair = new hash_map_pair_t;
	*new_pair = m_bucket[index];

	// zero out existing pair in map
	m_bucket[index] = hash_map_pair_t{};
	// std::memset(m_bucket[index], 0, sizeof(hash_map_pair_t));

	return new_pair;
}

// merge (mutation) two maps together
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::merge(const flat_unordered_hash_map& other)
{
	KB_CORE_ASSERT(false, "invalid implementation!");
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	// #TODO should we just reserve a size big enough in one pass?
	while (size() + other.size() >= static_cast<size_t>(static_cast<float>(m_max_elements) * m_load_factor))
		rebuild();

	// iterate through other map and insert values
	for (size_t i = 0; i < other.m_max_elements; ++i)
		if (is_slot_occupied(other.m_bucket[i]))
			insert(other.m_bucket[i]);
}

// reserve more space in the map
// throws error if the operation attempts to make the map smaller
// size is number of elements (not size in bytes)
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::reserve(size_t new_size)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");

	KB_CORE_ASSERT(m_max_elements < new_size, "cannot resize map to be smaller!")

	hash_map_pair_t* old_bucket = m_bucket;
	metadata_t* old_metadata_bucket = m_metadata_bucket;
	m_bucket = new hash_map_pair_t[new_size];
	m_metadata_bucket = new metadata_t[new_size];

	// move old bucket to newly allocated space
	// insert will also update metadata
	for (size_t i = 0; i < m_max_elements; ++i)
		if (old_metadata_bucket[i].is_slot_occupied())
			insert(std::move(old_bucket[i])); 

	m_max_elements = new_size;

	// free old bucket memory
	if (old_bucket)
		delete[] old_bucket;

	// free old metadata memory
	if (old_metadata_bucket)
		delete[] old_metadata_bucket;
}

// resize the map to a specific size
// can make the map smaller, but will not guarantee which keys remain
template <typename K, typename V>
void flat_unordered_hash_map<K, V>::resize(size_t new_size)
{
	KB_CORE_ASSERT(m_bucket, "bucket pointer is invalid, did you forget to construct the map?");
	KB_CORE_ASSERT(new_size > 0, "cannot resize map to size 0, try using clear() instead");

	hash_map_pair_t* old_bucket = m_bucket;
	metadata_t* old_metadata_bucket = m_metadata_bucket;
	m_bucket = new hash_map_pair_t[new_size]{};
	m_metadata_bucket = new metadata_t[new_size]{};

	// insert old elements into new map
	const size_t min_new_size = std::min(m_max_elements, new_size);
	for (size_t i = 0; i < min_new_size; ++i)
	{
		if (!old_metadata_bucket[i].is_slot_occupied())
			continue;
		
		insert(std::move(old_bucket[i]));
	}

	if (old_bucket)
		delete[] old_bucket;

	if (old_metadata_bucket)
		delete[] old_metadata_bucket;
}

// returns a reference to a value via key
// exception occurs if the key does not exist
template <typename K, typename V>
V& flat_unordered_hash_map<K, V>::at(const K& key)
{
	const size_t index = find_index_of(key);
	hash_map_pair_t& found_pair = m_bucket[index];

	KB_CORE_ASSERT(is_slot_occupied(found_pair), "key does not exist in the map!");

	return found_pair;
}

// returns a reference to a value via key
// exception occurs if the key does not exist
template <typename K, typename V>
const V& flat_unordered_hash_map<K, V>::at(const K& key) const
{
	const size_t index = find_index_of(key);
	hash_map_pair_t& found_pair = m_bucket[index];

	KB_CORE_ASSERT(is_slot_occupied(found_pair), "key does not exist in the map!");

	return found_pair;
}

// index operator
template <typename K, typename V>
V& flat_unordered_hash_map<K, V>::operator[](const K& key)
{
	return m_bucket[find_index_of(key)].value;
}

// counting the number of key entries in the map does not make sense since we only use one bucket?
template <typename K, typename V>
size_t flat_unordered_hash_map<K, V>::count(const K& key) const
{
	KB_CORE_ASSERT(false, "not implemented");
	return 0;
}

// check whether the map contains a specific key
template <typename K, typename V>
bool flat_unordered_hash_map<K, V>::contains(const K& key) const
{
	return is_slot_occupied(m_bucket[find_index_of(key)]);
}

// ==========================
// end implementation details
// ==========================

} // end namespace Kablunk::util::container

// overloads for structured binding
namespace std
{ // start namespace std

// let the tuple now how many elements it contains
template<typename K, typename V>
struct tuple_size<Kablunk::util::container::details::hash_map_pair<K, V>>
	: integral_constant<size_t, 2> {};

// tuple element 0 specialization
template<typename K, typename V>
struct tuple_element<0, Kablunk::util::container::details::hash_map_pair<K, V>>
{
	using type = K;
};

// tuple element 1 specialization
template<typename K, typename V>
struct tuple_element<1, Kablunk::util::container::details::hash_map_pair<K, V>>
{
	using type = V;
};

} // end namespace std

#endif
