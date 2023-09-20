#include <signal.h>

#include "client.h"
#include "src/joining_thread.h"

constexpr auto default_ip = "127.0.0.1";
constexpr auto default_port = 9527;
constexpr auto default_max_len = 500;
constexpr auto default_test_times = 1000;
constexpr auto default_lost_rate = 0;
constexpr auto default_send_interval = 30; // ms

bool is_running = true;

void signal_handler( int sig )
{
    is_running = false;
}
void handle_signal()
{
    signal( SIGPIPE, SIG_IGN );
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
}

int main( int argc, char ** argv )
{
    handle_signal();

    int mode = 0;
    std::string ip = default_ip;
    uint16_t port = default_port;
    size_t max_len = default_max_len;
    int test_times = default_test_times;
    int lost_rate = default_lost_rate;
    int send_interval = default_send_interval;

    if ( argc >= 2 ) {
        ip = argv[1];
    }

    if ( argc >= 3 ) {
        port = atoi( argv[2] );
    }

    if ( argc >= 4 ) {
        mode = atoi( argv[3] );
    }

    if ( argc >= 5 ) {
        max_len = atoi( argv[4] );
    }

    if ( argc >= 6 ) {
        test_times = atoi( argv[5] );
    }

    if ( argc >= 7 ) {
        lost_rate = atoi( argv[6] );
    }

    if ( argc >= 8 ) {
        send_interval = atoi( argv[7] );
    }

    printf( "Usage:<%s> <ip>:%s <port>:%d  <mode>:%s <max_len>:%d <times>:%d <lost_rate>:%d% <send_interval>:%dms\n",
        argv[0],
        ip.c_str(),
        port,
        util::get_mode_name( mode ),
        max_len,
        test_times,
        lost_rate,
        send_interval );

    std::unique_ptr<Client> client = std::make_unique<Client>( ip.c_str(), port, conv );
    client->setmode( mode );
    client->setauto( true, test_times, max_len );
    client->setlostrate( lost_rate );
    client->setsendinterval( send_interval );

    joining_thread work( &Client::run, client.get() );
    joining_thread input( &Client::input, client.get() );
    joining_thread auto_input( &Client::auto_input, client.get() );

    return 0;
}
