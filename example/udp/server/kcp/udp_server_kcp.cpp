//#include <asio2/asio2.hpp>
#include <iostream>
#include <asio2/udp/udp_server.hpp>

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	std::string_view host = "0.0.0.0";
	std::string_view port = "8036";

	asio2::udp_server server;

	server.bind_recv([](std::shared_ptr<asio2::udp_session> & session_ptr, std::string_view s)
	{
		printf("recv : %u %.*s\n", (unsigned)s.size(), (int)s.size(), s.data());
		session_ptr->async_send(s, []() {});
	}).bind_connect([](auto & session_ptr)
	{
		printf("client enter : %s %u %s %u\n", session_ptr->remote_address().c_str(), session_ptr->remote_port(),
			session_ptr->local_address().c_str(), session_ptr->local_port());
	}).bind_disconnect([](auto & session_ptr)
	{
		printf("client leave : %s %u %s\n", session_ptr->remote_address().c_str(),
			session_ptr->remote_port(), asio2::last_error_msg().c_str());
	}).bind_handshake([](auto & session_ptr, asio::error_code ec)
	{
		(void)session_ptr;
		printf("client handshake : %d %s\n", ec.value(), ec.message().c_str());
	}).bind_start([&](asio::error_code ec)
	{
		if (ec)
			printf("start udp server kcp failure : %d %s\n", asio2::last_error_val(), asio2::last_error_msg().c_str());
		else
			printf("start udp server kcp success : %s %u\n", server.listen_address().c_str(), server.listen_port());
		//server.stop();
	}).bind_stop([&](asio::error_code ec)
	{
		printf("stop : %d %s\n", ec.value(), ec.message().c_str());
	}).bind_init([&]()
	{
		//// Join the multicast group. you can set this option in the on_init(_fire_init) function.
		//server.acceptor().set_option(
		//	// for ipv6, the host must be a ipv6 address like 0::0
		//	asio::ip::multicast::join_group(asio::ip::make_address("ff31::8000:1234")));
		//	// for ipv4, the host must be a ipv4 address like 0.0.0.0
		//	//asio::ip::multicast::join_group(asio::ip::make_address("239.255.0.1")));
	});

	// to use kcp, the last param must be : asio2::use_kcp
	server.start(host, port, asio2::use_kcp); 

	while (std::getchar() != '\n');

	server.stop();

	return 0;
}
