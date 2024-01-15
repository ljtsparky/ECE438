#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <queue>
using Clock = std::chrono::high_resolution_clock;
using Microseconds = std::chrono::microseconds;
Clock::time_point getCurrentTime() {
    return Clock::now();
}
int calculateRTT(const Clock::time_point& start, const Clock::time_point& end) {
    auto duration = std::chrono::duration_cast<Microseconds>(end - start).count();
    if (duration > std::numeric_limits<int>::max()) {
        std::cerr << "Warning: RTT duration exceeds the maximum value of int." << std::endl;
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(duration);
}
void diep(const std::string& msg) {
    perror(msg.c_str());
    exit(EXIT_FAILURE);
}
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

#define LOCAL_PORT 5000
struct sockaddr_in si_other; struct sockaddr_in si_me;
socklen_t si_len = sizeof(si_other);
int s;

int SampleRTT = 0;
int EstimatedRTT = 0;
int DevRTT = 0;
int TimeoutInterval = 0;

void update_RTT(int new_sampleRTT) {
    SampleRTT = new_sampleRTT;
    EstimatedRTT = static_cast<int>(0.875 * EstimatedRTT + 0.125 * SampleRTT);
    DevRTT = static_cast<int>(0.75 * DevRTT + 0.25 * std::abs(SampleRTT - EstimatedRTT)); // beta is 0.25
    TimeoutInterval = EstimatedRTT + 4 * DevRTT;
}

//packet to be sent
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

// std::queue<packet> packet_queue;


//congestion control 
enum State{fast_recovery, slow_start, congestion_avoidance};
State curr_state = slow_start;
#define MSS 1
int cwnd = MSS;
int ssthresh = 64;
int dupACKcount = 0;
int newest_seq_num = -1;//by far, from 0 to newest seqnum inclusively is already acked.
struct packets_array_struct{
    int32_t seq_num;
    Clock::time_point send_time;
    unsigned int pad_7 : 7; unsigned int retransmitted : 1;
    packet packet_this;
    enum packet_state_sendack{sent_not_acked, acked, not_sent} packet_state;
};
std::vector<packets_array_struct> packets_state_array;
int cwnd_start=0; int cwnd_end=cwnd_start + cwnd;
inline void updateCwndPosition() {
    cwnd_end = cwnd_start + cwnd;
}


// class CountdownTimer {
// public:
//     using TimeoutCallback = std::function<void()>; // type alias

//     CountdownTimer(const TimeoutCallback& callback) // type alias to std::function<void()>
//         : timeout_callback(callback), timer_thread(&CountdownTimer::timerFunction, this) {} //this is the member initializer list. 
// //     &CountdownTimer::timerFunction: This is a pointer to the timerFunction member function of CountdownTimer. It's the function that will be run in the new thread.
// // this: The this pointer is a pointer to the current instance of CountdownTimer. It's passed to timerFunction because timerFunction is a non-static member function, which means it needs to know which instance of CountdownTimer it is working with.
//     ~CountdownTimer() {
//         stopTimerThread();
//     }

//     void setTimer(int seconds) {
//         stopTimerThread();
//         timer_duration = std::chrono::seconds(seconds);
//         startTimerThread();
//     }

// private:
//     void stopTimerThread() {
//         {
//             std::lock_guard<std::mutex> lock(mtx);
//             timer_thread_running = false;
//             cv.notify_one();
//         }
//         if (timer_thread.joinable()) {
//             timer_thread.join();
//         }
//         if (timeout_thread.joinable()) {
//             timeout_thread.join();
//         }
//     }

//     void startTimerThread() {
//         timer_thread_running = true;
//         timer_thread = std::thread(&CountdownTimer::timerFunction, this);
//     }

//     void timerFunction() {
//         std::unique_lock<std::mutex> lock(mtx);
//         while (timer_thread_running) {
//             if (cv.wait_for(lock, timer_duration, [this]{ return !timer_thread_running; })) {
//                 // Exit loop if timer_thread_running is false
//                 break;
//             }
//             if (timer_thread_running && timeout_callback) {
//                 // Call the callback function in a separate thread
//                 std::thread(timeout_callback).detach();
//                 // Reset or stop the timer as required
//                 timer_thread_running = false; // Stop the timer after callback
//             }
//         }
//     }

//     TimeoutCallback timeout_callback;
//     std::thread timer_thread;
//     std::thread timeout_thread;
//     std::mutex mtx;
//     std::condition_variable cv;
//     std::chrono::milliseconds timer_duration{0};
//     bool timer_thread_running{false};
// };

void retransmit(){
    packets_state_array[cwnd_start].packet_this.RETRANSMIT_BIT=1;
    if (sendto(s, &packets_state_array[cwnd_start].packet_this, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
        diep("sendto");
    }
    packets_state_array[cwnd_start].packet_state = packets_array_struct::sent_not_acked;
}
//check from the back end of the cwnd to send all the not_sent packets.
void transmit_new(){
    for (int i=cwnd_end-1;i>=cwnd_start;i--){
        if (packets_state_array[i].packet_state == packets_array_struct::not_sent){
            packets_state_array[i].retransmitted = 0;
            packets_state_array[i].send_time = getCurrentTime();
            if (sendto(s, &packets_state_array[i].packet_this, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
                diep("sendto");
            }
            packets_state_array[i].packet_state = packets_array_struct::sent_not_acked;
            continue;
        }
        break;
    }
}
void congestion_control(bool dupACK, bool newACK, bool timeout){
    int true_count=0;
    if (dupACK) true_count++; 
    if (newACK) true_count++; 
    if (timeout) true_count++;
    if (true_count>1){
        std::cerr << "Error: More than one condition is true." << std::endl;
        return;
    }
    switch (curr_state)
    {
    case fast_recovery:
        if (timeout){
            ssthresh = cwnd/2; cwnd = MSS; dupACKcount = 0; updateCwndPosition(); retransmit();
        }
        else if(dupACK){
            cwnd+=MSS;
            updateCwndPosition();
            transmit_new();
        }
        else if(newACK){
            cwnd = ssthresh;
            dupACKcount=0;
            updateCwndPosition();
            curr_state = congestion_avoidance;
        }
        else{
            std::cerr << "Error: More than one condition is true." << std::endl;
            return;
        }
        break;
    case slow_start:
        if (timeout){
            ssthresh = cwnd/2; cwnd = MSS; dupACKcount = 0; updateCwndPosition(); retransmit();
            if (cwnd>=ssthresh){
                curr_state=congestion_avoidance;
            }
        }
        else if(dupACK){
            dupACKcount++;
            if (dupACKcount==3){
                ssthresh=cwnd/2;
                cwnd = ssthresh+3;
                updateCwndPosition();
                retransmit();
                curr_state=fast_recovery;
            }
        }
        else if(newACK){
            cwnd=cwnd+MSS;
            dupACKcount=0;
            cwnd_start = newest_seq_num + 1;
            cwnd_end=cwnd_start+cwnd;
            transmit_new();
            if (cwnd>=ssthresh){
                curr_state=congestion_avoidance;
            }
        }
        else{
            std::cerr << "Error: More than one condition is true." << std::endl;
            return;
        }
        break;

    case congestion_avoidance:
        if (timeout){
            ssthresh = cwnd/2; cwnd = MSS; dupACKcount = 0; retransmit();
        }
        else if(dupACK){
            dupACKcount++;
            if (dupACKcount==3){
                ssthresh=cwnd/2;
                cwnd = ssthresh+3;
                cwnd_end = cwnd_start + cwnd;
                retransmit();
                curr_state=fast_recovery;
            }
        }
        else if(newACK){
            cwnd += MSS * (MSS/cwnd);
            dupACKcount = 0;
            cwnd_end = cwnd_start + cwnd;
            transmit_new();
        }
        else{
            std::cerr << "Error: More than one condition is true." << std::endl;
            return;
        }
        break;
    default:
        break;
    }
}

void onTimeout() {
    std::cout << "Timer timed out! This is running in a separate thread." << std::endl;
    congestion_control(false, false, true);
}

void reliablyTransfer(const std::string& hostname, unsigned short int hostUDPport, const std::string& filename, unsigned long long int bytesToTransfer) {
    // Open the file
    in_addr_t netIP = inet_addr(hostname.c_str());
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file to send." << std::endl;
        exit(EXIT_FAILURE);
    }
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    size_t numPackets = (fileSize + DATA_SIZE -1) / DATA_SIZE;
    packets_state_array.resize(numPackets, {packets_array_struct::not_sent});
    file.seekg(0, std::ios::beg);
    for (int i=0;i<numPackets;i++){
        std::memset(&packets_state_array[i].packet_this, 0, sizeof(packet));
        packets_state_array[i].seq_num=i;
        packets_state_array[i].packet_this.ACK_BIT=0; packets_state_array[i].packet_this.SYN_BIT=0; packets_state_array[i].packet_this.RETRANSMIT_BIT=0;
        packets_state_array[i].packet_this.ack_seq_num=0; packets_state_array[i].packet_state = packets_array_struct::not_sent;
        file.read(packets_state_array[i].packet_this.data_of_packet, DATA_SIZE);
        std::streamsize bytes_read = file.gcount();
        packets_state_array[i].packet_this.bytes_of_data = static_cast<size_t>(bytes_read);
    } // NOW THE PACKETS ARE READY, start to build socket

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) diep("socket");

    std::memset((char*)&si_me, 0, sizeof(si_me));    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(LOCAL_PORT); // The port you want to bind to
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) < 0) {
        diep("bind");
    }

    std::memset(&si_other, 0, sizeof(si_other));    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname.c_str(), &si_other.sin_addr) == 0) {
        std::cerr << "inet_aton() failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    bool SYNC_SENT = false ;bool ESTAB=false; bool FIN=false; int seq_num_y = 0; bool TIMED_WAIT=false; bool FIN_WAIT=false;
    while(!SYNC_SENT){
        packet pkt;
        std::memset(&pkt, 0, sizeof(packet));
        pkt.SYN_BIT=1;
        pkt.seq_num=numPackets;
        std::strcpy(pkt.data_of_packet, "this is data of syncsend");
        if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
            diep("sendto");
        }
        packet recv_packet;
        std::memset(&recv_packet, 0, sizeof(packet));
        struct sockaddr_in si_recvfrom; socklen_t si_recvfrom_len;
        ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_recvfrom), &si_recvfrom_len);
        if (recv_len == -1) diep("recvfrom");
        if (si_recvfrom.sin_addr.s_addr != netIP || si_recvfrom.sin_port != htons(hostUDPport)){
            continue;
        }
        if (recv_len == sizeof(recv_packet)) {
            if (recv_packet.ACK_BIT==1 && recv_packet.SYN_BIT==1 && recv_packet.ack_seq_num == numPackets+1){
                seq_num_y=recv_packet.seq_num;
                SYNC_SENT=true;
                continue;
            }
            else{
                std::cout << "packet content wrong, seqnum, ackbit, synbit" << recv_packet.seq_num<< recv_packet.ACK_BIT<<recv_packet.SYN_BIT << std::endl;
            }
        }
        else{
            diep("impossible packet size");
        }
    }
    while(!ESTAB){
        packet pkt;
        pkt.ACK_BIT=1;
        pkt.ack_seq_num=seq_num_y+1;
        std::strcpy(pkt.data_of_packet, "this is data of establishing connection");
        if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
            diep("sendto");
        }
        ESTAB=true;
    }
    while(!FIN_WAIT){//do the main send and receive work until all the packets are sent and acked.
        if(packets_state_array[numPackets-1].packet_state == packets_array_struct::acked){
            FIN_WAIT=true;
            break;
        } 
        transmit_new();
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        struct timeval tv;
        tv.tv_sec = TimeoutInterval/1000000;  // Set timeout to 5 seconds
        tv.tv_usec = TimeoutInterval%1000000;
        int retval = select(s + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select()");
        } else if (retval) {
            packet recv_packet;
            struct sockaddr_in si_recvfrom; socklen_t si_recvfrom_len;
            ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_recvfrom), &si_recvfrom_len);
            Clock::time_point recv_time = getCurrentTime();
            if (recv_len == -1) diep("recvfrom");
            if (si_recvfrom.sin_addr.s_addr != netIP || si_recvfrom.sin_port != htons(hostUDPport)){
                diep("incorrect source");
                continue;
            }
            if (recv_len == sizeof(recv_packet)) {
                if (recv_packet.ACK_BIT==1){
                    int32_t recv_ack_seq_num = recv_packet.ack_seq_num;
                    if (recv_ack_seq_num>=cwnd_start){
                        cwnd_start = recv_ack_seq_num+1;
                        if (packets_state_array[recv_ack_seq_num].packet_this.RETRANSMIT_BIT==0){
                            int new_sampled_RTT = calculateRTT(packets_state_array[recv_ack_seq_num].send_time, recv_time);
                            update_RTT(new_sampled_RTT);
                        }
                        congestion_control(false, true, false);
                    }
                    else{
                        congestion_control(true, false, false);
                    }
                    packets_state_array[recv_packet.ack_seq_num].packet_state = packets_array_struct::acked;
                }
                else{
                    std::cout << "packet content wrong, seqnum, ackbit, synbit" << recv_packet.seq_num<< recv_packet.ACK_BIT<<recv_packet.SYN_BIT << std::endl;
                }
            }
            else{
                diep("impossible packet size");
            }
        } else {//timed out
            congestion_control(false, false, true);
        }
    }
    // while(!TIMED_WAIT){//
    //     packet pkt;
    //     std::memset(&pkt, 0, sizeof(packet));
    //     pkt.FIN_BIT=1;
    //     pkt.seq_num=numPackets;
    //     std::strcpy(pkt.data_of_packet, "this is data of syncsend");
    //     if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
    //         diep("sendto");
    //     }
    //     packet recv_packet;
    //     struct sockaddr_in si_recvfrom; socklen_t si_recvfrom_len;
    //     ssize_t recv_len = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, reinterpret_cast<struct sockaddr*>(&si_recvfrom), &si_recvfrom_len);
    //     if (recv_len == -1) diep("recvfrom");
    //     if (si_recvfrom.sin_addr.s_addr != netIP || si_recvfrom.sin_port != htons(hostUDPport)){
    //         continue;
    //     }
    //     if (recv_len == sizeof(recv_packet)) {
    //         if (recv_packet.ACK_BIT==1 && recv_packet.SYN_BIT==1 && recv_packet.ack_seq_num == numPackets+1){
    //             seq_num_y=recv_packet.seq_num;
    //             SYNC_SENT=true;
    //             continue;
    //         }
    //         else{
    //             std::cout << "packet content wrong, seqnum, ackbit, synbit" << recv_packet.seq_num<< recv_packet.ACK_BIT<<recv_packet.SYN_BIT << std::endl;
    //         }
    //     }
    //     else{
    //         diep("impossible packet size");
    //     }
    // }
    // Data to send
    // CountdownTimer timer(onTimeout);
    // timer.setTimer(5000);
    // for (int32_t i = 0; i < 10; ++i) {
    //     packet pkt;
    //     pkt.seq_num = i; // Assign sequence number
    //     pkt.ack_seq_num = 0; // Assuming ack_seq_num is not used in this example
    //     std::strcpy(pkt.data_of_packet, "Hello, UDP receiver!"); // Set data

    //     if (sendto(s, &pkt, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
    //         diep("sendto");
    //     }
    //     std::cout << "Sent packet with seq_num: " << i << std::endl;
    // }
    // std::cout << "Main thread is free to do other work." << std::endl;
    // // Simulate main thread work
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    // std::cout << "Main thread work first finished." << std::endl;
    // timer.setTimer(3000);
    // std::this_thread::sleep_for(std::chrono::seconds(7));
    // std::cout << "Main thread work done." << std::endl;
    std::cout << "Closing the socket" << std::endl;
    close(s);
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: " << argv[0] << " receiver_hostname receiver_port filename_to_xfer bytes_to_xfer" << std::endl;
        exit(EXIT_FAILURE);
    }

    unsigned short int udpPort = static_cast<unsigned short int>(std::atoi(argv[2]));
    unsigned long long int numBytes = std::atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return EXIT_SUCCESS;
}
