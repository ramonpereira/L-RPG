#ifndef MYPOP_SAS_PLUS_DTG_REACHABLE_FACT_H
#define MYPOP_SAS_PLUS_DTG_REACHABLE_FACT_H

#include <iostream>
#include <vector>
#include <cstring>

namespace MyPOP {

class Atom;
class Bindings;
	
namespace SAS_Plus {

class BoundedAtom;
class EquivalentObjectGroup;
class EquivalentObjectGroupManager;
class ReachableFactMemoryPool;
	
class ReachableFact
{
public:
	ReachableFact(const BoundedAtom& bounded_atom, const Bindings& bindings, const EquivalentObjectGroupManager& eog_manager);
	
	ReachableFact(const Atom& atom, EquivalentObjectGroup** term_domain_mapping);
	
	~ReachableFact();
	
	static void* operator new (size_t size);
	
	static void operator delete (void* p);
	
	/**
	 * This method is called everytime a merge has taken place which involves a Equivalent Object Group 
	 * which is part of this reachable fact. In such an occasion we end up with at least one term in this
	 * reachable fact which is no longer a root node (and thus yields incomplete information).
	 * 
	 * In order to fix this problem this method updates all the equivalent object group points so they link
	 * to the proper root node.
	 * 
	 * @return True if a Equivalent Object Group had to be updated, false otherwise.
	 * @note This function should always return true, we only want to call it if an update is due!
	 */
	bool updateTermsToRoot();
	
	/**
	 * Two reachable facts are equivalent iff:
	 * 1) All the objects have the same signature.
	 * 2) Those variables which have been labeled as unbalanced must be identical.
	 */
	bool isEquivalentTo(const ReachableFact& other) const;
	
	/**
	 * Two reachable facts are identical iff:
	 * 1) All the objects have the same signature.
	 * 2) All variables are identical.
	 */
	bool isIdenticalTo(const ReachableFact& other) const;
	
//	void printGrounded(std::ostream& os) const;
	
	EquivalentObjectGroup& getTermDomain(unsigned int index) const;
	
	EquivalentObjectGroup** getTermDomains() const { return term_domain_mapping_; }
	
	const Atom& getAtom() const { return *atom_; }
	
	/**
	 * When updating the Equivalent Object Group, we need to update the Reachable Facts. We pick a single ReachableFact to update its 
	 * EOGs and create a link for all all reachable facts which are subsumed.
	 * @param replacement The ReachableFact which subsumes this reachable fact.
	 */
	void replaceBy(ReachableFact& replacement);
	
	/**
	 * Check if this reachable fact has been subsumed by another reachable fact. Call @ref getReplacement to get its replacement.
	 * @return True if this reachable fact has been subsumed, false otherwise.
	 */
	bool isMarkedForRemoval() const { return replaced_by_ != NULL; }
	
	/**
	 * @return The reachable fact which has subsumed this fact, or this if it has not been subsumed.
	 */
	const ReachableFact& getReplacement() const;
	
private:

	const Atom* atom_;
	
	EquivalentObjectGroup** term_domain_mapping_;
	
	// During the construction of the reachability graph terms can be merged and because of that some reachable facts are
	// removed because they have become identical to others. E.g. consider the following two reachable facts:
	//
	// * (at truck1 s1)
	// * (at truck2 s1)
	//
	// Suppose that truck1 and truck2 become equivalent, then we remove one of the two and update the other to:
	// * (at {truck1, truck2} s1)
	//
	// Reachable facts can be shared among multiple objects, so in this case the EOG linked to s1 will contain the following 
	// reachable facts:
	// * (at truck1 s1)
	// * (at {truck1, truck2} s1)
	//
	// By marking the former for removal we can remove the remaining reachable fact.
	ReachableFact* replaced_by_;
	
	//friend class ReachableFactMemoryPool;
	
	friend std::ostream& operator<<(std::ostream& os, const ReachableFact& reachable_fact);
};

std::ostream& operator<<(std::ostream& os, const ReachableFact& reachable_fact);

struct MemoryElement
{
//	ReachableFact* element_;
	MemoryElement* next_free_memory_slot_;
};

class MemoryChunk
{
public:
	MemoryChunk(size_t unit_size, MemoryChunk* previous_chunk, unsigned int nr_units = 100000);
	~MemoryChunk();

	MemoryElement* begin() const { return (struct MemoryElement*)allocated_memory_; }
	
private:
	
	void* allocated_memory_;
	
	size_t unit_size_;
	MemoryChunk* previous_chunk_;
	unsigned int nr_units_;
};

/**
 * This is a memory pool which is used to make the usage of reachable facts more efficient in both time and memory.
 */
class MemoryPool
{
public:
	/**
	 * Create a memory pool for the given set of arities.
	 */
	MemoryPool(size_t unit_size);
	
	~MemoryPool();
	
	void* allocate(size_t size);
	
	void free(void* p);
	
private:
	
	size_t unit_size_;
	
	void createNewMemoryChunk();
	
	MemoryElement* current_free_slot_;
	
	MemoryChunk* latest_created_chunk_;
};

static MemoryPool* g_reachable_fact_memory_pool = new MemoryPool(sizeof(ReachableFact));

};

};

#endif // MYPOP_SAS_PLUS_DTG_REACHABLE_FACT_H