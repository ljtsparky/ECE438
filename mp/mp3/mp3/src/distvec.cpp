#include <iostream>

#include <fstream>

#include <unordered_map>

#include <vector>

#include <limits>

#include <algorithm>

#include <sstream>

#include <unordered_set>

#include <deque>



#include <random>

#include <chrono>



// #include <map>

struct DistanceInfo {

    int distance;

    int nextHop;

};



using Graph = std::unordered_map<int, std::unordered_map<int, int>>;

using DistanceVector = std::unordered_map<int, DistanceInfo>;



Graph readGraph(std::ifstream& fileStream) {

    Graph graph;

    int node1, node2, cost;

    while (fileStream >> node1 >> node2 >> cost) {

        graph[node1][node2] = cost;

        graph[node2][node1] = cost; // If the graph is undirected

        fileStream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');//ignore everything until a newline is reached

    }

    return graph;

}



// Initialize distance vectors for each node

std::unordered_map<int, DistanceVector> initializeDistanceVectors(const Graph& graph) {

    std::unordered_map<int, DistanceVector> distanceVectors;



    for (const auto& node : graph) {

        DistanceVector dv;

        for (const auto& adjNode : graph) {

            dv[adjNode.first] = {std::numeric_limits<int>::max(), -1}; // Initialize as unknown/infinite distance

        }

        // Set distance to immediate neighbors

        for (const auto& neighbor : node.second) { //go through all the map element of node's adjacent map;

            dv[neighbor.first] = {neighbor.second, neighbor.first};

        }

        // Distance to itself is 0

        dv[node.first] = {0, node.first};

        distanceVectors[node.first] = dv;

    }

    return distanceVectors;

}



bool updateDistanceVectors(std::unordered_map<int, DistanceVector>& distanceVectors, Graph& graph) {

    bool changed = false;

    

    // Create a vector of node IDs and shuffle it for randomness

    std::vector<int> nodeIDs;

    for (const auto& node : graph) {

        nodeIDs.push_back(node.first);

    }

    auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());

    std::shuffle(nodeIDs.begin(), nodeIDs.end(), rng);



    // Iterate over nodes in random order

    for (int nodeID : nodeIDs) {

        auto& distvec = distanceVectors[nodeID];

        auto& neighbors = graph.at(nodeID);



        // Shuffle neighbors for randomness

        std::vector<int> neighborIDs;

        for (const auto& neighbor : neighbors) {

            neighborIDs.push_back(neighbor.first);

        }

        std::shuffle(neighborIDs.begin(), neighborIDs.end(), rng);



        // Iterate over neighbors in random order

        for (int neighborID : neighborIDs) {

            for (const auto& dest : distvec) {

                if (distanceVectors[neighborID][dest.first].distance == std::numeric_limits<int>::max()) {

                    // Skip if the neighbor's distance to the destination is 'infinite'

                    continue;

                }



                int newDist = neighbors[neighborID] + distanceVectors[neighborID][dest.first].distance; // new Dist = u-v + v-dest for all v as neighbor

                // Update the distance if the new distance is less or if the new distance is the same but next hop is smaller

                if (newDist < dest.second.distance || 

                    (newDist == dest.second.distance && neighborID < dest.second.nextHop)) {

                    distanceVectors[nodeID][dest.first] = {newDist, neighborID};

                    changed = true;

                }

            }

        }

    }



    return changed;

}





// bool updateDistanceVectors(std::unordered_map<int, DistanceVector>& distanceVectors, const Graph& graph) {

//     bool changed = false;

//     //node is <nodeid, distancevector>

//     for (auto& node : distanceVectors) {

//         //neighbor is <neighborid, cost from curr node to neighbor>

//         for (const auto& neighbor : graph.at(node.first)) {//note that neighbor is an unorderedmap, .first is the neighbor node id and .second is the edge value to that neighbor 

//             //dest is distancevector, which is <destid, <dist from curr node to destid, nexthop to go>  >

//             for (const auto& dest : node.second) {

//                 if (distanceVectors[neighbor.first][dest.first].distance == std::numeric_limits<int>::max()) {

//                     // Skip if the neighbor's distance to the destination is 'infinite'

//                     continue;

//                 }



//                 int newDist = neighbor.second + distanceVectors[neighbor.first][dest.first].distance; // new Dist = u-v + v-dest for all v as neighbor

//                 // Update the distance if the new distance is less or if the new distance is the same but next hop is smaller

//                 if (newDist < dest.second.distance || 

//                     (newDist == dest.second.distance && neighbor.first < dest.second.nextHop)) {

//                     distanceVectors[node.first][dest.first] = {newDist, neighbor.first};

//                     changed = true;

//                 }

//             }

//         }

//     }

//     return changed;

// }



void print_topology_entries(const std::vector<int> nodeIDs, std::unordered_map<int, DistanceVector> distancevectors, std::ofstream& fpOut){

    int next_hop;

    for (int nodeID : nodeIDs) {

        DistanceVector& distancevector = distancevectors[nodeID];

        for (int destID : nodeIDs) {

            if (distancevector[destID].distance==std::numeric_limits<int>::max()){

                continue;

            }

            auto destIt = distancevector.find(destID);

            if (destIt != distancevector.end()) {

                if (destID==nodeID){

                    next_hop=nodeID;

                }

                else{

                    next_hop=distancevector[destID].nextHop;

                }

                fpOut << destID << " " << next_hop << " " << distancevector[destID].distance<< "\n";

            }

        }

        // fpOut << "\n"; // Separating different nodes' results

    }

}



void print_message_paths(std::unordered_map<int, DistanceVector> distancevectors, std::ofstream& fpOut, std::ifstream& fpMessagefile) {

    std::string line;

    fpMessagefile.clear();

    fpMessagefile.seekg(0, std::ios::beg);



    while (std::getline(fpMessagefile, line)) {

        std::istringstream iss(line);

        int srcID, destID;

        std::string message;

        iss >> srcID >> destID;

        std::getline(iss, message); // Read the remainder of the line as the message



        if (distancevectors.find(srcID) != distancevectors.end() && distancevectors[srcID].find(destID) != distancevectors[srcID].end()) {

            if(distancevectors[srcID][destID].distance==std::numeric_limits<int>::max()){

                fpOut << "from " << srcID << " to " << destID << " cost infinite hops unreachable message" << message << "\n";

                continue;

            }

            DistanceVector& distancevector = distancevectors[srcID];

            fpOut << "from " << srcID << " to " << destID << " cost " << distancevector[destID].distance << " hops " << srcID << " ";

            // if (srcID != destID){

            //     // int hop = distancevector[destID].nextHop;

            //     while (distancevector[destID].nextHop!=destID){

            //         fpOut << distancevector[destID].nextHop << " ";

            //         distancevector = distancevectors[distancevector[destID].nextHop];

            //         // hop = distancevector[destID].nextHop;

            //     }

            // }

            if (srcID != destID) {

                std::unordered_set<int> visited;

                int nextHop = distancevector[destID].nextHop;



                while (nextHop != destID) {

                    // Check for loop or excessive hops

                    if (!visited.insert(nextHop).second) { // || visited.size() > 10000000

                        fpOut << " loop detected or max hops exceeded";

                        break;

                    }



                    fpOut << nextHop << " ";

                    nextHop = distancevectors[nextHop][destID].nextHop; // Update nextHop for the next iteration

                }

            }



            // for (int hop = distancevector[destID].nextHop; hop!=destID ; hop = distancevector[destID].nextHop) {

            //     distancevector = distancevectors[hop];

            //     if (hop != destID) { // Exclude destination node from hops

            //         fpOut << hop << " ";

            //     }

            // }

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

            }

            if (std::find(nodeIDs.begin(), nodeIDs.end(), node2) == nodeIDs.end()) {

                nodeIDs.push_back(node2);

            }

        }



        std::sort(nodeIDs.begin(), nodeIDs.end()); // Ensure nodeIDs are sorted after updates

        return true; // A change was applied

    }



    return false; // No more changes to apply

}



int countLinesInFile(std::ifstream& file) {

    file.clear();  // Clear any end-of-file flags

    file.seekg(0, std::ios::beg);  // Move file pointer to the beginning



    return std::count(std::istreambuf_iterator<char>(file), 

                      std::istreambuf_iterator<char>(), '\n') + 1;

}



void deleteLastNLines(std::string filename, int n) {

    std::ifstream file(filename);

    std::vector<std::string> lines;

    std::string line;



    while (std::getline(file, line)) {

        lines.push_back(line);

    }



    file.close();



    // Remove the last n lines

    if (n < lines.size()) {

        lines.resize(lines.size() - n);

    } else {

        lines.clear();

    }



    std::ofstream outFile(filename, std::ios::trunc);  // Overwrite file

    for (const auto& l : lines) {

        outFile << l << "\n";

    }

    outFile.close();

}



int main(int argc, char** argv) {

    if (argc != 4) {

        printf("Usage: ./distvec topofile messagefile changesfile\n");

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



    std::vector<int> nodeIDs; // for printing the topo 

    for (const auto& node : graph) {

        nodeIDs.push_back(node.first);

    }

    std::sort(nodeIDs.begin(), nodeIDs.end()); 



    

    std::unordered_map<int, DistanceVector> distanceVectors = initializeDistanceVectors(graph);

    

    // Process the initial graph

    bool changed;

    int noChangeCount = 0;



    do {

        changed = updateDistanceVectors(distanceVectors, graph);

        if (!changed) {

            noChangeCount++;

        } else {

            noChangeCount = 0;

        }

    } while (noChangeCount < 3);



    print_topology_entries(nodeIDs, distanceVectors, fpOut);

    print_message_paths(distanceVectors, fpOut, fpmessagefile);

    

    do {

        // Apply a single change

        bool changeApplied = apply_changes(graph, nodeIDs, fpchangesfile);



        if (changeApplied) {

            // Recalculate Dijkstra's algorithm and print results

            std::unordered_map<int, DistanceVector> distanceVectors = initializeDistanceVectors(graph);

            int noChangeCount = 0;

            bool changed;

            do {

                changed = updateDistanceVectors(distanceVectors, graph);

                if (!changed) {

                    noChangeCount++;

                } else {

                    noChangeCount = 0;

                }

            } while (noChangeCount < 3);

            print_topology_entries(nodeIDs, distanceVectors, fpOut);

            print_message_paths(distanceVectors, fpOut, fpmessagefile);

        }



    } while (!fpchangesfile.eof());

    // int messageLineCount = countLinesInFile(fpmessagefile);

    // std::cout<<messageLineCount;

    fpOut.close();

    fpchangesfile.close();

    fpmessagefile.close();



    // deleteLastNLines("output.txt", messageLineCount);

    return 0;

}

