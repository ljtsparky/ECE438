#include<stdio.h>

#include<string.h>

#include<stdlib.h>

#include<vector>

#include<fstream>

#include<sstream>

#include<iostream>

#include<random>

int main(int argc, char** argv) {

    //printf("Number of arguments: %d", argc);

    if (argc != 2) {

        printf("Usage: ./csma input.txt\n");

        return -1;

    }

    int N, L, M, T;

    std::vector<int> R;

    int global_clock = 0;



    std::ifstream file("input.txt");

    std::string line;

    while (std::getline(file, line)) {

        std::istringstream iss(line);

        char type;

        int number;



        iss >> type; // Read the type character

        if (type=='R'){

            while (iss >> number) { // Read all numbers in the line

                R.push_back(number);

            }

        }

        else if (type=='N'){

            iss >> N;

        }

        else if (type=='M'){

            iss >> M;

        }

        else if (type=='L'){

            iss >> L;

        }

        else if (type=='T'){

            iss >> T;

        }

    }

    // std::cout<<N<<L<<M<<T<<R[0]<<R[1]<<R[2]<<R[3]<<R[4]<<R[5]<<std::endl;

    struct node_info{

        int countdown; // countdown, which should be a number initiated within [0, R), 

        int gear_R; //from R[0] to R[M-1], so there can be M different values of gear_R, range from 0 to M-1

        int nodeID;

        bool need_reload_countdown;

    };

    // std::random_device rand_device;

    // std::mt19937 gen(rand_device());

    // std::uniform_int_distribution<> distrib(0, R);

    int tick_time = 0;



    node_info nodes_array[N];

    for (int id=0;id<N;++id){

        nodes_array[id].nodeID = id;

        nodes_array[id].countdown = (tick_time+id) % R[0]; 

        nodes_array[id].gear_R = 0;

        nodes_array[id].need_reload_countdown = false;

    }

    bool channel_idle = true;

    int count_engaged_nodes=0;

    int engaged_nodes_id[N];

    int valid_job_node_id;

    int valid_job_time_left;

    int utilization_time_count =0;

    while(tick_time<T){

        std::cout<<"ticktime:"<<tick_time<<" channel_idle:"<<channel_idle<<" ";

        if (channel_idle){

            for (int id=0; id<N; id++){

                node_info this_node = nodes_array[id];

                if (!this_node.need_reload_countdown){

                    if (this_node.countdown==0){

                        engaged_nodes_id[count_engaged_nodes] = this_node.nodeID;

                        count_engaged_nodes++;

                    }

                }

                else{

                    this_node.countdown = (tick_time+id) % R[this_node.gear_R];

                }

            }

        }

        else{

            valid_job_time_left--;

            if (valid_job_time_left==0){

                channel_idle = true;

                count_engaged_nodes = 0;

                nodes_array[valid_job_node_id].need_reload_countdown = true;

            }

        }

        std::cout<<" count engaged nodes"<<count_engaged_nodes<<" channel idle: "<<channel_idle<<std::endl;

        if (count_engaged_nodes>1){

            for (int i=0; i<count_engaged_nodes; i++){

                int gearR = ++nodes_array[engaged_nodes_id[i]].gear_R;

                nodes_array[engaged_nodes_id[i]].gear_R = gearR % M;

                nodes_array[engaged_nodes_id[i]].need_reload_countdown=true;

            }

            channel_idle=true;

        }

        else if(count_engaged_nodes==1){ // set channel to busy, start job, set time to T

            valid_job_node_id = engaged_nodes_id[0];

            if (channel_idle==true){

                valid_job_time_left=L;

            }

            channel_idle=false;

            utilization_time_count++;

            std::cout<<" utilcount: "<<utilization_time_count<<std::endl;

        }

        else if(count_engaged_nodes==0){//update the countdown if the channel is still idle 

            channel_idle=true;

            for (int id=0;id<N;id++){

                if (nodes_array[id].need_reload_countdown){

                    nodes_array[id].countdown = (tick_time+id) % R[nodes_array[id].gear_R];

                    nodes_array[id].need_reload_countdown = false;

                }

                nodes_array[id].countdown--;

            }

        }

        tick_time++;

    }



    float utilized_rate = 1.0*utilization_time_count/T;

    std::cout<<utilization_time_count<<utilized_rate<<std::endl;

    FILE *fpOut;

    fpOut = fopen("output.txt", "w");

    fprintf(fpOut, "%.2f", utilized_rate);

    fclose(fpOut);



    return 0;

}



