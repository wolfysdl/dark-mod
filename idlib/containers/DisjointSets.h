#pragma once

/*
 * stgatilov: "disjoint sets" is a simple and convenient data structure
 * for tracking connected components in graph (while inserting edges into it).
 *
 * Every component has one element called "head".
 * The structure allows to quickly find head of any element (and hence check if two elements are connected),
 * and to quickly merge two components together.
 *
 * Only path compression heuristic is used (no ranks).
 * See more: 
 *   https://en.wikipedia.org/wiki/Disjoint-set_data_structure
 */
namespace idDisjointSets {
	//initialize new structure in given list
	//with elements numbered [0...num-1] and no connections
	void Init(idList<int> &arr, int num);

	//find head of component which element number v belongs to
	int GetHead(idList<int> &arr, int v);

	//merges two components with elements numbered u and v
	//returns true if they were separate, and false if they were connected
	bool Merge(idList<int> &arr, int u, int v);

	//makes sure arr[i] points to the head of i-th set
	void CompressAll(idList<int> &arr);

	//enumerate components with integers [0..k-1]
	//and set each arr[i] to number of its component
	//note: arr will no longer be "disjoint sets" structure!
	int ConvertToColors(idList<int> &arr);
};