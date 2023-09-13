#include <signal.h>

#include "server.h"
#include "src/joining_thread.h"

constexpr auto ip = "127.0.0.1";
constexpr auto port = 9527;

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
    if ( argc >= 2 ) {
        mode = atoi( argv[1] );
    }
    std::unique_ptr<Server> server = std::make_unique<Server>( port, conv );
    server->setmode( mode );
    // server->show_data( true );
    //     util::ikcp_set_log(IKCP_LOG_INPUT|IKCP_LOG_OUTPUT);

    joining_thread accept( &Server::accept, server.get() );
    joining_thread work( &Server::run, server.get() );

    return 0;
}
