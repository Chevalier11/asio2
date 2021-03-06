#ifndef ASIO2_USE_SSL
#define ASIO2_USE_SSL
#endif

//#include <asio2/asio2.hpp>
#include <asio2/http/wss_client.hpp>
#include <iostream>

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	std::string_view host = "127.0.0.1";
	std::string_view port = "8007";

	asio2::wss_client client;

	client.connect_timeout(std::chrono::seconds(10));

	//client.upgrade_target("/ws");

	client.set_verify_mode(asio::ssl::verify_peer);
	client.set_cert_file(
		"../../cert/ca.crt",
		"../../cert/client.crt",
		"../../cert/client.key",
		"123456");

	client.post([]() {}, std::chrono::seconds(3));

	client.bind_init([&]()
	{
		// how to set custom websocket request data : 
		client.ws_stream().set_option(websocket::stream_base::decorator(
			[](websocket::request_type& req)
		{
			req.set(http::field::authorization, " ssl-websocket-client-coro");
		}));

	}).bind_recv([&](std::string_view data)
	{
		printf("recv : %u %.*s\n", (unsigned)data.size(), (int)data.size(), data.data());

		client.async_send(data);

	}).bind_connect([&](asio::error_code ec)
	{
		if (ec)
			printf("connect failure : %d %s\n", ec.value(), ec.message().c_str());
		else
			printf("connect success : %s %u\n", client.local_address().c_str(), client.local_port());

		// a new thread.....
		std::thread([&]()
		{
			// inner this new thread, we post a task, the task must be executed
			// in the client's io_context thread, not in this new thread.
			client.post([&]()
			{
				std::string s;
				s += '<';
				int len = 128 + std::rand() % (300);
				for (int i = 0; i < len; i++)
				{
					s += (char)((std::rand() % 26) + 'a');
				}
				s += '>';

				client.async_send(std::move(s));
			});
		}).join();

	}).bind_upgrade([&](asio::error_code ec)
	{
		if (ec)
			std::cout << "upgrade failure : " << ec.value() << " " << ec.message() << std::endl;
		else
			std::cout << "upgrade success : " << client.upgrade_response() << std::endl;
	}).bind_disconnect([](asio::error_code ec)
	{
		printf("disconnect : %d %s\n", ec.value(), ec.message().c_str());
	});

	asio2::rdc::option rdc_option{ [](std::string_view) { return 0; } };

	asio2::socks5::option<asio2::socks5::method::anonymous> sock5_option
	{
		"127.0.0.1",
		10808
	};

	client.async_start(host, port);
	client.async_start(host, port, "/user");
	client.async_start(host, port, "/user", std::move(rdc_option), std::move(sock5_option));

	while (std::getchar() != '\n');

	return 0;
}

