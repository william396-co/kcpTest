#include <signal.h>
#include <thread>
#include <chrono>
#include <vector>

#include "client.h"
#include "joining_thread.h"

constexpr auto default_ip = "127.0.0.1";
constexpr auto default_port = 9527;
constexpr auto default_max_len = 2000;
constexpr auto default_test_times = 10;
constexpr auto default_lost_rate = 0;
constexpr auto default_send_interval = 30; // ms
constexpr auto default_client_cnt = 10;

bool g_running = true;

void signal_handler( int sig )
{
    g_running = false;
}
void handle_signal()
{
    signal( SIGPIPE, SIG_IGN );
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
}

std::unique_ptr<Client> start_client( int idx, const char * ip, uint16_t port, int mode, int max_len, int test_times, int lost_rate, int interval = default_send_interval )
{
    std::unique_ptr<Client> client = std::make_unique<Client>( ip, port, conv );
    client->setmode( mode );
    client->setauto( true, test_times, max_len );
    client->setlostrate( lost_rate );
    client->setsendinterval( interval );
    client->set_index( idx );
    // client->set_show_info( true );
    return client;
}

int main( int argc, char ** argv )
{
    handle_signal();

    int mode = 2;
    uint16_t port = default_port;
    std::string ip = default_ip;
    int max_len = default_max_len;
    int test_times = default_test_times;
    int lost_rate = default_lost_rate;
    int send_interval = default_send_interval;
    int client_cnt = default_client_cnt;

    if ( argc > 1 ) {
        client_cnt = atoi( argv[1] );
    }

    printf( "Usage:<%s> ClientCount:%d\n", argv[0], client_cnt );

    std::vector<std::unique_ptr<Client>> clients;
    for ( int i = 0; i != client_cnt; ++i ) {
        clients.push_back( start_client( i + 1, ip.c_str(), port, mode, max_len, test_times, lost_rate, send_interval ) );
    }

    std::vector<joining_thread> threads;
    for ( int i = 0; i != client_cnt; ++i ) {
        threads.emplace_back( &Client::run, clients[i].get() );
    }

    while ( g_running ) {
        std::this_thread::sleep_for( std::chrono::milliseconds { 1 } );
    }

    for ( int i = 0; i != client_cnt; ++i ) {
        clients[i]->terminate();
    }

    return 0;
}
