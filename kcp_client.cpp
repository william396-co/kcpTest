#include <signal.h>

#include "client.h"
#include "src/joining_thread.h"

constexpr auto default_ip = "127.0.0.1";
constexpr auto default_port = 9527;

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

    if ( argc >= 2 ) {
        ip = argv[1];
    }

    if ( argc >= 3 ) {
        port = atoi( argv[2] );
    }

    if ( argc >= 4 ) {
        mode = atoi( argv[3] );
    }

    srand( time( nullptr ) );
    std::unique_ptr<Client> client = std::make_unique<Client>( ip.c_str(), port, conv );
    client->setmode( mode );
    client->setauto( true, 1000 );

    joining_thread work( &Client::run, client.get() );
    //  joining_thread input( &Client::input, client.get() );
    joining_thread auto_input( &Client::auto_input, client.get() );

    return 0;
}
