//#include <asio2/asio2.hpp>
#include <iostream>
#include <asio2/tcp/tcp_server.hpp>

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	std::string_view host = "0.0.0.0";
	std::string_view port = "8025";

	asio2::tcp_server server;

	server.bind_recv([&](auto & session_ptr, std::string_view s)
	{
		printf("recv : %u %.*s\n", (unsigned)s.size(), (int)s.size(), s.data());

		// ##Use this to check whether the send operation is running in current thread.
		//if (session_ptr->io().strand().running_in_this_thread())
		//{
		//}

		// force one packet data to be sent twice, and the client will recvd compeleted packet
		session_ptr->async_send(s.substr(0, s.size() / 2), []() {});
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		session_ptr->async_send(s.substr(s.size() / 2), [](std::size_t bytes_sent) {std::ignore = bytes_sent; });

	}).bind_connect([&](auto & session_ptr)
	{
		session_ptr->no_delay(true);

		//session_ptr->stop(); // You can close the connection directly here.

		printf("client enter : %s %u %s %u\n",
			session_ptr->remote_address().c_str(), session_ptr->remote_port(),
			session_ptr->local_address().c_str(), session_ptr->local_port());

	}).bind_disconnect([&](auto & session_ptr)
	{
		printf("client leave : %s %u %s\n",
			session_ptr->remote_address().c_str(),
			session_ptr->remote_port(), asio2::last_error_msg().c_str());
	}).bind_start([&](asio::error_code ec)
	{
		printf("start tcp server character : %s %u %d %s\n",
			server.listen_address().c_str(), server.listen_port(),
			ec.value(), ec.message().c_str());
	}).bind_stop([&](asio::error_code ec)
	{
		printf("stop : %d %s\n", ec.value(), ec.message().c_str());
	});

	//server.start(host, port, '>');
	server.start(host, port, "\r\n");

	while (std::getchar() != '\n');

	return 0;
}
