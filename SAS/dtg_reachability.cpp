#include <cstring>
#include <iterator>

#include "dtg_reachability.h"
#include "dtg_manager.h"
#include "dtg_graph.h"
#include "dtg_node.h"
#include "property_space.h"
#include "transition.h"
#include "type_manager.h"
#include "../predicate_manager.h"
#include "../term_manager.h"


namespace MyPOP {
	
namespace SAS_Plus {

EquivalentObjectGroup::EquivalentObjectGroup(const Object& object)
{
	initial_mapping_[&object] = new std::vector<const DomainTransitionGraphNode*>();
}
	
EquivalentObjectGroup::EquivalentObjectGroup(const Object& object, std::vector<const DomainTransitionGraphNode*>& initial_dtgs)
{
	initial_mapping_[&object] = &initial_dtgs;
}

bool EquivalentObjectGroup::addInitialDTGNodeMapping(const Object& object, const DomainTransitionGraphNode& dtg_node)
{
	std::map<const Object*, std::vector<const DomainTransitionGraphNode*> *>::iterator i = initial_mapping_.find(&object);
	std::vector<const DomainTransitionGraphNode*>* mapping;
	if (i == initial_mapping_.end())
	{
		mapping = new std::vector<const DomainTransitionGraphNode*>();
		mapping->push_back(&dtg_node);
		return true;
	}
	
	mapping = (*i).second;
	if (std::find(mapping->begin(), mapping->end(), &dtg_node) == mapping->end())
	{
		mapping->push_back(&dtg_node);
		return true;
	}
	return false;
}

bool EquivalentObjectGroup::tryToMergeWith(const EquivalentObjectGroup& other_group, const std::map<const DomainTransitionGraphNode*, std::vector<const DomainTransitionGraphNode*>* >& reachable_nodes)
{
	std::cout << "Try to merge: " << *this << " with " << other_group << "." << std::endl;

	// Check if this is reachable from other and visa versa.
	for (std::map<const Object*, std::vector<const DomainTransitionGraphNode*> *>::const_iterator ci = initial_mapping_.begin(); ci != initial_mapping_.end(); ci++)
	{
		const Object* this_object = (*ci).first;
		const std::vector<const DomainTransitionGraphNode*>* this_initial_dtgs = (*ci).second;
		
		if (this_initial_dtgs->size() == 0)
		{
			continue;
		}
		
		for (std::map<const Object*, std::vector<const DomainTransitionGraphNode*> *>::const_iterator ci = other_group.initial_mapping_.begin(); ci != other_group.initial_mapping_.end(); ci++)
		{
			const Object* other_object = (*ci).first;
			
			// If objects are not of the same type they cannot be part of the same equivalent object group.
			// TODO: Refine types based on membership of DTG nodes.
			if (!this_object->getType()->isEqual(*other_object->getType()))
			{
				continue;
			}
			
			const std::vector<const DomainTransitionGraphNode*>* other_initial_dtgs = (*ci).second;
			
			if (other_initial_dtgs->size() == 0)
			{
				continue;
			}
			
			for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = this_initial_dtgs->begin(); ci != this_initial_dtgs->end(); ci++)
			{
				const DomainTransitionGraphNode* this_initial_dtg = *ci;
				std::vector<const DomainTransitionGraphNode*>* reachable_nodes_from_this = (*reachable_nodes.find(this_initial_dtg)).second;

				for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = other_initial_dtgs->begin(); ci != other_initial_dtgs->end(); ci++)
				{
					const DomainTransitionGraphNode* other_initial_dtg = *ci;
					std::vector<const DomainTransitionGraphNode*>* reachable_nodes_from_other = (*reachable_nodes.find(other_initial_dtg)).second;
					
					std::cout << "Check if " << *this_initial_dtg << " is reachable in the set: " << std::endl;
					for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = reachable_nodes_from_this->begin(); ci != reachable_nodes_from_this->end(); ci++)
					{
						std::cout << **ci << std::endl;
					}
					
					std::cout << "Check if " << *other_initial_dtg << " is reachable in the set: " << std::endl;
					for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = reachable_nodes_from_other->begin(); ci != reachable_nodes_from_other->end(); ci++)
					{
						std::cout << **ci << std::endl;
					}

					if (std::find(reachable_nodes_from_this->begin(), reachable_nodes_from_this->end(), other_initial_dtg) != reachable_nodes_from_this->end() &&
					    std::find(reachable_nodes_from_other->begin(), reachable_nodes_from_other->end(), this_initial_dtg) != reachable_nodes_from_other->end())
					{
						std::cout << *this << " is reachable from " << other_group << "." << std::endl;
						initial_mapping_.insert(other_group.initial_mapping_.begin(), other_group.initial_mapping_.end());
						return true;
					}
				}
			}
		}
	}
	return false;
}

std::ostream& operator<<(std::ostream& os, const EquivalentObjectGroup& group)
{
	os << " -= EquivalentObjectGroup =- " << std::endl;
	
	for (std::map<const Object*, std::vector<const DomainTransitionGraphNode*> *>::const_iterator ci = group.initial_mapping_.begin(); ci != group.initial_mapping_.end(); ci++)
	{
		os << *(*ci).first << " -> " << std::endl;
		
		std::vector<const DomainTransitionGraphNode*>* initial_nodes = (*ci).second;
		
		for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = initial_nodes->begin(); ci != initial_nodes->end(); ci++)
		{
			os << "* ";
			(*ci)->print(os);
			os << std::endl;
		}
	}
	
	return os;
}

EquivalentObjectGroupManager::EquivalentObjectGroupManager(const DTGReachability& dtg_reachability, const DomainTransitionGraph& dtg_graph, const TermManager& term_manager, const std::vector<const BoundedAtom*>& initial_facts)
	: dtg_reachability_(&dtg_reachability)
{
	// Create initial data structures.
	for (std::vector<const Object*>::const_iterator ci = term_manager.getAllObjects().begin(); ci != term_manager.getAllObjects().end(); ci++)
	{
		const Object* object = *ci;
		EquivalentObjectGroup* equivalent_group = new EquivalentObjectGroup(*object);
		equivalent_groups_.push_back(equivalent_group);
		object_to_equivalent_group_mapping_[object] = equivalent_group;
	}
	std::cout << "Done initialising data strucutres." << std::endl;
	
	// Look for the DTG nodes which are supported in the initial state.
	for (std::vector<DomainTransitionGraphNode*>::const_iterator ci = dtg_graph.getNodes().begin(); ci != dtg_graph.getNodes().end(); ci++)
	{
		const DomainTransitionGraphNode* dtg_node = *ci;
		const std::vector<BoundedAtom*>& atoms_to_achieve = dtg_node->getAtoms();
		std::vector<std::vector<const BoundedAtom*>* > supporting_tupples;
		std::map<const std::vector<const Object*>*, const std::vector<const Object*>* > variable_assignments;
		std::vector<const BoundedAtom*> initial_supporting_facts;
		dtg_reachability.getSupportingFacts(supporting_tupples, variable_assignments, atoms_to_achieve, initial_supporting_facts, initial_facts);

		for (std::vector<std::vector<const BoundedAtom*>* >::const_iterator ci = supporting_tupples.begin(); ci != supporting_tupples.end(); ci++)
		{
			std::cout << "Found a set of supporting facts for the DTG node: " << *dtg_node << std::endl;
			const std::vector<const BoundedAtom*>* supporting_tupple = *ci;

			for (std::vector<const BoundedAtom*>::const_iterator ci = supporting_tupple->begin(); ci != supporting_tupple->end(); ci++)
			{
				const BoundedAtom* bounded_atom = *ci;
				
				std::cout << " * ";
				bounded_atom->print(std::cout, dtg_graph.getBindings());
				std::cout << "." << std::endl;
				
				for (std::vector<const Property*>::const_iterator ci = bounded_atom->getProperties().begin(); ci != bounded_atom->getProperties().end(); ci++)
				{
					const Property* property = *ci;
					if (property->getIndex() == NO_INVARIABLE_INDEX)
						continue;
					
					std::cout << "the index " << property->getIndex() << " of the atom ";
					bounded_atom->print(std::cout, dtg_graph.getBindings());
					std::cout << " is invariable!" << std::endl;
					
					const std::vector<const Object*>& domain = bounded_atom->getVariableDomain(property->getIndex(), dtg_graph.getBindings());
					for (std::vector<const Object*>::const_iterator ci = domain.begin(); ci != domain.end(); ci++)
					{
						assert (object_to_equivalent_group_mapping_.find(*ci) != object_to_equivalent_group_mapping_.end());
						EquivalentObjectGroup* matching_equivalent_group = object_to_equivalent_group_mapping_[*ci];
						matching_equivalent_group->addInitialDTGNodeMapping(**ci, *dtg_node);
					}
				}
			}
		}
	}
}

void EquivalentObjectGroupManager::updateEquivalences(const std::map<const DomainTransitionGraphNode*, std::vector<const DomainTransitionGraphNode*>* >& reachable_nodes_)
{
	bool to_remove[equivalent_groups_.size()];
	memset(&to_remove[0], false, sizeof(bool) * equivalent_groups_.size());
	
	// Check if an initial mapping for an object can be reached from the initial mapping of another object.
	for (std::vector<EquivalentObjectGroup*>::const_iterator ci = equivalent_groups_.begin(); ci != equivalent_groups_.end() - 1; ci++)
	{
		EquivalentObjectGroup* equivalent_group1 = *ci;
		if (to_remove[std::distance((std::vector<EquivalentObjectGroup*>::const_iterator)equivalent_groups_.begin(), ci)])
		{
			continue;
		}
		
		for (std::vector<EquivalentObjectGroup*>::const_iterator ci2 = ci + 1; ci2 != equivalent_groups_.end(); ci2++)
		{
			EquivalentObjectGroup* equivalent_group2 = *ci2;
			if (to_remove[std::distance((std::vector<EquivalentObjectGroup*>::const_iterator)equivalent_groups_.begin(), ci2)])
			{
				continue;
			}
			
			// Check if any of the initial DTG nodes of both groups can be reached from one another.
			if (equivalent_group1->tryToMergeWith(*equivalent_group2, reachable_nodes_))
			{
				// Remove group2 if it has merged with group 1.
				to_remove[std::distance((std::vector<EquivalentObjectGroup*>::const_iterator)equivalent_groups_.begin(), ci2)] = true;
			}
		}
	}
	
	// Remove the nodes which have been merged.
	for (unsigned int i = 0; i < equivalent_groups_.size(); i++)
	{
		if (to_remove[i])
		{
			equivalent_groups_.erase(equivalent_groups_.begin() + i);
		}
	}
}
	
	
DTGReachability::DTGReachability(const DomainTransitionGraph& dtg_graph)
	: dtg_graph_(&dtg_graph)
{
	for (std::vector<DomainTransitionGraphNode*>::const_iterator ci = dtg_graph.getNodes().begin(); ci != dtg_graph.getNodes().end(); ci++)
	{
		supported_facts_[*ci] = new std::vector<std::vector<const BoundedAtom*> *>();
		reachable_nodes_[*ci] = new std::vector<const DomainTransitionGraphNode*>();
	}
}

void DTGReachability::propagateReachableNodes()
{
	std::set<const DomainTransitionGraphNode*> open_list;
	open_list.insert(dtg_graph_->getNodes().begin(), dtg_graph_->getNodes().end());
	
	std::set<const DomainTransitionGraphNode*> to_update_list;
	
	while (!open_list.empty())
	{
		const DomainTransitionGraphNode* dtg_node = *open_list.begin();
		open_list.erase(open_list.begin());
		
		std::cout << "Process: " << *dtg_node << "." << std::endl;
		
		std::vector<const DomainTransitionGraphNode*>* reachable_nodes = reachable_nodes_[dtg_node];
		assert (reachable_nodes != NULL);
		
		// For each of the reachable nodes, add the nodes which are reachable from this node to those reachable from this node.
		for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = reachable_nodes->begin(); ci != reachable_nodes->end(); ci++)
		{
			const DomainTransitionGraphNode* reachable_dtg_node = *ci;
			if (dtg_node == reachable_dtg_node)
				continue;
			
			assert (reachable_dtg_node != NULL);
			
//			std::cout << "find the DTG node: " << *reachable_dtg_node << std::endl;
			
			std::vector<const DomainTransitionGraphNode*>* reachable_from_reachable_nodes = reachable_nodes_[reachable_dtg_node];
			assert (reachable_from_reachable_nodes != NULL);

			// Merge the two vectors.
			unsigned int pre_size = reachable_from_reachable_nodes->size();
			for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = reachable_nodes->begin(); ci != reachable_nodes->end(); ci++)
			{
				const DomainTransitionGraphNode* dtg_node_to_add = *ci;
				bool already_part = false;
				for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = reachable_from_reachable_nodes->begin(); ci != reachable_from_reachable_nodes->end(); ci++)
				{
					const DomainTransitionGraphNode* existing_dtg_node = *ci;
					if (existing_dtg_node == dtg_node_to_add)
					{
						already_part = true;
						break;
					}
				}
				
				if (!already_part)
				{
					reachable_from_reachable_nodes->push_back(dtg_node_to_add);
				}
			}
			
			// If new items have been inserted, make sure to take the reachable node into account on the next iteration.
			if (pre_size != reachable_from_reachable_nodes->size())
			{
				open_list.insert(reachable_dtg_node);
			}
		}
	}
	
	// Check if we can make any of the objects equivalent.
	
}

bool DTGReachability::makeReachable(const DomainTransitionGraphNode& dtg_node, std::vector<const BoundedAtom*>& new_reachable_facts)
{
	std::vector<std::vector<const BoundedAtom*> *>* already_reachable_facts = supported_facts_[&dtg_node];
	
	// Make sure the set of new reachable facts is not already part of the supported set.
	for (std::vector<std::vector<const BoundedAtom*> *>::const_iterator ci = already_reachable_facts->begin(); ci != already_reachable_facts->end(); ci++)
	{
		const std::vector<const BoundedAtom*>* reachable_facts = *ci;
		
		if (reachable_facts->size() != new_reachable_facts.size()) continue;
		
		bool all_facts_are_equal = true;
		for (unsigned int i = 0; i < reachable_facts->size(); i++)
		{
			if (!dtg_graph_->getBindings().areEquivalent((*reachable_facts)[i]->getAtom(), (*reachable_facts)[i]->getId(), new_reachable_facts[i]->getAtom(), new_reachable_facts[i]->getId()))
			{
				all_facts_are_equal = false;
				break;
			}
		}
		
		if (all_facts_are_equal)
		{
			return false;
		}
	}
	
	already_reachable_facts->push_back(&new_reachable_facts);
	return true;
}

void DTGReachability::performReachabilityAnalsysis(const std::vector<const BoundedAtom*>& initial_facts, const TermManager& term_manager)
{
	std::cout << "Start performing reachability analysis." << std::endl;
	
	// Initialise the individual groups per object.
	equivalent_object_manager_ = new EquivalentObjectGroupManager(*this, *dtg_graph_, term_manager, initial_facts);
	
	// Keep a list of all established facts so far.
	std::vector<const BoundedAtom*> established_facts(initial_facts);
	
	// List of already achieved transitions.
	std::set<const Transition*> achieved_transitions;
	
	unsigned int pre_size = 0;

	// Keep on iterator as long as we can establish new facts.
	do 
	{
		pre_size = established_facts.size();
		iterateThroughFixedPoint(established_facts, achieved_transitions);
		std::cout << pre_size << " v.s. " << established_facts.size() << std::endl;
		
		// After no other transitions can be reached we establish the object equivalence relations.
		equivalent_object_manager_->updateEquivalences(reachable_nodes_);
		
		// Check for DTG nodes which have a transition in which a grounded node links two facts which are part of different
		// balanced sets.
		for (std::vector<DomainTransitionGraphNode*>::const_iterator ci = dtg_graph_->getNodes().begin(); ci != dtg_graph_->getNodes().end(); ci++)
		{
			const DomainTransitionGraphNode* dtg_node = *ci;
			
			std::map<const Transition*, std::vector<const std::vector<const Object*>* >* > transitions;
			dtg_node->getExternalDependendTransitions(transitions);
			
			/**
			 * For each transition which contains terms with an external dependency we evaluate all the values these
			 * external dependend terms can have and see if any other nodes are reachable from the from node of the
			 * transition.
			 *
			 * Examples where this situation can occur is in driverlog in the unload transitions between { (in package truck)
			 * AND (at truck loc) } -> (at package loc). The final location of the package is dependend on the location of the
			 * truck. However, the location of the truck is not handled by the object package and the driver action is not
			 * part of the package's property space.
			 *
			 * Therefore we check which trucks can have a package on board and what locations these trucks can occupy. This will
			 * determine where packages can be unloaded.
			 */
			for (std::map<const Transition*, std::vector<const std::vector<const Object*>* >* >::const_iterator ci = transitions.begin(); ci != transitions.end(); ci++)
			{
				const Transition* transition = (*ci).first;
				const std::vector<const std::vector<const Object*>* >* dependend_term_domains = (*ci).second;
				
				std::cout << "The transition: " << *transition << " has external dependencies!" << std::endl;
				
				// Check if atom which is part of the external dependency can take on different values for the grounded term.
				const DomainTransitionGraphNode& from_node = transition->getFromNode();
				std::vector<std::vector<const BoundedAtom*>* >* supporing_facts_from_node = supported_facts_[&from_node];

				// Prepate a mask so we can identify which terms have external dependencies and which do not.
				unsigned int largest_arity = 0;
				for (std::vector<BoundedAtom*>::const_iterator ci = from_node.getAtoms().begin(); ci != from_node.getAtoms().end(); ci++)
				{
					if ((*ci)->getAtom().getArity() > largest_arity)
					{
						largest_arity = (*ci)->getAtom().getArity();
					}
				}
				bool dependend_term_mapping[from_node.getAtoms().size()][largest_arity];
				memset(&dependend_term_mapping[0][0], false, sizeof(bool) * largest_arity * dependend_term_domains->size());

				std::vector<const BoundedAtom*> equivalent_nodes_to_find;
				bool facts_with_external_dependencies[from_node.getAtoms().size()];
				memset(&facts_with_external_dependencies[0], false, sizeof(bool) * from_node.getAtoms().size());
				
				/**
				 * Determine which facts and terms contain external dependencies. We create a list of bounded atoms which
				 * is used to search for DTG nodes which contain the same facts as the from node of the transition except 
				 * for those terms which is external dependend. So in the example above of driverlog the location is the
				 * external dependend term and may vary in the DTG nodes we are looking for - the rest needs to the exactly
				 * the same!
				 */
				for (std::vector<BoundedAtom*>::const_iterator ci = from_node.getAtoms().begin(); ci != from_node.getAtoms().end(); ci++)
				{
					const BoundedAtom* from_node_bounded_atom = *ci;
					BoundedAtom& new_bounded_atom = BoundedAtom::createBoundedAtom(from_node_bounded_atom->getAtom(), from_node_bounded_atom->getProperties(), dtg_graph_->getBindings());
					
					// Make the term's domain equal to the original - except if has an external dependency.
					for (unsigned int i = 0; i < new_bounded_atom.getAtom().getArity(); i++)
					{
						const std::vector<const Object*>& org_domain = from_node_bounded_atom->getAtom().getTerms()[i]->getDomain(from_node_bounded_atom->getId(), dtg_graph_->getBindings());
						const Term* new_term = new_bounded_atom.getAtom().getTerms()[i];
						
						// It is not a dependend term - copy.
						if (std::find(dependend_term_domains->begin(), dependend_term_domains->end(), &org_domain) == dependend_term_domains->end())
						{
							new_term->makeDomainEqualTo(new_bounded_atom.getId(), org_domain, dtg_graph_->getBindings());
							dependend_term_mapping[std::distance(from_node.getAtoms().begin(), ci)][i] = false;
						}
						// Else it is a dependend term - leave it.
						else
						{
							facts_with_external_dependencies[std::distance(from_node.getAtoms().begin(), ci)] = true;
							dependend_term_mapping[std::distance(from_node.getAtoms().begin(), ci)][i] = true;
						}
					}
					equivalent_nodes_to_find.push_back(&new_bounded_atom);
				}
				
				// Now find all the DTG nodes which match this criterium.
				std::vector<const DomainTransitionGraphNode*> matching_dtgs;
				dtg_graph_->getNodes(matching_dtgs, equivalent_nodes_to_find);

				std::cout << "Found matching DTG nodes: " << std::endl;
				for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = matching_dtgs.begin(); ci != matching_dtgs.end(); ci++)
				{
					const DomainTransitionGraphNode* dtg_node = *ci;
					std::cout << *dtg_node << std::endl;
				}
				
				/**
				 * For every DTG node which conforms to the above requirements, we check if the external dependencies
				 * can be satisfied to make these nodes reachable from the from node.
				 */
				for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = matching_dtgs.begin(); ci != matching_dtgs.end() - 1; ci++)
				{
					const DomainTransitionGraphNode* equivalent_dtg_node = *ci;

					if (equivalent_dtg_node == &from_node) continue;
					assert (equivalent_dtg_node->getAtoms().size() == from_node.getAtoms().size());
					
					/**
					 * We construct the bounded atoms corresponding to the facts which need to be reached to satisfy the
					 * externally dependend facts.
					 */
					for (std::vector<std::vector<const BoundedAtom*>* >::const_iterator ci = supporing_facts_from_node->begin(); ci != supporing_facts_from_node->end(); ci++)
					{
						std::vector<const BoundedAtom*>* supporting_facts = *ci;
					
						/**
						* Check all the facts of the potential to nodes and check if we can reach them - we only need to
						* check the facts which contain an external dependency.
						*/
						bool all_externally_dependend_facts_can_be_reached = true;
						std::vector<const BoundedAtom*>* reachable_facts = new std::vector<const BoundedAtom*>();
						for (unsigned int i = 0; i < from_node.getAtoms().size(); i++)
						{
							if (!facts_with_external_dependencies[i])
							{
								reachable_facts->push_back((*supporting_facts)[i]);
								continue;
							}
							
							const BoundedAtom* from_supporting_fact = (*supporting_facts)[i];
							const BoundedAtom* equivalent_fact_to_reach = equivalent_dtg_node->getAtoms()[i];
							
							// Check if the fact from from_node can reach the fact in the equivalent dtg node.
							std::cout << "Can we reach: ";
							equivalent_fact_to_reach->print(std::cout, dtg_graph_->getBindings());
							std::cout << " from {";
							
							for (std::vector<std::vector<const BoundedAtom*>* >::const_iterator ci = supporing_facts_from_node->begin(); ci != supporing_facts_from_node->end(); ci++)
							{
								std::vector<const BoundedAtom*>* supporting_facts = *ci;
								if (supporting_facts->size() != from_node.getAtoms().size())
								{
									std::cout << "The supporting facts for the DTG node:" << std::endl;
									std::cout << from_node << ": " << std::endl;
									for (std::vector<const BoundedAtom*>::const_iterator ci = supporting_facts->begin(); ci != supporting_facts->end(); ci++)
									{
										(*ci)->print(std::cout, dtg_graph_->getBindings());
										std::cout << std::endl;
									}
									assert (false);
								}
								(**ci)[i]->print(std::cout, dtg_graph_->getBindings());
							}
							std::cout << "}?" << std::endl;
						
							const BoundedAtom& atom_to_reach = BoundedAtom::createBoundedAtom(equivalent_fact_to_reach->getAtom(), equivalent_fact_to_reach->getProperties(), dtg_graph_->getBindings());
							for (unsigned int j = 0; j < atom_to_reach.getAtom().getArity(); j++)
							{
								const Term* atom_to_reach_term = atom_to_reach.getAtom().getTerms()[j];
								const Term* to_node_term = equivalent_fact_to_reach->getAtom().getTerms()[j];
								const Term* from_node_term = from_supporting_fact->getAtom().getTerms()[j];
								
								assert (i < from_node.getAtoms().size());
								assert (j < largest_arity);
								
								// Check if this term is externally dependend, if it is we make it equal to the to node.
								if (dependend_term_mapping[i][j])
								{
//									std::cout << "The " << j << "th term has an external dependency. Make it equal to the to node." << std::endl;
//									atom_to_reach_term->print(std::cout, dtg_graph_->getBindings(), atom_to_reach.getId());
//									std::cout << " -> ";
//									to_node_term->print(std::cout, dtg_graph_->getBindings(), equivalent_fact_to_reach->getId());
//									std::cout << "." << std::endl;
									atom_to_reach_term->makeDomainEqualTo(atom_to_reach.getId(), to_node_term->getDomain(equivalent_fact_to_reach->getId(), dtg_graph_->getBindings()), dtg_graph_->getBindings());
								}
								// Else we make it equal to the from node.
								else
								{
//									std::cout << "The " << j << "th term has NO external dependencies. Make it equal to the from node." << std::endl;
//									atom_to_reach_term->print(std::cout, dtg_graph_->getBindings(), atom_to_reach.getId());
//									std::cout << " -> ";
//									from_node_term->print(std::cout, dtg_graph_->getBindings(), from_supporting_fact->getId());
//									std::cout << "." << std::endl;
									atom_to_reach_term->makeDomainEqualTo(atom_to_reach.getId(), from_node_term->getDomain(from_supporting_fact->getId(), dtg_graph_->getBindings()), dtg_graph_->getBindings());
								}
							}
							reachable_facts->push_back(&atom_to_reach);
							
							std::cout << "Atom to search for: ";
							atom_to_reach.print(std::cout, dtg_graph_->getBindings());
							std::cout << std::endl;
							
							// TODO: Very inefficient, in the future we will use object equivalence groups to handle this.
							bool has_been_achieved = false;
							for (std::vector<const BoundedAtom*>::const_iterator ci = established_facts.begin(); ci != established_facts.end(); ci++)
							{
								const BoundedAtom* reached_atom = *ci;
								if (dtg_graph_->getBindings().canUnifyBoundedAtoms(*reached_atom, atom_to_reach))
								{
									has_been_achieved = true;
									break;
								}
							}
							
							if (!has_been_achieved)
							{
								all_externally_dependend_facts_can_be_reached = false;
								break;
							}
						}
						
						if (all_externally_dependend_facts_can_be_reached)
						{
							std::cout << *equivalent_dtg_node << " can be reached from: " << std::endl;
							
							for (std::vector<const BoundedAtom*>::const_iterator ci = supporting_facts->begin(); ci != supporting_facts->end(); ci++)
							{
								const BoundedAtom* bounded_atom = *ci;
								std::cout << " * ";
								bounded_atom->print(std::cout, dtg_graph_->getBindings());
								std::cout << "." << std::endl;
							}
							
							// Add the new facts to the list! :)
							std::cout << "New bounded atoms to add:" << std::endl;
							for (std::vector<const BoundedAtom*>::const_iterator ci = reachable_facts->begin(); ci != reachable_facts->end(); ci++)
							{
								std::cout << "* ";
								(*ci)->print(std::cout, dtg_graph_->getBindings());
								std::cout << std::endl;
							}
							
							//supported_facts_[equivalent_dtg_node]->push_back(reachable_facts);
							makeReachable(*equivalent_dtg_node, *reachable_facts);
						}
					}
				}
			}
		}
		
	} while (pre_size != established_facts.size());
	
	std::cout << " -= All supported facts! :D =- " << std::endl;
	for (std::map<const DomainTransitionGraphNode*, std::vector<std::vector<const BoundedAtom*>* >* >::const_iterator ci = supported_facts_.begin(); ci != supported_facts_.end(); ci++)
	{
		const DomainTransitionGraphNode* dtg_node = (*ci).first;
		std::vector<std::vector<const BoundedAtom*>* >* supported_tupples = (*ci).second;
		
		std::cout << "The DTG node: ";
		dtg_node->print(std::cout);
		std::cout << " is supported by the following tupples:" << std::endl;
		
		for (std::vector<std::vector<const BoundedAtom*>* >::const_iterator ci = supported_tupples->begin(); ci != supported_tupples->end(); ci++)
		{
			std::vector<const BoundedAtom*>* tupple = *ci;
			std::cout << "{" << std::endl;
			for (std::vector<const BoundedAtom*>::const_iterator ci = tupple->begin(); ci != tupple->end(); ci++)
			{
				std::cout << "* ";
				(*ci)->print(std::cout, dtg_graph_->getBindings());
				std::cout << "." << std::endl;
			}
			std::cout << "}" << std::endl;
		}
	}
}


void DTGReachability::iterateThroughFixedPoint(std::vector<const BoundedAtom*>& established_facts, std::set<const Transition*>& achieved_transitions)
{
	std::cout << "Start new iteration." << std::endl;
	
	std::vector<const Transition*> open_list;
	
	std::vector<const DomainTransitionGraphNode*> initial_satisfied_nodes;
	
	// Find all the DTG nodes which are supported in the initial state. For each node we only need to find a single
	// instance of a set of objects which satisfies it.
	std::cout << "Find initial supported DTG nodes." << std::endl;
	for (std::vector<DomainTransitionGraphNode*>::const_iterator ci = dtg_graph_->getNodes().begin(); ci != dtg_graph_->getNodes().end(); ci++)
	{
		// Initialise the reachability structure(s) with the values from the initial state.
		const std::vector<BoundedAtom*>& atoms_to_achieve = (*ci)->getAtoms();
		std::vector<std::vector<const BoundedAtom*>* > supporting_tupples;
		std::map<const std::vector<const Object*>*, const std::vector<const Object*>* > variable_assignments;
		std::vector<const BoundedAtom*> initial_supporting_facts;
		getSupportingFacts(supporting_tupples, variable_assignments, atoms_to_achieve, initial_supporting_facts, established_facts);

		// Mark those transitions whose node have been 'filled' by the initial state.
		if (supporting_tupples.size() > 0)
		{
			std::cout << "Supported node: " << **ci << std::endl;
			//open_list.insert(open_list.end(), (*ci)->getTransitions().begin(), (*ci)->getTransitions().end());
			
			std::vector<std::vector<const BoundedAtom*>* >* supported_facts = supported_facts_[*ci];
			//supported_facts->insert(supported_facts->end(), supporting_tupples.begin(), supporting_tupples.end());
			supported_facts->push_back(*supporting_tupples.begin());
			initial_satisfied_nodes.push_back(*ci);
		}
		open_list.insert(open_list.end(), (*ci)->getTransitions().begin(), (*ci)->getTransitions().end());
	}
	
	// While there are transitions achieved:
	bool new_transition_achieved = true;
	while (new_transition_achieved)
	{
		new_transition_achieved = false;
		
		// Propagate the reachable nodes.
		propagateReachableNodes();

		// For each transition of a marked node:
		for (std::vector<const Transition*>::reverse_iterator ri = open_list.rbegin(); ri != open_list.rend(); ri++)
		{
			/// Check if the preconditions of the transition have been satisfied.
			const Transition* transition = *ri;
			
			if (achieved_transitions.count(transition) != 0)
				continue;
			
//			std::cout << " * Work on the transition: " << *transition << "." << std::endl;
			const DomainTransitionGraphNode& from_dtg_node = transition->getFromNode();
			
			// Instantiate the DTG node by assigning the terms to domains we have already determined to be reachable.
			std::vector<std::vector<const BoundedAtom*>* >* assignable_atoms = supported_facts_[&from_dtg_node];
			
			for (std::vector<std::vector<const BoundedAtom*>* >::const_iterator ci = assignable_atoms->begin(); ci != assignable_atoms->end(); ci++)
			{
				std::vector<const BoundedAtom*>* possible_assignment = *ci;
				
//				std::cout << "Possible assignments: ";
//				for (std::vector<const BoundedAtom*>::const_iterator ci = possible_assignment->begin(); ci != possible_assignment->end(); ci++)
//				{
//					const BoundedAtom* assignment = *ci;
//					assignment->print(std::cout, dtg_graph_->getBindings());
//					if (ci != possible_assignment->end() - 1)
//					{
//						std::cout << ", ";
//					}
//				}
//				std::cout << "." << std::endl;
				
				
				// Map the action variable's domain to a set of objects which supports the transition. The variable domains of the
				// action variables will match the fact in the DTG nodes which allows us to find a set of facts which satisfy the
				// action's precondition and take the effects as newly established facts.
				std::map<const std::vector<const Object*>*, const std::vector<const Object*>* > term_assignments;
				
				// Assign the predetermined assignments.
				for (std::vector<const BoundedAtom*>::iterator possible_assignment_ci = possible_assignment->begin(); possible_assignment_ci != possible_assignment->end(); possible_assignment_ci++)
				{
					const BoundedAtom* possible_atom_assignment = *possible_assignment_ci;
					const BoundedAtom* dtg_node_atom = from_dtg_node.getAtoms()[std::distance(possible_assignment->begin(), possible_assignment_ci)];
					
					for (std::vector<const Term*>::const_iterator ci = dtg_node_atom->getAtom().getTerms().begin(); ci != dtg_node_atom->getAtom().getTerms().end(); ci++)
					{
						const Term* dtg_node_atom_term = *ci;
						const Term* possible_atom_assignment_term = possible_atom_assignment->getAtom().getTerms()[std::distance(dtg_node_atom->getAtom().getTerms().begin(), ci)];
						
						const std::vector<const Object*>& dtg_node_atom_term_domain = dtg_node_atom_term->getDomain(dtg_node_atom->getId(), dtg_graph_->getBindings());
						const std::vector<const Object*>& possible_atom_assignment_term_domain = possible_atom_assignment_term->getDomain(possible_atom_assignment->getId(), dtg_graph_->getBindings());
						
						term_assignments[&dtg_node_atom_term_domain] = &possible_atom_assignment_term_domain;
					}
				}

				const std::vector<std::pair<const Atom*, InvariableIndex> >& preconditions = transition->getAllPreconditions();
				
				// Convert into bounded atoms for algorithm.
				std::vector<BoundedAtom*> bounded_preconditions;
				
				for (std::vector<std::pair<const Atom*, InvariableIndex> >::const_iterator ci = preconditions.begin(); ci != preconditions.end(); ci++)
				{
					const Atom* precondition = (*ci).first;
					bounded_preconditions.push_back(new BoundedAtom(transition->getStep()->getStepId(), *precondition));
				}
				
				std::vector<const BoundedAtom*> initial_supporting_facts;
				std::vector<std::vector<const BoundedAtom*>* >* supporting_tupples = new std::vector<std::vector<const BoundedAtom*>* >();
				getSupportingFacts(*supporting_tupples, term_assignments, bounded_preconditions, initial_supporting_facts, established_facts);
				
				// If tupple(s) of possible assignments have been found we assign these to the action variables and extract the facts which have been achieved.
				if (!supporting_tupples->empty())
				{
					achieved_transitions.insert(transition);
//					open_list.erase(ri.base() - 1);
					std::vector<const DomainTransitionGraphNode*>* reachable_nodes = reachable_nodes_[&from_dtg_node];
					if (std::find(reachable_nodes->begin(), reachable_nodes->end(), &transition->getToNode()) == reachable_nodes->end())
					{
						reachable_nodes_[&from_dtg_node]->push_back(&transition->getToNode());
					}
					
					assert (&transition->getToNode().getDTG() == dtg_graph_);
					
//					std::cout << "Add the node: " << transition->getToNode() << std::endl;
					
					new_transition_achieved = true;

//					std::cout << " ** Found supporting tupple(s)!" << std::endl;
					// For each tupple of supporting facts determine the domains of each of the action parameters and use these to determine the achieved facts.
					//for (std::vector<std::vector<const BoundedAtom*>* >::const_iterator ci = supporting_tupples->begin(); ci != supporting_tupples->end(); ci++)
					{
						// Bind each term of the action to the supporting atom's term domains.
						const std::vector<const Object*>* action_parameter_domains[transition->getStep()->getAction().getVariables().size()];
						for (unsigned int i = 0; i < transition->getStep()->getAction().getVariables().size(); i++)
						{
							action_parameter_domains[i] = NULL;
						}
						
						//const std::vector<const BoundedAtom*>* supporting_atoms = *ci;
						const std::vector<const BoundedAtom*>* supporting_atoms = *supporting_tupples->begin();
//								std::cout << "< ";
						for (std::vector<const BoundedAtom*>::const_iterator ci = supporting_atoms->begin(); ci != supporting_atoms->end(); ci++)
						{
							const BoundedAtom* supporting_bounded_atom = *ci;
//									supporting_bounded_atom->print(std::cout, initial_bindings);
							
							unsigned int precondition_index = std::distance(supporting_atoms->begin(), ci);
							const std::pair<const Atom*, InvariableIndex>& matching_precondition = preconditions[precondition_index];
							
							for (std::vector<const Variable*>::const_iterator ci = transition->getStep()->getAction().getVariables().begin(); ci != transition->getStep()->getAction().getVariables().end(); ci++)
							{
								const Variable* action_variable = *ci;
								const std::vector<const Object*>& action_variable_domain = action_variable->getDomain(transition->getStep()->getStepId(), dtg_graph_->getBindings());
								
								unsigned int action_variable_index = std::distance(transition->getStep()->getAction().getVariables().begin(), ci);
								
								// Map the supporting domains to the variables of the action.
								for (std::vector<const Term*>::const_iterator ci = matching_precondition.first->getTerms().begin(); ci != matching_precondition.first->getTerms().end(); ci++)
								{
									const Term* precondition_term = *ci;
									const std::vector<const Object*>& term_variable_domain = precondition_term->getDomain(transition->getStep()->getStepId(), dtg_graph_->getBindings());
									unsigned int term_index = std::distance(matching_precondition.first->getTerms().begin(), ci);

									if (&action_variable_domain == &term_variable_domain)
									{
										const std::vector<const Object*>& supporting_atom_variable_domain = supporting_bounded_atom->getAtom().getTerms()[term_index]->getDomain(supporting_bounded_atom->getId(), dtg_graph_->getBindings());
										
										// Debug, if we have already assigned a value to the action variable, make sure that the new one
										// is the same, otherwise something is wrong.
										if (action_parameter_domains[action_variable_index] != NULL)
										{
											bool domains_are_equal = true;
											if (action_parameter_domains[action_variable_index]->size() != supporting_atom_variable_domain.size())
												domains_are_equal = false;
											else
											{
												for (unsigned int i = 0; i < supporting_atom_variable_domain.size(); i++)
												{
													if ((*action_parameter_domains[action_variable_index])[i] != supporting_atom_variable_domain[i])
													{
														domains_are_equal = false;
														break;
													}
												}
											}
											
											if (!domains_are_equal)
											{
												std::cout << "Replace: { ";
												for (std::vector<const Object*>::const_iterator ci = action_parameter_domains[action_variable_index]->begin(); ci != action_parameter_domains[action_variable_index]->end(); ci++)
												{
													assert (*ci != NULL);
													(*ci)->print(std::cout, dtg_graph_->getBindings(), transition->getStep()->getStepId());
													if (ci + 1 != action_parameter_domains[action_variable_index]->end())
													{
														std::cout << ", ";
													}
												}
												std::cout << " with ";
												for (std::vector<const Object*>::const_iterator ci = supporting_atom_variable_domain.begin(); ci != supporting_atom_variable_domain.end(); ci++)
												{
													(*ci)->print(std::cout, dtg_graph_->getBindings(), supporting_bounded_atom->getId());
													if (ci + 1 != supporting_atom_variable_domain.end())
													{
														std::cout << ", ";
													}
												}
												std::cout << std::endl;
												assert (false);
											}
										}
										else
										{
											action_parameter_domains[action_variable_index] = &supporting_atom_variable_domain;
										}
									}
								}
							}
//									std::cout << ", ";
						}
//								std::cout << ">" << std::endl;
						
						// Add the achieved nodes to the established facts.
						const DomainTransitionGraphNode& to_node = transition->getToNode();
						std::vector<const BoundedAtom*>* to_node_achievers = new std::vector<const BoundedAtom*>();
						for (std::vector<BoundedAtom*>::const_iterator ci = to_node.getAtoms().begin(); ci != to_node.getAtoms().end(); ci++)
						{
							BoundedAtom* to_node_bounded_atom = *ci;
							std::vector<const Term*>* new_atom_terms = new std::vector<const Term*>();
							std::vector<const std::vector<const Object*>* > new_atom_domains;
							
//							std::cout << "!!! Achieved the following TO Node: (" << to_node_bounded_atom->getAtom().getPredicate().getName() << " ";

							// Bind the terms of the to node to the action variables to get the achieved facts.
							bool valid_assignments = true;
							for (std::vector<const Term*>::const_iterator ci = to_node_bounded_atom->getAtom().getTerms().begin(); ci != to_node_bounded_atom->getAtom().getTerms().end(); ci++)
							{
								const Term* to_node_term = *ci;
								new_atom_terms->push_back(to_node_term);
								const std::vector<const Object*>& to_node_term_domain = to_node_term->getDomain(to_node_bounded_atom->getId(), dtg_graph_->getBindings());
//										assert (to_node_term_domain.size() == 1);
								
								bool is_bounded = false;
								
								for (unsigned int i = 0; i < transition->getStep()->getAction().getVariables().size(); i++)
								{
									const std::vector<const Object*>& action_variable_domain = transition->getStep()->getAction().getVariables()[i]->getDomain(transition->getStep()->getStepId(), dtg_graph_->getBindings());
									if (&to_node_term_domain == &action_variable_domain)
									{
//												if (&to_node_term_domain == invariable_domain)
//												{
//													std::cout << "!%$";
//												}
										
										const std::vector<const Object*>* matching_variable_domain = action_parameter_domains[i];

										// If no matching variable domain has been found, all values are possible.
										if (matching_variable_domain == NULL)
										{
//												std::cout << "Not bound variable domain: ";
//												to_node_term->print(std::cout, dtg_graph_->getBindings(), to_node_bounded_atom->getId());
//												std::cout << "." << std::endl;
											matching_variable_domain  = &to_node_term->getDomain(to_node_bounded_atom->getId(), dtg_graph_->getBindings());
											//std::vector<const DomainTransitionGraphNode*> new_nodes
										}
										
										assert (matching_variable_domain != NULL);
										//assert (matching_variable_domain->size() == 1);
										new_atom_domains.push_back(matching_variable_domain);
//										for (std::vector<const Object*>::const_iterator ci = matching_variable_domain->begin(); ci != matching_variable_domain->end(); ci++)
//										{
//											std::cout << **ci << ", ";
//										}
										assert (matching_variable_domain->size() > 0);
										is_bounded = true;
										break;
									}
								}
								
								if (!is_bounded)
								{
//									std::cout << "The " << std::distance(to_node_bounded_atom->getAtom().getTerms().begin(), ci) << "th term is not bounded! :(((" << std::endl;
									valid_assignments = false;
									break;
								}
							}
							if (!valid_assignments)
							{
								//continue;
								break;
							}
//									std::cout << "|";
							
							Atom* achieved_fact = new Atom(to_node_bounded_atom->getAtom().getPredicate(), *new_atom_terms, to_node_bounded_atom->getAtom().isNegative());
							BoundedAtom& achieved_bounded_atom = BoundedAtom::createBoundedAtom(*achieved_fact, dtg_graph_->getBindings());
							
							assert (achieved_fact->getArity() == achieved_fact->getPredicate().getArity());
							assert (new_atom_domains.size() == achieved_fact->getPredicate().getArity());
							assert (new_atom_terms->size() == achieved_fact->getPredicate().getArity());
							assert (achieved_fact->getTerms().size() == achieved_fact->getPredicate().getArity());
							
							// Bound the achieved fact to the supporting domains.
							for (unsigned int i = 0; i < achieved_fact->getArity(); i++)
							{
	//							std::cout << "Make ";
	//							achieved_fact->getTerms()[i]->print(std::cout, initial_bindings, achieved_bounded_atom.getId());
	//							std::cout << " equal to ";
								
//										for (std::vector<const Object*>::const_iterator ci = new_atom_domains[i]->begin(); ci != new_atom_domains[i]->end(); ci++)
//										{
//											std::cout << **ci << ", ";
//										}
//										std::cout << std::endl;
								achieved_fact->getTerms()[i]->makeDomainEqualTo(achieved_bounded_atom.getId(), *new_atom_domains[i], dtg_graph_->getBindings());
								assert (achieved_fact->getTerms()[i]->getDomain(achieved_bounded_atom.getId(), dtg_graph_->getBindings()).size() > 0);
							}
							
							bool present = false;
							for (std::vector<const BoundedAtom*>::const_iterator ci = established_facts.begin(); ci != established_facts.end(); ci++)
							{
								const BoundedAtom* bounded_atom = *ci;
								if (dtg_graph_->getBindings().canUnify(bounded_atom->getAtom(), bounded_atom->getId(), achieved_bounded_atom.getAtom(), achieved_bounded_atom.getId()))
								{
									bool terms_match = true;
									for (std::vector<const Term*>::const_iterator ci = bounded_atom->getAtom().getTerms().begin(); ci != bounded_atom->getAtom().getTerms().end(); ci++)
									{
										const Term* established_fact_term = *ci;
										const Term* newly_achieved_fact_term = achieved_fact->getTerms()[std::distance(bounded_atom->getAtom().getTerms().begin(), ci)];
										
										if (!established_fact_term->isEquivalentTo(bounded_atom->getId(), *newly_achieved_fact_term, achieved_bounded_atom.getId(), dtg_graph_->getBindings()))
										{
											terms_match = false;
											break;
										}
									}
									
									if (!terms_match)
										continue;
									
									present = true;
									
//									std::cout << "ALREADY ACHIEVED -=(";
//									achieved_bounded_atom.print(std::cout, dtg_graph_->getBindings());
//									std::cout << ")=-" << std::endl;
									break;
								}
							}

							if (!present)
							{
								established_facts.push_back(&achieved_bounded_atom);
//								std::cout << "-=(";
//								achieved_bounded_atom.print(std::cout, dtg_graph_->getBindings());
//								std::cout << ")=-" << std::endl;
							}
							to_node_achievers->push_back(&achieved_bounded_atom);
						}
						
						if (to_node.getAtoms().size() != to_node_achievers->size())
						{
							continue;
/*							std::cout << "Found the following achievers for: " << to_node << ":" << std::endl;
							
							for (std::vector<const BoundedAtom*>::const_iterator ci = to_node_achievers->begin(); ci != to_node_achievers->end(); ci++)
							{
								std::cout << " * ";
								(*ci)->print(std::cout, dtg_graph_->getBindings());
								std::cout << "." << std::endl;
							}
							assert (false);*/
						}
						
						//supported_facts_[&to_node]->push_back(to_node_achievers);
						makeReachable(to_node, *to_node_achievers);
//						std::cout << "." << std::endl;
					}
				}
			}
			
			
			
			/// If so mark the transition as "achieved".
			
			/// Add to the from node of that transition the to node - as it is achievable from there.
			
			/// Mark the node of the end point of the transition - but only if it contains unachieved transitions.
		}
		
		// Propagate the achievable nodes per DTG node.
	}

	// List for each DTG node which other nodes are reachable.
	for (std::vector<DomainTransitionGraphNode*>::const_iterator ci = dtg_graph_->getNodes().begin(); ci != dtg_graph_->getNodes().end(); ci++)
	{
		const DomainTransitionGraphNode* dtg_node = *ci;
		std::cout << "Reachable nodes from: ";
		dtg_node->print(std::cout);
		std::cout << ":" << std::endl;
		
		std::vector<const DomainTransitionGraphNode*>* reachable_dtg_node = reachable_nodes_[dtg_node];
		for (std::vector<const DomainTransitionGraphNode*>::const_iterator ci = reachable_dtg_node->begin(); ci != reachable_dtg_node->end(); ci++)
		{
			std::cout << "* ";
			(*ci)->print(std::cout);
			std::cout << "." << std::endl;
		}
	}

	// List all nodes which are reachable.
	std::cout << "List of all achievable facts: " << std::endl;
	for (std::vector<const BoundedAtom*>::const_iterator ci  = established_facts.begin(); ci != established_facts.end(); ci++)
	{
		std::cout << "- ";
		(*ci)->print(std::cout, dtg_graph_->getBindings());
		std::cout << std::endl;
	}
}


void DTGReachability::getSupportingFacts(std::vector<std::vector<const BoundedAtom*>* >& supporting_tupples, const std::map<const std::vector<const Object*>*, const std::vector<const Object*>* >& variable_assignments, const std::vector<BoundedAtom*>& atoms_to_achieve, const std::vector<const BoundedAtom*>& initial_supporting_facts, const std::vector<const BoundedAtom*>& initial_facts) const
{
	assert (atoms_to_achieve.size() > initial_supporting_facts.size());
	const BoundedAtom* atom_to_process = atoms_to_achieve[initial_supporting_facts.size()];
	
//	std::cout << "[" << initial_supporting_facts.size() << "] The atom to achieve: ";
//	atom_to_process->print(std::cout, dtg_graph_->getBindings());
//	std::cout << std::endl;

	for (std::vector<const BoundedAtom*>::const_iterator ci = initial_facts.begin(); ci != initial_facts.end(); ci++)
	{
		StepID initial_fact_id = (*ci)->getId();
		const Atom& initial_fact = (*ci)->getAtom();
		
		if (dtg_graph_->getBindings().canUnify(initial_fact, initial_fact_id, atom_to_process->getAtom(), atom_to_process->getId()))
		{
//			std::cout << "Initial fact which can unify: ";
//			initial_fact.print(std::cout, dtg_graph_->getBindings(), initial_fact_id);
//			std::cout << std::endl;

			// Check if all terms can be supported.
			bool terms_supported = true;
			std::map<const std::vector<const Object*>*, const std::vector<const Object*>* >* variable_assignments_clone = new std::map<const std::vector<const Object*>*, const std::vector<const Object*>* >(variable_assignments);
			
			for (std::vector<const Term*>::const_iterator ci = atom_to_process->getAtom().getTerms().begin(); ci != atom_to_process->getAtom().getTerms().end(); ci++)
			{
				const Term* atom_term = *ci;
				unsigned int term_index = std::distance(atom_to_process->getAtom().getTerms().begin(), ci);
				
				const std::vector<const Object*>& term_domain_atom_to_process = atom_term->getDomain(atom_to_process->getId(), dtg_graph_->getBindings());
				const std::vector<const Object*>& initial_fact_domain = initial_fact.getTerms()[term_index]->getDomain(initial_fact_id, dtg_graph_->getBindings());

				// Find the assignments made to the term's domain.
				std::map<const std::vector<const Object*>*, const std::vector<const Object*>* >::const_iterator found_domain = variable_assignments_clone->find(&term_domain_atom_to_process);
				
				// If no assignments have been made yet we make them equal to the initial fact's domain.
				if (found_domain == variable_assignments_clone->end())
				{
					(*variable_assignments_clone)[&term_domain_atom_to_process] = &initial_fact_domain;
/*					std::cout << "Bind the " << term_index << "th term to: ";
					for (std::vector<const Object*>::const_iterator ci = initial_fact_domain.begin(); ci != initial_fact_domain.end(); ci++)
					{
						(*ci)->print(std::cout, dtg_graph_->getBindings(), initial_fact_id);
						if (ci + 1 != initial_fact_domain.end())
						{
							std::cout << ", ";
						}
					}
					std::cout << "." << std::endl;*/
				}
				// If previous assignments have been made, we take the intersection between the previous assignments and the fact we found
				// to be unifiable with this fact.
				else
				{
					std::vector<const Object*> existing_domain = *(*variable_assignments_clone)[&term_domain_atom_to_process];
					
					// Get the intersection of the variable assignments made and the new assignment just made.
					std::vector<const Object*> initial_fact_domain_sorted_copy(initial_fact_domain);
					std::sort(initial_fact_domain_sorted_copy.begin(), initial_fact_domain_sorted_copy.end());
					std::sort(existing_domain.begin(), existing_domain.end());
					std::vector<const Object*>* intersection = new std::vector<const Object*>(std::max(initial_fact_domain_sorted_copy.size(), existing_domain.size()));
					std::vector<const Object*>::iterator intersection_end = std::set_intersection(initial_fact_domain_sorted_copy.begin(), initial_fact_domain_sorted_copy.end(), existing_domain.begin(), existing_domain.end(), intersection->begin());
					
					// If the intersection is empty we know that the term cannot be supported.
					if (intersection_end == intersection->begin())
					{
						terms_supported = false;
						break;
					}
					
					// Otherwise, we need to update the variable domain which has been modified.
					intersection->resize(std::distance(intersection->begin(), intersection_end));
					(*variable_assignments_clone)[&term_domain_atom_to_process] = intersection;
				}
			}
			
			if (!terms_supported)
			{
				continue;
			}
			
			// Construct the facts which support the preconditions.
			std::vector<const BoundedAtom*>* initial_supporting_facts_clone = new std::vector<const BoundedAtom*>(initial_supporting_facts);
			initial_supporting_facts_clone->push_back(new BoundedAtom(initial_fact_id, initial_fact));
			
			if (initial_supporting_facts_clone->size() == atoms_to_achieve.size())
			{
				std::vector<const BoundedAtom*>* finalized_supporting_facts = new std::vector<const BoundedAtom*>();
				
//				std::cout << "The following nodes support the DTG node!" << std::endl;
				for (std::vector<BoundedAtom*>::const_iterator ci = atoms_to_achieve.begin(); ci != atoms_to_achieve.end(); ci++)
				{
					const BoundedAtom* atom_to_achieve = *ci;
					const BoundedAtom& new_bounded_atom = BoundedAtom::createBoundedAtom(atom_to_achieve->getAtom(), atom_to_achieve->getProperties(), dtg_graph_->getBindings());
					
					finalized_supporting_facts->push_back(&new_bounded_atom);
					
//					std::cout << " * (" << atom_to_achieve->getAtom().getPredicate().getName();
					for (std::vector<const Term*>::const_iterator ci = atom_to_achieve->getAtom().getTerms().begin(); ci != atom_to_achieve->getAtom().getTerms().end(); ci++)
					{
						const Term* term_of_atom_to_achieve = *ci;
						unsigned int term_index = std::distance(atom_to_achieve->getAtom().getTerms().begin(), ci);
						const Term* new_bounded_atom_term = new_bounded_atom.getAtom().getTerms()[term_index];
						
						const std::vector<const Object*>& variable_domain_of_atom_to_achieve = term_of_atom_to_achieve->getDomain(atom_to_achieve->getId(), dtg_graph_->getBindings());
						const std::vector<const Object*>* possible_assignments = (*variable_assignments_clone)[&variable_domain_of_atom_to_achieve];

						new_bounded_atom_term->makeDomainEqualTo(new_bounded_atom.getId(), *possible_assignments, dtg_graph_->getBindings());
					}
				}
				

				supporting_tupples.push_back(finalized_supporting_facts);
			}
			else
			{
				getSupportingFacts(supporting_tupples, *variable_assignments_clone, atoms_to_achieve, *initial_supporting_facts_clone, initial_facts);
			}
		}
	}
}

};
	
};
