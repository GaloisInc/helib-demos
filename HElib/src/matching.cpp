#include "matching.h"
#include <iostream>

static long augmenting_path(vector<long>& p, FlowGraph& g, long src, long sink);
// Run BFS to find a shortest augmenting path

void printFlow(FlowGraph& fg)
{
  cout << "Flow graph in format from->to: flow(capacity):\n";
  for (long i=0; i<(long)fg.size(); i++)
    for (FNeighborList::iterator it=fg[i].begin(); it!=fg[i].end(); ++it) {
      long j = it->first; // found an edge i->j
      if (it->second.capacity <= 0) continue; // ignore dummy edges
      cout <<"  "<< i <<"->"<< j <<": "<< it->second.flow 
	   << "("<< it->second.capacity<<")\n";
    }
  cout << endl;
}

void BipartitleGraph::printout()
{
  cout << "Bipartite graph in format left->right: label, color\n";
  for (long i=0; i<(long)left.size(); i++) {
    for (LNeighborList::iterator it=left[i].neighbors.begin(); 
	 it!=left[i].neighbors.end(); ++it) {
      cout <<"  "<< i+1 <<"->"<< it->first+1 <<": "
	   << it->second.label <<", "<< it->second.color << endl;
    }
    cout << endl;
  }
}

// Partition the graph into maximum matchings, color by i the edges of the
// i'th matching (i=1,2,...).
void BipartitleGraph::partitionToMatchings()
{
  // Make sure that all the edges are colored 0
  for (long i=0; i<(long)left.size(); i++) {
    for (LNeighborList::iterator it=left[i].neighbors.begin(); 
	 it!=left[i].neighbors.end(); ++it)
      it->second.color = 0;
  }

  // Build the flow graph for this bipartite graph
  FlowGraph fg;
  buildFlowGraph(fg);
  long sink = fg.size()-1;

  // keep finding maximum matching until none exist
  long leftSize = left.size();
  for (long color = 1; maximum_flow(fg,0,sink)>0; color++) {

    //    cout << color << "th "; printFlow(fg);

    // Remove the flow and reduce the capacity of the flow edges accordingly
    // and also color the bipartite edges corresponding to flow-carrying edges.
    for (long i=0; i<leftSize; i++) {
      vector<long> edges2remove;
      // left node i has index i+1 in the flow graph
      FNeighborList& v = fg[i+1];
      // Go over the edges of v, look for flow-carrying edges
      for (FNeighborList::iterator it=v.begin(); it!=v.end(); ++it) {
	long flow = it->second.flow;
	if (flow<=0) continue; // no flow here

	// Remove the flow and decrease the capacity
	it->second.flow = 0;
	it->second.capacity -= flow;
	if (it->second.capacity == 0)
	  edges2remove.push_back(it->first); // mark for later removal

	// Compute the index of right neighbor in the bipartite graph
	long j = it->first -leftSize -1;

        // Get all the edges i->j in the bipartite graph, and color
        // the first #flow of them that are still uncolored
        pair<LNeighborList::iterator,LNeighborList::iterator> range
	  = left[i].neighbors.equal_range(j);
	while (range.first!=range.second && flow>0) {
	  LabeledEdge& e = range.first->second; // the joys of C++ STL
	  if (e.color == 0) { // an uncolored edge, color it
	    e.color = color;
	    --flow; // one less edge to color
	  }
	  ++(range.first); // next edge in range
	}
	assert (flow==0); // sanity check, we must have enough edges to color
      }
      // Remove flow edges that are marked for removal
      for (long ii=0; ii<(long)edges2remove.size(); ii++) {
	long j = edges2remove[ii]; // neighbor index in the flow graph
	v.erase(j);
	// also erase the reverse direction of this edge from the flow graph
	fg[j].erase(ii+1);
      }
    }
    // Remove the flow from the source and sink edges
    for (FNeighborList::iterator it=fg[0].begin(); it!=fg[0].end(); ++it)
      it->second.flow = 0;
    for (long i=leftSize+1; i<sink; i++)
      fg[i][sink].flow = 0;
  }
}


// Generate a flow graph with capacity-1 edges, put edges from the sink to all
// the left side, all the bipartite edges from left to right, and edges from
// all the right side to the sink. The bipartite graph can have parallel edges,
// and they will all be replaced by a single flow edge with the corresponding
// capacity.
void BipartitleGraph::buildFlowGraph(FlowGraph& fg)
{
  long leftSize=left.size();
  long rightSize=0; // will compute this later

  // Initialize with only the left size, we will find the right size later
  fg.assign(leftSize+1,FNeighborList()); // Ensure that we start from scratch

  // Put a neighbor-list for the sink
  for (long i=0; i<leftSize; i++) // left-nodes have indexes 1,2,...,leftSize
    fg[0].insert(pair<long,FlowEdge>(i+1,FlowEdge(1))); // 1-capacity edges

  // Put the neighbor lists of the left-side nodes
  for (long i=0; i<leftSize; i++) {
    // Go over the right-neighbors in the bipartite graph and add them to the
    // flow graph. The index of right-node i in the flow graph is leftSize+i+1
    for (LNeighborList::iterator it=left[i].neighbors.begin(); 
	 it!=left[i].neighbors.end(); ++it) {
      if (rightSize <= it->second.to)
	rightSize = it->second.to +1; // Compute size of right side as you go

      // The index of the i'th left node in the flow graph is i+1, and the
      // index of the j'th right node in the flow graph is leftSize +j +1.
      long j = leftSize + it->second.to +1;
      FNeighborList::iterator fEdge = fg[i+1].find(j);
      if (fEdge == fg[i+1].end()) { // no such edge in the flow graph, add it
	fg[i+1].insert(pair<long,FlowEdge>(j,FlowEdge(1))); // 1-capacity edge
      }
      else // an edge exists already, increase its capacity
	fEdge->second.capacity++;
    }
  }

  // Put the neighbor lists of the right-side nodes and the sink
  long sink = leftSize +rightSize +1; // Index of the sink in flow graph
  fg.resize(sink+1);
  for (long i=0; i<rightSize; i++) {
    // The index of this node in the flow graph is leftSize +i +1
    fg[leftSize+i+1].insert(pair<long,FlowEdge>(sink,FlowEdge(1)));
  }
}

/* Use the Edmonds-Karp max-flow algorithm */
long maximum_flow(FlowGraph& fg, long src, long sink)
{
  long flowVal = 0;

  // Make sure that if fg[i][j] is defined then so is fg[j][i]
  FlowEdge dummyEdge; // 0-capacity, 0-flow edge
  for (long i=0; i<(long)fg.size(); i++) {
    vector<long> missing;
    for (FNeighborList::iterator it=fg[i].begin(); it!=fg[i].end(); ++it) {
      long j = it->first; // found an endge i->j, look for edge j->i

      if (j >= (long)fg.size())   //ensure that we have a node j in the graph
        fg.resize(j+1,FNeighborList()); // add a node with empty neighbor list

      if (fg[j].find(i) == fg[j].end()) // i not found in j'th neighbor list
	missing.push_back(j);           // mark the edge j->i as missing
    }
    // Insert all the missing edges that point to node i
    for (long j=0; j<(long)missing.size(); j++) {
      long k=missing[j];
      fg[k].insert(pair<long,FlowEdge>(i,dummyEdge)); // insert dummy edge
      // Hopefully dummyEdge is copied, not referenced (else it's a bug)
    }
  }

  // The Ford-Fulkerson/Edmonds-Karp/Dinic algorithm itself
  while (true) {
    // Find an augmenting path in the network
    vector<long> path;
    long more = augmenting_path(path, fg, src, sink);
    if (more==0) break; // no more flow can be added

    // Add the augmenting path to the flow
    long next = sink;
    while (next != src) {
      long prev = path[next]; // path is pointing back from sink to source

      // Ensure that fg[prev][next] and fg[next][prev] exist
      assert(fg[prev].find(next) != fg[prev].end());
      assert(fg[next].find(prev) != fg[next].end());

      FlowEdge& back = fg[next][prev];
      FlowEdge& forward = fg[prev][next];
      if (back.flow >= more) back.flow -= more; // only reduce backward flow
      else {                                    // also increase forward flow
	forward.flow += more - back.flow;
	back.flow = 0;
      }
      next = prev;
    }
    flowVal += more;
  }

  /**
  // Remove from the graph all the flow-zero edges
  for (long i=0; i<(long)fg.size(); i++) {
    FNeighborList::iterator it1=fg[i].begin();
    do {
      FNeighborList::iterator it2 = it1;
      it1++; // increment the iterator before potentially erasing the edge
      if (it2->second.flow == 0) fg[i].erase(it2);
    } while (it1 != fg[i].end());
  }
  ************/
  return flowVal;
}

/* The function augmenting_path performs a BFS on the netwrok 
 * to find a shortest sugmenting path                        
 */
static long augmenting_path(vector<long>& path, FlowGraph& fg,
			    long src, long sink)
{
  assert(src >=0 && src <(long)fg.size());
  assert(sink>=0 && sink<(long)fg.size());

  // initialize the path to an empty one
  path.assign(fg.size(), -1); // path[i]=-1 for all i

  // Initialize a queue with the source in it
  queue<long> que;
  que.push(src);
  path[src] = src;        // mark the source as visited

  bool done = (src == sink);
  while (!done && !que.empty()) {
    long current = que.front();
    que.pop();

    // go over the neighbor list, look for an available edge
    for (FNeighborList::iterator it=fg[current].begin();
	 it != fg[current].end(); ++it) {
      long neighbor = it->first;
      long capacity = it->second.capacity;

      if (path[neighbor] >= 0) continue; // already visited here

      long flowPlus = it->second.flow;
      long flowMinus = fg[neighbor][current].flow;

      if (capacity>flowPlus || flowMinus>0) {
	// insert the neighbor into the queue and the path
	path[neighbor] = current; // The path points backward from sink to src
	que.push(neighbor);

	// Test if the new neighbor is the sink
	if (neighbor == sink) {
	  done = true;
	  break;
	}
      }
    }
  }

  // If we reached the sink, calculate the flow on the augmenting path
  long flowVal = LONG_MAX;
  if (done) {
    long next = sink;
    while (next != src) {
      long prev = path[next]; // 

      // The additional flow that we can push from i to j in the path is the
      // sum of the under-utilization (capacity minus flow) in the direction
      // i->j and the flow in the direction j->i.
      long capacity = fg[prev][next].capacity;
      long flowPlus = fg[prev][next].flow;
      long flowMinus= fg[next][prev].flow;
      //      long capacity = fg[prev].find(next)->second.capacity;
      //      long flowPlus = fg[prev].find(next)->second.flow;
      //      long flowMinus= fg[next].find(prev)->second.flow;
      if (flowVal > (capacity-flowPlus) + flowMinus)
          flowVal = (capacity-flowPlus) + flowMinus;
      next = prev;
    }
  }
  else flowVal = 0;

  return flowVal;
 }

#if 0
/*********** Simple networks to test the code from above ***********/
/******* a directed flow graph *******
         /-------3-\   /-------3-\
        /           \ /           \
A -3-> B -2-> C -3-> D -4-> E -3-> F
 \           / \           /
  \-------3-/   \-------2-/
**************************************/
int main()
{
  // initializing in C++ is such a pain
  FlowGraph fg;
  { // Node 0 with edges to 1 (capacity 3) and 2 (capacity 3)
    FNeighborList l;
    l.insert(pair<long,FlowEdge>(1,FlowEdge(3)));
    l.insert(pair<long,FlowEdge>(2,FlowEdge(3)));
    fg.push_back(l);
  }
  { // Node 1 with edges to 2 (capacity 2) and 3 (capacity 3)
    FNeighborList l;
    l.insert(pair<long,FlowEdge>(2,FlowEdge(2)));
    l.insert(pair<long,FlowEdge>(3,FlowEdge(3)));
    fg.push_back(l);
  }
  { // Node 2 with edges to 3 (capacity 3) and 4 (capacity 2)
    FNeighborList l;
    l.insert(pair<long,FlowEdge>(3,FlowEdge(3)));
    l.insert(pair<long,FlowEdge>(4,FlowEdge(2)));
    fg.push_back(l);
  }
  { // Node 3 with edges to 4 (capacity 4) and 5 (capacity 3)
    FNeighborList l;
    l.insert(pair<long,FlowEdge>(4,FlowEdge(4)));
    l.insert(pair<long,FlowEdge>(5,FlowEdge(3)));
    fg.push_back(l);
  }
  { // Node 4 with an edge to 5 (capacity 3)
    FNeighborList l;
    l.insert(pair<long,FlowEdge>(5,FlowEdge(3)));
    fg.push_back(l);
  }

  long f = maximum_flow(fg, 0, 5); // compute max-flow
  cout << "Max-flow value = " << f << endl;
  printFlow(fg);

/*********** an undirected bipartite graph ***********
 0 -> 1,3,4    2 -> 0,2,3    4 -> 1,3,4
 1 -> 0,1,4    3 -> 0,2,2
*****************************************************/
  BipartitleGraph bg;
  bg.addEdge(0,1,1);  bg.addEdge(0,3,2);  bg.addEdge(0,4,3);
  bg.addEdge(1,0,4);  bg.addEdge(1,1,5);  bg.addEdge(1,4,6);
  bg.addEdge(2,0,7);  bg.addEdge(2,2,8);  bg.addEdge(2,3,9);
  bg.addEdge(3,0,10);  bg.addEdge(3,2,11);  bg.addEdge(3,2,12);
  bg.addEdge(4,1,13);  bg.addEdge(4,3,14);  bg.addEdge(4,4,15);
  bg.partitionToMatchings();
  bg.printout();
}
/*********************************************************************/
#endif
