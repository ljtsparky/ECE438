#include <iostream>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <queue>
int s;
struct sockaddr_in si_me, si_other;
socklen_t si_len = sizeof(si_other);


#define PACKET_SIZE 1024
#define HEADER_SIZE 13 // size of header
#define DATA_SIZE (PACKET_SIZE - HEADER_SIZE)
struct packet {
    int32_t seq_num;
    int32_t ack_seq_num;
    int32_t bytes_of_data;
    unsigned int padding7_1 : 4; unsigned int RETRANSMIT_BIT : 1; unsigned int SYN_BIT : 1; unsigned int ACK_BIT : 1; unsigned int FIN_BIT : 1; 
    char data_of_packet[DATA_SIZE];
    // structure members
};
// #define QUEUE_MAX_SIZE 10
// std::queue<packet> packet_queue;


bool receive_packet_flag=true;


void diep(const std::string& msg) {
    perror(msg.c_str());
    exit(EXIT_FAILURE);
}


int numPackets=0;
struct packets_array_struct{
    int32_t seq_num;
    unsigned int pad_7 : 7; unsigned int received : 1;
    packet packet_this;
};
int current_acked_index=0;
std::vector<packets_array_struct> packets_state_array;
void update_current_acked_index(){
    for (int i=current_acked_index;i<numPackets;i++){
        if (packets_state_array[i].received==1){
            current_acked_index++;
            continue;
        }
        break;
    }
}
bool CLOSED = false ; 
void send_ack(){
    packet pkt;
    std::memset(&pkt, 0, sizeof(packet));
    pkt.ACK_BIT=1; pkt.ack_seq_num=current_acked_index-1;
    if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
        diep("sendto");
    }
    if (packets_state_array[numPackets-1].received==1){
        CLOSED=true;
    }
}
void write_file_to_local(const std::string& filename) {
    std::ofstream outfile(filename, std::ios::binary | std::ios::out);
    if (!outfile.is_open()) {
        std::cerr << "Could not open file for writing." << std::endl;
        return;
    }

    for (const auto& packet_state : packets_state_array) {
        if (packet_state.received) {
            outfile.write(packet_state.packet_this.data_of_packet, packet_state.packet_this.bytes_of_data);
        } else {
            std::cerr << "Packet not received. File may be incomplete." << std::endl;
            break;
        }
    }

    outfile.close();
    std::cout << "File written successfully." << std::endl;
}
void reliablyReceive(unsigned short int myUDPport, const std::string& destinationFile) {
    // CountdownTimer timer(onTimeout);
    // timer.setTimer(5);
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        diep("socket");
    }
    std::memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    std::cout << "Now binding to port " << myUDPport << std::endl;
    if (bind(s, reinterpret_cast<struct sockaddr*>(&si_me), sizeof(si_me)) == -1) {
        diep("bind");
    }
    char received_msg[DATA_SIZE];
    bool SYNC_RECV = false ; bool ESTAB=false; bool FIN=false; int seq_num_y = 0; bool TIMED_WAIT=false; bool FIN_WAIT=false;
    // while(!CLOSED){
    while(!SYNC_RECV){
        packet recv_packet;
        ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_other), &si_len);
        if (recv_len == -1) {
            diep("recvfrom");
        } else if (recv_len == sizeof(recv_packet)) {
            std::cout << "Received packet with seq_num: " << recv_packet.seq_num << std::endl;
            std::strcpy(received_msg, recv_packet.data_of_packet);
            printf("received packet data:");
            printf(recv_packet.data_of_packet);
            if (recv_packet.SYN_BIT!=1){
                diep("incorrect synbit");
                continue;
            }
            numPackets = recv_packet.seq_num;
            SYNC_RECV=true;            
        } else {
            std::cerr << "Received packet of unexpected size." << std::endl;
        }
    }
    while(!ESTAB){
        packet pkt;
        std::memset(&pkt, 0, sizeof(packet));
        pkt.SYN_BIT=1; pkt.ACK_BIT=1;
        pkt.seq_num=666; pkt.ack_seq_num=numPackets+1;
        if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
            diep("sendto");
            continue;
        }
        packet recv_packet;
        ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_other), &si_len);
        if (recv_len == -1) {
            diep("recvfrom");
        } else if (recv_len == sizeof(recv_packet)) {
            if (recv_packet.ACK_BIT!=1 || recv_packet.ack_seq_num!=pkt.seq_num+1){
                diep("incorrect ");
                continue;
            }         
            ESTAB=true;
        } else {
            std::cerr << "Received packet of unexpected size." << std::endl;
        }
    }
    
    packets_state_array.resize(numPackets);
    for (int i=0;i<numPackets;i++){
        std::memset(&packets_state_array[i].packet_this, 0, sizeof(packet));
        packets_state_array[i].received=0;
        packets_state_array[i].seq_num=i;
    }
    // Enter a loop to wait for incoming packets
    while (!CLOSED) {
        packet recv_packet;
        ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_other), &si_len);
        if (recv_len == -1) {
            diep("recvfrom");
        } else if (recv_len == sizeof(recv_packet)) {
            packets_state_array[recv_packet.seq_num].packet_this=recv_packet;
            packets_state_array[recv_packet.seq_num].received=1;
            update_current_acked_index();
            send_ack();
        } else {
            std::cerr << "Received packet of unexpected size." << std::endl;
        }
    }
    // while(){
    //     packet recv_packet;
    //     ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_other), &slen);
    //     if (recv_len == -1) {
    //         diep("recvfrom");
    //     } else if (recv_len == sizeof(recv_packet)) {
    //         if (recv_packet.FIN_BIT!=1){
    //             continue;
    //         }
    //         packet pkt;
    //         std::memset(&pkt, 0, sizeof(packet));
    //         pkt.SYN_BIT=1; pkt.ACK_BIT=1;
    //         pkt.seq_num=666; pkt.ack_seq_num=numPackets+1;
    //         if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
    //             diep("sendto");
    //             continue;
    //         }
    //     } else {
    //         std::cerr << "Received packet of unexpected size." << std::endl;
    //     }
    // }
    // }
    write_file_to_local(destinationFile);
    close(s);
    std::cout << destinationFile << " received." << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " UDP_port filename_to_write" << std::endl;
        exit(EXIT_FAILURE);
    }

    unsigned short int udpPort = static_cast<unsigned short int>(std::atoi(argv[1]));

    reliablyReceive(udpPort, argv[2]);

    return 0;
}
