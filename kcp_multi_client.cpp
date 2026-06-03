#include <signal.h>
#include <thread>
#include <chrono>
#include <vector>

#include "client.h"
#include "joining_thread.h"

constexpr auto default_ip = "127.0.0.1";
constexpr auto default_port = 9527;
constexpr auto default_max_len = 2000;
constexpr auto default_test_times = 100;
constexpr auto default_lost_rate = 0;
constexpr auto default_send_interval = 30; // ms
constexpr auto default_client_cnt = 5;

bool g_running = true;

void signal_handler( int sig )
{
    g_running = false;
}
void handle_signal()
{
#ifdef SIGPIPE
    signal( SIGPIPE, SIG_IGN );
#endif
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
}

std::unique_ptr<Client> start_client( int idx, const char * ip, uint16_t port, int mode, int max_len, int test_times, int lost_rate, int interval = default_send_interval )
{
    // Build one client with its own timing and sequence state.
    std::unique_ptr<Client> client = std::make_unique<Client>( ip, port );
    client->setmode( mode );
    client->setauto( true, test_times, max_len );
    client->setlostrate( lost_rate );
    client->setsendinterval( interval );
    client->set_index( idx );
    client->set_connected_handler(
        [idx](uint32_t conv) {
            std::cout << "[Client " << idx << " connected] conv=" << conv << "\n";
        });
    // client->set_show_info( true );
    return client;
}

int main( int argc, char ** argv )
{
#ifdef _WIN32
    // Windows needs explicit Winsock initialization before creating client sockets.
    WSADATA wsaData{};
    int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0) {
        printf("WSAStartup failed: %d\n", rc);
        return 1;
    }
#endif

    handle_signal();

    // This entrypoint spawns multiple independent clients to stress the server concurrently.
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

    // Each client gets its own recv/send threads, so they behave like separate peers.
    std::vector<std::unique_ptr<Client>> clients;
    for ( int i = 0; i != client_cnt; ++i ) {
        clients.push_back( start_client( i + 1, ip.c_str(), port, mode, max_len, test_times, lost_rate, send_interval ) );
    }
    for (auto& c : clients) {
        c->set_show_info(true);
    }

    std::vector<joining_thread> recv_threads;
    for ( int i = 0; i != client_cnt; ++i ) {
        recv_threads.emplace_back( &Client::recv_work, clients[i].get() );
    }
    // rand_send_work() generates random payloads to exercise packet reordering and loss handling.
    std::vector<joining_thread> send_threads;
    for (int i = 0; i != client_cnt;++i) {
        send_threads.emplace_back(&Client::rand_send_work, clients[i].get());
    }

    // Stop when all clients have finished or the process receives a signal.
    while ( g_running ) {
        bool any_running = false;
        for (const auto& client : clients) {
            if (client->running()) {
                any_running = true;
                break;
            }
        }
        if (!any_running) {
            break;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds { 1 } );
    }

    for ( int i = 0; i != client_cnt; ++i ) {
        clients[i]->terminate();
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
