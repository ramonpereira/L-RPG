#include <cstring>
#include <iterator>
#include <sys/time.h>

#include "equivalent_object_group.h"
#include "dtg_reachability.h"
#include "sas/dtg_manager.h"
#include "sas/dtg_graph.h"
#include "sas/dtg_node.h"
#include "sas/property_space.h"
#include "sas/transition.h"
#include "type_manager.h"
#include "predicate_manager.h"
#include "term_manager.h"

//#define MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//#define MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_DEBUG

namespace MyPOP {
	
namespace REACHABILITY {

/**
 * Equivalent Object.
 */
EquivalentObject::EquivalentObject(const Object& object, EquivalentObjectGroup& equivalent_object_group)
	: object_(&object), equivalent_group_(&equivalent_object_group)
{
	
}
	
void EquivalentObject::addInitialFact(ReachableFact& reachable_fact)
{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
	std::cout << "Add " << reachable_fact << " to the initial state of " << *this << std::endl;
#endif
	if (std::find(initial_facts_.begin(), initial_facts_.end(), &reachable_fact) == initial_facts_.end())
	{
		initial_facts_.push_back(&reachable_fact);
		equivalent_group_->addReachableFact(reachable_fact);
	}
}

bool EquivalentObject::areEquivalent(const EquivalentObject& other) const
{
//	std::cout << "Are " << other << " and " << *this << " equivalent?" << std::endl;

//	if (initial_facts_.size() != other.initial_facts_.size() ||
//	    initial_facts_.empty())
	if (initial_facts_.empty() || other.initial_facts_.empty())
	{
//		std::cout << initial_facts_.size() << " v.s. " << other.initial_facts_.size() << std::endl;
		return false;
	}
	
	for (std::vector<const ReachableFact*>::const_iterator ci = initial_facts_.begin(); ci != initial_facts_.end(); ci++)
	{
		const ReachableFact* this_reachable_fact = *ci;
		
		bool is_fact_reachable = false;
		for (std::vector<const ReachableFact*>::const_iterator ci = other.initial_facts_.begin(); ci != other.initial_facts_.end(); ci++)
		{
			const ReachableFact* other_reachable_fact = *ci;
			
			if (this_reachable_fact->isEquivalentTo(*other_reachable_fact))
			{
				is_fact_reachable = true;
				break;
			}
		}
		
		if (!is_fact_reachable)
		{
//			std::cout << "The fact: " << *this_reachable_fact << " is not reachable!" << std::endl;
			return false;
		}
	}

//	std::cout << "Are equivalent!" << std::endl;
	return true;
}

bool EquivalentObject::isInitialStateReachable(const std::vector<ReachableFact*>& reachable_facts) const
{
	// Check if these initial facts are reachable by the facts reachable by this group of objects.
	for (std::vector<const ReachableFact*>::const_iterator ci = initial_facts_.begin(); ci != initial_facts_.end(); ci++)
	{
		const ReachableFact* initial_fact = *ci;

#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//		std::cout << "Compare: " << *initial_fact << "." << std::endl;
#endif
		
		bool is_initial_fact_reachable = false;
		for (std::vector<ReachableFact*>::const_iterator ci = reachable_facts.begin(); ci != reachable_facts.end(); ci++)
		{
			const ReachableFact* reachable_fact = *ci;
			
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//		std::cout << " with: " << *reachable_fact<< "." << std::endl;
#endif
			
			if (initial_fact->isEquivalentTo(*reachable_fact))
			{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//				std::cout << *initial_fact << " is equivalent to " << *reachable_fact << std::endl;
#endif
				is_initial_fact_reachable = true;
				break;
			}
		}
		
		if (!is_initial_fact_reachable)
		{
			return false;
		}
	}
	// Got there!
	return true;
}

std::ostream& operator<<(std::ostream& os, const EquivalentObject& equivalent_object)
{
	os << *equivalent_object.object_;
	os << " Initial facts: {" << std::endl;
	for (std::vector<const ReachableFact*>::const_iterator ci = equivalent_object.initial_facts_.begin(); ci != equivalent_object.initial_facts_.end(); ci++)
	{
		os << **ci << std::endl;
	}
	os << " }";
	return os;
}

/**
 * Equivalent Object Group.
 */
MyPOP::UTILITY::MemoryPool** EquivalentObjectGroup::g_eog_arrays_memory_pool_;

unsigned int EquivalentObjectGroup::max_arity_;
EquivalentObjectGroup::EquivalentObjectGroup(const SAS_Plus::DomainTransitionGraph& dtg_graph, const Object* object, bool is_grounded)
	: is_grounded_(is_grounded), link_(NULL), finger_print_(NULL), merged_at_iteration_(std::numeric_limits<unsigned int>::max())
{
	if (object != NULL)
	{
		initialiseFingerPrint(dtg_graph, *object);
	}
}

EquivalentObjectGroup::~EquivalentObjectGroup()
{
	delete[] finger_print_;
	
	// Only delete the equivalent object if this EOG is root. Otherwise the object groups are used and contained by another EOG.
	//delete *equivalent_objects_.begin();
	if (link_ == NULL)
	{
		for (std::vector<EquivalentObject*>::const_iterator ci = equivalent_objects_.begin(); ci != equivalent_objects_.end(); ci++)
		{
			delete *ci;
		}
	}
}

void EquivalentObjectGroup::initMemoryPool(unsigned int max_arity)
{
	g_eog_arrays_memory_pool_ = new MyPOP::UTILITY::MemoryPool*[max_arity];
	for (unsigned int i = 0; i < max_arity; i++)
	{
		g_eog_arrays_memory_pool_[i] = new MyPOP::UTILITY::MemoryPool(sizeof(EquivalentObjectGroup*) * i);
	}
	max_arity_ = max_arity;
}

void EquivalentObjectGroup::deleteMemoryPool()
{
	for (unsigned int i = 0; i < max_arity_; i++)
	{
		delete g_eog_arrays_memory_pool_[i];
	}
	delete[] g_eog_arrays_memory_pool_;
}

void* EquivalentObjectGroup::operator new[](size_t size)
{
	// Check which memory pool to use.
	return g_eog_arrays_memory_pool_[size / sizeof(EquivalentObjectGroup*)]->allocate(size);
}

void EquivalentObjectGroup::operator delete[](void* p)
{
	// We don't know where it is stored :X.
}

bool EquivalentObjectGroup::contains(const Object& object) const
{
	for (std::vector<EquivalentObject*>::const_iterator ci = equivalent_objects_.begin(); ci != equivalent_objects_.end(); ci++)
	{
		const EquivalentObject* eo = *ci;
		if (&eo->getObject() == &object) return true;
	}
	return false;
}

bool EquivalentObjectGroup::contains(const Object& object, unsigned int iteration) const
{
	// Check if we were merged with another EOG.
	if (merged_at_iteration_ <= iteration)
	{
		assert (link_ != NULL);
		return link_->contains(object, iteration);
	}
	else
	{
		assert (size_per_iteration_.size() > iteration);
		for (unsigned int i = 0; i < size_per_iteration_[iteration]; i++)
		{
			if (&equivalent_objects_[i]->getObject() == &object) return true;
		}
	}
	return false;
}

bool EquivalentObjectGroup::isIdenticalTo(EquivalentObjectGroup& other)
{
	return getRootNode() == other.getRootNode();
}

bool EquivalentObjectGroup::hasSameFingerPrint(const EquivalentObjectGroup& other) const
{
	return memcmp(finger_print_, other.finger_print_, finger_print_size_ * sizeof(bool)) == 0;
//	for (unsigned int i = 0; i < finger_print_size_; i++)
//	{
//		if (finger_print_[i] != other.finger_print_[i]) return false;
//	}
//	return true;
}
	
void EquivalentObjectGroup::initialiseFingerPrint(const SAS_Plus::DomainTransitionGraph& dtg_graph, const Object& object)
{
	// Check the DTG graph and check which DTG nodes an object can be part of based on its type.
	unsigned int number_of_facts = 0;
	for (std::vector<SAS_Plus::DomainTransitionGraphNode*>::const_iterator ci = dtg_graph.getNodes().begin(); ci != dtg_graph.getNodes().end(); ci++)
	{
		const SAS_Plus::DomainTransitionGraphNode* dtg_node = *ci;
		for (std::vector<SAS_Plus::BoundedAtom*>::const_iterator ci = dtg_node->getAtoms().begin(); ci != dtg_node->getAtoms().end(); ci++)
		{
			number_of_facts += (*ci)->getAtom().getArity();
		}
	}
	
	finger_print_size_ = number_of_facts;
	finger_print_ = new bool[number_of_facts];
	memset(&finger_print_[0], false, sizeof(bool) * number_of_facts);
	
	number_of_facts = 0;
	for (std::vector<SAS_Plus::DomainTransitionGraphNode*>::const_iterator ci = dtg_graph.getNodes().begin(); ci != dtg_graph.getNodes().end(); ci++)
	{
		const SAS_Plus::DomainTransitionGraphNode* dtg_node = *ci;
		for (std::vector<SAS_Plus::BoundedAtom*>::const_iterator ci = dtg_node->getAtoms().begin(); ci != dtg_node->getAtoms().end(); ci++)
		{
			const SAS_Plus::BoundedAtom* bounded_atom = *ci;
			for (std::vector<const Term*>::const_iterator ci = bounded_atom->getAtom().getTerms().begin(); ci != bounded_atom->getAtom().getTerms().end(); ci++)
			{
				const Term* term = *ci;
				
				if (object.getType()->isSubtypeOf(*term->getType()) || object.getType()->isEqual(*term->getType()))
				{
					finger_print_[number_of_facts] = true;
				}
				++number_of_facts;
			}
		}
	}
}

void EquivalentObjectGroup::addEquivalentObject(EquivalentObject& eo)
{
	equivalent_objects_.push_back(&eo);
}

void EquivalentObjectGroup::addReachableFact(ReachableFact& reachable_fact)
{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_DEBUG
	// Make sure we do not add any double instances!
	for (std::vector<ReachableFact*>::const_iterator ci = reachable_facts_.begin(); ci != reachable_facts_.end(); ci++)
	{
		const ReachableFact* existing_reachable_fact = *ci;
		if (existing_reachable_fact->isIdenticalTo(reachable_fact))
		{
			std::cout << "Trying to add: " << reachable_fact << " to the following set: " << std::endl;
			
			for (std::vector<ReachableFact*>::const_iterator ci = reachable_facts_.begin(); ci != reachable_facts_.end(); ci++)
			{
				const ReachableFact* existing_reachable_factz = *ci;
				std::cout << "* " << *existing_reachable_factz << std::endl;
			}
			
			std::cout << "However it is identical to: " << *existing_reachable_fact << std::endl;

			assert (false);
		}
	}
#endif
	reachable_facts_.push_back(&reachable_fact);
}

bool EquivalentObjectGroup::tryToMergeWith(EquivalentObjectGroup& other_group, std::vector<EquivalentObjectGroup*>& affected_groups, unsigned int iteration)
{
	// If the object has been grounded it cannot be merged!
	if (is_grounded_ || other_group.is_grounded_)
	{
		return false;
	}
	
	// If two object groups are part of the same root node they are already merged!
	EquivalentObjectGroup& this_root_node = getRootNode();
	EquivalentObjectGroup& other_root_node = other_group.getRootNode();
	if (&this_root_node == &other_root_node)
	{
		return true;
	}
	
	// Make sure to only call this method with the root nodes.
	if (link_ != NULL)
	{
		return this_root_node.tryToMergeWith(other_root_node, affected_groups, iteration);
	}
	else if (other_group.link_ != NULL)
	{
		return tryToMergeWith(other_root_node, affected_groups, iteration);
	}

	// Only allow to merge two equivalent object groups if the fingerprints are equal!
	assert (finger_print_size_ == other_group.finger_print_size_);
	if (memcmp(finger_print_, other_group.finger_print_, finger_print_size_) != 0)
	{
		return false;
	}
	
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//	std::cout << "Try to merge: " << *this << " with " << other_group << "." << std::endl;
#endif

	// TODO: This implementation isn't fast at all...
	bool can_merge = false;
	for (std::vector<EquivalentObject*>::const_iterator ci = other_group.equivalent_objects_.begin(); ci != other_group.equivalent_objects_.end(); ci++)
	{
		const EquivalentObject* other_equivalent_object = *ci;
		if (other_equivalent_object->isInitialStateReachable(reachable_facts_))
		{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//			std::cout << other_group << " can reach initial state of " << *this << std::endl;
#endif
			can_merge = true;
			break;
		}
	}
	if (!can_merge) return false;
	can_merge = false;
	
	for (std::vector<EquivalentObject*>::const_iterator ci = equivalent_objects_.begin(); ci != equivalent_objects_.end(); ci++)
	{
		const EquivalentObject* equivalent_object = *ci;
		if (equivalent_object->isInitialStateReachable(other_group.reachable_facts_))
		{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//			std::cout << *this << " can reach initial state of " << other_group << std::endl;
#endif
			can_merge = true;
			break;
		}
	}
	if (!can_merge) return false;
	
	merge(other_group, affected_groups);
	other_group.merged_at_iteration_ = iteration;
	return true;
}

bool EquivalentObjectGroup::operator==(const EquivalentObjectGroup& other) const
{
	// Determine the root nodes.
	const EquivalentObjectGroup* this_root = this;
	const EquivalentObjectGroup* other_root = &other;
	while (other_root->link_ != NULL) other_root = other_root->link_;
	while (this_root->link_ != NULL) this_root = this_root->link_;
	
	return this_root == other_root;
}

bool EquivalentObjectGroup::operator!=(const EquivalentObjectGroup& other) const
{
	return !(*this == other);
}

void EquivalentObjectGroup::printObjects(std::ostream& os) const
{
	for (std::vector<EquivalentObject*>::const_iterator ci = equivalent_objects_.begin(); ci != equivalent_objects_.end(); ci++)
	{
		os << (*ci)->getObject();
		if (ci + 1 != equivalent_objects_.end())
		{
			os << ", ";
		}
	}
}

void EquivalentObjectGroup::printObjects(std::ostream& os, unsigned int iteration) const
{
	// Check if we were merged with another EOG.
	if (merged_at_iteration_ <= iteration)
	{
		assert (link_ != NULL);
		link_->printObjects(os, iteration);
	}
	else
	{
		assert (size_per_iteration_.size() > iteration);
		for (unsigned int i = 0; i < size_per_iteration_[iteration]; i++)
		{
			os << equivalent_objects_[i]->getObject();
			if (i + 1 != size_per_iteration_[iteration])
			{
				os << ", ";
			}
		}
	}
}

void EquivalentObjectGroup::printGrounded(std::ostream& os) const
{
//	for (std::multimap<std::pair<const DomainTransitionGraphNode*, const BoundedAtom*>, ReachableFact*>::const_iterator ci = reachable_facts_.begin(); ci != reachable_facts_.end(); ci++)
//	{
//		(*ci).second->printGrounded(os);
//	}
}

void EquivalentObjectGroup::merge(EquivalentObjectGroup& other_group, std::vector<EquivalentObjectGroup*>& affected_groups)
{
	assert (other_group.link_ == NULL);
	
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
	std::cout << "Merging " << *this << " with " << other_group << "." << std::endl;
#endif
	
	equivalent_objects_.insert(equivalent_objects_.end(), other_group.equivalent_objects_.begin(), other_group.equivalent_objects_.end());
	other_group.link_ = this;
	
	// TODO: Need to make sure we do not end up with multiple reachable facts which are identical!
	//std::vector<EquivalentObjectGroup*> affected_groups;
	
	// Facts which are already part of this EOG should already contain the updated reachable fact, so any facts which need to 
	// be updated can safely be removed.
	for (std::vector<ReachableFact*>::reverse_iterator ri = reachable_facts_.rbegin(); ri != reachable_facts_.rend(); ri++)
	{
		ReachableFact* reachable_fact = *ri;
		
		for (unsigned int i = 0; i < reachable_fact->getAtom().getArity(); i++)
		{
			EquivalentObjectGroup& eog = reachable_fact->getTermDomain(i);
			if (!eog.isRootNode())
			{
				reachable_facts_.erase(ri.base() - 1);
				for (unsigned int i = 0; i < reachable_fact->getAtom().getArity(); i++)
				{
					EquivalentObjectGroup& eog = reachable_fact->getTermDomain(i);
					
					if (&eog != this && std::find(affected_groups.begin(), affected_groups.end(), &eog) == affected_groups.end())
					{
						affected_groups.push_back(&eog);
					}
				}
				break;
			}
		}
	}
	std::vector<ReachableFact*> updated_facts(reachable_facts_);
	for (std::vector<ReachableFact*>::const_iterator ri = other_group.reachable_facts_.begin(); ri != other_group.reachable_facts_.end(); ri++)
	{
		ReachableFact* reachable_fact = *ri;
		// The set of reachable facts in this EOG and the other EOG should be disjunct so there is no way any of the facts 
		// are yet marked for removal.
		// However, if a fact contains a reference to the same EOG which is then merged with another EOG we cannot select the EOG with the same 
		// references as the "head" node.

		// If multiple EOGs have been updated it can be that one of the facts in this EOG is also affected. We should ignore the 
		// nodes marked for removal.
		if (reachable_fact->isMarkedForRemoval()) continue;
		
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//		std::cout << "Check if " << *reachable_fact << " needs to be updated!" << std::endl;
#endif
		// If the reachable fact contains a EOG which is not a root node, it means that a merge has taken place and we need to delete this node and
		// replace it with a reachable fact containing only root nodes.
		bool already_present = false;
		if (reachable_fact->updateTermsToRoot())
		{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//			std::cout << "Updated the reachable fact: " << *reachable_fact << std::endl;
#endif

			ReachableFact* identical_fact = NULL;
			for (std::vector<ReachableFact*>::const_iterator ci = updated_facts.begin(); ci != updated_facts.end(); ci++)
			{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
//				std::cout << "Check if " << **ci << " and " << *reachable_fact << " are identical!" << std::endl;
#endif
				if ((*ci)->isIdenticalTo(*reachable_fact))
				{
					assert (*ci != reachable_fact);
					identical_fact = *ci;
					already_present = true;
					break;
				}
			}
			
			if (already_present)
			{
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
				std::cout << "Remove: " << *reachable_fact << "[r=" << &reachable_fact->getReplacement() << "] because it is identical to the fact: " << *identical_fact << "[r=" << &identical_fact->getReplacement() << "]." << std::endl;
#endif
				reachable_fact->replaceBy(*identical_fact);
				
				for (unsigned int i = 0; i < reachable_fact->getAtom().getArity(); i++)
				{
					EquivalentObjectGroup& eog = reachable_fact->getTermDomain(i);
					
					if (/*&eog != this && */std::find(affected_groups.begin(), affected_groups.end(), &eog) == affected_groups.end())
					{
						affected_groups.push_back(&eog);
					}
				}
			}
			else
			{
				updated_facts.push_back(reachable_fact);
			}
		}
		
		if (!already_present)
		{
			addReachableFact(*reachable_fact);
		}
	}

	// Clean up all the groups which have been affected by removing all the reachable facts which have been marked for removal.
	// NOTE: This can be done later after the whole merging is done! But this is just an optimisation we can perform
	// later too.
//	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = affected_groups.begin(); ci != affected_groups.end(); ci++)
//	{
//		EquivalentObjectGroup* eog = *ci;
//		eog->deleteRemovedFacts();
//	}
	
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
	std::cout << "Result of merging: " << *this << "." << std::endl;
#endif
}

void EquivalentObjectGroup::deleteRemovedFacts()
{
	for (std::vector<ReachableFact*>::reverse_iterator ri = reachable_facts_.rbegin(); ri != reachable_facts_.rend(); ri++)
	{
		if ((*ri)->isMarkedForRemoval())
		{
			reachable_facts_.erase(ri.base() - 1);
		}
	}
}

void EquivalentObjectGroup::updateEquivalences(const std::vector<EquivalentObjectGroup*>& all_eogs, std::vector<EquivalentObjectGroup*>& affected_groups, unsigned int iteration)
{
	// If we are not a root node, we cannot merge this EOG.
	if (isRootNode())
	{
		// Try to merge this EOG with any other root EOG.
		for (std::vector<EquivalentObjectGroup*>::const_iterator ci = all_eogs.begin(); ci != all_eogs.end(); ci++)
		{
			EquivalentObjectGroup* eog = *ci;
			if (!eog->isRootNode()) continue;
			
			tryToMergeWith(*eog, affected_groups, iteration);
		}
	}
	
	size_per_iteration_.push_back(equivalent_objects_.size());
}

EquivalentObjectGroup& EquivalentObjectGroup::getRootNode()
{
	if (link_ == NULL)
		return *this;
	return link_->getRootNode();
}

std::ostream& operator<<(std::ostream& os, const EquivalentObjectGroup& group)
{
	if (group.link_ != NULL)
	{
		os << *group.link_;
		return os;
	}
	
//	os << " -= EquivalentObjectGroup =- " << std::endl;
	os << "{ ";
	for (std::vector<EquivalentObject*>::const_iterator ci = group.equivalent_objects_.begin(); ci != group.equivalent_objects_.end(); ci++)
	{
		const EquivalentObject* eo = *ci;
		os << eo->getObject() << std::endl;
		
//		os << eo->getObject();
//		if (ci + 1 != group.equivalent_objects_.end())
//		{
//			os << ", ";
//		}
	}
	os << " }" << std::endl;

	std::cout << "Reachable facts: " << std::endl;
	for (std::vector<ReachableFact*>::const_iterator ci = group.reachable_facts_.begin(); ci != group.reachable_facts_.end(); ci++)
	{
		std::cout << "- " << **ci << std::endl;
	}
	return os;
}

/**
 * Equivalent Object Group Manager.
 */
EquivalentObjectGroupManager::EquivalentObjectGroupManager(const SAS_Plus::DomainTransitionGraphManager& dtg_manager, const SAS_Plus::DomainTransitionGraph& dtg_graph, const TermManager& term_manager)
{
	unsigned int max_arity = 0;
	for (std::vector<const SAS_Plus::Property*>::const_iterator ci = dtg_graph.getPredicates().begin(); ci != dtg_graph.getPredicates().end(); ci++)
	{
		const SAS_Plus::Property* property = *ci;
		if (property->getPredicate().getArity() > max_arity) max_arity = property->getPredicate().getArity();
	}
	
	EquivalentObjectGroup::initMemoryPool(max_arity);
	
	// Create initial data structures.
	for (std::vector<const Object*>::const_iterator ci = term_manager.getAllObjects().begin(); ci != term_manager.getAllObjects().end(); ci++)
	{
		const Object* object = *ci;
		EquivalentObjectGroup* equivalent_object_group = new EquivalentObjectGroup(dtg_graph, object, dtg_manager.isObjectGrounded(*object));
		EquivalentObject* equivalent_object = new EquivalentObject(*object, *equivalent_object_group);
		equivalent_object_group->addEquivalentObject(*equivalent_object);
		
		equivalent_groups_.push_back(equivalent_object_group);
		object_to_equivalent_object_mapping_[object] = equivalent_object;
	}

	zero_arity_equivalent_object_group_ = new EquivalentObjectGroup(dtg_graph, NULL, true);
	equivalent_groups_.push_back(zero_arity_equivalent_object_group_);
	
#ifdef MYPOP_SAS_PLUS_EQUIAVLENT_OBJECT_COMMENT
	std::cout << "Done initialising data structures." << std::endl;
#endif
}

EquivalentObjectGroupManager::~EquivalentObjectGroupManager()
{
	EquivalentObjectGroup::deleteMemoryPool();
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end(); ci++)
	{
		delete *ci;
	}
}

void EquivalentObjectGroupManager::initialise(const std::vector<ReachableFact*>& initial_facts)
{
	// Record the set of initial facts every object is part of.
	for (std::vector<ReachableFact*>::const_iterator ci = initial_facts.begin(); ci != initial_facts.end(); ci++)
	{
		ReachableFact* initial_reachable_fact = *ci;
		
		if (initial_reachable_fact->getAtom().getArity() > 0)
		{
			for (unsigned int i = 0; i < initial_reachable_fact->getAtom().getArity(); i++)
			{
				EquivalentObjectGroup& eog = initial_reachable_fact->getTermDomain(i);
				for (std::vector<EquivalentObject*>::const_iterator ci = eog.getEquivalentObjects().begin(); ci != eog.getEquivalentObjects().end(); ci++)
				{
					(*ci)->addInitialFact(*initial_reachable_fact);
				}
			}
		}
		else
		{
			zero_arity_equivalent_object_group_->addReachableFact(*initial_reachable_fact);
		}
	}
}


void EquivalentObjectGroupManager::updateEquivalences(unsigned int iteration)
{
	std::vector<EquivalentObjectGroup*> affected_groups;
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end(); ci++)
	{
		EquivalentObjectGroup* eog = *ci;
		eog->updateEquivalences(equivalent_groups_, affected_groups, iteration);
	}

	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = affected_groups.begin(); ci != affected_groups.end(); ci++)
	{
		EquivalentObjectGroup* eog = *ci;
		if (eog->isRootNode())
		{
			eog->deleteRemovedFacts();
		}
	}
}

EquivalentObject& EquivalentObjectGroupManager::getEquivalentObject(const Object& object) const
{
	std::map<const Object*, EquivalentObject*>::const_iterator ci = object_to_equivalent_object_mapping_.find(&object);
	if (ci == object_to_equivalent_object_mapping_.end())
	{
		std::cout << "Could not find the Equivalent Object for the object: " << object << std::endl;
		assert (false);
	}
	
	return *(*ci).second;
}

void EquivalentObjectGroupManager::getAllReachableFacts(std::vector<const ReachableFact*>& result) const
{
	std::set<const EquivalentObjectGroup*> closed_list;
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end(); ci++)
	{
		EquivalentObjectGroup* eog = *ci;
		if (!eog->isRootNode()) continue;
		const std::vector<ReachableFact*>& reachable_fact = eog->getReachableFacts();
		
		for (std::vector<ReachableFact*>::const_iterator ci = reachable_fact.begin(); ci != reachable_fact.end(); ci++)
		{
			// Make sure it hasn't been processed yet.
			ReachableFact* reachable_fact = *ci;
			bool has_been_processed = false;
			
			for (unsigned int i = 0; i < reachable_fact->getAtom().getArity(); i++)
			{
				if (closed_list.count(&reachable_fact->getTermDomain(i)) != 0)
				{
					has_been_processed = true;
					break;
				}
			}
			
			if (has_been_processed) continue;

			result.push_back(reachable_fact);
		}
		
		closed_list.insert(eog);
	}
}

unsigned int EquivalentObjectGroupManager::getNumberOfEquivalentGroups() const
{
	unsigned int index = 0;
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end(); ci++)
	{
		if ((*ci)->isRootNode())
		{
			++index ;
		}
	}
	return index;
}

void EquivalentObjectGroupManager::print(std::ostream& os) const
{
	os << "All equivalence groups:" << std::endl;
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end(); ci++)
	{
		if (!(*ci)->isRootNode()) continue;
		os << **ci << std::endl;
	}
}

void EquivalentObjectGroupManager::printAll(std::ostream& os) const
{
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end(); ci++)
	{
		if (!(*ci)->isRootNode()) continue;
		std::cout << "Print all grounded facts of the EOG: " << **ci << std::endl;
		
		(*ci)->printGrounded(os);
//		os << **ci << std::endl;
	}
}

};

};