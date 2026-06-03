#include <signal.h>
#include <string>

#include "server.h"
#include "src/connection.h"
#include "src/joining_thread.h"
#include "src/packet.h"

constexpr auto default_port = 9527;
constexpr auto default_lost_rate = 0;

bool is_running = false;

void signal_handler( int sig )
{
    is_running = false;
}
void handle_signal()
{
#ifdef SIGPIPE
    signal( SIGPIPE, SIG_IGN );
#endif
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
}

int main( int argc, char ** argv )
{
#ifdef _WIN32
    // Windows needs explicit Winsock initialization before the server opens its UDP socket.
    WSADATA wsaData{};
    int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0) {
        printf("WSAStartup failed: %d\n", rc);
        return 1;
    }
#endif

    handle_signal();

    // CLI overrides keep the server simple to run in different test modes.
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
    // The server owns the shared listen socket and all logical KCP connections.
    std::unique_ptr<Server> server = std::make_unique<Server>( port );
    server->setmode( mode );
    server->setlostrate( lost_rate );
    server->show_data( true );
    // Demo application behavior: echo each decoded KCP payload back with a server prefix.
    server->set_application_message_handler(
        [](Connection& conn, const char* data, size_t len) {
            constexpr char prefix[] = "[Server send back to you] -> ";
            DecodedAppMessage message{};
            if (!decode_app_message(data, len, message)) {
                return;
            }

            std::string reply_text(prefix);
            reply_text.append(message.payload, message.payload + message.size);

            std::string reply = encode_app_message(
                message.sn,
                message.ts,
                reply_text.data(),
                static_cast<uint32_t>(reply_text.size()));
            conn.push_snd_queu(std::move(reply));
        });
    // startService() launches the UDP receive thread plus shard worker threads.
    server->startService();
    is_running = true;

    // Main thread only waits for a signal; work happens on the server threads.
    while (is_running) {

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
