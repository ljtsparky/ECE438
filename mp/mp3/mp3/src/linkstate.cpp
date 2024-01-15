#include <iostream>

#include <string>

#include <fstream>

#include <unordered_map>

#include <vector>

#include <limits>

#include <set>

#include <algorithm>

#include <sstream>

#include <queue>

#include <functional>

struct NodeInfo {

    int distance;

    std::vector<int> path;

};



using Graph = std::unordered_map<int, std::unordered_map<int, int>>;

using DijkstraResult = std::unordered_map<int, NodeInfo>;



// DijkstraResult dijkstra(const Graph& graph, int startNode) {

//     DijkstraResult results;

//     std::set<std::pair<int, int>> nodeQueue; // Pair of distance and node ID



//     // Initialize distances to maximum possible value

//     for (const auto& node : graph) {

//         results[node.first].distance = std::numeric_limits<int>::max();

//     }



//     // Distance to start node is 0

//     results[startNode].distance = 0;

//     results[startNode].path.push_back(startNode);

//     nodeQueue.insert({0, startNode});



//     while (!nodeQueue.empty()) {

//         int currentNode = nodeQueue.begin()->second;

//         nodeQueue.erase(nodeQueue.begin());



//         // Visit each neighbor of the current node

//         for (const auto& neighbor : graph.at(currentNode)) {

//             int nextNode = neighbor.first;

//             int weight = neighbor.second;

//             int newDistance = results[currentNode].distance + weight;



//             // Update distance and path if a shorter path is found

//             if (newDistance < results[nextNode].distance) {

//                 nodeQueue.erase({results[nextNode].distance, nextNode});

//                 results[nextNode].distance = newDistance;

//                 results[nextNode].path = results[currentNode].path;

//                 results[nextNode].path.push_back(nextNode);

//                 nodeQueue.insert({results[nextNode].distance, nextNode});

//             }

//         }

//     }



//     return results;

// }

// DijkstraResult dijkstra(Graph& graph, int startNode) {

//     DijkstraResult results;

//     std::set<int> finished_nodes_set;

//     // Initialize distances to maximum possible value

//     // std::cout<<" "<<std::endl;

    

//     for (auto& node : graph) {//for all the nodes, set them to infinite cost to startnode, and then update the cost of neighbors of startnode to correct value

//         results[node.first].distance = std::numeric_limits<int>::max();

//     }

//     results[startNode].path.push_back(startNode);

//     for (auto& neighborid_cost : graph.at(startNode)){

//         results[neighborid_cost.first].distance = graph[neighborid_cost.first][startNode]; 

//     }

//     finished_nodes_set.insert(startNode);

//     while(finished_nodes_set.size()<graph.size()){

//         int Dw_min = std::numeric_limits<int>::max();

//         int Dw_min_nodeid = std::numeric_limits<int>::max();

//         int prev_node = std::numeric_limits<int>::max();

//         int w;

//         for(int curr_node : finished_nodes_set){

//             for (auto& neighborid_cost : graph.at(curr_node)){

//                 w = neighborid_cost.first;

//                 if (finished_nodes_set.find(w)!=finished_nodes_set.end()){ // if already in finished set, skip

//                     continue;

//                 }

//                 if (results[w].distance<Dw_min || (results[w].distance == Dw_min && w<Dw_min_nodeid) || (results[w].distance == Dw_min && w==Dw_min_nodeid && curr_node<prev_node)){

//                     Dw_min = results[w].distance;

//                     Dw_min_nodeid = w;

//                     prev_node = curr_node;

//                 }

//             }

//         }

//         if (Dw_min == std::numeric_limits<int>::max()){ break; }//if there is no any other nodes to reach, just break.

//         results[Dw_min_nodeid].path = results[prev_node].path;

//         results[Dw_min_nodeid].path.push_back(Dw_min_nodeid);

//         results[Dw_min_nodeid].distance = Dw_min;

//         // add w to N'

//         finished_nodes_set.insert(Dw_min_nodeid);



//         //update D(v) for all v adjacent to w and not in N' :  D(v) = min( D(v), D(w) + c(w,v) )

//         int node_v_id;

//         for (auto& neighborid_cost : graph[Dw_min_nodeid]){ //for all v adjacent to w and not in N'

//             if ( finished_nodes_set.find(neighborid_cost.first)!=finished_nodes_set.end()){

//                 continue;

//             }

//             node_v_id = neighborid_cost.first;

//             results[node_v_id].distance = std::min(results[node_v_id].distance, results[Dw_min_nodeid].distance + graph[node_v_id][Dw_min_nodeid]);

//         }



//     }

//     return results;

// }



DijkstraResult dijkstra(const Graph& graph, int startNode) {

    DijkstraResult results;

    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> nodeQueue;



    // Initialize distances to maximum possible value

    for (const auto& node : graph) {

        results[node.first].distance = std::numeric_limits<int>::max();

        results[node.first].path = {startNode};  // Initialize path to start with the startNode itself

    }



    // Distance to start node is 0

    results[startNode].distance = 0;

    nodeQueue.push({0, startNode});



    while (!nodeQueue.empty()) {

        int currentNode = nodeQueue.top().second;

        int currentDistance = nodeQueue.top().first;

        nodeQueue.pop();



        if (currentDistance > results[currentNode].distance) {

            continue;  // Skip if we have already found a shorter path

        }



        // Visit each neighbor of the current node

        for (const auto& neighbor : graph.at(currentNode)) {

            int nextNode = neighbor.first;

            int weight = neighbor.second;

            int newDistance = results[currentNode].distance + weight;



            // Update distance and path if a shorter path is found

            if (newDistance < results[nextNode].distance) {

                results[nextNode].distance = newDistance;

                results[nextNode].path = results[currentNode].path;

                results[nextNode].path.push_back(nextNode);

                nodeQueue.push({newDistance, nextNode});

            }

        }

    }



    return results;

}





// Function to read the graph from file

Graph readGraph(std::ifstream& fileStream) {

    Graph graph;

    int node1, node2, cost;

    while (fileStream >> node1 >> node2 >> cost) {

        graph[node1][node2] = cost;

        graph[node1][node1] = 0;

        graph[node2][node2] = 0;

        graph[node2][node1] = cost; // If the graph is undirected

        fileStream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    }

    return graph;

}



void print_topology_entries(const std::vector<int> nodeIDs, std::unordered_map<int, DijkstraResult> allResults, std::ofstream& fpOut){

    int next_hop;

    for (int nodeID : nodeIDs) {

        DijkstraResult& nodeResult = allResults[nodeID];

        for (int destID : nodeIDs) {

            if (nodeResult[destID].distance==std::numeric_limits<int>::max()){

                continue;

            }

            auto destIt = nodeResult.find(destID);

            if (destIt != nodeResult.end()) {

                auto& destination = *destIt;

                if (destID==nodeID){

                    next_hop=nodeID;

                }

                else{

                    next_hop=destination.second.path[1];

                }

                fpOut << destID << " " << next_hop << " " << destination.second.distance<< "\n";

            }

        }

        // fpOut << "\n"; // Separating different nodes' results

    }

}



void print_message_paths(std::unordered_map<int, DijkstraResult> allResults, std::ofstream& fpOut, std::ifstream& fpMessagefile) {

    std::string line;

    fpMessagefile.clear();

    fpMessagefile.seekg(0, std::ios::beg);



    while (std::getline(fpMessagefile, line)) {

        std::istringstream iss(line);

        int srcID, destID;

        std::string message;

        iss >> srcID >> destID;

        std::getline(iss, message); // Read the remainder of the line as the message



        if (allResults.find(srcID) != allResults.end() && allResults[srcID].find(destID) != allResults[srcID].end()) {

            if (allResults[srcID][destID].distance == std::numeric_limits<int>::max()){

                fpOut << "from " << srcID << " to " << destID << " cost infinite hops unreachable message" << message << "\n";

                continue;

            }

            const auto& pathInfo = allResults[srcID][destID];

            fpOut << "from " << srcID << " to " << destID << " cost " << pathInfo.distance << " hops ";

            for (auto hop : pathInfo.path) {

                if (hop == srcID){

                    fpOut << hop << " ";

                    continue;

                }

                if (hop != destID) { // Exclude destination node from hops

                    fpOut << hop << " ";

                }

            }

            fpOut << "message" << message << "\n";

        } else {

            fpOut << "from " << srcID << " to " << destID << " cost infinite hops unreachable message" << message << "\n";

        }

    }

}



bool apply_changes(Graph& graph, std::vector<int>& nodeIDs, std::ifstream& fpchangesfile) {

    int node1, node2, cost;



    if (fpchangesfile >> node1 >> node2 >> cost) {

        if (cost == -999) {

            // Remove the connection

            graph[node1].erase(node2);

            graph[node2].erase(node1);

        } else {

            // Update or add the new cost/connection

            graph[node1][node2] = cost;

            graph[node2][node1] = cost;



            // Add new nodes to nodeIDs if they don't already exist

            if (std::find(nodeIDs.begin(), nodeIDs.end(), node1) == nodeIDs.end()) {

                nodeIDs.push_back(node1);

                graph[node1][node1] = 0;

            }

            if (std::find(nodeIDs.begin(), nodeIDs.end(), node2) == nodeIDs.end()) {

                nodeIDs.push_back(node2);

                graph[node2][node2] = 0;

            }

        }



        std::sort(nodeIDs.begin(), nodeIDs.end()); // Ensure nodeIDs are sorted after updates

        return true; // A change was applied

    }



    return false; // No more changes to apply

}





int main(int argc, char** argv) {

    if (argc != 4) {

        std::cout << "Usage: ./linkstate topofile messagefile changesfile\n";

        return -1;

    }



    std::string topofile_name = argv[1];

    std::ifstream fptopo(topofile_name);

    if (!fptopo.is_open()) {std::cerr << "Error opening file " << topofile_name << std::endl; return -1;}



    std::ofstream fpOut("output.txt");

    if (!fpOut.is_open()) {std::cerr << "Error opening output file" << std::endl; return -1;}



    std::string messagefile_name = argv[2];

    std::ifstream fpmessagefile(messagefile_name);

    if (!fpmessagefile.is_open()) {std::cerr << "Error opening file " << messagefile_name << std::endl; return -1;}



    std::string changesfile_name = argv[3];

    std::ifstream fpchangesfile(changesfile_name);

    if (!fpchangesfile.is_open()) {std::cerr << "Error opening file " << changesfile_name << std::endl; return -1;}



    Graph graph = readGraph(fptopo);

    fptopo.close();



    std::vector<int> nodeIDs;

    for (const auto& node : graph) {

        nodeIDs.push_back(node.first);

    }

    std::sort(nodeIDs.begin(), nodeIDs.end());

    // Process the initial graph

    std::unordered_map<int, DijkstraResult> allResults;

    for (int nodeID : nodeIDs) {

        allResults[nodeID] = dijkstra(graph, nodeID);

    }

    print_topology_entries(nodeIDs, allResults, fpOut);

    print_message_paths(allResults, fpOut, fpmessagefile);



    do {

        // Apply a single change

        bool changeApplied = apply_changes(graph, nodeIDs, fpchangesfile);



        if (changeApplied) {

            // Recalculate Dijkstra's algorithm and print results

            std::unordered_map<int, DijkstraResult> allResults;

            for (int nodeID : nodeIDs) {

                allResults[nodeID] = dijkstra(graph, nodeID);

            }



            print_topology_entries(nodeIDs, allResults, fpOut);

            print_message_paths(allResults, fpOut, fpmessagefile);

        }



    } while (!fpchangesfile.eof());

    // fptopo.close();

    fpOut.close();

    fpchangesfile.close();

    fpmessagefile.close();

    return 0;

}

