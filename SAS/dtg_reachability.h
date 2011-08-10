#ifndef MYPOP_SAS_PLUS_DTG_REACHABLITY_H
#define MYPOP_SAS_PLUS_DTG_REACHABLITY_H

#include <map>
#include <vector>
#include <iosfwd>
#include <set>

namespace MyPOP {
	
class Object;
class Predicate;
class TermManager;
	
namespace SAS_Plus {

class PropertySpace;
class EOGFact;

class BoundedAtom;
class DomainTransitionGraph;
class DomainTransitionGraphNode;
class DTGReachability;
class Transition;

/**
 * Equivalent objects are object for which the following property holds:
 * If two equivalent objects A and B both can reach the same DTG node then all transitions which can be
 * applied to A can also be applied to B. This does not mean that all objects which belong to the same
 * equivalent object group can reach the same DTG nodes, this is only the case if the initial location
 * of A is reachable by B and vice versa.
 *
 * If an object A reaches the initial location of B we merge the equivalent object group of B with that
 * of A. If A can reach its initial DTG nodes than A and B are equivalent, but until that is proven B is
 * a sub set of A.
 *
 * Note: This is not implemented like this at the moment. Only when two objects are truly equivalent will
 * they become part of the the same EOG. Otherwise we will not be able to differentiate between two objects
 * which are part of the same EOG as the facts they can reach are dependable on its the initial state. Something
 * to do later :).
 */
class EquivalentObjectGroup
{
public:
	EquivalentObjectGroup(const Object& object, const DomainTransitionGraph& dtg_graph);
	
	EquivalentObjectGroup(const Object& object, const DomainTransitionGraph& dtg_graph, std::vector<const DomainTransitionGraphNode*>& initial_dtgs);
	
	/**
	 * Add an object and initial DTG node to this object group.
	 */
	bool addInitialDTGNodeMapping(const Object& object, const DomainTransitionGraphNode& dtg_node);
	
	void updateReachableFacts(const Object& object, const DomainTransitionGraphNode& dtg_node);
	
	/**
	 * Try to merge the given objectGroup with this group. If the merge can take place, the other object place is merged with
	 * this one. We can merge two groups if the initial DTG node of this group is reachable from the initial DTG node of the other
	 * group and visa versa, and - in addition - if the types of the objects are the same.
	 * @param objectGroup The object group which we try to merge with this node.
	 * @param reachable_nodes Reachability mapping from all DTG nodes.
	 * @return True if the groups could be merged, false otherwise.
	 */
	bool tryToMergeWith(EquivalentObjectGroup& object_group, const std::map<const DomainTransitionGraphNode*, std::vector<const DomainTransitionGraphNode*>* >& reachable_nodes);
	
private:
	
	/**
	 * As equivalent object groups are merged the merged node will become a child node of the node it got merged into. Internally
	 * we store this relationship which means that EOGs do not need to be deleted and any calls to the methods will automatically
	 * be redirected to the root node.
	 * @return The root node of this EOG.
	 */
	EquivalentObjectGroup& getRootNode();
	
	std::map<const Object*, std::vector<const DomainTransitionGraphNode*> *> initial_mapping_;
	
	// All the facts which are reachable by all objects in this domain.
	std::vector<const EOGFact*> reachable_lifted_facts_;
	
	const DomainTransitionGraph* dtg_graph_;

	// If the EOG is in use link_ is equal to NULL. Once it is made obsolete due to being merged with
	// another Equivalent Object Group link will link to that object instead.
	EquivalentObjectGroup* link_;
	
	/**
	 * Every equivalent object group has a finger print which correlates to the terms of the facts in the DTG nodes
	 * the object can be a part of. At the mommnt we do not consider sub / super sets yet.
	 */
	void initialiseFingerPrint(const Object& object, const DomainTransitionGraph& dtg_graph);
	
	/**
	 * Merge the given group with this group.
	 */
	void merge(EquivalentObjectGroup& other_group);
	
	bool* finger_print_;
	unsigned int finger_print_size_;

	friend std::ostream& operator<<(std::ostream& os, const EquivalentObjectGroup& group);
};

std::ostream& operator<<(std::ostream& os, const EquivalentObjectGroup& group);

/**
 * Manager the individual objects groups.
 */
class EquivalentObjectGroupManager
{
public:
	/**
	 * Initialise the individual groups.
	 */
	EquivalentObjectGroupManager(const DTGReachability& dtg_reachability, const DomainTransitionGraph& dtg_graph, const TermManager& term_manager, const std::vector<const BoundedAtom*>& initial_facts);
	
	void updateEquivalences(const std::map<const DomainTransitionGraphNode*, std::vector<const DomainTransitionGraphNode*>* >& reachable_nodes_);
	
	void print(std::ostream& os) const;
	
private:
	
	/**
	 * Merge two equivalent groups and declare them identical.
	 */
	void merge(const Object& object1, const Object& object2);

	std::map<const Object*, EquivalentObjectGroup*> object_to_equivalent_group_mapping_;
	std::vector<EquivalentObjectGroup*> equivalent_groups_;
	
	const DTGReachability* dtg_reachability_;
	
	const DomainTransitionGraph* dtg_graph_;
};

/**
 * Basic fact used in reachability. This represents a lifted fact where the objects involved are handled by
 * the EquivalentObjectGroup attached. This means that we deal with sets of equivalent objects rather than 
 * with individual objects.
 */
class EOGFact
{
public:
	EOGFact(const Predicate& predicate, const std::vector<const EquivalentObjectGroup*>& terms);
	
	bool canUnify(const BoundedAtom& bounded_atom) const;
	
private:
	const Predicate* predicate_;
	
	const std::vector<const EquivalentObjectGroupManager*> terms_;
};
	
/**
 * Utility class to perform relaxed reachability analysis on a given DTG.
 */
class DTGReachability
{
public:
	/**
	 * Constructor.
	 */
	DTGReachability(const DomainTransitionGraph& dtg_graph);
	
	void performReachabilityAnalsysis(const std::vector<const BoundedAtom*>& initial_facts, const TermManager& term_manager);

	/** 
	 * Find all possible supports for @ref(atoms_to_achieve) from all the facts in @ref(initial_facts). Whilst working
	 * though this list all variable assignments are recorded in @ref(variable_assignments), all facts choosen for supporting the facts
	 * are stored in @ref(initial_supporting_facts). Each full valid assignment is stored in @ref(supporting_tupples).
	 * @param supporting_tupples All found sets which can be unified with all the items of @ref(atoms_to_achieve)
	 * are inserted in this vector.
	 * @param variable_assignments Maps variable domains to a set of objects which has been assigned to that domain. As the
	 * algorithm works through all the facts to be achieved it stores the assignments made so far and if an assignment
	 * cannot be made - there is a conflict - the algorithm will backtrack and try other assignments until it finds one
	 * which supports all the facts in @ref(atoms_to_achieve). This assignment is then added to @ref(supporting_tupples).
	 * @param atoms_to_achieve The set of facts we want to achieve.
	 * @param initial_supporting_facts Set of facts which support the atoms to achieve. This list will 
	 * progressively be filled with supporting facts. The size of this list determines which fact from
	 * @ref(atoms_to_achieve) to work on next (the initial_supporting_facts.size()'th fact to be precise).
	 * @param initial_facts List of facts which we know to be true. From this set the supporting facts will
	 * be drawn.
	 */
	void getSupportingFacts(std::vector<std::vector<const BoundedAtom*>* >& supporting_tupples, const std::map<const std::vector<const Object*>*, const std::vector<const Object*>* >& variable_assignments, const std::vector<BoundedAtom*>& atoms_to_achieve, const std::vector<const BoundedAtom*>& initial_supporting_facts, const std::vector<const BoundedAtom*>& initial_facts) const;
	
private:
	
	void iterateThroughFixedPoint(std::vector<const BoundedAtom*>& established_facts, std::set<const Transition*>& achieved_transitions);
	
	/**
	 * After every iteration the reachable nodes are propagated through the graph.
	 */
	void propagateReachableNodes();
	
	/**
	 * This method is called every time a DTG node is reachable from another node. It effectively makes
	 * the from node a subset of the to node.
	 * @param dtg_node The DTG node for which we want to add a new set of supporting facts.
	 * @param reachable_facts The reachable facts.
	 */
	bool makeReachable(const DomainTransitionGraphNode& dtg_node, std::vector<const BoundedAtom*>& reachable_facts);
	
	void handleExternalDependencies(std::vector<const BoundedAtom*>& established_facts);
	
	/**
	 * The combined DTG graph we are working on.
	 */
	const DomainTransitionGraph* dtg_graph_;
	
	/**
	 * Record for every DTG node which facts support it.
	 */
	std::map<const DomainTransitionGraphNode*, std::vector<std::vector<const BoundedAtom*>* >* > supported_facts_;
	
	/**
	 * Per node we record which nodes are reachable from it.
	 */
	std::map<const DomainTransitionGraphNode*, std::vector<const DomainTransitionGraphNode*>* > reachable_nodes_;
	
	EquivalentObjectGroupManager* equivalent_object_manager_;
};

};

};

#endif // MYPOP_SAS_PLUS_DTG_REACHABLITY_H