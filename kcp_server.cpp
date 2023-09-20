#include <signal.h>

#include "server.h"
#include "src/joining_thread.h"

constexpr auto default_port = 9527;
constexpr auto default_lost_rate = 0;

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

    uint16_t port = default_port;
    int mode = 0;
    int lost_rate = default_lost_rate;

    if ( argc >= 2 ) {
        port = atoi( argv[1] );
    }

    if ( argc >= 3 ) {
        mode = atoi( argv[2] );
    }

    if ( argc >= 4 ) {
        lost_rate = atoi( argv[3] );
    }

    printf( "Usage:<%s>  <port>:%d  <mode>:%s <lost_rate>:%d\n", argv[0], port, util::get_mode_name( mode ), lost_rate );
    std::unique_ptr<Server> server = std::make_unique<Server>( port, conv );
    server->setmode( mode );
    server->setlostrate( lost_rate );

    // server->show_data( true );
    //     util::ikcp_set_log(IKCP_LOG_INPUT|IKCP_LOG_OUTPUT);

    joining_thread accept( &Server::accept, server.get() );
    joining_thread work( &Server::run, server.get() );

    return 0;
}
